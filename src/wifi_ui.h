// ============================================================
//  wifi_ui.h — раздел Wi-Fi (подключается внутрь main.cpp)
//
//  Только приём: пассивный скан (слушаем маячки точек доступа, сами
//  ничего не отправляем) и подсчёт пакетов в эфире. Содержимое пакетов
//  не читается — только заголовки (кто кому шлёт), и только для счёта.
//  ESP32-S3 работает только в диапазоне 2.4 ГГц — сети 5 ГГц не видны.
//
//  Экраны:
//    NETWORKS  — список сетей + подробности
//    CHANNELS  — загрузка каналов («холмики») и совет по каналу
//    ACTIVITY  — пакеты в секунду и шум по каналам
//    CLIENTS   — сколько устройств работает в каждой сети, сколько ищут Wi-Fi
//    FIND      — «горячо-холодно» по выбранной сети (быстрый скан одного канала)
// ============================================================
#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "ouidb.h"

enum { W_NETS = 0, W_CHANNELS, W_ACTIVITY, W_CLIENTS, W_FIND, W_NUM };
int  g_wMode = 0;
bool g_wOn = false;
bool g_wEverScanned = false;
inline bool wNeedsProm(int m) { return m == W_ACTIVITY || m == W_CLIENTS; }

// ---------- точки доступа ----------
struct Ap {
  uint8_t  bssid[6];
  char     ssid[33];
  int8_t   rssi, rssiS, rmin, rmax;
  uint8_t  ch, enc;
  int8_t   sec;        // второй канал (40 МГц): +1 выше, -1 ниже, 0 — обычные 20 МГц
  uint8_t  gen;        // 6 = Wi-Fi 6 (ax), 4 = Wi-Fi 4 (n), 3 = 11g, 1 = 11b
  bool     hotspot;    // похоже на раздачу с телефона
  const char* vendor;  // производитель по OUI (или nullptr)
  uint32_t t, first;
};
std::map<uint64_t, Ap> g_aps;
static const uint32_t AP_REMEMBER_MS = 60000;
static const int CH_MAX = 13;

bool     g_scanning = false;
uint32_t g_scanRetryMs = 0;
int      g_scanCh = 0;           // 0 — все каналы, иначе один (режим FIND)

// список
std::vector<uint64_t> g_wOrder;
uint64_t g_wSel = 0;
int      g_wTop = 0;
int      g_wMove = 0;
bool     g_wDetail = false;
uint32_t g_wTouch = 0;
#define W_SPARK_N 60
int8_t   g_wSpark[W_SPARK_N];
int      g_wSparkN = 0;
uint64_t g_wSparkFor = 0;
// FIND
#define F_SPARK_N 100
int8_t   g_fSpark[F_SPARK_N];
int      g_fSparkN = 0;
uint64_t g_fFor = 0;
uint32_t g_fLastHit = 0;

// ---------- приём пакетов (promiscuous) ----------
volatile uint32_t g_pMgmt = 0, g_pData = 0;
volatile int32_t  g_nfSum = 0;
volatile uint32_t g_nfCnt = 0;
bool     g_prom = false;
int      g_hopCh = 1;
uint32_t g_hopMs = 0, g_lastMgmt = 0, g_lastData = 0;
float    g_ppsMgmt[CH_MAX + 1] = {0}, g_ppsData[CH_MAX + 1] = {0};
float    g_noise[CH_MAX + 1] = {0};       // шум по каналам, дБм (0 = нет данных)
#define ACT_HIST_N 120
float    g_actHist[ACT_HIST_N];
int      g_actHistN = 0;
uint32_t g_actHistMs = 0;
static const uint32_t HOP_MS = 200;

// события из обработчика пакетов -> основной цикл (кольцевой буфер)
struct PEv { uint8_t kind; uint8_t a[6]; uint8_t b[6]; };   // 1: data (a=устройство, b=сеть), 2: probe (a=источник)
#define PEV_N 128
PEv g_pev[PEV_N];
volatile uint16_t g_pevHead = 0, g_pevTail = 0;

// устройства в сетях и «ищущие Wi-Fi» (только в памяти, только для счёта)
struct Sta { uint64_t bssid; uint32_t t; };
std::map<uint64_t, Sta> g_stas;
std::map<uint64_t, uint32_t> g_probes;
static const uint32_t STA_REMEMBER_MS = 120000;
static const size_t   STA_MAX = 500;

static inline void IRAM_ATTR pushEv(uint8_t kind, const uint8_t* a, const uint8_t* b) {
  uint16_t h = g_pevHead, n = (h + 1) % PEV_N;
  if (n == g_pevTail) return;                    // буфер полон — пропускаем
  PEv& e = g_pev[h];
  e.kind = kind;
  for (int i = 0; i < 6; i++) { e.a[i] = a[i]; e.b[i] = b ? b[i] : 0; }
  g_pevHead = n;
}

void IRAM_ATTR promCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  int nf = p->rx_ctrl.noise_floor;
  if (nf < -20 && nf > -120) { g_nfSum += nf; g_nfCnt++; }
  const uint8_t* f = p->payload;
  int len = p->rx_ctrl.sig_len;
  if (type == WIFI_PKT_MGMT) {
    g_pMgmt++;
    if (len >= 24 && (f[0] & 0xFC) == 0x40) pushEv(2, f + 10, nullptr);   // probe request
  } else if (type == WIFI_PKT_DATA) {
    g_pData++;
    if (len >= 24) {
      uint8_t ds = f[1] & 0x03;                                            // ToDS / FromDS
      if (ds == 1)      pushEv(1, f + 10, f + 4);   // устройство -> сеть
      else if (ds == 2) pushEv(1, f + 4,  f + 10);  // сеть -> устройство
    }
  }
}

