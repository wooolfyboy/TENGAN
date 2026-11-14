// ============================================================
//  BLE RADAR  —  визуальный сканер Bluetooth для LilyGO T-Embed CC1101
//
//  Режим 1 (RADAR):     сколько BLE-устройств вокруг (число + кольцо).
//  Режим 2 (PROXIMITY): ближайшее устройство: шкала, ~метры, dBm, вендор.
//  Режим 3 (SCOPE):     радар-скоуп. Радиус = реальный (RSSI), угол = по MAC.
//                       Цвет блипа = вендор (Apple/MS/Samsung/Google/...).
//  Режим 4 (TIMELINE):  число устройств во времени.
//  Режим 5 (TYPES):     кольцевая диаграмма: какие устройства вокруг
//                       (телефоны, наушники, часы, метки, ...).
//  Режим 6 (LIST):      список устройств (ближние сверху). Колесо — выбор,
//                       нажатие колеса — подробности. Боковая кнопка — дальше.
//  Режим 8 (SETTINGS):  дальность, светодиоды, яркость (запоминаются).
//  Режим 7 (WEATHER):   температура/влажность от BLE-термометров вокруг.
//  Счёт идёт по «физическим» устройствам: смена случайного MAC-адреса
//  склеивается по отпечатку рекламы, чтобы один телефон не считался дважды.
//  Кнопка (энкодер или боковая) листает режимы.
//
//  Весь код: Claude. Распиновка — из репозитория LilyGO.
// ============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include <functional>
#include <math.h>
#include "devtypes.h"
#include "sensors.h"
#include "btdb.h"
#include <stdio.h>
#include <string.h>

// ---------- пины ----------
#define BOARD_PWR_EN 15
#define PIN_TFT_CS   41
#define PIN_SD_CS    13
#define PIN_LORA_CS  12
#define PIN_NRF_CS   44
#define PIN_ENC_KEY  0
#define PIN_ENC_A    4    // энкодер фаза A
#define PIN_ENC_B    5    // энкодер фаза B
#define PIN_USER_KEY 6
#define PIN_WS2812   14
#define WS2812_N     8

static const uint32_t REMEMBER_MS = 30000;
static const uint32_t REDRAW_MS   = 66;
// Разделы главного меню и экраны раздела Bluetooth
enum { S_MENU = 0, S_BLE, S_WIFI, S_SET };
enum { M_RADAR = 0, M_PROX, M_SCOPE, M_TIMELINE, M_TYPES, M_LIST, M_WEATHER, NUM_MODES };
static const int W_SCREENS = 5;   // экранов в разделе Wi-Fi (см. wifi_ui.h)
static const uint32_t SENSOR_REMEMBER_MS = 300000;  // датчики шлют редко — помним 5 минут
static const size_t   MAX_SENSORS = 24;
static const size_t   MAX_DEVS    = 600;     // предохранитель памяти
// склейка смены адреса: старый адрес должен замолчать хотя бы на STALE_MIN
// и не дольше STALE_MAX назад, а сила сигнала — быть похожей
static const uint32_t STALE_MIN   = 1500;
static const uint32_t STALE_MAX   = 12000;
static const int      MERGE_DB    = 10;

TFT_eSPI    tft;
TFT_eSprite spr = TFT_eSprite(&tft);

// категории вендоров
// 0 = неизвестно (private), 1 = прочий производитель (имя берётся из базы btdb.h),
// 2.. = крупные бренды со своим цветом (таблица BRANDS ниже)
enum { V_PRIV = 0, V_OTHER = 1 };

// Dev — один MAC-адрес. ent — номер «физического» устройства:
// у нескольких адресов одного телефона (после смены MAC) он общий.
// виды адресов
enum { A_PUBLIC = 0, A_STATIC, A_ROTATING, A_NONRES };

struct Dev {
  uint32_t t;        // когда последний раз слышали
  uint32_t first;    // когда впервые услышали (с учётом склейки)
  uint32_t ent;      // id физического устройства
  uint32_t sig;      // отпечаток рекламы
  uint32_t pkts;     // сколько пакетов поймали
  int8_t   rssi;     // последний сигнал
  int8_t   rssiS;    // сглаженный сигнал (для сортировки)
  int8_t   rmin, rmax;
  uint8_t  cat;      // бренд (V_* / индекс в BRANDS)
  uint8_t  vkind;    // откуда узнали производителя: 0 нет, 1 Company ID, 2 UUID сервиса
  uint8_t  vscore;   // насколько надёжно (3 бренд, 2 прочий, 1 производитель чипа)
  uint16_t vcode;    // сам Company ID или UUID
  uint8_t  type;     // тип устройства (DevType)
  uint8_t  akind;    // вид адреса (A_*)
  bool     rnd;      // адрес случайный и меняющийся
  bool     merged;   // уже стал «предшественником» нового адреса
  bool     inRange;  // ближе порога дальности (с гистерезисом)
  bool     hasCid;
  uint16_t cid;      // Company ID
  uint16_t appearance;
  uint8_t  mac[6];   // адрес (младший байт первым)
  uint8_t  mfr[6];   // первые байты данных производителя
  uint8_t  mfrLen;   // их полная длина
  uint16_t uuids[4]; // сервисы
  uint8_t  nU;
  char     name[20];
};
// Ent — физическое устройство (агрегат по его адресам)
struct Ent {
  uint32_t t, first, pkts;
  int      rssi, rssiS;
  uint8_t  cat, type, nMacs;
  uint8_t  vkind, vscore;
  uint16_t vcode;
  uint64_t key;      // ключ самого свежего адреса
  char     name[20];
};
// датчик погоды
struct Sensor {
  uint32_t t;
  int8_t   rssi;
  SensorReading r;
  uint8_t  mac[6];
  char     name[20];
};
std::map<uint64_t, Sensor> g_sens;
struct Blip { int rssi; uint32_t h; uint8_t cat; };
std::map<uint64_t, Dev> g_seen;
uint32_t g_nextEnt = 0;
uint32_t g_merges  = 0;         // сколько смен адреса склеили (для статистики)

// --- состояние списка ---
std::vector<uint32_t> g_order;  // порядок устройств в списке (id), обновляется раз в секунду
uint32_t g_orderMs = 0;
uint32_t g_sel = 0;             // выбранное устройство (id)
int      g_top = 0;             // первая видимая строка
volatile int g_listMove = 0;    // накопленные щелчки колеса
bool     g_detail = false;      // открыт экран подробностей
uint32_t g_lastTouch = 0;       // когда последний раз трогали колесо/кнопку в списке
static const int      ORDER_HYST_DB = 8;      // меняем местами, только если разница > 8 dB
static const uint32_t ORDER_EVERY   = 2000;   // как часто обновлять порядок
static const uint32_t ORDER_FREEZE  = 4000;   // после касания колеса порядок замирает
// история сигнала выбранного устройства (для графика в подробностях)
#define SPARK_N 60
int8_t   g_spark[SPARK_N];
int      g_sparkN = 0;
uint32_t g_sparkEnt = 0, g_sparkMs = 0;
SemaphoreHandle_t g_lock;

volatile int g_mode = 0;         // экран внутри раздела Bluetooth
int  g_section = S_MENU;         // где мы: меню / Bluetooth / Wi-Fi / настройки
int  g_menuSel = 0;              // выбранный пункт меню (0 BT, 1 Wi-Fi, 2 настройки)
bool g_sub = true;               // в разделе показано подменю (список экранов), а не сам экран
int  g_subSel = 0;               // курсор в подменю
bool g_bleActive = true;
int  g_sensCount = 0;            // сколько датчиков погоды сейчас известно         // сейчас сканирует Bluetooth (иначе — Wi-Fi)
inline bool bleIs(int m) { return g_section == S_BLE && g_mode == m; }
int g_sweep = 0;

// --- настраиваемый порог события (крутилка) ---
int      g_thresh = 7;           // порог событий для светодиодов (0 = выкл)

// --- настройки (хранятся в памяти платы) ---
// Дальность: устройство засчитывается, когда его сглаженный сигнал >= порога,
// и выбывает, только когда упал ниже порога ещё на RANGE_HYST_DB.
struct RangeOpt { int dbm; const char* name; };
static const RangeOpt RANGES[] = {
  { -127, "all" }, { -90, "far" }, { -80, "room" }, { -70, "near" }, { -60, "very near" },
};
static const int N_RANGES = sizeof(RANGES) / sizeof(RANGES[0]);
static const int RANGE_HYST_DB = 5;
static const uint32_t MIN_PKTS = 2;        // одиночный случайный пакет не считается
int      g_range = 0;                      // индекс в RANGES
int      g_ledBright = 50;                 // % яркости ленты
int      g_scrBright = 100;                // % яркости экрана
int      g_setItem = 0;                    // выбранный пункт настроек
bool     g_setDirty = false;
uint32_t g_setChangedMs = 0;
Preferences g_prefs;
uint32_t g_encActiveMs = 0;

// --- история для TIMELINE ---
#define HIST_N 150
struct Sample { int count; float base; int8_t ev; };
Sample   g_hist[HIST_N];
int      g_histLen = 0;
float    g_base = -1.0f;
volatile int g_lastCount = 0;
int      g_lastRaw = 0;          // сколько MAC-адресов (без склейки)
float    g_pieDisp[T_NUM] = {0}; // плавная анимация диаграммы
uint32_t g_lastSampleMs = 0;
uint32_t g_cooldownMs = 0;

// --- светодиоды на события ---
Adafruit_NeoPixel strip(WS2812_N, PIN_WS2812, NEO_GRB + NEO_KHZ800);
int      g_ledEvent = 0;        // 0 нет, +1 пришли, -1 ушли
uint32_t g_ledStart = 0;
const uint32_t LED_DUR = 1200;

