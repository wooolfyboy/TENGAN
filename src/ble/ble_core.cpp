// ============================================================
//  ble_core.cpp — раздел Bluetooth: сканирование, склейка адресов,
//  сведение в «физические» устройства, история, список, навигация.
//
//  Только приём: палка слушает рекламу (широковещательные пакеты),
//  ни к чему не подключается.
// ============================================================
#include <NimBLEDevice.h>
#include <algorithm>
#include "ble/ble.h"
#include "ble/ble_model.h"
#include "ble/vendors.h"
#include "core/display.h"
#include "core/leds.h"
#include "core/menu.h"
#include "core/settings.h"
#include "wifi/wifi.h"

// ---------- сырые данные (пишет колбэк сканера, под мьютексом) ----------
static std::map<uint64_t, Dev>    g_seen;     // по MAC-адресам
static std::map<uint64_t, Sensor> g_sens;     // датчики погоды
static uint32_t g_nextEnt = 0;
static SemaphoreHandle_t g_lock;

// ---------- состояние раздела ----------
volatile int g_mode = 0;
bool  g_bleActive = true;
int   g_lastCount = 0;
int   g_lastRaw = 0;
int   g_sensCount = 0;

Sample g_hist[HIST_N];
int    g_histLen = 0;
static float    g_base = -1.0f;
static uint32_t g_lastSampleMs = 0;
static uint32_t g_cooldownMs = 0;

std::vector<uint32_t> g_order;
static uint32_t g_orderMs = 0;
uint32_t g_sel = 0;
int      g_top = 0;
static int      g_listMove = 0;
bool     g_detail = false;
static uint32_t g_lastTouch = 0;
int8_t   g_spark[SPARK_N];
int      g_sparkN = 0;
static uint32_t g_sparkEnt = 0, g_sparkMs = 0;

// ---------- снимок на текущий кадр (bleUpdate) ----------
static std::map<uint32_t, Ent> g_ents;        // физические устройства
static std::vector<Sensor> g_sensSnap;
static std::vector<Blip>   g_snap;
static int   g_typeCnt[T_NUM];
static Ent   g_bestEnt;
static int   g_bestRssi = -127;
static bool  g_have = false;
static Dev   g_detDev;                        // последние известные подробности выбранного
static bool  g_detValid = false;
static uint32_t g_detFor = 0;

static bool bleIs(int m) { return navActive(&bleSection) && g_mode == m; }