uint64_t macKey(const uint8_t* m) {
  uint64_t k = 0;
  for (int i = 0; i < 6; i++) k = (k << 8) | m[i];
  return k;
}

// ---------- подписи и цвета ----------
const char* encName(uint8_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Ent";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    case WIFI_AUTH_OWE:             return "OWE";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
    default:                        return "other";
  }
}
uint16_t encColor(uint8_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN:            return tft.color565(235, 80, 80);
    case WIFI_AUTH_WEP:
    case WIFI_AUTH_WPA_PSK:         return tft.color565(240, 150, 50);
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK:
    case WIFI_AUTH_OWE:             return tft.color565(60, 205, 205);
    case WIFI_AUTH_WPA2_ENTERPRISE: return tft.color565(160, 120, 245);
    default:                        return tft.color565(70, 210, 130);
  }
}
const char* genName(uint8_t g) {
  switch (g) { case 6: return "Wi-Fi 6"; case 4: return "Wi-Fi 4"; case 3: return "11g"; default: return "11b"; }
}
uint16_t apColor(uint64_t key) {
  static const uint8_t pal[8][3] = {
    {80,150,240},{70,210,130},{240,200,70},{240,90,160},{60,205,205},{240,150,50},{170,220,80},{160,120,245} };
  uint32_t h = (uint32_t)(key ^ (key >> 29)) * 2654435761u;
  const uint8_t* c = pal[(h >> 24) & 7];
  return tft.color565(c[0], c[1], c[2]);
}
uint16_t dimColor(uint16_t c, int pct) {
  uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return tft.color565(r * pct / 100, g * pct / 100, b * pct / 100);
}
const char* ssidOr(const Ap& a) { return a.ssid[0] ? a.ssid : "(hidden)"; }

// раздача с телефона? (по имени сети и производителю)
bool looksLikeHotspot(const char* ssid, const char* vendor) {
  char low[33]; size_t i = 0;
  for (; ssid[i] && i < 32; i++) low[i] = (char)tolower((unsigned char)ssid[i]);
  low[i] = 0;
  static const char* const kw[] = { "iphone", "ipad", "androidap", "android", "galaxy", "redmi",
    "poco", "honor", "realme", "oneplus", "pixel", "hotspot", "huawei p", "huawei mate",
    "mi phone", "xiaomi 1", "nothing phone", "moto ", nullptr };
  for (int k = 0; kw[k]; k++) if (strstr(low, kw[k])) return true;
  if (vendor && (!strncmp(vendor, "Apple", 5) || !strncmp(vendor, "Samsung", 7) ||
                 !strncmp(vendor, "Xiaomi Comm", 11) || !strncmp(vendor, "Motorola Mob", 12) ||
                 !strncmp(vendor, "OnePlus", 7))) return true;
  return false;
}

// ---------- управление радио ----------
void promOn() {
  if (g_prom || !g_wOn) return;
  esp_wifi_scan_stop();
  WiFi.scanDelete();
  g_scanning = false;
  wifi_promiscuous_filter_t f = { WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA };
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous_rx_cb(promCb);
  esp_wifi_set_promiscuous(true);
  g_hopCh = 1;
  esp_wifi_set_channel(g_hopCh, WIFI_SECOND_CHAN_NONE);
  g_hopMs = millis();
  g_lastMgmt = g_pMgmt; g_lastData = g_pData;
  g_nfSum = 0; g_nfCnt = 0;
  g_prom = true;
}
void promOff() {
  if (!g_prom) return;
  esp_wifi_set_promiscuous(false);
  g_prom = false;
}
void ledsOff() { strip.clear(); strip.show(); }

void wifiStart() {
  if (g_wOn) return;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  g_wOn = true;
  g_scanning = false;
  g_scanRetryMs = 0;
}
void wifiStop() {
  if (!g_wOn) return;
  promOff();
  esp_wifi_scan_stop();
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  g_wOn = false;
  g_scanning = false;
}
void wifiEnter() {
  wifiStart();
  if (wNeedsProm(g_wMode)) promOn();
}
void wifiLeave() {
  promOff();
  ledsOff();
}
int wifiNetCount() { return g_wEverScanned ? (int)g_aps.size() : -1; }

void wifiSetMode(int m) {
  int prev = g_wMode;
  g_wMode = (m + W_NUM) % W_NUM;
  g_wDetail = false;
  if (wNeedsProm(g_wMode)) promOn(); else promOff();
  // скан «всех каналов» и «одного канала» несовместимы — перезапускаем
  if ((prev == W_FIND) != (g_wMode == W_FIND) && g_scanning) {
    esp_wifi_scan_stop(); WiFi.scanDelete(); g_scanning = false;
  }
  ledsOff();
}

// цель для FIND: выбранная в списке сеть, иначе самая сильная
uint64_t findTarget() {
  if (g_wSel && g_aps.count(g_wSel)) return g_wSel;
  uint64_t best = 0; int br = -127;
  for (auto& kv : g_aps) if (kv.second.rssiS > br) { br = kv.second.rssiS; best = kv.first; }
  g_wSel = best;
  return best;
}

// ---------- скан: забираем результаты и запускаем следующий ----------
void wifiOrderUpdate(bool force);