// ---------- производители ----------
// Крупные бренды: имя на экране, цвет, ключ (первое слово имени в базе Bluetooth)
struct Brand { const char* label; const char* key; uint8_t r, g, b; };
static const Brand BRANDS[] = {
  { "private",   "",          120, 120, 130 },   // 0  V_PRIV
  { "other",     "",          175, 145, 105 },   // 1  V_OTHER
  { "Apple",     "apple",     240, 240, 240 },
  { "Microsoft", "microsoft",  80, 150, 240 },
  { "Samsung",   "samsung",    60, 205, 205 },
  { "Google",    "google",    240, 200,  70 },
  { "Xiaomi",    "xiaomi",    255, 120,  30 },
  { "Huawei",    "huawei",    230,  60,  60 },
  { "Honor",     "honor",     100, 130, 255 },
  { "OnePlus",   "oneplus",   245,  50,  70 },
  { "Oppo",      "oppo",       40, 180,  90 },
  { "Realme",    "realme",    250, 220,   0 },
  { "vivo",      "vivo",       70, 110, 245 },
  { "Motorola",  "motorola",  110, 110, 220 },
  { "Sony",      "sony",      190, 120, 240 },
  { "LG",        "lg",        220,  60, 140 },
  { "Amazfit",   "huami",     150, 230,  80 },
  { "Garmin",    "garmin",     60, 160, 210 },
  { "Bose",      "bose",      200, 170, 130 },
  { "JBL",       "harman",    255,  90,  40 },
  { "Amazon",    "amazon",    255, 165,  60 },
  { "Meta",      "meta",       70, 110, 250 },
  { "Tile",      "tile",       60, 200, 240 },
  { "Nothing",   "nothing",   210, 210, 210 },
  { "Anker",     "anker",      70, 170, 230 },
  { "Logitech",  "logitech",    0, 190, 170 },
};
static const int N_BRANDS = sizeof(BRANDS) / sizeof(BRANDS[0]);

// производители Bluetooth-чипов: их код говорит о «начинке», а не о бренде устройства
bool isChipMaker(uint16_t cid) {
  switch (cid) {
    case 0x0002: case 0x0009: case 0x000A: case 0x000D: case 0x000F: case 0x001D:
    case 0x0046: case 0x0059: case 0x005D: case 0x0094: case 0x00D7: case 0x0131:
    case 0x0211: case 0x02B0: case 0x02E5: case 0x02FF: case 0x03E0: case 0x05D6:
    case 0x0642: case 0x07E3: case 0x0917: case 0x00D2:
      return true;
  }
  return false;
}

// UUID, которыми пользуются устройства любых брендов (о производителе ничего не говорят)
bool isSharedUuid(uint16_t u) {
  return u == 0xFD6F /* уведомления о контактах */ || u == 0xFEAA /* Eddystone */ ||
         u == 0xFE2C /* Fast Pair в чужих наушниках */;
}

uint8_t brandFromName(const char* name) {
  if (!name) return V_OTHER;
  char w[16]; int i = 0;
  while (name[i] && name[i] != ' ' && name[i] != '.' && i < 15) { w[i] = (char)tolower((unsigned char)name[i]); i++; }
  w[i] = 0;
  for (int b = 2; b < N_BRANDS; b++) if (!strcmp(w, BRANDS[b].key)) return (uint8_t)b;
  return V_OTHER;
}

struct Vend { uint8_t cat, kind, score; uint16_t code; };

// Выбираем лучшего «кандидата в производители» из Company ID и UUID сервисов
Vend resolveVendor(const AdvInfo& ai) {
  Vend best = { V_PRIV, 0, 0, 0 };
  auto consider = [&](uint8_t kind, uint16_t code, const char* name, bool chip) {
    uint8_t cat = brandFromName(name);
    uint8_t score = (cat >= 2) ? 3 : (chip || !name) ? 1 : 2;
    if (score > best.score) best = { cat, kind, score, code };
  };
  if (ai.hasMfr) consider(1, ai.cid, companyName(ai.cid), isChipMaker(ai.cid));
  for (uint8_t i = 0; i < ai.nUuid; i++) {
    uint16_t u = ai.uuids[i];
    if (isSharedUuid(u)) continue;
    const char* owner = uuidOwner(u);
    if (owner) consider(2, u, owner, false);
  }
  return best;
}

uint16_t vendorColor(uint8_t c) {
  const Brand& b = BRANDS[c < N_BRANDS ? c : V_OTHER];
  return tft.color565(b.r, b.g, b.b);
}
// имя производителя для экрана
const char* vendorLabel(uint8_t cat, uint8_t kind, uint16_t code) {
  if (cat >= 2 && cat < N_BRANDS) return BRANDS[cat].label;
  if (kind == 1) { const char* n = companyName(code); if (n) return n; }
  if (kind == 2) { const char* n = uuidOwner(code);   if (n) return n; }
  if (kind == 1) {
    static char hex[8]; sprintf(hex, "0x%04X", code); return hex;
  }
  return "-";
}

// RSSI -> ~метры (грубая оценка по модели затухания)
float estDistance(int rssi) {
  const float MEASURED = -59.0f; // ожидаемый RSSI на 1 м
  const float N = 2.5f;          // показатель затухания (помещение)
  float d = powf(10.0f, (MEASURED - (float)rssi) / (10.0f * N));
  if (d < 0.1f) d = 0.1f;
  if (d > 99.0f) d = 99.0f;
  return d;
}

// ---------- RADAR цвета ----------
uint16_t colorForCount(int n) {
  if (n <= 2)  return tft.color565(70, 200, 130);
  if (n <= 6)  return tft.color565(235, 205, 70);
  if (n <= 12) return tft.color565(240, 150, 50);
  return tft.color565(235, 80, 80);
}
const char* labelForCount(int n) {
  if (n <= 2) return "calm"; if (n <= 6) return "busy";
  if (n <= 12) return "crowded"; return "packed";
}

uint16_t typeColor(uint8_t t) {
  uint8_t r, g, b; typeRGB(t, r, g, b);
  return tft.color565(r, g, b);
}

// переносим разобранные поля рекламы в запись об адресе
void fillAdv(Dev& v, const AdvInfo& ai) {
  if (ai.nameLen && !v.name[0]) {
    strncpy(v.name, ai.name, sizeof(v.name) - 1); v.name[sizeof(v.name) - 1] = 0;
  }
  if (ai.hasMfr) {
    v.hasCid = true; v.cid = ai.cid; v.mfrLen = ai.mfrLen;
    memcpy(v.mfr, ai.mfr, sizeof(v.mfr));
  }
  if (ai.hasApp) v.appearance = ai.appearance;
  for (uint8_t i = 0; i < ai.nUuid; i++) {
    bool have = false;
    for (uint8_t k = 0; k < v.nU; k++) if (v.uuids[k] == ai.uuids[i]) have = true;
    if (!have && v.nU < 4) v.uuids[v.nU++] = ai.uuids[i];
  }
}

// ---------- колбэк сканера (NimBLE 2.x) ----------
class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    const NimBLEAddress& addr = d->getAddress();
    uint64_t key = (uint64_t)addr | ((uint64_t)addr.getType() << 48);
    int rssi = d->getRSSI();
    const uint8_t* mac = addr.getVal();

    AdvInfo ai;
    const std::vector<uint8_t>& pl = d->getPayload();
    parseAdv(pl.data(), pl.size(), ai);
    SensorReading sr;
    bool isSensor = parseSensor(pl.data(), pl.size(), sr);
    Vend     vend = resolveVendor(ai);
    uint8_t  type = classify(ai);
    if (isSensor) type = T_SENSOR;
    uint32_t sig  = advSignature(ai);
    uint8_t akind = A_PUBLIC;
    if (addr.getType() == BLE_ADDR_RANDOM) {
      switch (mac[5] >> 6) {          // два старших бита адреса
        case 3:  akind = A_STATIC;   break;
        case 1:  akind = A_ROTATING; break;
        default: akind = A_NONRES;   break;
      }
    }
    bool rnd = (akind == A_ROTATING || akind == A_NONRES);

    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(20)) != pdTRUE) return;
    uint32_t now = millis();
    auto it = g_seen.find(key);
    if (it != g_seen.end()) {
      Dev& v = it->second;
      v.t = now; v.rssi = rssi; v.pkts++;
      v.rssiS = (int8_t)((v.rssiS * 3 + rssi) / 4);
      if (rssi < v.rmin) v.rmin = rssi;
      if (rssi > v.rmax) v.rmax = rssi;
      if (vend.score > v.vscore) { v.cat = vend.cat; v.vkind = vend.kind; v.vcode = vend.code; v.vscore = vend.score; }
      if (type != T_UNKNOWN) v.type = type;
      if (!v.sig) v.sig = sig;
      fillAdv(v, ai);
    } else if (g_seen.size() < MAX_DEVS) {
      Dev nd; memset(&nd, 0, sizeof(nd));
      nd.t = now; nd.first = now; nd.sig = sig; nd.pkts = 1;
      nd.rssi = nd.rssiS = nd.rmin = nd.rmax = (int8_t)rssi;
      nd.cat = vend.cat; nd.vkind = vend.kind; nd.vcode = vend.code; nd.vscore = vend.score;
      nd.type = type; nd.akind = akind; nd.rnd = rnd;
      memcpy(nd.mac, mac, 6);
      fillAdv(nd, ai);
      // ищем «предшественника»: адрес с тем же отпечатком, который недавно замолчал
      if (rnd && sig) {
        Dev* best = nullptr; int bestDiff = MERGE_DB + 1;
        for (auto& kv : g_seen) {
          Dev& o = kv.second;
          if (!o.rnd || o.merged || o.sig != sig) continue;
          uint32_t age = now - o.t;
          if (age < STALE_MIN || age > STALE_MAX) continue;
          int diff = abs((int)o.rssi - rssi);
          if (diff < bestDiff) { bestDiff = diff; best = &o; }
        }
        if (best) {
          best->merged = true;
          nd.ent = best->ent;
          nd.first = best->first;
          nd.pkts += best->pkts;
          if (best->rmin < nd.rmin) nd.rmin = best->rmin;
          if (best->rmax > nd.rmax) nd.rmax = best->rmax;
          if (nd.type == T_UNKNOWN) nd.type = best->type;
          if (best->vscore > nd.vscore) {
            nd.cat = best->cat; nd.vkind = best->vkind; nd.vcode = best->vcode; nd.vscore = best->vscore;
          }
          if (!nd.name[0]) memcpy(nd.name, best->name, sizeof(nd.name));
          g_merges++;
        }
      }
      if (!nd.ent) nd.ent = ++g_nextEnt;
      g_seen[key] = nd;
    }

    // показания датчика погоды
    if (isSensor) {
      auto st = g_sens.find(key);
      if (st == g_sens.end() && g_sens.size() < MAX_SENSORS) {
        Sensor ns; memset(&ns, 0, sizeof(ns));
        memcpy(ns.mac, mac, 6);
        st = g_sens.insert({ key, ns }).first;
      }
      if (st != g_sens.end()) {
        Sensor& S = st->second;
        S.t = now; S.rssi = rssi;
        if (sr.hasT)   { S.r.temp = sr.temp;   S.r.hasT = true; }
        if (sr.hasH)   { S.r.hum = sr.hum;     S.r.hasH = true; }
        if (sr.hasB)   { S.r.batt = sr.batt;   S.r.hasB = true; }
        if (sr.hasP)   { S.r.press = sr.press; S.r.hasP = true; }
        if (sr.hasCO2) { S.r.co2 = sr.co2;     S.r.hasCO2 = true; }
        S.r.model = sr.model;
        if (ai.nameLen && !S.name[0]) {
          strncpy(S.name, ai.name, sizeof(S.name) - 1); S.name[sizeof(S.name) - 1] = 0;
        }
      }
    }
    xSemaphoreGive(g_lock);
  }
};