// переносим разобранные поля рекламы в запись об адресе
static void fillAdv(Dev& v, const AdvInfo& ai) {
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

// ---------- сэмплер истории (раз в секунду) + детектор событий ±7 ----------
void bleSample(uint32_t now) {
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
  if (ev != 0) ledsEvent(ev);                       // запустить анимацию ленты
  g_base += 0.15f * (c - g_base);                 // дрейф базовой линии (EMA)

  Sample s = { c, g_base, ev };
  if (g_histLen < HIST_N) g_hist[g_histLen++] = s;
  else { memmove(&g_hist[0], &g_hist[1], sizeof(Sample) * (HIST_N - 1)); g_hist[HIST_N - 1] = s; }
}

void bleBegin() {
  g_lock = xSemaphoreCreateMutex();
  NimBLEDevice::init("");
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(new ScanCB(), true);   // true — получать каждый пакет, а не только первый
  s->setActiveScan(true);
  s->setInterval(100);
  s->setWindow(99);
  s->setMaxResults(0);                        // не копить результаты — всё в колбэке
  s->start(0, false);
  g_bleActive = true;
}

void bleRadioOn() {
  if (g_bleActive) return;
  NimBLEDevice::getScan()->start(0, false);
  g_bleActive = true;
  g_base = -1;                              // заново набрать базовую линию событий
  g_cooldownMs = millis() + 10000;          // и не мигать, пока список наполняется
}

void bleRadioOff() {
  if (!g_bleActive) return;
  NimBLEDevice::getScan()->stop();
  g_bleActive = false;
}

// ---------- каждый кадр: адреса -> физические устройства ----------
void bleUpdate(uint32_t now) {
  g_ents.clear();
  g_sensSnap.clear();
  g_snap.clear();
  if (g_detFor != g_sel) { g_detValid = false; g_detFor = g_sel; }
  bool wantDetail = bleIs(M_LIST) && g_detail;
  uint32_t detT = 0;
  int raw = 0;
  if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
    int thr = RANGES[g_range].dbm;
    for (auto it = g_seen.begin(); it != g_seen.end(); ) {
      if (now - it->second.t > REMEMBER_MS) { it = g_seen.erase(it); continue; }
      Dev& d = it->second;
      // фильтр дальности с гистерезисом + отсев одиночных пакетов
      if (!d.inRange && d.rssiS >= thr) d.inRange = true;
      else if (d.inRange && d.rssiS < thr - RANGE_HYST_DB) d.inRange = false;
      if (!d.inRange || d.pkts < MIN_PKTS) { ++it; continue; }
      Ent& e = g_ents[d.ent];           // новый Ent создаётся обнулённым
      if (d.t >= e.t) { e.t = d.t; e.rssi = d.rssi; e.rssiS = d.rssiS; e.key = it->first; }
      if (!e.first || d.first < e.first) e.first = d.first;
      if (d.pkts > e.pkts) e.pkts = d.pkts;
      e.nMacs++;
      if (d.type != T_UNKNOWN) e.type = d.type;
      if (d.vscore > e.vscore) { e.cat = d.cat; e.vkind = d.vkind; e.vcode = d.vcode; e.vscore = d.vscore; }
      if (d.name[0] && !e.name[0]) memcpy(e.name, d.name, sizeof(e.name));
      if (wantDetail && d.ent == g_sel && d.t >= detT) { g_detDev = d; detT = d.t; g_detValid = true; }
      ++it;
    }
    raw = (int)g_seen.size();
    g_sensCount = 0;
    for (auto it = g_sens.begin(); it != g_sens.end(); ) {
      if (now - it->second.t > SENSOR_REMEMBER_MS) { it = g_sens.erase(it); continue; }
      g_sensCount++;
      if (bleIs(M_WEATHER)) g_sensSnap.push_back(it->second);
      ++it;
    }
    xSemaphoreGive(g_lock);
  }

  g_have = false; g_bestRssi = -127; memset(&g_bestEnt, 0, sizeof(g_bestEnt));
  memset(g_typeCnt, 0, sizeof(g_typeCnt));
  bool wantScope = bleIs(M_SCOPE);
  for (auto& kv : g_ents) {
    const Ent& e = kv.second;
    if (e.rssi > g_bestRssi) { g_bestRssi = e.rssi; g_bestEnt = e; g_have = true; }
    g_typeCnt[e.type < T_NUM ? (int)e.type : (int)T_UNKNOWN]++;
    // угол блипа — от id физического устройства: не прыгает при смене MAC
    if (wantScope) g_snap.push_back({ e.rssi, kv.first * 2654435761u, e.cat, e.type });
  }
  g_lastCount = (int)g_ents.size();
  g_lastRaw = raw;
}

// ---------- список: «липкий» порядок и выбор строки ----------
static void listUpdate(uint32_t now) {
  // ближние сверху, но строки не прыгают:
  //  - пока крутишь колесо или открыты подробности — порядок заморожен;
  //  - соседи меняются местами, только если разница сигнала > ORDER_HYST_DB;
  //  - новые устройства вставляются на своё место, ушедшие убираются.
  bool frozen = g_detail || (now - g_lastTouch < ORDER_FREEZE);
  if (g_order.empty() || (!frozen && now - g_orderMs >= ORDER_EVERY)) {
    auto rs = [&](uint32_t id) { return g_ents[id].rssiS; };
    std::vector<uint32_t> o;
    o.reserve(g_ents.size());
    for (uint32_t id : g_order) if (g_ents.count(id)) o.push_back(id);
    for (size_t pass = 0; pass < o.size(); pass++) {
      bool sw = false;
      for (size_t i = 0; i + 1 < o.size(); i++)
        if (rs(o[i + 1]) > rs(o[i]) + ORDER_HYST_DB) { std::swap(o[i], o[i + 1]); sw = true; }
      if (!sw) break;
    }
    std::vector<uint32_t> fresh;
    for (auto& kv : g_ents)
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
    idx = constrain(idx + mv, 0, n - 1);
    g_sel = g_order[idx];
    if (idx < g_top) g_top = idx;
    if (idx >= g_top + LIST_ROWS) g_top = idx - LIST_ROWS + 1;
  }
  if (g_top > n - LIST_ROWS) g_top = n - LIST_ROWS;
  if (g_top < 0) g_top = 0;
}