void wifiScanTick(uint32_t now) {
  if (!g_wOn || g_prom) return;
  if (!g_scanning) {
    if (now < g_scanRetryMs) return;
    g_scanCh = 0;
    uint32_t perCh = 150;
    if (g_wMode == W_FIND) {
      uint64_t t = findTarget();
      if (t) { g_scanCh = g_aps[t].ch; perCh = 110; }
    }
    // async, show_hidden, passive (ничего не отправляем), время на канал, канал (0 = все)
    if (WiFi.scanNetworks(true, true, true, perCh, g_scanCh) == WIFI_SCAN_FAILED) {
      g_scanRetryMs = now + 1000; return;
    }
    g_scanning = true;
    return;
  }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  g_scanning = false;
  if (n < 0) { g_scanRetryMs = now + 500; return; }
  for (int i = 0; i < n; i++) {
    wifi_ap_record_t* r = (wifi_ap_record_t*)WiFi.getScanInfoByIndex(i);
    if (!r) continue;
    uint64_t k = macKey(r->bssid);
    auto it = g_aps.find(k);
    if (it == g_aps.end()) {
      Ap a; memset(&a, 0, sizeof(a));
      memcpy(a.bssid, r->bssid, 6);
      a.first = now; a.rssiS = a.rmin = a.rmax = r->rssi;
      a.vendor = ouiVendor(r->bssid);
      it = g_aps.insert({ k, a }).first;
    }
    Ap& a = it->second;
    strncpy(a.ssid, (const char*)r->ssid, 32); a.ssid[32] = 0;
    a.ch  = r->primary;
    a.enc = (uint8_t)r->authmode;
    a.sec = r->second == WIFI_SECOND_CHAN_ABOVE ? 1 : r->second == WIFI_SECOND_CHAN_BELOW ? -1 : 0;
    a.gen = r->phy_11ax ? 6 : r->phy_11n ? 4 : r->phy_11g ? 3 : 1;
    a.hotspot = looksLikeHotspot(a.ssid, a.vendor);
    a.rssi  = r->rssi;
    a.rssiS = (int8_t)((a.rssiS + r->rssi) / 2);
    if (r->rssi < a.rmin) a.rmin = r->rssi;
    if (r->rssi > a.rmax) a.rmax = r->rssi;
    a.t = now;
  }
  WiFi.scanDelete();

  if (g_scanCh == 0) {                       // полный проход
    g_wEverScanned = true;
    for (auto it = g_aps.begin(); it != g_aps.end(); )
      if (now - it->second.t > AP_REMEMBER_MS) it = g_aps.erase(it); else ++it;
    if (g_wDetail && g_wSel) {               // история для подробностей
      if (g_wSparkFor != g_wSel) { g_wSparkFor = g_wSel; g_wSparkN = 0; }
      auto it = g_aps.find(g_wSel);
      int8_t v = (it != g_aps.end() && it->second.t == now) ? it->second.rssi : (int8_t)-127;
      if (g_wSparkN < W_SPARK_N) g_wSpark[g_wSparkN++] = v;
      else { memmove(g_wSpark, g_wSpark + 1, W_SPARK_N - 1); g_wSpark[W_SPARK_N - 1] = v; }
    }
    wifiOrderUpdate(false);
  } else {                                    // FIND: быстрый проход одного канала
    uint64_t t = g_wSel;
    if (g_fFor != t) { g_fFor = t; g_fSparkN = 0; }
    auto it = g_aps.find(t);
    bool hit = it != g_aps.end() && it->second.t == now;
    if (hit) g_fLastHit = now;
    int8_t v = hit ? it->second.rssi : (int8_t)-127;
    if (g_fSparkN < F_SPARK_N) g_fSpark[g_fSparkN++] = v;
    else { memmove(g_fSpark, g_fSpark + 1, F_SPARK_N - 1); g_fSpark[F_SPARK_N - 1] = v; }
  }
}

// ---------- приём: прыгаем по каналам, считаем пакеты, шум и устройства ----------
void wifiHopTick(uint32_t now) {
  if (!g_prom) return;
  // разбираем события от обработчика пакетов
  while (g_pevTail != g_pevHead) {
    PEv e = g_pev[g_pevTail];
    g_pevTail = (g_pevTail + 1) % PEV_N;
    if (e.a[0] & 0x01) continue;                       // групповые адреса — не устройства
    uint64_t a = macKey(e.a);
    if (e.kind == 1) {
      uint64_t b = macKey(e.b);
      if (a == b) continue;
      auto it = g_stas.find(a);
      if (it != g_stas.end()) it->second = { b, now };
      else if (g_stas.size() < STA_MAX) g_stas[a] = { b, now };
    } else {
      auto it = g_probes.find(a);
      if (it != g_probes.end()) it->second = now;
      else if (g_probes.size() < STA_MAX) g_probes[a] = now;
    }
  }

  uint32_t dt = now - g_hopMs;
  if (dt < HOP_MS) return;
  uint32_t m = g_pMgmt, d = g_pData;
  float sm = (m - g_lastMgmt) * 1000.0f / dt, sd = (d - g_lastData) * 1000.0f / dt;
  g_lastMgmt = m; g_lastData = d;
  g_ppsMgmt[g_hopCh] = g_ppsMgmt[g_hopCh] * 0.4f + sm * 0.6f;
  g_ppsData[g_hopCh] = g_ppsData[g_hopCh] * 0.4f + sd * 0.6f;
  int32_t nfs = g_nfSum; uint32_t nfc = g_nfCnt;
  g_nfSum = 0; g_nfCnt = 0;
  if (nfc > 0) {
    float nf = (float)nfs / nfc;
    g_noise[g_hopCh] = g_noise[g_hopCh] == 0 ? nf : g_noise[g_hopCh] * 0.5f + nf * 0.5f;
  }
  g_hopCh = g_hopCh % CH_MAX + 1;
  esp_wifi_set_channel(g_hopCh, WIFI_SECOND_CHAN_NONE);
  g_hopMs = now;

  if (now - g_actHistMs >= 1000) {
    g_actHistMs = now;
    float tot = 0;
    for (int c = 1; c <= CH_MAX; c++) tot += g_ppsMgmt[c] + g_ppsData[c];
    if (g_actHistN < ACT_HIST_N) g_actHist[g_actHistN++] = tot;
    else { memmove(g_actHist, g_actHist + 1, sizeof(float) * (ACT_HIST_N - 1)); g_actHist[ACT_HIST_N - 1] = tot; }
    for (auto it = g_stas.begin(); it != g_stas.end(); )
      if (now - it->second.t > STA_REMEMBER_MS) it = g_stas.erase(it); else ++it;
    for (auto it = g_probes.begin(); it != g_probes.end(); )
      if (now - it->second > STA_REMEMBER_MS) it = g_probes.erase(it); else ++it;
  }
}