// ---------- (настройка порога пока отключена; колесо листает режимы) ----------
void drawThresh() {}

// ---------- RADAR ----------
void drawRadar(int count) {
  int W = spr.width(), H = spr.height();
  uint16_t theme = colorForCount(count);
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("BLE RADAR", W / 2, 6, 4);
  int cx = W / 2, cy = H / 2 + 8, R = 54, slots = 24;
  for (int i = 0; i < slots; i++) {
    float a = ((float)i / slots) * 2.0f * PI - (PI / 2.0f);
    int x = cx + (int)(cos(a) * R), y = cy + (int)(sin(a) * R);
    uint16_t c = (i < count) ? theme : tft.color565(40, 40, 48);
    spr.fillCircle(x, y, 3, c);
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawNumber(count, cx, cy, 7);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(theme, TFT_BLACK);
  spr.drawString(labelForCount(count), W / 2, H - 6, 4);
  // сколько было «сырых» адресов до склейки
  if (g_lastRaw != count) {
    char buf[24]; sprintf(buf, "%d MAC", g_lastRaw);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString(buf, W - 4, 4, 2);
  }
  if (g_range > 0) {                 // включён фильтр дальности
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString(RANGES[g_range].name, 4, 4, 2);
  }
  drawThresh();
  spr.pushSprite(0, 0);
}

// ---------- PROXIMITY ----------
void drawProximity(bool have, int rssi, const Ent& best, int count) {
  uint8_t cat = best.cat, type = best.type;
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("PROXIMITY", W / 2, 4, 4);

  if (!have) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString("no devices", W / 2, H / 2, 4);
    drawThresh();
    spr.pushSprite(0, 0);
    return;
  }

  int pct = (rssi + 95) * 100 / 50;
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  uint16_t col; const char* word;
  if (pct > 66)      { col = tft.color565(235, 80, 80);  word = "CLOSE"; }
  else if (pct > 33) { col = tft.color565(240, 185, 60); word = "NEAR";  }
  else               { col = tft.color565(70, 200, 130); word = "FAR";   }

  int bw = W - 60, bh = 26, bx = 30, by = H / 2 - 14;
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(col, TFT_BLACK);
  spr.drawString(word, W / 2, by - 6, 4);
  spr.drawRoundRect(bx, by, bw, bh, 4, tft.color565(70, 70, 80));
  int fillw = (bw - 4) * pct / 100;
  if (fillw > 0) spr.fillRoundRect(bx + 2, by + 2, fillw, bh - 4, 3, col);

  char buf[48];
  float d = estDistance(rssi);
  if (d < 10.0f) sprintf(buf, "~%.1f m  %d dBm", d, rssi);
  else           sprintf(buf, "~%.0f m  %d dBm", d, rssi);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W / 2, by + bh + 8, 4);

  // строка «тип  вендор  (N near)» — тип и вендор каждый своим цветом
  char vbuf[48];
  sprintf(vbuf, "  %s   (%d near)", vendorLabel(cat, best.vkind, best.vcode), count);
  const char* tn = typeName(type);
  int tw = spr.textWidth(tn, 2), vw = spr.textWidth(vbuf, 2);
  int x0 = W / 2 - (tw + vw) / 2, ty = by + bh + 34;
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(typeColor(type), TFT_BLACK);
  spr.drawString(tn, x0, ty, 2);
  spr.setTextColor(vendorColor(cat), TFT_BLACK);
  spr.drawString(vbuf, x0 + tw, ty, 2);
  drawThresh();
  spr.pushSprite(0, 0);
}

// ---------- SCOPE ----------
void drawScope(const std::vector<Blip>& bl) {
  int W = spr.width(), H = spr.height();
  int cx = W / 2 + 26, cy = H / 2;     // радар сдвинут вправо, слева место под легенду
  int maxR = (H / 2) - 8;
  spr.fillSprite(TFT_BLACK);
  uint16_t ring = tft.color565(0, 70, 32);
  for (int k = 1; k <= 3; k++) spr.drawCircle(cx, cy, maxR * k / 3, ring);
  spr.drawLine(cx - maxR, cy, cx + maxR, cy, ring);
  spr.drawLine(cx, cy - maxR, cx, cy + maxR, ring);

  for (int t = 0; t < 40; t++) {
    float a = (g_sweep - t) * PI / 180.0f;
    int g = 140 - t * 3; if (g < 0) g = 0;
    spr.drawLine(cx, cy, cx + (int)(cos(a) * maxR), cy + (int)(sin(a) * maxR),
                 tft.color565(0, g, g / 2));
  }

  // блипы — только цветные точки, без букв
  for (size_t i = 0; i < bl.size(); i++) {
    float ang = (bl[i].h % 360) * PI / 180.0f;
    int pct = (bl[i].rssi + 95) * 100 / 50;
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    int r = maxR - (maxR - 8) * pct / 100;
    int x = cx + (int)(cos(ang) * r), y = cy + (int)(sin(ang) * r);
    spr.fillCircle(x, y, 3, vendorColor(bl[i].cat));
  }

  // легенда слева: 6 самых частых производителей на экране сейчас
  int bc[N_BRANDS] = {0};
  for (size_t i = 0; i < bl.size(); i++) if (bl[i].cat < N_BRANDS) bc[bl[i].cat]++;
  spr.setTextDatum(ML_DATUM);
  for (int row = 0; row < 6; row++) {
    int bi = -1;
    for (int b = 0; b < N_BRANDS; b++) if (bc[b] > 0 && (bi < 0 || bc[b] > bc[bi])) bi = b;
    if (bi < 0) break;
    int y = 14 + row * 24;
    char lb[8]; snprintf(lb, sizeof(lb), "%s", BRANDS[bi].label);
    spr.fillCircle(7, y, 3, vendorColor(bi));
    spr.setTextColor(vendorColor(bi), TFT_BLACK);
    spr.drawString(lb, 14, y, 1);
    char nb[6]; sprintf(nb, "%d", bc[bi]);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString(nb, 14, y + 9, 1);
    bc[bi] = 0;
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(0, 170, 80), TFT_BLACK);
  spr.drawString("SCOPE", cx, 2, 2);
  char buf[16]; sprintf(buf, "%d", (int)bl.size());
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(tft.color565(90, 255, 150), TFT_BLACK);
  spr.drawString(buf, W - 4, 2, 4);

  drawThresh();
  spr.pushSprite(0, 0);
  g_sweep = (g_sweep + 6) % 360;
}

// ---------- сэмплер истории (раз в секунду) + детектор событий ±7 ----------
void sampleTick() {
  uint32_t now = millis();
  if (!g_bleActive) return;                       // Bluetooth на паузе — историю не пишем
  if (now - g_lastSampleMs < 1000) return;
  g_lastSampleMs = now;
  int c = g_lastCount;

  if (g_base < 0) g_base = c;                     // инициализация
  int8_t ev = 0;
  float delta = c - g_base;
  if (g_thresh > 0 && now >= g_cooldownMs) {
    if (delta >= g_thresh)       { ev = +1; g_base = c; g_cooldownMs = now + 3000; }
    else if (delta <= -g_thresh) { ev = -1; g_base = c; g_cooldownMs = now + 3000; }
  }
  if (ev != 0) { g_ledEvent = ev; g_ledStart = now; }   // запустить анимацию ленты
  g_base += 0.15f * (c - g_base);                 // дрейф базовой линии (EMA)

  Sample s = { c, g_base, ev };
  if (g_histLen < HIST_N) g_hist[g_histLen++] = s;
  else { memmove(&g_hist[0], &g_hist[1], sizeof(Sample) * (HIST_N - 1)); g_hist[HIST_N - 1] = s; }
}