static void drawListOrDetail(uint32_t now) {
  listUpdate(now);
  if (!(g_detail && g_sel)) { drawList(g_ents); return; }
  auto it = g_ents.find(g_sel);
  bool inMap = it != g_ents.end();
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
  if (g_detValid) drawDetail(g_detDev, lastEnt, inMap, now);
  else            drawList(g_ents);
}

// ---------- раздел для меню ----------
static void bleSubtitle(char* out, size_t n) {
  if (g_bleActive) snprintf(out, n, "%d devices", g_lastCount);
  else             snprintf(out, n, "paused");
}
static void bleEnter() { wifiRadioOff(); bleRadioOn(); }
static void bleLeave() {}

static void bleScreenInfo(int i, SubItem& it) {
  static const char* const names[NUM_MODES] = { "Radar", "Proximity", "Scope", "Timeline", "Types", "Devices", "Weather" };
  static const char* const descs[NUM_MODES] = { "how many around", "nearest device", "radar picture",
    "count over time", "what kind of", "list + details", "BLE thermometers" };
  it.name = names[i]; it.desc = descs[i]; it.value[0] = 0;
  switch (i) {
    case M_RADAR: case M_LIST: sprintf(it.value, "%d", g_lastCount); break;
    case M_PROX:
      if (g_have) sprintf(it.value, "%d dBm", g_bestRssi); else strcpy(it.value, "-");
      break;
    case M_TYPES: {
      int top = -1;
      for (int t = 1; t < T_NUM; t++) if (g_typeCnt[t] > 0 && (top < 0 || g_typeCnt[t] > g_typeCnt[top])) top = t;
      if (top > 0) snprintf(it.value, sizeof(it.value), "%s %d", typeName(top), g_typeCnt[top]);
      break;
    }
    case M_WEATHER:
      if (g_sensCount) sprintf(it.value, "%d", g_sensCount); else strcpy(it.value, "-");
      break;
  }
}
static int  bleScreenNow() { return g_mode; }
static void bleOpen(int m) { g_mode = m; g_detail = false; }
static void bleClose() { g_detail = false; }

static void bleClick() {
  if (g_mode == M_LIST) { g_detail = !g_detail; g_lastTouch = millis(); }
  else navToSub();                                 // на обычном экране нажатие — назад в подменю
}
static void bleRotate(int dir) {
  static uint32_t last = 0;
  if (g_mode == M_LIST) { g_listMove += dir; g_lastTouch = millis(); return; }
  if (millis() - last < 120) return;               // экраны не проскакивают при быстром вращении
  last = millis();
  g_mode = (g_mode + dir + NUM_MODES) % NUM_MODES;
  g_detail = false;
}
static bool bleBack() {
  if (g_detail) { g_detail = false; return true; }
  return false;
}
static void bleDraw(uint32_t now) {
  switch (g_mode) {
    case M_RADAR:    drawRadar(g_lastCount); break;
    case M_PROX:     drawProximity(g_have, g_bestRssi, g_bestEnt, g_lastCount); break;
    case M_SCOPE:    drawScope(g_snap); break;
    case M_TIMELINE: drawTimeline(); break;
    case M_TYPES:    drawTypes(g_typeCnt, g_lastCount); break;
    case M_LIST:     drawListOrDetail(now); break;
    default:         drawWeather(g_sensSnap, now); break;
  }
}

const Section bleSection = {
  "Bluetooth", 80, 150, 240, iconBluetooth, bleSubtitle,
  bleEnter, bleLeave,
  NUM_MODES, bleScreenInfo, bleScreenNow, bleOpen, bleClose,
  bleClick, bleRotate, bleBack, nullptr, bleDraw,
};