int clientsOf(uint64_t bssid) {
  int n = 0;
  for (auto& kv : g_stas) if (kv.second.bssid == bssid) n++;
  return n;
}

// ---------- список сетей: «липкий» порядок, как у Bluetooth ----------
void wifiOrderUpdate(bool force) {
  bool frozen = g_wDetail || (millis() - g_wTouch < 4000);
  if (frozen && !force && !g_wOrder.empty()) return;
  std::vector<uint64_t> o;
  for (uint64_t k : g_wOrder) if (g_aps.count(k)) o.push_back(k);
  auto rs = [&](uint64_t k) { return (int)g_aps[k].rssiS; };
  for (size_t pass = 0; pass < o.size(); pass++) {
    bool sw = false;
    for (size_t i = 0; i + 1 < o.size(); i++)
      if (rs(o[i + 1]) > rs(o[i]) + 6) { std::swap(o[i], o[i + 1]); sw = true; }
    if (!sw) break;
  }
  for (auto& kv : g_aps) {
    if (std::find(o.begin(), o.end(), kv.first) != o.end()) continue;
    size_t pos = 0;
    while (pos < o.size() && rs(o[pos]) >= kv.second.rssiS) pos++;
    o.insert(o.begin() + pos, kv.first);
  }
  g_wOrder.swap(o);
}

void wifiApplyMove() {
  int n = (int)g_wOrder.size();
  int idx = -1;
  for (int i = 0; i < n; i++) if (g_wOrder[i] == g_wSel) { idx = i; break; }
  int mv = g_wMove; g_wMove = 0;
  if (n == 0) { if (mv) wifiSetMode(g_wMode + (mv > 0 ? 1 : -1)); return; }
  if (idx < 0) { if (g_wDetail && mv == 0) return; idx = 0; }
  if (!g_wDetail && mv < 0 && idx == 0)     { wifiSetMode(g_wMode - 1); return; }   // за верхний край
  if (!g_wDetail && mv > 0 && idx == n - 1) { wifiSetMode(g_wMode + 1); return; }   // за нижний край
  idx = constrain(idx + mv, 0, n - 1);
  g_wSel = g_wOrder[idx];
  if (idx < g_wTop) g_wTop = idx;
  if (idx >= g_wTop + LIST_ROWS) g_wTop = idx - LIST_ROWS + 1;
  if (g_wTop > n - LIST_ROWS) g_wTop = n - LIST_ROWS;
  if (g_wTop < 0) g_wTop = 0;
}

// колесо и нажатие в разделе Wi-Fi
void wifiRotate(int dir) {
  if (g_wMode == W_NETS) { g_wMove += dir; g_wTouch = millis(); }
  else wifiSetMode(g_wMode + dir);
}
void wifiClick() {
  if (g_wMode == W_NETS && !g_wOrder.empty()) { g_wDetail = !g_wDetail; g_wTouch = millis(); return; }
  if (g_wMode == W_FIND && !g_wOrder.empty()) {        // следующая сеть-цель
    int idx = -1;
    for (size_t i = 0; i < g_wOrder.size(); i++) if (g_wOrder[i] == g_wSel) idx = (int)i;
    g_wSel = g_wOrder[(idx + 1) % g_wOrder.size()];
    if (g_scanning) { esp_wifi_scan_stop(); WiFi.scanDelete(); g_scanning = false; }
    return;
  }
  wifiSetMode(g_wMode + 1);
}

// ---------- экраны ----------
void drawScanDot(int x, int y) {
  if ((g_scanning || g_prom) && (millis() / 300) % 2) spr.fillCircle(x, y, 2, tft.color565(70, 210, 130));
}
void drawWaiting(int W, int H) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString(g_wEverScanned ? "no networks" : "scanning...", W / 2, H / 2, 4);
}