// ---------- TIMELINE: число устройств во времени ----------
void drawTimeline() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);

  int plotL = 6, plotR = W - 6, plotT = 22, plotB = H - 22;
  int plotW = plotR - plotL, plotH = plotB - plotT;

  int peak = 1, nowv = 0;
  for (int i = 0; i < g_histLen; i++) if (g_hist[i].count > peak) peak = g_hist[i].count;
  if (g_histLen > 0) nowv = g_hist[g_histLen - 1].count;
  int ymax = peak + peak / 6 + 1;                 // запас сверху

  spr.drawRect(plotL, plotT, plotW, plotH, tft.color565(40, 40, 48));
  float step = (HIST_N > 1) ? (float)plotW / (HIST_N - 1) : plotW;

  // площадь + линия
  int prevx = 0, prevy = 0;
  for (int i = 0; i < g_histLen; i++) {
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    int y = plotB - (int)((long)g_hist[i].count * plotH / ymax);
    spr.drawFastVLine(x, y, plotB - y, tft.color565(15, 55, 35));
    if (i > 0) spr.drawLine(prevx, prevy, x, y, tft.color565(70, 220, 130));
    prevx = x; prevy = y;
  }

  // базовая линия (сглаженная) — тускло
  for (int i = 1; i < g_histLen; i++) {
    int x0 = plotR - (int)((g_histLen - 1 - (i - 1)) * step);
    int x1 = plotR - (int)((g_histLen - 1 - i) * step);
    int y0 = plotB - (int)(g_hist[i - 1].base * plotH / ymax);
    int y1 = plotB - (int)(g_hist[i].base * plotH / ymax);
    spr.drawLine(x0, y0, x1, y1, tft.color565(95, 95, 45));
  }

  // метки событий: ▲ пришли (зел), ▼ ушли (оранж)
  for (int i = 0; i < g_histLen; i++) {
    if (g_hist[i].ev == 0) continue;
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    if (g_hist[i].ev > 0)
      spr.fillTriangle(x, plotT + 2, x - 4, plotT + 10, x + 4, plotT + 10, tft.color565(70, 220, 130));
    else
      spr.fillTriangle(x - 4, plotT + 2, x + 4, plotT + 2, x, plotT + 10, tft.color565(240, 150, 50));
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("TIMELINE", W / 2, 4, 2);

  char buf[48];
  sprintf(buf, "now %d   peak %d", nowv, peak);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(tft.color565(70, 220, 130), TFT_BLACK);
  spr.drawString(buf, W / 2, H - 4, 2);

  drawThresh();
  spr.pushSprite(0, 0);
}

// ---------- TYPES: кольцевая диаграмма по типам устройств ----------
void drawTypes(const int* cnt, int total) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);

  // плавно подтягиваем отображаемые доли к реальным
  float dispSum = 0;
  for (int t = 0; t < T_NUM; t++) {
    g_pieDisp[t] += 0.2f * ((float)cnt[t] - g_pieDisp[t]);
    if (g_pieDisp[t] < 0.02f) g_pieDisp[t] = 0;
    dispSum += g_pieDisp[t];
  }

  int cx = 82, cy = H / 2 + 6, R = 68, IR = 42;
  if (dispSum < 0.05f) {
    spr.drawArc(cx, cy, R, IR, 0, 360, tft.color565(35, 35, 42), TFT_BLACK, false);
  } else {
    // порядок секторов фиксированный (по типам), начинаем сверху, по часовой
    float acc = 0;
    for (int t = 0; t < T_NUM; t++) {
      if (g_pieDisp[t] <= 0) continue;
      int a0 = (int)(acc / dispSum * 360.0f + 0.5f);
      acc += g_pieDisp[t];
      int a1 = (int)(acc / dispSum * 360.0f + 0.5f);
      if (a1 <= a0) continue;
      if (a1 - a0 >= 360) {
        spr.drawArc(cx, cy, R, IR, 0, 360, typeColor(t), TFT_BLACK, false);
      } else {
        // у drawArc 0° внизу -> сдвиг на 180°, чтобы начинать сверху
        spr.drawArc(cx, cy, R, IR, (a0 + 180) % 360, (a1 + 180) % 360,
                    typeColor(t), TFT_BLACK, false);
        // тонкий разделитель между секторами
        float ar = (a1 - 90) * PI / 180.0f;
        spr.drawLine(cx + (int)(cos(ar) * IR), cy + (int)(sin(ar) * IR),
                     cx + (int)(cos(ar) * R),  cy + (int)(sin(ar) * R), TFT_BLACK);
      }
    }
  }

  // в центре — общее число
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawNumber(total, cx, cy - 4, 4);
  spr.setTextColor(tft.color565(110, 110, 125), TFT_BLACK);
  spr.drawString("devices", cx, cy + 16, 1);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("TYPES", 4, 2, 2);

  // легенда справа: только непустые, по убыванию
  int order[T_NUM], n = 0;
  for (int t = 0; t < T_NUM; t++) if (cnt[t] > 0) order[n++] = t;
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && cnt[order[j - 1]] < cnt[order[j]]; j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }
  const int maxRows = 8, rowH = 20;
  int rows = n < maxRows ? n : maxRows;
  int ly = (H - rows * rowH) / 2 + 2;
  int lx = 168;
  char buf[24];
  for (int i = 0; i < rows; i++) {
    int t = order[i], y = ly + i * rowH;
    uint16_t c = typeColor(t);
    spr.fillRoundRect(lx, y + 3, 10, 10, 2, c);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(c, TFT_BLACK);
    spr.drawString(typeName(t), lx + 16, y, 2);
    sprintf(buf, "%d", cnt[t]);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString(buf, W - 40, y, 2);
    sprintf(buf, "%d%%", total ? (cnt[t] * 100 + total / 2) / total : 0);
    spr.setTextColor(tft.color565(110, 110, 125), TFT_BLACK);
    spr.drawString(buf, W - 4, y, 2);
  }
  if (n == 0) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString("no devices", lx + (W - lx) / 2, H / 2, 2);
  }

  drawThresh();
  spr.pushSprite(0, 0);
}

// ============================================================
//  LIST / DETAIL / WEATHER
// ============================================================
uint16_t C_GRAY()  { return tft.color565(120, 120, 135); }
uint16_t C_DIM()   { return tft.color565(80, 80, 92); }

void macStr(const uint8_t* m, char* out) {   // m — младший байт первым
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[5], m[4], m[3], m[2], m[1], m[0]);
}

const char* akindName(uint8_t k) {
  switch (k) {
    case A_PUBLIC:   return "public";
    case A_STATIC:   return "random static";
    case A_ROTATING: return "rotating";
    default:         return "random";
  }
}

const char* uuidName(uint16_t u) {
  switch (u) {
    case 0x180F: return "Battery";   case 0x180A: return "DevInfo";
    case 0x1800: return "GAP";       case 0x1801: return "GATT";
    case 0xFE2C: return "FastPair";  case 0xFD6F: return "ExpNotif";
    case 0xFEAA: return "Eddystone"; case 0x181A: return "EnvSens";
    case 0xFCD2: return "BTHome";    case 0xFE95: return "Xiaomi";
    case 0x1812: return "HID";       case 0x180D: return "HeartRate";
    case 0xFEED: case 0xFEEC: return "Tile";
    case 0xFD5A: return "SmartTag";  case 0xFE9F: return "Google";
    case 0xFD3D: return "SwitchBot"; case 0xFDCD: return "Qingping";
    case 0x1809: return "Thermo";    case 0x184E: return "LEAudio";
    case 0xFE33: return "Chipolo";   case 0xFE07: return "Sonos";
    case 0xFE03: return "Amazon";    case 0xFD5F: return "Meta";
    default:     return "";
  }
}

// укоротить строку до ширины px (font), добавив ".."
void fitText(const char* src, char* dst, size_t cap, int px, int font) {
  strncpy(dst, src, cap - 1); dst[cap - 1] = 0;
  if (spr.textWidth(dst, font) <= px) return;
  size_t n = strlen(dst);
  while (n > 1) {
    dst[--n] = 0;
    char tmp[48]; snprintf(tmp, sizeof(tmp), "%s..", dst);
    if (spr.textWidth(tmp, font) <= px) { strcpy(dst, tmp); return; }
  }
}

// 4 столбика уровня сигнала
void drawBars(int x, int y, int rssi, uint16_t col) {
  int lvl = rssi > -60 ? 4 : rssi > -70 ? 3 : rssi > -80 ? 2 : rssi > -90 ? 1 : 0;
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 3;
    spr.fillRect(x + i * 6, y + 12 - h, 4, h, i < lvl ? col : tft.color565(45, 45, 55));
  }
}

const int LIST_ROWS = 8, LIST_ROWH = 18, LIST_Y0 = 18;

void drawList(const std::map<uint32_t, Ent>& ents) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  sprintf(buf, "DEVICES  %d", (int)g_order.size());
  spr.drawString(buf, 4, 1, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("wheel: pick  click: info", W - 4, 5, 1);

  if (g_order.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString("no devices", W / 2, H / 2, 4);
    spr.pushSprite(0, 0);
    return;
  }

  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = g_top + i;
    if (idx >= (int)g_order.size()) break;
    uint32_t id = g_order[idx];
    auto it = ents.find(id);
    int y = LIST_Y0 + i * LIST_ROWH;
    bool sel = (id == g_sel);
    if (sel) spr.fillRoundRect(0, y, W - 4, LIST_ROWH - 1, 3, tft.color565(28, 38, 62));
    uint16_t bg = sel ? tft.color565(28, 38, 62) : TFT_BLACK;
    if (it == ents.end()) {
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C_DIM(), bg);
      spr.drawString("(gone)", 12, y + 1, 2);
      continue;
    }
    const Ent& e = it->second;
    uint16_t tc = typeColor(e.type);
    spr.fillRect(3, y + 3, 4, LIST_ROWH - 7, tc);

    fitText(e.name[0] ? e.name : typeName(e.type), buf, sizeof(buf), 132, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(e.name[0] ? TFT_WHITE : tc, bg);
    spr.drawString(buf, 12, y + 1, 2);

    spr.setTextColor(vendorColor(e.cat), bg);
    char vb[16];
    fitText(vendorLabel(e.cat, e.vkind, e.vcode), vb, sizeof(vb), 84, 2);
    spr.drawString(vb, 150, y + 1, 2);

    sprintf(buf, "%d", e.rssi);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_GRAY(), bg);
    spr.drawString(buf, 268, y + 1, 2);
    drawBars(274, y + 2, e.rssi, tft.color565(90, 220, 140));
  }

  // полоса прокрутки
  int n = (int)g_order.size();
  if (n > LIST_ROWS) {
    int trackH = LIST_ROWS * LIST_ROWH;
    int barH = trackH * LIST_ROWS / n; if (barH < 8) barH = 8;
    int barY = LIST_Y0 + (trackH - barH) * g_top / (n - LIST_ROWS);
    spr.fillRect(W - 2, LIST_Y0, 2, trackH, tft.color565(30, 30, 38));
    spr.fillRect(W - 2, barY, 2, barH, C_GRAY());
  }
  spr.pushSprite(0, 0);
}

