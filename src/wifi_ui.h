// ============================================================
//  wifi_ui.h — раздел Wi-Fi (подключается внутрь main.cpp)
//
//  Только приём: пассивный скан (слушаем маячки точек доступа, сами
//  ничего не отправляем) и подсчёт пакетов в эфире по каналам.
//  ESP32-S3 работает только в диапазоне 2.4 ГГц — сети 5 ГГц не видны.
//
//  Экраны: NETWORKS (список + подробности), CHANNELS (загрузка каналов),
//          ACTIVITY (пакеты в секунду по каналам).
// ============================================================
#pragma once
#include <WiFi.h>
#include <esp_wifi.h>

enum { W_NETS = 0, W_CHANNELS, W_ACTIVITY, W_NUM };
int  g_wMode = 0;
bool g_wOn = false;
bool g_wEverScanned = false;

// ---------- точки доступа ----------
struct Ap {
  uint8_t  bssid[6];
  char     ssid[33];
  int8_t   rssi, rssiS, rmin, rmax;
  uint8_t  ch, enc;
  uint32_t t, first;
};
std::map<uint64_t, Ap> g_aps;
static const uint32_t AP_REMEMBER_MS = 60000;
static const int CH_MAX = 13;

bool     g_scanning = false;
uint32_t g_scanRetryMs = 0;

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

// ---------- активность (promiscuous) ----------
volatile uint32_t g_pMgmt = 0, g_pData = 0;
bool     g_prom = false;
int      g_hopCh = 1;
uint32_t g_hopMs = 0, g_lastMgmt = 0, g_lastData = 0;
float    g_ppsMgmt[CH_MAX + 1] = {0}, g_ppsData[CH_MAX + 1] = {0};
#define ACT_HIST_N 120
float    g_actHist[ACT_HIST_N];
int      g_actHistN = 0;
uint32_t g_actHistMs = 0;
static const uint32_t HOP_MS = 200;

void IRAM_ATTR promCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MGMT) g_pMgmt++;
  else if (type == WIFI_PKT_DATA) g_pData++;
}

uint64_t macKey(const uint8_t* m) {
  uint64_t k = 0;
  for (int i = 0; i < 6; i++) k = (k << 8) | m[i];
  return k;
}

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
// стабильный цвет сети для «холмиков»
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

// ---------- управление радио ----------
void promOn() {
  if (g_prom) return;
  esp_wifi_scan_stop();
  g_scanning = false;
  wifi_promiscuous_filter_t f = { WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA };
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous_rx_cb(promCb);
  esp_wifi_set_promiscuous(true);
  g_hopCh = 1;
  esp_wifi_set_channel(g_hopCh, WIFI_SECOND_CHAN_NONE);
  g_hopMs = millis();
  g_lastMgmt = g_pMgmt; g_lastData = g_pData;
  g_prom = true;
}
void promOff() {
  if (!g_prom) return;
  esp_wifi_set_promiscuous(false);
  g_prom = false;
  strip.clear(); strip.show();
}