void drawNets(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  sprintf(buf, "NETWORKS  %d", (int)g_wOrder.size());
  spr.drawString(buf, 4, 1, 2);
  drawScanDot(spr.textWidth(buf, 2) + 10, 9);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("wheel: pick  click: info", W - 4, 5, 1);

  if (g_wOrder.empty()) { drawWaiting(W, H); spr.pushSprite(0, 0); return; }
  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = g_wTop + i;
    if (idx >= (int)g_wOrder.size()) break;
    uint64_t k = g_wOrder[idx];
    auto it = g_aps.find(k);
    if (it == g_aps.end()) continue;
    const Ap& a = it->second;
    int y = LIST_Y0 + i * LIST_ROWH;
    bool sel = (k == g_wSel);
    uint16_t bg = sel ? tft.color565(28, 38, 62) : TFT_BLACK;
    if (sel) spr.fillRoundRect(0, y, W - 4, LIST_ROWH - 1, 3, bg);
    spr.fillRect(3, y + 3, 4, LIST_ROWH - 7, encColor(a.enc));
    fitText(ssidOr(a), buf, sizeof(buf), 150, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(a.ssid[0] ? TFT_WHITE : C_DIM(), bg);
    spr.drawString(buf, 12, y + 1, 2);
    sprintf(buf, "%d", a.ch);
    spr.setTextColor(apColor(k), bg);
    spr.drawString(buf, 168, y + 1, 2);
    // метка: открытая > раздача с телефона > поколение
    if (a.enc == WIFI_AUTH_OPEN)  { spr.setTextColor(encColor(a.enc), bg); spr.drawString("open", 190, y + 1, 2); }
    else if (a.hotspot)           { spr.setTextColor(tft.color565(240, 150, 50), bg); spr.drawString("phone", 190, y + 1, 2); }
    else if (a.gen == 6)          { spr.setTextColor(tft.color565(60, 205, 205), bg); spr.drawString("W6", 190, y + 1, 2); }
    sprintf(buf, "%d", a.rssi);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(now - a.t > 6000 ? C_DIM() : C_GRAY(), bg);
    spr.drawString(buf, 268, y + 1, 2);
    drawBars(274, y + 2, a.rssi, tft.color565(90, 220, 140));
  }
  int n = (int)g_wOrder.size();
  if (n > LIST_ROWS) {
    int trackH = LIST_ROWS * LIST_ROWH;
    int barH = trackH * LIST_ROWS / n; if (barH < 8) barH = 8;
    int barY = LIST_Y0 + (trackH - barH) * g_wTop / (n - LIST_ROWS);
    spr.fillRect(W - 2, LIST_Y0, 2, trackH, tft.color565(30, 30, 38));
    spr.fillRect(W - 2, barY, 2, barH, C_GRAY());
  }
  spr.pushSprite(0, 0);
}

// полуширина сети в «каналах» (канал = 5 МГц): 20 МГц -> 2, 40 МГц -> 4
float apCenter(const Ap& a) { return a.ch + a.sec * 2.0f; }
float apHalf(const Ap& a)   { return a.sec ? 4.0f : 2.0f; }