void drawDetail(const Dev& d, const Ent& e, bool inMap, uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[64], tmp[48];
  uint16_t tc = typeColor(e.type);

  // заголовок: имя (или тип)
  fitText(d.name[0] ? d.name : typeName(e.type), tmp, sizeof(tmp), W - 70, 4);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(d.name[0] ? TFT_WHITE : tc, TFT_BLACK);
  spr.drawString(tmp, 4, 2, 4);
  int pos = -1;
  for (size_t i = 0; i < g_order.size(); i++) if (g_order[i] == g_sel) pos = (int)i;
  if (pos >= 0) {
    sprintf(buf, "%d/%d", pos + 1, (int)g_order.size());
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString(buf, W - 4, 6, 2);
  }

  // тип + производитель
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tc, TFT_BLACK);
  spr.drawString(typeName(e.type), 4, 30, 2);
  int x = 4 + spr.textWidth(typeName(e.type), 2) + 10;
  if (e.vkind) sprintf(buf, "%s  %s%04X", vendorLabel(e.cat, e.vkind, e.vcode),
                      e.vkind == 1 ? "0x" : "svc ", e.vcode);
  else          sprintf(buf, "maker unknown");
  spr.setTextColor(vendorColor(e.cat), TFT_BLACK);
  spr.drawString(buf, x, 30, 2);

  // левая колонка
  uint32_t ago = (now - e.t) / 1000;
  int y = 50;
  const int LH = 15;
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  if (!inMap)          { spr.setTextColor(tft.color565(235, 80, 80), TFT_BLACK); sprintf(buf, "out of range"); }
  else if (ago >= 3)   sprintf(buf, "signal %d (%lus ago)", e.rssi, (unsigned long)ago);
  else                 sprintf(buf, "signal %d dBm", e.rssi);
  spr.drawString(buf, 4, y, 2); y += LH;
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  sprintf(buf, "range  %d .. %d", d.rmin, d.rmax);
  spr.drawString(buf, 4, y, 2); y += LH;
  float dist = estDistance(e.rssi);
  if (dist < 10) sprintf(buf, "dist   ~%.1f m", dist); else sprintf(buf, "dist   ~%.0f m", dist);
  spr.drawString(buf, 4, y, 2); y += LH;
  uint32_t secs = (now - (e.first ? e.first : now)) / 1000;
  sprintf(buf, "seen   %lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  spr.drawString(buf, 4, y, 2); y += LH;
  sprintf(buf, "pkts   %lu", (unsigned long)e.pkts);
  spr.drawString(buf, 4, y, 2);

  // правая колонка: график сигнала за ~30 с
  int gx = 176, gy = 50, gw = W - gx - 4, gh = 70;
  spr.drawRect(gx, gy, gw, gh, tft.color565(40, 40, 50));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("signal, 30s", gx + 3, gy + 3, 1);
  // шкала: -100..-30 dBm
  int px = -1, py = -1;
  for (int i = 0; i < g_sparkN; i++) {
    int v = g_spark[i];
    int xx = gx + 1 + (gw - 3) * (i + (SPARK_N - g_sparkN)) / (SPARK_N - 1);
    if (v <= -127) { px = -1; continue; }
    int c = v < -100 ? -100 : v > -30 ? -30 : v;
    int yy = gy + gh - 2 - (c + 100) * (gh - 4) / 70;
    if (px >= 0) spr.drawLine(px, py, xx, yy, tft.color565(90, 220, 140));
    else         spr.drawPixel(xx, yy, tft.color565(90, 220, 140));
    px = xx; py = yy;
  }

  // низ: технические подробности мелким шрифтом
  y = 126;
  macStr(d.mac, tmp);
  if (e.nMacs > 1) sprintf(buf, "MAC %s  %s  (%d MACs)", tmp, akindName(d.akind), e.nMacs);
  else             sprintf(buf, "MAC %s  %s", tmp, akindName(d.akind));
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString(buf, 4, y, 1); y += 10;

  if (d.nU) {
    int n = sprintf(buf, "SVC");
    for (uint8_t i = 0; i < d.nU && n < 52; i++) {
      const char* nm = uuidName(d.uuids[i]);
      if (!nm[0]) { const char* o = uuidOwner(d.uuids[i]); if (o) nm = o; }
      n += snprintf(buf + n, sizeof(buf) - n, nm[0] ? " %04X %s" : " %04X", d.uuids[i], nm);
    }
    spr.drawString(buf, 4, y, 1); y += 10;
  }
  if (d.hasCid) {
    int n = sprintf(buf, "MFR %02X %02X |", d.cid & 0xFF, d.cid >> 8);
    int shown = d.mfrLen < 6 ? d.mfrLen : 6;
    for (int i = 0; i < shown; i++) n += sprintf(buf + n, " %02X", d.mfr[i]);
    sprintf(buf + n, "%s  (len %d)", d.mfrLen > 6 ? " .." : "", d.mfrLen);
    spr.drawString(buf, 4, y, 1); y += 10;
  }
  if (d.appearance) {
    sprintf(buf, "APP 0x%04X", d.appearance);
    spr.drawString(buf, 4, y, 1);
  }
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("click: back", W - 4, H - 1, 1);
  spr.pushSprite(0, 0);
}

uint16_t tempColor(float t) {
  if (t < 0)  return tft.color565(130, 200, 255);
  if (t < 16) return tft.color565(80, 150, 240);
  if (t < 25) return tft.color565(70, 210, 130);
  if (t < 29) return tft.color565(240, 185, 60);
  return tft.color565(235, 80, 80);
}

void drawWeather(const std::vector<Sensor>& ss, uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("WEATHER", 4, 1, 2);

  if (ss.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_GRAY(), TFT_BLACK);
    spr.drawString("no sensors yet", W / 2, H / 2 - 16, 4);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString("Xiaomi PVVX/ATC, BTHome, Govee,", W / 2, H / 2 + 14, 2);
    spr.drawString("Qingping, SwitchBot, RuuviTag", W / 2, H / 2 + 32, 2);
    spr.pushSprite(0, 0);
    return;
  }

  const int PER = 3, RH = 50;
  int pages = ((int)ss.size() + PER - 1) / PER;
  int page = pages > 1 ? (int)((now / 4000) % pages) : 0;

  sprintf(buf, "%d sensor%s", (int)ss.size(), ss.size() == 1 ? "" : "s");
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString(buf, W - 4 - (pages > 1 ? pages * 8 + 4 : 0), 1, 2);
  for (int p = 0; p < pages && pages > 1; p++)
    spr.fillCircle(W - 4 - (pages - 1 - p) * 8 - 3, 9, 2, p == page ? TFT_WHITE : C_DIM());

  for (int i = 0; i < PER; i++) {
    int k = page * PER + i;
    if (k >= (int)ss.size()) break;
    const Sensor& S = ss[k];
    int y = 20 + i * RH;
    if (i > 0) spr.drawFastHLine(4, y - 3, W - 8, tft.color565(30, 30, 38));

    // подпись: имя или модель + хвост MAC
    if (S.name[0]) fitText(S.name, buf, sizeof(buf), 150, 2);
    else sprintf(buf, "%s %02X%02X", S.r.model, S.mac[1], S.mac[0]);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_GRAY(), TFT_BLACK);
    spr.drawString(buf, 6, y, 2);

    uint32_t ago = (now - S.t) / 1000;
    char right[32] = "";
    int n = 0;
    if (S.r.hasB) n += sprintf(right + n, "bat %d%%  ", S.r.batt);
    if (ago < 60) sprintf(right + n, "%lus", (unsigned long)ago);
    else          sprintf(right + n, "%lum", (unsigned long)(ago / 60));
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString(right, W - 6, y, 2);

    int vy = y + 17;
    int x = 6;
    if (S.r.hasT) {
      sprintf(buf, "%.1f", S.r.temp);
      uint16_t c = tempColor(S.r.temp);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(c, TFT_BLACK);
      spr.drawString(buf, x, vy, 4);
      x += spr.textWidth(buf, 4) + 3;
      spr.drawCircle(x + 2, vy + 4, 2, c);          // значок градуса
      spr.drawString("C", x + 7, vy, 4);
    }
    if (S.r.hasH) {
      sprintf(buf, "%.0f%%", S.r.hum);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(tft.color565(90, 170, 245), TFT_BLACK);
      spr.drawString(buf, 130, vy, 4);
    }
    int ex = 214, ey = vy + 5;
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_GRAY(), TFT_BLACK);
    if (S.r.hasP)   { sprintf(buf, "%.0f hPa", S.r.press); spr.drawString(buf, ex, ey, 2); ey += 15; }
    if (S.r.hasCO2) { sprintf(buf, "%d ppm", S.r.co2);    spr.drawString(buf, ex, ey, 2); }
  }
  spr.pushSprite(0, 0);
}

// ---------- анимация светодиодной ленты ----------
void updateLeds() {
  if (g_ledEvent == 0) return;
  uint32_t t = millis() - g_ledStart;
  if (t >= LED_DUR) { strip.clear(); strip.show(); g_ledEvent = 0; return; }
  float p = (float)t / LED_DUR;                 // 0..1
  strip.clear();
  if (g_ledEvent > 0) {
    // зелёная волна снизу вверх с хвостом
    int head = (int)(p * (WS2812_N + 4));
    for (int i = 0; i < WS2812_N; i++) {
      int d = head - i;
      if (d >= 0 && d < 4) {
        int b = 220 - d * 60; if (b < 0) b = 0;
        strip.setPixelColor(i, strip.Color(0, b, b / 3));
      }
    }
  } else {
    // оранжевая вспышка с общим спадом
    int b = (int)(220 * (1.0f - p));
    for (int i = 0; i < WS2812_N; i++)
      strip.setPixelColor(i, strip.Color(b, b * 2 / 5, 0));
  }
  strip.show();
}