void wifiStart() {
  if (g_wOn) return;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  g_wOn = true;
  g_scanning = false;
  g_scanRetryMs = 0;
  if (g_wMode == W_ACTIVITY) promOn();
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
int wifiNetCount() { return g_wEverScanned ? (int)g_aps.size() : -1; }

void wifiSetMode(int m) {
  g_wMode = (m + W_NUM) % W_NUM;
  g_wDetail = false;
  if (g_wMode == W_ACTIVITY) promOn(); else promOff();
}

// ---------- скан: забираем результаты и запускаем следующий ----------
void wifiOrderUpdate(bool force);

void wifiScanTick(uint32_t now) {
  if (!g_wOn || g_prom) return;
  if (!g_scanning) {
    if (now < g_scanRetryMs) return;
    // async, show_hidden, passive, 150 мс на канал (~2 с на весь диапазон)
    if (WiFi.scanNetworks(true, true, true, 150) == WIFI_SCAN_FAILED) { g_scanRetryMs = now + 1000; return; }
    g_scanning = true;
    return;
  }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  g_scanning = false;
  if (n < 0) { g_scanRetryMs = now + 1000; return; }
  for (int i = 0; i < n; i++) {
    const uint8_t* b = WiFi.BSSID(i);
    if (!b) continue;
    uint64_t k = macKey(b);
    int rssi = WiFi.RSSI(i);
    auto it = g_aps.find(k);
    if (it == g_aps.end()) {
      Ap a; memset(&a, 0, sizeof(a));
      memcpy(a.bssid, b, 6);
      a.first = now; a.rssiS = a.rmin = a.rmax = (int8_t)rssi;
      it = g_aps.insert({ k, a }).first;
    }
    Ap& a = it->second;
    String ss = WiFi.SSID(i);
    strncpy(a.ssid, ss.c_str(), 32); a.ssid[32] = 0;
    a.ch = (uint8_t)WiFi.channel(i);
    a.enc = (uint8_t)WiFi.encryptionType(i);
    a.rssi = (int8_t)rssi;
    a.rssiS = (int8_t)((a.rssiS + rssi) / 2);
    if (rssi < a.rmin) a.rmin = rssi;
    if (rssi > a.rmax) a.rmax = rssi;
    a.t = now;
  }
  WiFi.scanDelete();
  g_wEverScanned = true;
  for (auto it = g_aps.begin(); it != g_aps.end(); )
    if (now - it->second.t > AP_REMEMBER_MS) it = g_aps.erase(it); else ++it;

  // история сигнала выбранной сети (точка на каждый проход скана)
  if (g_wDetail && g_wSel) {
    if (g_wSparkFor != g_wSel) { g_wSparkFor = g_wSel; g_wSparkN = 0; }
    auto it = g_aps.find(g_wSel);
    int8_t v = (it != g_aps.end() && it->second.t == now) ? it->second.rssi : (int8_t)-127;
    if (g_wSparkN < W_SPARK_N) g_wSpark[g_wSparkN++] = v;
    else { memmove(g_wSpark, g_wSpark + 1, W_SPARK_N - 1); g_wSpark[W_SPARK_N - 1] = v; }
  }
  wifiOrderUpdate(false);
}

// ---------- активность: прыгаем по каналам и считаем пакеты ----------
void wifiHopTick(uint32_t now) {
  if (!g_prom) return;
  uint32_t dt = now - g_hopMs;
  if (dt < HOP_MS) return;
  uint32_t m = g_pMgmt, d = g_pData;
  float sm = (m - g_lastMgmt) * 1000.0f / dt, sd = (d - g_lastData) * 1000.0f / dt;
  g_lastMgmt = m; g_lastData = d;
  // сглаживание: канал посещается раз в ~2.6 с
  g_ppsMgmt[g_hopCh] = g_ppsMgmt[g_hopCh] * 0.4f + sm * 0.6f;
  g_ppsData[g_hopCh] = g_ppsData[g_hopCh] * 0.4f + sd * 0.6f;
  g_hopCh = g_hopCh % CH_MAX + 1;
  esp_wifi_set_channel(g_hopCh, WIFI_SECOND_CHAN_NONE);
  g_hopMs = now;

  if (now - g_actHistMs >= 1000) {          // общий трафик раз в секунду — в историю
    g_actHistMs = now;
    float tot = 0;
    for (int c = 1; c <= CH_MAX; c++) tot += g_ppsMgmt[c] + g_ppsData[c];
    if (g_actHistN < ACT_HIST_N) g_actHist[g_actHistN++] = tot;
    else { memmove(g_actHist, g_actHist + 1, sizeof(float) * (ACT_HIST_N - 1)); g_actHist[ACT_HIST_N - 1] = tot; }
  }
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
  // прокрутка за край списка — соседний экран
  if (!g_wDetail && mv < 0 && idx == 0)     { wifiSetMode(g_wMode - 1); return; }
  if (!g_wDetail && mv > 0 && idx == n - 1) { wifiSetMode(g_wMode + 1); return; }
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
  if (g_wMode == W_NETS && !g_wOrder.empty()) { g_wDetail = !g_wDetail; g_wTouch = millis(); }
  else wifiSetMode(g_wMode + 1);
}

// ---------- экраны ----------
void drawScanDot(int x, int y) {      // мигающая точка «идёт скан»
  if (g_scanning && (millis() / 300) % 2) spr.fillCircle(x, y, 2, tft.color565(70, 210, 130));
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

  if (g_wOrder.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString(g_wEverScanned ? "no networks" : "scanning...", W / 2, H / 2, 4);
    spr.pushSprite(0, 0);
    return;
  }
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
    fitText(a.ssid[0] ? a.ssid : "(hidden)", buf, sizeof(buf), 150, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(a.ssid[0] ? TFT_WHITE : C_DIM(), bg);
    spr.drawString(buf, 12, y + 1, 2);
    sprintf(buf, "ch%d", a.ch);
    spr.setTextColor(apColor(k), bg);
    spr.drawString(buf, 170, y + 1, 2);
    spr.setTextColor(encColor(a.enc), bg);
    spr.drawString(a.enc == WIFI_AUTH_OPEN ? "open" : "", 208, y + 1, 2);
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

void drawNetDetail(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[64], tmp[48];
  auto it = g_aps.find(g_wSel);
  if (it == g_aps.end()) { g_wDetail = false; drawNets(now); return; }
  const Ap& a = it->second;

  fitText(a.ssid[0] ? a.ssid : "(hidden)", tmp, sizeof(tmp), W - 70, 4);
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

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(encColor(a.enc), TFT_BLACK);
  spr.drawString(encName(a.enc), 4, 30, 2);
  int sameCh = 0, near = 0;
  for (auto& kv : g_aps) {
    if (kv.first == g_wSel) continue;
    int d = abs((int)kv.second.ch - (int)a.ch);
    if (d == 0) sameCh++; else if (d < 5) near++;
  }
  sprintf(buf, "channel %d   2.4 GHz", a.ch);
  spr.setTextColor(apColor(g_wSel), TFT_BLACK);
  spr.drawString(buf, 4 + spr.textWidth(encName(a.enc), 2) + 12, 30, 2);

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
  sprintf(buf, "same ch  %d nets", sameCh);
  spr.drawString(buf, 4, y, 2); y += LH;
  sprintf(buf, "overlap  %d nets", near);
  spr.drawString(buf, 4, y, 2);

  // график сигнала: точка на каждый проход скана (~2 с) => ~2 мин
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

  const uint8_t* b = a.bssid;
  sprintf(buf, "BSSID %02X:%02X:%02X:%02X:%02X:%02X%s", b[0], b[1], b[2], b[3], b[4], b[5],
          (b[0] & 0x02) ? "  (local)" : "");
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString(buf, 4, 132, 1);
  if (a.enc == WIFI_AUTH_OPEN) {
    spr.setTextColor(tft.color565(235, 80, 80), TFT_BLACK);
    spr.drawString("open network: traffic is not encrypted", 4, 144, 1);
  }
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("click: back", W - 4, H - 1, 1);
  spr.pushSprite(0, 0);
}

// «помеха» от сети на канале c: 20 МГц сеть задевает соседние ±4 канала
float overlapW(int d) { d = abs(d); return d >= 5 ? 0.0f : 1.0f - d / 5.0f; }

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

  // сетка
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

  // холмики: слабые снизу, сильные поверх
  std::vector<std::pair<int, uint64_t>> v;
  for (auto& kv : g_aps) if (kv.second.ch >= 1 && kv.second.ch <= CH_MAX) v.push_back({ kv.second.rssiS, kv.first });
  std::sort(v.begin(), v.end());
  for (auto& p : v) {
    const Ap& a = g_aps[p.second];
    uint16_t c = apColor(p.second);
    uint16_t fill = dimColor(c, 22);
    int top = dbY(a.rssiS);
    int prevx = -1, prevy = -1;
    for (int s = 0; s <= 24; s++) {
      float f = -1.0f + s / 12.0f;                 // -1..1
      float chf = a.ch + f * 2.0f;                 // ±2 канала (20 МГц)
      int x = chX(chf);
      int y = pb - (int)((pb - top) * (1.0f - f * f));
      if (x < pl || x > pr) { prevx = -1; continue; }
      if (y < pb) spr.drawFastVLine(x, y, pb - y, fill);
      if (prevx >= 0) spr.drawLine(prevx, prevy, x, y, c);
      prevx = x; prevy = y;
    }
  }
  // подписи 4 самых сильных
  int labeled = 0;
  for (int i = (int)v.size() - 1; i >= 0 && labeled < 4; i--, labeled++) {
    const Ap& a = g_aps[v[i].second];
    char nm[20];
    fitText(a.ssid[0] ? a.ssid : "(hidden)", nm, sizeof(nm), 70, 1);
    spr.setTextDatum(BC_DATUM);
    spr.setTextColor(apColor(v[i].second), TFT_BLACK);
    spr.drawString(nm, chX(a.ch), dbY(a.rssiS) - 2, 1);
  }

  // совет: лучший из 1 / 6 / 11
  if (!g_aps.empty()) {
    const int cand[3] = { 1, 6, 11 };
    float best = 1e9; int bestCh = 1;
    for (int k = 0; k < 3; k++) {
      float score = 0;
      for (auto& kv : g_aps) score += overlapW(kv.second.ch - cand[k]) * (kv.second.rssiS + 100);
      if (score < best) { best = score; bestCh = cand[k]; }
    }
    sprintf(buf, "best channel: %d", bestCh);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(tft.color565(70, 210, 130), TFT_BLACK);
    spr.drawString(buf, W - 4, 1, 2);
  } else {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString(g_wEverScanned ? "no networks" : "scanning...", W / 2, H / 2, 4);
  }
  spr.pushSprite(0, 0);
}

void drawActivity(uint32_t now) {
  int W = spr.width(), H = spr.height();
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
  sprintf(buf, "%d pkt/s", (int)(tot + 0.5f));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W - 4, 1, 2);

  // столбики по каналам: данные (зелёные) + служебные/маячки (синие), шкала — корень
  int pl = 8, pr = W - 8, pt = 22, pb = 104;
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
    spr.drawString(buf, x + bw / 2, pb + 5, 1);
  }
  // легенда
  spr.setTextDatum(TL_DATUM);
  spr.fillRect(8, 120, 6, 6, tft.color565(70, 210, 130));
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("data", 17, 119, 1);
  spr.fillRect(50, 120, 6, 6, tft.color565(80, 150, 240));
  spr.drawString("beacons/service", 59, 119, 1);

  // история общего трафика ~2 мин
  int gx = 8, gy = 130, gw = W - 16, gh = 36;
  spr.drawRect(gx, gy, gw, gh, tft.color565(40, 40, 50));
  float hm = 1;
  for (int i = 0; i < g_actHistN; i++) if (g_actHist[i] > hm) hm = g_actHist[i];
  for (int i = 0; i < g_actHistN; i++) {
    int x = gx + 1 + (gw - 3) * (i + (ACT_HIST_N - g_actHistN)) / (ACT_HIST_N - 1);
    int h = (int)((gh - 3) * g_actHist[i] / hm);
    spr.drawFastVLine(x, gy + gh - 1 - h, h, tft.color565(70, 170, 120));
  }
  sprintf(buf, "2 min, peak %d", (int)hm);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString(buf, gx + gw - 3, gy + 3, 1);
  spr.pushSprite(0, 0);

  // светодиоды: уровень общего трафика (если нет события)
  if (g_ledEvent == 0) {
    int lv = tot < 1 ? 0 : (int)(log10f(tot) * 2.7f);   // 10 пак/с ≈ 3, 1000 ≈ 8
    lv = constrain(lv, 0, WS2812_N);
    strip.clear();
    for (int i = 0; i < lv; i++) {
      int r = i * 255 / (WS2812_N - 1), g = 255 - r;
      strip.setPixelColor(i, strip.Color(r / 3, g / 3, 0));
    }
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
  else                            drawActivity(now);
}