void drawNetDetail(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[64], tmp[48];
  auto it = g_aps.find(g_wSel);
  if (it == g_aps.end()) { g_wDetail = false; drawNets(now); return; }
  const Ap& a = it->second;

  fitText(ssidOr(a), tmp, sizeof(tmp), W - 70, 4);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(a.ssid[0] ? TFT_WHITE : C_DIM(), TFT_BLACK);
  spr.drawString(tmp, 4, 2, 4);
  int pos = -1;
  for (size_t i = 0; i < g_wOrder.size(); i++) if (g_wOrder[i] == g_wSel) pos = (int)i;
  if (pos >= 0) {
    sprintf(buf, "%d/%d", pos + 1, (int)g_wOrder.size());
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString(buf, W - 4, 6, 2);
  }

  // строка 2: защита · канал · ширина · поколение
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(encColor(a.enc), TFT_BLACK);
  spr.drawString(encName(a.enc), 4, 30, 2);
  int x = 4 + spr.textWidth(encName(a.enc), 2) + 10;
  sprintf(buf, "ch %d  %d MHz  %s", a.ch, a.sec ? 40 : 20, genName(a.gen));
  spr.setTextColor(apColor(g_wSel), TFT_BLACK);
  spr.drawString(buf, x, 30, 2);

  // левая колонка
  int sameCh = 0, near = 0;
  for (auto& kv : g_aps) {
    if (kv.first == g_wSel) continue;
    float d = fabsf(apCenter(kv.second) - apCenter(a));
    if (d < 0.5f) sameCh++; else if (d < apHalf(kv.second) + apHalf(a)) near++;
  }
  int y = 50; const int LH = 15;
  uint32_t ago = (now - a.t) / 1000;
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  if (ago >= 6) sprintf(buf, "signal %d (%lus ago)", a.rssi, (unsigned long)ago);
  else          sprintf(buf, "signal %d dBm", a.rssi);
  spr.drawString(buf, 4, y, 2); y += LH;
  sprintf(buf, "range  %d .. %d", a.rmin, a.rmax);
  spr.drawString(buf, 4, y, 2); y += LH;
  uint32_t secs = (now - a.first) / 1000;
  sprintf(buf, "seen   %lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  spr.drawString(buf, 4, y, 2); y += LH;
  sprintf(buf, "rivals %d same, %d near", sameCh, near);
  spr.drawString(buf, 4, y, 2); y += LH;
  int cl = clientsOf(g_wSel);
  if (cl > 0) sprintf(buf, "devices %d", cl);
  else        sprintf(buf, "devices ? (see CLIENTS)");
  spr.drawString(buf, 4, y, 2);

  // график сигнала (~2 мин)
  int gx = 176, gy = 50, gw = W - gx - 4, gh = 70;
  spr.drawRect(gx, gy, gw, gh, tft.color565(40, 40, 50));
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("signal, 2 min", gx + 3, gy + 3, 1);
  int px = -1, py = -1;
  for (int i = 0; i < g_wSparkN; i++) {
    int v = g_wSpark[i];
    int xx = gx + 1 + (gw - 3) * (i + (W_SPARK_N - g_wSparkN)) / (W_SPARK_N - 1);
    if (v <= -127) { px = -1; continue; }
    int c = constrain(v, -100, -30);
    int yy = gy + gh - 2 - (c + 100) * (gh - 4) / 70;
    if (px >= 0) spr.drawLine(px, py, xx, yy, tft.color565(90, 220, 140));
    else         spr.drawPixel(xx, yy, tft.color565(90, 220, 140));
    px = xx; py = yy;
  }

  // низ: производитель и BSSID
  const uint8_t* b = a.bssid;
  bool local = b[0] & 0x02;
  if (a.hotspot)      sprintf(buf, "MAKER %s  -> phone hotspot", a.vendor ? a.vendor : "?");
  else if (a.vendor)  sprintf(buf, "MAKER %s", a.vendor);
  else if (local)     sprintf(buf, "MAKER hidden (random MAC: hotspot or extra network)");
  else                sprintf(buf, "MAKER unknown");
  spr.setTextColor(a.hotspot ? tft.color565(240, 150, 50) : C_GRAY(), TFT_BLACK);
  spr.drawString(buf, 4, 130, 1);
  sprintf(buf, "BSSID %02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString(buf, 4, 141, 1);
  if (a.enc == WIFI_AUTH_OPEN) {
    spr.setTextColor(tft.color565(235, 80, 80), TFT_BLACK);
    spr.drawString("open network: traffic is not encrypted", 4, 152, 1);
  }
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("click: back", W - 4, H - 1, 1);
  spr.pushSprite(0, 0);
}

// насколько сеть мешает каналу k (0..1)
float interference(const Ap& a, int k) {
  float d = fabsf(apCenter(a) - k);
  float reach = apHalf(a) + 2.0f + 1.0f;      // наша ширина 20 МГц (±2) + запас
  return d >= reach ? 0.0f : 1.0f - d / reach;
}

void drawChannels(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[40];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("CHANNELS", 4, 1, 2);
  drawScanDot(78, 9);

  int pl = 24, pr = W - 8, pt = 22, pb = H - 22;
  auto chX = [&](float ch) { return pl + (int)((ch - 1) * (pr - pl) / (CH_MAX - 1)); };
  auto dbY = [&](int db) { int c = constrain(db, -100, -30); return pb - (c + 100) * (pb - pt) / 70; };

  for (int db = -90; db <= -40; db += 10) {
    int y = dbY(db);
    spr.drawFastHLine(pl, y, pr - pl, tft.color565(28, 28, 34));
    sprintf(buf, "%d", db);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(tft.color565(70, 70, 80), TFT_BLACK);
    spr.drawString(buf, pl - 2, y, 1);
  }
  spr.drawFastHLine(pl, pb, pr - pl, tft.color565(60, 60, 70));
  for (int c = 1; c <= CH_MAX; c++) {
    bool main3 = (c == 1 || c == 6 || c == 11);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(main3 ? C_GRAY() : tft.color565(70, 70, 80), TFT_BLACK);
    sprintf(buf, "%d", c);
    spr.drawString(buf, chX(c), pb + 3, main3 ? 2 : 1);
  }

  // «холмики»: ширина по реальной полосе сети (20 или 40 МГц), слабые снизу
  std::vector<std::pair<int, uint64_t>> v;
  for (auto& kv : g_aps) if (kv.second.ch >= 1 && kv.second.ch <= CH_MAX) v.push_back({ kv.second.rssiS, kv.first });
  std::sort(v.begin(), v.end());
  for (auto& p : v) {
    const Ap& a = g_aps[p.second];
    uint16_t c = apColor(p.second);
    uint16_t fill = dimColor(c, 22);
    int top = dbY(a.rssiS);
    float cen = apCenter(a), half = apHalf(a);
    int prevx = -1, prevy = -1;
    for (int s = 0; s <= 24; s++) {
      float f = -1.0f + s / 12.0f;
      int x = chX(cen + f * half);
      int y = pb - (int)((pb - top) * (1.0f - f * f));
      if (x < pl || x > pr) { prevx = -1; continue; }
      if (y < pb) spr.drawFastVLine(x, y, pb - y, fill);
      if (prevx >= 0) spr.drawLine(prevx, prevy, x, y, c);
      prevx = x; prevy = y;
    }
  }
  int labeled = 0;
  for (int i = (int)v.size() - 1; i >= 0 && labeled < 4; i--, labeled++) {
    const Ap& a = g_aps[v[i].second];
    char nm[20];
    fitText(ssidOr(a), nm, sizeof(nm), 70, 1);
    spr.setTextDatum(BC_DATUM);
    spr.setTextColor(apColor(v[i].second), TFT_BLACK);
    spr.drawString(nm, constrain(chX(apCenter(a)), pl + 20, pr - 20), dbY(a.rssiS) - 2, 1);
  }

  if (!g_aps.empty()) {
    const int cand[3] = { 1, 6, 11 };
    float best = 1e9; int bestCh = 1;
    for (int k = 0; k < 3; k++) {
      float score = 0;
      for (auto& kv : g_aps) score += interference(kv.second, cand[k]) * (kv.second.rssiS + 100);
      if (score < best) { best = score; bestCh = cand[k]; }
    }
    sprintf(buf, "best channel: %d", bestCh);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(tft.color565(70, 210, 130), TFT_BLACK);
    spr.drawString(buf, W - 4, 1, 2);
  } else {
    drawWaiting(W, H);
  }
  spr.pushSprite(0, 0);
}

void drawActivity(uint32_t now) {
  int W = spr.width();
  spr.fillSprite(TFT_BLACK);
  char buf[40];
  float tot = 0, mx = 1;
  for (int c = 1; c <= CH_MAX; c++) {
    float t = g_ppsMgmt[c] + g_ppsData[c];
    tot += t; if (t > mx) mx = t;
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("ACTIVITY", 4, 1, 2);
  drawScanDot(76, 9);
  sprintf(buf, "%d pkt/s", (int)(tot + 0.5f));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W - 4, 1, 2);

  // столбики: данные (зелёные) + маячки/служебные (синие), шкала — корень
  int pl = 28, pr = W - 4, pt = 20, pb = 92;
  int bw = (pr - pl) / CH_MAX;
  float smx = sqrtf(mx);
  for (int c = 1; c <= CH_MAX; c++) {
    int x = pl + (c - 1) * bw;
    float t = g_ppsMgmt[c] + g_ppsData[c];
    int h = (int)((pb - pt) * sqrtf(t) / smx);
    int hd = t > 0 ? (int)(h * g_ppsData[c] / t) : 0;
    spr.fillRect(x + 2, pt, bw - 4, pb - pt, tft.color565(22, 22, 28));
    if (h > 0) {
      spr.fillRect(x + 2, pb - h, bw - 4, h - hd, tft.color565(80, 150, 240));
      spr.fillRect(x + 2, pb - hd, bw - 4, hd, tft.color565(70, 210, 130));
    }
    if (c == g_hopCh) spr.fillRect(x + 2, pb + 1, bw - 4, 2, TFT_WHITE);
    bool main3 = (c == 1 || c == 6 || c == 11);
    sprintf(buf, "%d", c);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(main3 ? C_GRAY() : tft.color565(70, 70, 80), TFT_BLACK);
    spr.drawString(buf, x + bw / 2, pb + 4, 1);
    // шум канала: обычно ~ -95..-90; выше -85 — что-то сильно шумит
    if (g_noise[c] != 0) {
      int nf = (int)lroundf(g_noise[c]);
      uint16_t col = nf > -80 ? tft.color565(235, 80, 80) : nf > -88 ? tft.color565(240, 150, 50) : tft.color565(90, 90, 104);
      sprintf(buf, "%d", -nf);
      spr.setTextColor(col, TFT_BLACK);
      spr.drawString(buf, x + bw / 2, pb + 14, 1);
    }
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("ch", 4, pb + 4, 1);
  spr.drawString("-dBm", 0, pb + 14, 1);
  spr.drawString("noise", 0, pb + 23, 1);
  spr.fillRect(pl + 40, pb + 26, 6, 6, tft.color565(70, 210, 130));
  spr.drawString("data", pl + 49, pb + 25, 1);
  spr.fillRect(pl + 82, pb + 26, 6, 6, tft.color565(80, 150, 240));
  spr.drawString("beacons/service", pl + 91, pb + 25, 1);

  // история общего трафика ~2 мин
  int gx = 4, gy = 128, gw = W - 8, gh = 38;
  spr.drawRect(gx, gy, gw, gh, tft.color565(40, 40, 50));
  float hm = 1;
  for (int i = 0; i < g_actHistN; i++) if (g_actHist[i] > hm) hm = g_actHist[i];
  for (int i = 0; i < g_actHistN; i++) {
    int xx = gx + 1 + (gw - 3) * (i + (ACT_HIST_N - g_actHistN)) / (ACT_HIST_N - 1);
    int h = (int)((gh - 3) * g_actHist[i] / hm);
    spr.drawFastVLine(xx, gy + gh - 1 - h, h, tft.color565(70, 170, 120));
  }
  sprintf(buf, "2 min, peak %d", (int)hm);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString(buf, gx + gw - 3, gy + 3, 1);
  spr.pushSprite(0, 0);

  if (g_ledEvent == 0) {                       // лента — уровень трафика
    int lv = tot < 1 ? 0 : (int)(log10f(tot) * 2.7f);
    lv = constrain(lv, 0, WS2812_N);
    strip.clear();
    for (int i = 0; i < lv; i++) {
      int r = i * 255 / (WS2812_N - 1), g = 255 - r;
      strip.setPixelColor(i, strip.Color(r / 3, g / 3, 0));
    }
    strip.show();
  }
}

void drawClients(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];
  // считаем устройства по сетям
  std::map<uint64_t, int> per;
  for (auto& kv : g_stas) per[kv.second.bssid]++;
  std::vector<std::pair<int, uint64_t>> v;
  for (auto& kv : per) v.push_back({ -kv.second, kv.first });
  std::sort(v.begin(), v.end());

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("CLIENTS", 4, 1, 2);
  drawScanDot(66, 9);
  sprintf(buf, "%d devices in %d nets", (int)g_stas.size(), (int)per.size());
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W - 4, 1, 2);

  int rows = 6, rh = 19, y0 = 22;
  int maxc = v.empty() ? 1 : -v[0].first;
  if (v.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString("listening...", W / 2, 70, 4);
    spr.drawString("devices appear as they send data", W / 2, 96, 2);
  }
  for (int i = 0; i < rows && i < (int)v.size(); i++) {
    uint64_t b = v[i].second;
    int cnt = -v[i].first;
    int y = y0 + i * rh;
    auto it = g_aps.find(b);
    const char* nm;
    char vb[24];
    if (it != g_aps.end()) nm = ssidOr(it->second);
    else {
      uint8_t m[6]; for (int k = 0; k < 6; k++) m[k] = (b >> (8 * (5 - k))) & 0xFF;
      const char* ven = ouiVendor(m);
      snprintf(vb, sizeof(vb), "? %s", ven ? ven : "unknown net");
      nm = vb;
    }
    uint16_t c = apColor(b);
    fitText(nm, buf, sizeof(buf), 120, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(it != g_aps.end() ? TFT_WHITE : C_DIM(), TFT_BLACK);
    spr.drawString(buf, 4, y, 2);
    int bx = 130, bwid = W - bx - 40;
    int len = bwid * cnt / maxc; if (len < 2) len = 2;
    spr.fillRoundRect(bx, y + 4, len, 9, 3, c);
    sprintf(buf, "%d", cnt);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString(buf, W - 4, y, 2);
  }

  spr.drawFastHLine(4, H - 34, W - 8, tft.color565(35, 35, 45));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tft.color565(240, 200, 70), TFT_BLACK);
  sprintf(buf, "searching for Wi-Fi: %d", (int)g_probes.size());
  spr.drawString(buf, 4, H - 30, 2);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("last 2 min; phones change addresses, so it's approximate", 4, H - 11, 1);
  spr.pushSprite(0, 0);
}

void drawFind(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];
  uint64_t t = findTarget();
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("FIND", 4, 1, 2);
  drawScanDot(40, 9);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("click: next network", W - 4, 5, 1);
  if (!t) { drawWaiting(W, H); spr.pushSprite(0, 0); ledsOff(); return; }
  const Ap& a = g_aps[t];

  fitText(ssidOr(a), buf, sizeof(buf), W - 16, 4);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W / 2, 18, 4);

  // средний сигнал за последние ~5 замеров (так меньше прыгает)
  int sum = 0, cnt = 0;
  for (int i = g_fSparkN - 1; i >= 0 && i >= g_fSparkN - 5; i--)
    if (g_fSpark[i] > -127) { sum += g_fSpark[i]; cnt++; }
  bool lost = (cnt == 0) || (now - g_fLastHit > 4000);
  int r = cnt ? sum / cnt : a.rssi;
  const char* word; uint16_t col;
  if (lost)          { word = "no signal"; col = tft.color565(120, 120, 130); }
  else if (r > -55)  { word = "excellent"; col = tft.color565(70, 210, 130); }
  else if (r > -67)  { word = "good";      col = tft.color565(170, 220, 80); }
  else if (r > -75)  { word = "fair";      col = tft.color565(240, 200, 70); }
  else if (r > -85)  { word = "weak";      col = tft.color565(240, 150, 50); }
  else               { word = "bad";       col = tft.color565(235, 80, 80); }

  // крупное число + оценка
  sprintf(buf, "%d", lost ? 0 : -r);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(col, TFT_BLACK);
  if (!lost) {
    spr.fillRect(14, 68, 14, 5, col);              // знак «минус»
    spr.drawString(buf, 32, 70, 7);
  }
  spr.setTextDatum(ML_DATUM);
  spr.drawString(word, 150, 60, 4);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  sprintf(buf, "dBm   ch %d", a.ch);
  spr.drawString(buf, 150, 84, 2);

  // шкала
  int pct = lost ? 0 : constrain((r + 95) * 100 / 60, 0, 100);
  int bx = 8, by = 102, bw = W - 16, bh = 12;
  spr.drawRoundRect(bx, by, bw, bh, 4, tft.color565(60, 60, 70));
  if (pct > 0) spr.fillRoundRect(bx + 2, by + 2, (bw - 4) * pct / 100, bh - 4, 3, col);

  // график
  int gx = 8, gy = 120, gw = W - 16, gh = 46;
  spr.drawRect(gx, gy, gw, gh, tft.color565(40, 40, 50));
  int px = -1, py = -1;
  for (int i = 0; i < g_fSparkN; i++) {
    int v = g_fSpark[i];
    int xx = gx + 1 + (gw - 3) * (i + (F_SPARK_N - g_fSparkN)) / (F_SPARK_N - 1);
    if (v <= -127) { px = -1; continue; }
    int c = constrain(v, -100, -30);
    int yy = gy + gh - 2 - (c + 100) * (gh - 4) / 70;
    if (px >= 0) spr.drawLine(px, py, xx, yy, col);
    else         spr.drawPixel(xx, yy, col);
    px = xx; py = yy;
  }
  spr.pushSprite(0, 0);

  if (g_ledEvent == 0) {                       // лента — уровень сигнала
    int lv = pct * WS2812_N / 100;
    strip.clear();
    uint8_t rr = ((col >> 11) & 0x1F) << 3, gg = ((col >> 5) & 0x3F) << 2, bb = (col & 0x1F) << 3;
    for (int i = 0; i < lv; i++) strip.setPixelColor(i, strip.Color(rr / 3, gg / 3, bb / 3));
    strip.show();
  }
}

// вызывается каждый кадр, пока открыт раздел Wi-Fi
void wifiFrame(uint32_t now) {
  wifiScanTick(now);
  wifiHopTick(now);
  if (g_wMode == W_NETS) {
    if (g_wOrder.empty() && !g_aps.empty()) wifiOrderUpdate(true);
    wifiApplyMove();
    if (g_wMode != W_NETS) return;            // ушли за край списка
    if (g_wDetail) drawNetDetail(now); else drawNets(now);
  }
  else if (g_wMode == W_CHANNELS) drawChannels(now);
  else if (g_wMode == W_ACTIVITY) drawActivity(now);
  else if (g_wMode == W_CLIENTS)  drawClients(now);
  else                            drawFind(now);
}