// ---------- настройки ----------
void applyBrightness() {
  strip.setBrightness(g_ledBright * 255 / 100);
  analogWrite(TFT_BL, g_scrBright * 255 / 100);
}

void loadSettings() {
  g_prefs.begin("radar", true);
  g_range     = g_prefs.getInt("range", 0);
  g_thresh    = g_prefs.getInt("thresh", 7);
  g_ledBright = g_prefs.getInt("ledbr", 50);
  g_scrBright = g_prefs.getInt("scrbr", 100);
  g_prefs.end();
  if (g_range < 0 || g_range >= N_RANGES) g_range = 0;
  if (g_thresh < 0 || g_thresh > 30) g_thresh = 7;
  if (g_ledBright < 10 || g_ledBright > 100) g_ledBright = 50;
  if (g_scrBright < 20 || g_scrBright > 100) g_scrBright = 100;
}

void saveSettings() {
  g_prefs.begin("radar", false);
  g_prefs.putInt("range", g_range);
  g_prefs.putInt("thresh", g_thresh);
  g_prefs.putInt("ledbr", g_ledBright);
  g_prefs.putInt("scrbr", g_scrBright);
  g_prefs.end();
  g_setDirty = false;
}

static const int N_SET_ITEMS = 4;

void settingsAdjust(int dir) {
  switch (g_setItem) {
    case 0: g_range = constrain(g_range + dir, 0, N_RANGES - 1); break;
    case 1: g_thresh = constrain(g_thresh + dir, 0, 30); break;
    case 2: g_ledBright = constrain(g_ledBright + dir * 10, 10, 100); break;
    case 3: g_scrBright = constrain(g_scrBright + dir * 10, 20, 100); break;
  }
  applyBrightness();
  if (g_setItem == 2) { g_ledEvent = +1; g_ledStart = millis(); }   // показать яркость ленты
  g_setDirty = true; g_setChangedMs = millis();
}

void drawSettings() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char val[32];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("SETTINGS", 4, 1, 2);

  const char* names[N_SET_ITEMS] = { "Range", "LED events", "LED bright", "Screen" };
  const char* hints[N_SET_ITEMS] = {
    "ignore devices weaker than this",
    "flash when count jumps by this",
    "brightness of the LED strip",
    "screen backlight",
  };
  for (int i = 0; i < N_SET_ITEMS; i++) {
    int y = 22 + i * 30;
    bool sel = (i == g_setItem);
    uint16_t bg = sel ? tft.color565(28, 38, 62) : TFT_BLACK;
    if (sel) spr.fillRoundRect(0, y - 3, W, 28, 4, bg);
    switch (i) {
      case 0:
        if (g_range == 0) sprintf(val, "all");
        else sprintf(val, "%s  %d dBm", RANGES[g_range].name, RANGES[g_range].dbm);
        break;
      case 1: if (g_thresh == 0) sprintf(val, "off"); else sprintf(val, "+-%d", g_thresh); break;
      case 2: sprintf(val, "%d%%", g_ledBright); break;
      case 3: sprintf(val, "%d%%", g_scrBright); break;
    }
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(sel ? TFT_WHITE : tft.color565(150, 150, 165), bg);
    spr.drawString(names[i], 10, y, 4);
    // значение крупно, а если не влезает рядом с названием — помельче
    int room = W - 20 - spr.textWidth(names[i], 4) - 12;
    bool big = spr.textWidth(val, 4) <= room;
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(sel ? tft.color565(90, 220, 140) : tft.color565(110, 110, 125), bg);
    spr.drawString(val, W - 10, big ? y : y + 5, big ? 4 : 2);
  }
  spr.setTextDatum(BL_DATUM);
  spr.setTextColor(tft.color565(110, 110, 125), TFT_BLACK);
  spr.drawString(hints[g_setItem], 4, H - 12, 1);
  spr.setTextColor(tft.color565(80, 80, 92), TFT_BLACK);
  spr.drawString("wheel: change   click: next item   side: back", 4, H - 1, 1);
  spr.pushSprite(0, 0);
}

// ---------- кнопки и колесо ----------
void changeMode(int dir) {
  g_mode = (g_mode + dir + NUM_MODES) % NUM_MODES;
  g_detail = false;
}

// ============================================================
//  ГЛАВНОЕ МЕНЮ и переключение радио
// ============================================================
// Wi-Fi и Bluetooth делят одну антенну, поэтому работает что-то одно.
void wifiStart();
void wifiStop();
void wifiClick();
void wifiRotate(int dir);
void wifiFrame(uint32_t now);
int  wifiNetCount();
void wifiEnter();
void wifiLeave();
void wifiOpenScreen(int m);
void wifiCloseScreen();
void wifiSubFrame(uint32_t now);
int  wifiScreenNow();
void subBack();
bool wifiCloseDetail();

void bleStart() {
  if (g_bleActive) return;
  wifiStop();
  NimBLEDevice::getScan()->start(0, false);
  g_bleActive = true;
  g_base = -1;                              // заново набрать базовую линию событий
  g_cooldownMs = millis() + 10000;          // и не мигать, пока список наполняется
}

void bleStop() {
  if (!g_bleActive) return;
  NimBLEDevice::getScan()->stop();
  g_bleActive = false;
}

void enterSection(int sel) {
  if (sel == 0)      { g_section = S_BLE; bleStart(); g_subSel = g_mode; }
  else if (sel == 1) { g_section = S_WIFI; bleStop(); wifiEnter(); g_subSel = wifiScreenNow(); }
  else               { g_section = S_SET; g_setItem = 0; }
  g_sub = true;
  g_detail = false;
}

// открыть экран, выбранный в подменю
void subOpen() {
  if (g_section == S_BLE)  { g_mode = g_subSel; g_detail = false; }
  if (g_section == S_WIFI) wifiOpenScreen(g_subSel);
  g_sub = false;
}

// из экрана — обратно в подменю
void subBack() {
  if (g_section == S_BLE)  g_subSel = g_mode;
  if (g_section == S_WIFI) { g_subSel = wifiScreenNow(); wifiCloseScreen(); }
  g_detail = false;
  g_sub = true;
}

void goMenu();
// боковая кнопка: на шаг назад (подробности -> экран -> подменю -> главное меню)
void goBack() {
  if ((g_section == S_BLE || g_section == S_WIFI) && !g_sub) {
    if (g_section == S_BLE && g_detail) { g_detail = false; return; }
    if (g_section == S_WIFI && wifiCloseDetail()) return;
    subBack();
    return;
  }
  goMenu();
}

void goMenu() {
  if (g_section == S_SET && g_setDirty) saveSettings();
  if (g_section == S_WIFI) wifiLeave();    // без раздела Wi-Fi пакеты не считаем, ленту гасим
  if (g_section == S_BLE)  g_menuSel = 0;
  if (g_section == S_WIFI) g_menuSel = 1;
  if (g_section == S_SET)  g_menuSel = 2;
  g_section = S_MENU;
  g_detail = false;
}

// --- подменю: список экранов раздела ---
struct SubItem { const char* name; const char* desc; char value[24]; };

void drawSubmenu(const char* title, uint16_t acc, SubItem* it, int n) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(acc, TFT_BLACK);
  spr.drawString(title, 4, 1, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(tft.color565(80, 80, 92), TFT_BLACK);
  spr.drawString("click: open   side: back", W - 4, 5, 1);
  int rh = n > 5 ? 21 : 28, y0 = 20;
  for (int i = 0; i < n; i++) {
    int y = y0 + i * rh;
    bool sel = (i == g_subSel);
    uint16_t bg = sel ? tft.color565(22, 26, 38) : TFT_BLACK;
    if (sel) {
      spr.fillRoundRect(0, y, W, rh - 2, 5, bg);
      spr.fillRoundRect(0, y + 3, 4, rh - 8, 2, acc);
    }
    int ty = y + (rh - 2) / 2;
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(sel ? TFT_WHITE : tft.color565(170, 170, 185), bg);
    spr.drawString(it[i].name, 12, ty, 2);
    spr.setTextColor(sel ? tft.color565(130, 130, 145) : tft.color565(75, 75, 88), bg);
    spr.drawString(it[i].desc, 104, ty + 1, 1);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(sel ? acc : tft.color565(110, 110, 125), bg);
    spr.drawString(it[i].value, W - 8, ty, 2);
  }
  spr.pushSprite(0, 0);
}

void drawBleSub(int count, int bestRssi, bool have, const int* typeCnt) {
  SubItem it[NUM_MODES] = {
    { "Radar",     "how many around",   "" },
    { "Proximity", "nearest device",    "" },
    { "Scope",     "radar picture",     "" },
    { "Timeline",  "count over time",   "" },
    { "Types",     "what kind of",      "" },
    { "Devices",   "list + details",    "" },
    { "Weather",   "BLE thermometers",  "" },
  };
  sprintf(it[M_RADAR].value, "%d", count);
  if (have) sprintf(it[M_PROX].value, "%d dBm", bestRssi); else strcpy(it[M_PROX].value, "-");
  int top = -1;
  for (int t = 1; t < T_NUM; t++) if (typeCnt[t] > 0 && (top < 0 || typeCnt[t] > typeCnt[top])) top = t;
  if (top > 0) snprintf(it[M_TYPES].value, 24, "%s %d", typeName(top), typeCnt[top]);
  sprintf(it[M_LIST].value, "%d", count);
  if (g_sensCount) sprintf(it[M_WEATHER].value, "%d", g_sensCount); else strcpy(it[M_WEATHER].value, "-");
  drawSubmenu("BLUETOOTH", tft.color565(80, 150, 240), it, NUM_MODES);
}

// --- иконки ---
void iconBluetooth(int x, int y, uint16_t c, uint16_t bg) {
  float w = 3.5f;
  spr.drawWideLine(x, y - 22, x, y + 22, w, c, bg);
  spr.drawWideLine(x, y - 22, x + 12, y - 11, w, c, bg);
  spr.drawWideLine(x + 12, y - 11, x - 11, y + 10, w, c, bg);
  spr.drawWideLine(x, y + 22, x + 12, y + 11, w, c, bg);
  spr.drawWideLine(x + 12, y + 11, x - 11, y - 10, w, c, bg);
}
void iconWifi(int x, int y, uint16_t c, uint16_t bg) {
  int cy = y + 16;
  for (int i = 0; i < 3; i++) {
    int r = 12 + i * 11;
    spr.drawArc(x, cy, r, r - 5, 135, 225, c, bg, true);   // дуги вверх
  }
  spr.fillSmoothCircle(x, cy - 1, 4, c, bg);
}
void iconGear(int x, int y, uint16_t c, uint16_t bg) {
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    spr.drawWideLine(x + cos(a) * 14, y + sin(a) * 14, x + cos(a) * 22, y + sin(a) * 22, 7, c, bg);
  }
  spr.fillSmoothCircle(x, y, 17, c, bg);
  spr.fillSmoothCircle(x, y, 7, bg, c);
}

void drawMenu(int bleCount) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  const char* names[3] = { "Bluetooth", "Wi-Fi", "Settings" };
  uint16_t acc[3] = { tft.color565(80, 150, 240), tft.color565(70, 210, 130), tft.color565(200, 160, 90) };
  char sub[3][24];
  if (g_bleActive) sprintf(sub[0], "%d devices", bleCount);
  else             sprintf(sub[0], "paused");
  int wn = wifiNetCount();
  if (wn >= 0) sprintf(sub[1], "%d networks", wn); else sprintf(sub[1], "scan networks");
  sprintf(sub[2], "range: %s", RANGES[g_range].name);

  int cw = 98, ch = 132, gap = 8, x0 = (W - (cw * 3 + gap * 2)) / 2, y0 = 8;
  for (int i = 0; i < 3; i++) {
    int x = x0 + i * (cw + gap);
    bool sel = (i == g_menuSel);
    uint16_t c = sel ? acc[i] : tft.color565(85, 85, 98);
    if (sel) {
      spr.fillRoundRect(x, y0, cw, ch, 10, tft.color565(22, 26, 38));
      spr.drawRoundRect(x, y0, cw, ch, 10, acc[i]);
      spr.drawRoundRect(x + 1, y0 + 1, cw - 2, ch - 2, 9, acc[i]);
    } else {
      spr.drawRoundRect(x, y0, cw, ch, 10, tft.color565(40, 40, 50));
    }
    int ix = x + cw / 2, iy = y0 + 44;
    uint16_t bg = sel ? tft.color565(22, 26, 38) : TFT_BLACK;
    if (i == 0) iconBluetooth(ix, iy, c, bg);
    else if (i == 1) iconWifi(ix, iy, c, bg);
    else iconGear(ix, iy, c, bg);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(sel ? TFT_WHITE : tft.color565(150, 150, 165), bg);
    spr.drawString(names[i], ix, y0 + 84, 2);
    spr.setTextColor(sel ? acc[i] : tft.color565(90, 90, 104), bg);
    spr.drawString(sub[i], ix, y0 + 106, 1);
  }
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(tft.color565(80, 80, 92), TFT_BLACK);
  spr.drawString("wheel: choose   click: open   hold side: power off", W / 2, H - 2, 1);
  spr.pushSprite(0, 0);
}

// ---------- питание: выключение / включение боковой кнопкой ----------
// «Выключено» = глубокий сон ESP32: экран, светодиоды и периферия обесточены,
// плата ждёт только нажатия боковой кнопки. Будит ТОЛЬКО боковая кнопка.
static const uint32_t PWR_SHOW_MS = 600;    // после стольких мс удержания показываем шкалу
static const uint32_t PWR_OFF_MS  = 2000;   // держать столько, чтобы выключить
static const uint32_t PWR_ON_MS   = 1000;   // держать столько, чтобы включить
uint32_t g_powerHold = 0;                   // сколько уже держат (для шкалы на экране)
bool     g_ignoreUser = false;              // игнорировать кнопку, пока её не отпустят

void sleepNow() {
  // ждём, пока кнопку отпустят, иначе плата тут же проснётся
  while (digitalRead(PIN_USER_KEY) == LOW) delay(10);
  delay(50);
  pinMode(TFT_BL, OUTPUT);       digitalWrite(TFT_BL, LOW);
  pinMode(BOARD_PWR_EN, OUTPUT); digitalWrite(BOARD_PWR_EN, LOW);   // обесточить периферию
  gpio_hold_en((gpio_num_t)TFT_BL);
  gpio_hold_en((gpio_num_t)BOARD_PWR_EN);
  gpio_deep_sleep_hold_en();
  rtc_gpio_pullup_en((gpio_num_t)PIN_USER_KEY);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_USER_KEY);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_USER_KEY, 0);       // проснуться по нажатию
  esp_deep_sleep_start();
}

void powerOff() {
  if (g_setDirty) saveSettings();
  int W = tft.width(), H = tft.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("bye", W / 2, H / 2, 4);
  spr.pushSprite(0, 0);
  strip.clear(); strip.show();
  NimBLEDevice::getScan()->stop();
  delay(400);
  tft.writecommand(0x28);   // дисплей: выкл
  tft.writecommand(0x10);   // дисплей: сон
  sleepNow();
}

// Вызывается в самом начале setup(): если нас разбудила кнопка — требуем
// удержания PWR_ON_MS, иначе (случайное касание) засыпаем обратно.
void checkWakeHold() {
  gpio_hold_dis((gpio_num_t)TFT_BL);
  gpio_hold_dis((gpio_num_t)BOARD_PWR_EN);
  gpio_deep_sleep_hold_dis();
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;   // обычное включение
  rtc_gpio_deinit((gpio_num_t)PIN_USER_KEY);
  pinMode(PIN_USER_KEY, INPUT_PULLUP);
  uint32_t t0 = millis();
  while (digitalRead(PIN_USER_KEY) == LOW && millis() - t0 < PWR_ON_MS) delay(10);
  if (millis() - t0 < PWR_ON_MS) sleepNow();     // отпустили раньше — не включаемся
  g_ignoreUser = true;                           // кнопку ещё держат — не считать нажатием
}

void drawPowerHold() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString("power off?", W / 2, H / 2 - 24, 4);
  int bw = W - 80, bx = 40, by = H / 2 + 4, bh = 14;
  uint32_t p = g_powerHold > PWR_OFF_MS ? PWR_OFF_MS : g_powerHold;
  int fill = (bw - 4) * (int)(p - PWR_SHOW_MS) / (int)(PWR_OFF_MS - PWR_SHOW_MS);
  if (fill < 0) fill = 0;
  spr.drawRoundRect(bx, by, bw, bh, 4, tft.color565(70, 70, 80));
  if (fill > 0) spr.fillRoundRect(bx + 2, by + 2, fill, bh - 4, 3, tft.color565(235, 80, 80));
  spr.setTextColor(tft.color565(110, 110, 125), TFT_BLACK);
  spr.drawString("keep holding the side button", W / 2, by + bh + 16, 2);
  spr.pushSprite(0, 0);
}

// Боковая кнопка: короткое нажатие — следующий режим, удержание 2 с — выключить.
// Нажатие колеса: в списке — подробности, в настройках — следующий пункт, иначе — следующий режим.
void pollButtons() {
  static int lastEnc = HIGH;
  static uint32_t lastEncMs = 0;
  static bool userDown = false;
  static uint32_t userDownMs = 0;
  int enc  = digitalRead(PIN_ENC_KEY);
  int user = digitalRead(PIN_USER_KEY);
  uint32_t now = millis();

  if (enc == LOW && lastEnc == HIGH && now - lastEncMs > 250) {
    if (g_section == S_MENU)     enterSection(g_menuSel);
    else if (g_section == S_SET) g_setItem = (g_setItem + 1) % N_SET_ITEMS;
    else if (g_sub)              subOpen();
    else if (g_section == S_WIFI) wifiClick();
    else if (g_mode == M_LIST)   { g_detail = !g_detail; g_lastTouch = now; }
    else                         subBack();            // на обычном экране нажатие — назад в подменю
    lastEncMs = now;
  }
  lastEnc = enc;

  if (g_ignoreUser) {                    // после включения ждём, пока отпустят
    if (user == HIGH) g_ignoreUser = false;
    return;
  }
  if (user == LOW && !userDown) { userDown = true; userDownMs = now; }
  if (userDown && user == LOW) {
    uint32_t held = now - userDownMs;
    g_powerHold = held >= PWR_SHOW_MS ? held : 0;
    if (held >= PWR_OFF_MS) powerOff();
  }
  if (userDown && user == HIGH) {
    uint32_t held = now - userDownMs;
    userDown = false;
    g_powerHold = 0;
    if (held >= 30 && held < PWR_SHOW_MS) goBack();   // короткое нажатие — на шаг назад
  }
}

// Колесо: в списке — выбор устройства, иначе — листает режимы.
void pollEncoder() {
  static uint8_t prev = 3;       // исходное 0b11 (оба HIGH)
  static int8_t  accum = 0;
  static uint32_t lastStepMs = 0;
  static const int8_t tbl[16] = { 0,-1, 1, 0,  1, 0, 0,-1, -1, 0, 0, 1,  0, 1,-1, 0 };
  uint8_t a = digitalRead(PIN_ENC_A);
  uint8_t b = digitalRead(PIN_ENC_B);
  uint8_t cur = (a << 1) | b;
  if (cur == prev) return;
  accum += tbl[(prev << 2) | cur];
  prev = cur;
  if (accum >= 4 || accum <= -4) {
    int dir = accum >= 4 ? +1 : -1;
    uint32_t now = millis();
    if (g_section == S_MENU) {
      if (now - lastStepMs > 120) { g_menuSel = (g_menuSel + dir + 3) % 3; lastStepMs = now; }
    } else if ((g_section == S_BLE || g_section == S_WIFI) && g_sub) {
      if (now - lastStepMs > 60) {
        int n = g_section == S_BLE ? NUM_MODES : W_SCREENS;
        g_subSel = (g_subSel + dir + n) % n;
        lastStepMs = now;
      }
    } else if (g_section == S_WIFI) {
      if (now - lastStepMs > 40) { wifiRotate(dir); lastStepMs = now; }
    } else if (bleIs(M_LIST)) {
      if (now - lastStepMs > 40) { g_listMove += dir; lastStepMs = now; g_lastTouch = now; }
    } else if (g_section == S_SET) {
      if (now - lastStepMs > 60) { settingsAdjust(dir); lastStepMs = now; }
    } else if (now - lastStepMs > 120) {        // мягкая защита от дребезга
      changeMode(dir);
      lastStepMs = now;
    }
    accum = 0;
  }
}

void setup() {
  checkWakeHold();      // включение после «выключения»: только удержанием боковой кнопки
  pinMode(BOARD_PWR_EN, OUTPUT); digitalWrite(BOARD_PWR_EN, HIGH);
  pinMode(PIN_TFT_CS,  OUTPUT);  digitalWrite(PIN_TFT_CS,  HIGH);
  pinMode(PIN_SD_CS,   OUTPUT);  digitalWrite(PIN_SD_CS,   HIGH);
  pinMode(PIN_LORA_CS, OUTPUT);  digitalWrite(PIN_LORA_CS, HIGH);
  pinMode(PIN_NRF_CS,  OUTPUT);  digitalWrite(PIN_NRF_CS,  HIGH);
  pinMode(PIN_ENC_KEY,  INPUT_PULLUP);
  pinMode(PIN_ENC_A,    INPUT_PULLUP);
  pinMode(PIN_ENC_B,    INPUT_PULLUP);
  pinMode(PIN_USER_KEY, INPUT_PULLUP);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  loadSettings();

  strip.begin();
  applyBrightness();
  strip.clear();
  strip.show();

  spr.setColorDepth(16);
  if (!spr.createSprite(tft.width(), tft.height())) {
    spr.setColorDepth(8);
    spr.createSprite(tft.width(), tft.height());
  }

  g_lock = xSemaphoreCreateMutex();
  NimBLEDevice::init("");
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(new ScanCB(), true);
  s->setActiveScan(true);
  s->setInterval(100);
  s->setWindow(99);
  s->setMaxResults(0);
  s->start(0, false);

  drawMenu(0);
}

uint32_t g_lastDraw = 0;

#include "wifi_ui.h"   // раздел Wi-Fi

void loop() {
  pollButtons();
  pollEncoder();
  sampleTick();
  updateLeds();
  uint32_t now = millis();
  if (now - g_lastDraw >= REDRAW_MS) {
    g_lastDraw = now;
    std::map<uint32_t, Ent> ents;     // физические устройства
    std::vector<Sensor> sens;
    static Dev detDev;                // последние известные подробности выбранного
    static bool detValid = false;
    static uint32_t detFor = 0;
    if (detFor != g_sel) { detValid = false; detFor = g_sel; }
    bool wantDetail = (bleIs(M_LIST) && g_detail);
    uint32_t detT = 0;
    int raw = 0;
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
      for (auto it = g_seen.begin(); it != g_seen.end(); ) {
        if (now - it->second.t > REMEMBER_MS) { it = g_seen.erase(it); continue; }
        Dev& d = it->second;
        // фильтр дальности с гистерезисом + отсев одиночных пакетов
        int thr = RANGES[g_range].dbm;
        if (!d.inRange && d.rssiS >= thr) d.inRange = true;
        else if (d.inRange && d.rssiS < thr - RANGE_HYST_DB) d.inRange = false;
        if (!d.inRange || d.pkts < MIN_PKTS) { ++it; continue; }
        Ent& e = ents[d.ent];             // новый Ent создаётся обнулённым
        if (d.t >= e.t) { e.t = d.t; e.rssi = d.rssi; e.rssiS = d.rssiS; e.key = it->first; }
        if (!e.first || d.first < e.first) e.first = d.first;
        if (d.pkts > e.pkts) e.pkts = d.pkts;
        e.nMacs++;
        if (d.type != T_UNKNOWN) e.type = d.type;
        if (d.vscore > e.vscore) { e.cat = d.cat; e.vkind = d.vkind; e.vcode = d.vcode; e.vscore = d.vscore; }
        if (d.name[0] && !e.name[0]) memcpy(e.name, d.name, sizeof(e.name));
        if (wantDetail && d.ent == g_sel && d.t >= detT) { detDev = d; detT = d.t; detValid = true; }
        ++it;
      }
      raw = (int)g_seen.size();
      g_sensCount = 0;
      for (auto& kv : g_sens) if (now - kv.second.t <= SENSOR_REMEMBER_MS) g_sensCount++;
      if (bleIs(M_WEATHER)) {
        for (auto it = g_sens.begin(); it != g_sens.end(); ) {
          if (now - it->second.t > SENSOR_REMEMBER_MS) { it = g_sens.erase(it); continue; }
          sens.push_back(it->second);
          ++it;
        }
      }
      xSemaphoreGive(g_lock);
    }

    int count = (int)ents.size();
    bool have = false; int bestRssi = -127; Ent bestEnt; memset(&bestEnt, 0, sizeof(bestEnt));
    int typeCnt[T_NUM] = {0};
    std::vector<Blip> snap;
    for (auto& kv : ents) {
      const Ent& e = kv.second;
      if (e.rssi > bestRssi) { bestRssi = e.rssi; bestEnt = e; have = true; }
      typeCnt[e.type < T_NUM ? e.type : T_UNKNOWN]++;
      // угол блипа — от id физического устройства: не прыгает при смене MAC
      if (bleIs(M_SCOPE)) snap.push_back({ e.rssi, kv.first * 2654435761u, e.cat });
    }
    g_lastCount = count;
    g_lastRaw = raw;

    if (g_powerHold) {
      drawPowerHold();
    }
    else if (g_section == S_MENU) drawMenu(count);
    else if (g_section == S_SET)  drawSettings();
    else if (g_section == S_WIFI) { if (g_sub) wifiSubFrame(now); else wifiFrame(now); }
    else if (g_sub)               drawBleSub(count, bestRssi, have, typeCnt);
    else if (g_mode == M_LIST) {
      // Порядок списка: ближние сверху, но «липкий», чтобы строки не прыгали:
      //  - пока крутишь колесо или открыты подробности — порядок заморожен;
      //  - соседи меняются местами, только если разница сигнала > ORDER_HYST_DB;
      //  - новые устройства вставляются на своё место, ушедшие убираются.
      bool frozen = g_detail || (now - g_lastTouch < ORDER_FREEZE);
      if (g_order.empty() || (!frozen && now - g_orderMs >= ORDER_EVERY)) {
        auto rs = [&](uint32_t id) { return ents[id].rssiS; };
        std::vector<uint32_t> o;
        o.reserve(ents.size());
        for (uint32_t id : g_order) if (ents.count(id)) o.push_back(id);
        for (size_t pass = 0; pass < o.size(); pass++) {
          bool sw = false;
          for (size_t i = 0; i + 1 < o.size(); i++)
            if (rs(o[i + 1]) > rs(o[i]) + ORDER_HYST_DB) { std::swap(o[i], o[i + 1]); sw = true; }
          if (!sw) break;
        }
        std::vector<uint32_t> fresh;
        for (auto& kv : ents)
          if (std::find(o.begin(), o.end(), kv.first) == o.end()) fresh.push_back(kv.first);
        for (uint32_t id : fresh) {
          size_t pos = 0;
          while (pos < o.size() && rs(o[pos]) >= rs(id)) pos++;
          o.insert(o.begin() + pos, id);
        }
        g_order.swap(o);
        g_orderMs = now;
      }
      int n = (int)g_order.size();
      int idx = -1;
      for (int i = 0; i < n; i++) if (g_order[i] == g_sel) { idx = i; break; }
      int mv = g_listMove; g_listMove = 0;
      bool keep = (idx < 0 && g_detail && mv == 0);   // выбранное пропало — держим его подробности
      if (!keep && n > 0) {
        if (idx < 0) idx = 0;
        idx += mv;
        if (idx < 0) idx = 0;
        if (idx > n - 1) idx = n - 1;
        g_sel = g_order[idx];
        if (idx < g_top) g_top = idx;
        if (idx >= g_top + LIST_ROWS) g_top = idx - LIST_ROWS + 1;
      }
      if (g_top > n - LIST_ROWS) g_top = n - LIST_ROWS;
      if (g_top < 0) g_top = 0;

      if (g_detail && g_sel) {
        auto it = ents.find(g_sel);
        bool inMap = it != ents.end();
        // история сигнала выбранного: точка раз в 0.5 с
        if (g_sparkEnt != g_sel) { g_sparkEnt = g_sel; g_sparkN = 0; g_sparkMs = 0; }
        if (now - g_sparkMs >= 500) {
          g_sparkMs = now;
          int8_t v = (inMap && now - it->second.t < 3000) ? (int8_t)it->second.rssi : (int8_t)-127;
          if (g_sparkN < SPARK_N) g_spark[g_sparkN++] = v;
          else { memmove(g_spark, g_spark + 1, SPARK_N - 1); g_spark[SPARK_N - 1] = v; }
        }
        static Ent lastEnt;
        if (inMap) lastEnt = it->second;
        if (detValid) drawDetail(detDev, lastEnt, inMap, now);
        else          drawList(ents);
      } else {
        drawList(ents);
      }
    }
    else if (g_mode == M_RADAR)    drawRadar(count);
    else if (g_mode == M_PROX)     drawProximity(have, bestRssi, bestEnt, count);
    else if (g_mode == M_SCOPE)    drawScope(snap);
    else if (g_mode == M_TIMELINE) drawTimeline();
    else if (g_mode == M_TYPES)    drawTypes(typeCnt, count);
    else                           drawWeather(sens, now);
    // настройки сохраняем через 3 с после последнего изменения
    if (g_setDirty && now - g_setChangedMs > 3000) saveSettings();
  }
  delay(10);
}
