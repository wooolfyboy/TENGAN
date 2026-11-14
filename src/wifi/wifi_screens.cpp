// ============================================================
//  wifi_screens.cpp — экраны раздела Wi-Fi
//
//  NETWORKS (+подробности) · CHANNELS · ACTIVITY · CLIENTS · FIND
//  Все экраны собраны из компонентов core/ui.h по общей сетке.
// ============================================================
#include <esp_wifi.h>
#include "core/timeutil.h"
#include "core/settings.h"
#include "wifi/wifi_model.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/leds.h"
#include "audio/audio.h"

// значок защиты сети; открытые и WEP — инверсией, как тревога
static char secGlyph(uint8_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN:            return 'O';
    case WIFI_AUTH_WEP:             return '!';
    case WIFI_AUTH_WPA_PSK:         return '1';
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:    return '2';
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK:
    case WIFI_AUTH_OWE:             return '3';
    case WIFI_AUTH_WPA2_ENTERPRISE: return 'E';
    default:                        return '?';
  }
}
static bool secAlert(uint8_t e) { return e == WIFI_AUTH_OPEN || e == WIFI_AUTH_WEP; }

static void upperCopy(const char* s, char* out, size_t n) {
  size_t i = 0;
  for (; s[i] && i < n - 1; i++) out[i] = (char)toupper((unsigned char)s[i]);
  out[i] = 0;
}

// «идёт приём» — мигающая точка в конце линии шапки
static void scanDot() {
  if ((g_scanning || g_prom) && (millis() / 300) % 2) spr.fillRect(spr.width() - UI_M - 3, 18, 3, 3, C_ACC());
}

static bool waiting() {
  if (!g_aps.empty()) return false;
  uiEmpty(g_wEverScanned ? "no networks" : "scanning", "listening for access points, 2.4 ghz only");
  return true;
}

// ============================================================
//  NETWORKS — список и подробности
// ============================================================
void drawNets(uint32_t now) {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d  strongest first", (int)g_wOrder.size());
  uiHeader("Networks", buf);
  scanDot();
  if (g_wOrder.empty()) { waiting(); uiFooter("", "side: back"); present(); return; }

  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = g_wTop + i;
    if (idx >= (int)g_wOrder.size()) break;
    uint64_t k = g_wOrder[idx];
    auto it = g_aps.find(k);
    if (it == g_aps.end()) continue;
    const Ap& a = it->second;
    int y = LIST_Y0 + i * LIST_ROWH, cy = y + LIST_ROWH / 2 - 1;
    if (k == g_wSel) uiRowHighlight(y, LIST_ROWH - 1);
    bool alert = secAlert(a.enc);
    drawGlyph(UI_M + 8, cy, secGlyph(a.enc), alert ? C_ALERT() : UI_LABEL(), alert);
    fitText(ssidOr(a), buf, sizeof(buf), 136, 2);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(a.ssid[0] ? C_HOT() : UI_DIM());
    txt(buf, UI_M + 18, cy, 2);
    snprintf(buf, sizeof(buf), "CH%d", a.ch);
    spr.setTextColor(UI_DIM());
    txt(buf, 166, cy, 1);
    const char* tag = alert ? "OPEN" : a.hotspot ? "PHONE" : a.gen == 6 ? "WI-FI6" : "";
    if (a.enc == WIFI_AUTH_WEP) tag = "WEP";
    spr.setTextColor(alert ? C_ALERT() : UI_LABEL());
    txt(tag, 196, cy, 1);
    bool stale = msSince(now, a.t) > 6000;
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(stale ? UI_DIM() : heat(a.rssi));
    txtNum(a.rssi, 266, cy, 2);
    drawBars(274, cy - 7, a.rssi, heat(a.rssi));
  }
  int n = (int)g_wOrder.size();
  if (n > LIST_ROWS) {
    int trackH = LIST_ROWS * LIST_ROWH;
    int barH = max(8, trackH * LIST_ROWS / n);
    int barY = LIST_Y0 + (trackH - barH) * g_wTop / (n - LIST_ROWS);
    spr.fillRect(W - UI_M + 2, LIST_Y0, 2, trackH, UI_LINE());
    spr.fillRect(W - UI_M + 2, barY, 2, barH, C_ACC());
  }
  uiFooter("wheel: pick   click: details", "side: back");
  present();
}

void drawNetDetail(uint32_t now) {
  int W = spr.width();
  char buf[96], tmp[48];
  auto it = g_aps.find(g_wSel);
  if (it == g_aps.end()) { g_wDetail = false; drawNets(now); return; }
  const Ap& a = it->second;
  spr.fillSprite(C_BG());
  int pos = -1;
  for (size_t i = 0; i < g_wOrder.size(); i++) if (g_wOrder[i] == g_wSel) pos = (int)i;
  if (pos >= 0) snprintf(buf, sizeof(buf), "%d / %d  click: back", pos + 1, (int)g_wOrder.size());
  else          snprintf(buf, sizeof(buf), "click: back");
  uiHeader("Network", buf);

  bool alert = secAlert(a.enc);
  drawGlyph(UI_M + 8, UI_TOP + 11, secGlyph(a.enc), alert ? C_ALERT() : UI_LABEL(), alert);
  fitText(ssidOr(a), tmp, sizeof(tmp), W - 2 * UI_M - 24, 4);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(a.ssid[0] ? C_HOT() : UI_DIM());
  txt(tmp, UI_M + 20, UI_TOP, 4);
  snprintf(buf, sizeof(buf), "%s  CH %d  %d MHZ  %s", encName(a.enc), a.ch, a.sec ? 40 : 20, genName(a.gen));
  char bu[96]; upperCopy(buf, bu, sizeof(bu));
  spr.setTextColor(alert ? C_ALERT() : UI_LABEL());
  txt(bu, UI_M + 20, UI_TOP + 26, 1);

  // показатели
  int sameCh = 0, near = 0;
  for (auto& kv : g_aps) {
    if (kv.first == g_wSel) continue;
    float d = fabsf(apCenter(kv.second) - apCenter(a));
    if (d < 0.5f) sameCh++; else if (d < apHalf(kv.second) + apHalf(a)) near++;
  }
  uint32_t ago = msSince(now, a.t) / 1000;
  int x0 = UI_M + 8, y0 = UI_TOP + 40, cw = 64;
  snprintf(tmp, sizeof(tmp), "%d", a.rssi);
  uiReadout(x0, y0, ago >= 6 ? "signal (old)" : "signal", tmp, heat(a.rssi));
  snprintf(tmp, sizeof(tmp), "%d/%d", a.rmin, a.rmax);
  uiReadout(x0 + cw, y0, "range", tmp, C_HOT());
  uint32_t secs = msSince(now, a.first) / 1000;
  snprintf(tmp, sizeof(tmp), "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  uiReadout(x0 + 2 * cw, y0, "seen", tmp, C_HOT());
  snprintf(tmp, sizeof(tmp), "%d+%d", sameCh, near);
  uiReadout(x0, y0 + 28, "rivals", tmp, sameCh + near > 3 ? C_ALERT() : C_HOT());
  int cl = clientsOf(g_wSel);
  if (cl > 0) snprintf(tmp, sizeof(tmp), "%d", cl); else snprintf(tmp, sizeof(tmp), "?");
  uiReadout(x0 + cw, y0 + 28, "devices", tmp, C_HOT());
  uiReadout(x0 + 2 * cw, y0 + 28, "band", "2.4G", C_HOT());

  int gx = 206, gy = y0, gw = W - UI_M - 4 - gx, gh = 52;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt("SIGNAL 2 MIN", gx + 5, gy + 3, 1);
  drawSpark(gx, gy, gw, gh, g_wSpark, g_wSparkN, W_SPARK_N, heat(a.rssi));

  int y = UI_TOP + 98;
  const uint8_t* b = a.bssid;
  bool local = b[0] & 0x02;
  if (a.hotspot)     snprintf(buf, sizeof(buf), "MAKER %s  -> PHONE HOTSPOT", a.vendor ? a.vendor : "?");
  else if (a.vendor) snprintf(buf, sizeof(buf), "MAKER %s", a.vendor);
  else if (local)    snprintf(buf, sizeof(buf), "MAKER HIDDEN (RANDOM MAC)");
  else               snprintf(buf, sizeof(buf), "MAKER UNKNOWN");
  upperCopy(buf, bu, sizeof(bu));
  spr.setTextColor(UI_LABEL());
  txt(bu, x0, y, 1); y += 10;
  snprintf(buf, sizeof(buf), "BSSID %02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  spr.setTextColor(UI_DIM());
  txt(buf, x0, y, 1); y += 10;
  if (alert) {
    spr.setTextColor(C_ALERT());
    txt(a.enc == WIFI_AUTH_WEP ? "! WEP IS BROKEN: TRAFFIC CAN BE READ" : "! OPEN NETWORK: TRAFFIC IS NOT ENCRYPTED", x0, y, 1);
  }
  present();
}

// ============================================================
//  CHANNELS — кто какой канал занимает
// ============================================================
void drawChannels(uint32_t now) {
  int W = spr.width();
  char buf[40];
  spr.fillSprite(C_BG());
  int best = bestChannel();
  if (best) snprintf(buf, sizeof(buf), "best channel %d", best); else buf[0] = 0;
  uiHeader("Channels", buf);
  scanDot();
  if (waiting()) { present(); return; }

  int pl = UI_M + 24, pr = W - UI_M - 8, pt = UI_TOP + 4, pb = UI_BOTTOM - 4;
  auto chX = [&](float ch) { return pl + (int)((ch - 1) * (pr - pl) / (CH_MAX - 1)); };
  auto dbY = [&](int db) { int c = constrain(db, -100, -30); return pb - (c + 100) * (pb - pt) / 70; };

  for (int db = -90; db <= -40; db += 10) {
    int y = dbY(db);
    for (int x = pl; x < pr; x += 4) spr.drawPixel(x, y, UI_LINE());
    snprintf(buf, sizeof(buf), "%d", db);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(UI_DIM());
    txt(buf, pl - 4, y, 1);
  }
  spr.drawFastHLine(pl, pb, pr - pl, UI_LABEL());
  for (int c = 1; c <= CH_MAX; c++) {
    bool main3 = (c == 1 || c == 6 || c == 11);
    spr.drawFastVLine(chX(c), pb, 3, main3 ? UI_LABEL() : UI_LINE());
    snprintf(buf, sizeof(buf), "%d", c);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(c == best ? C_ACC() : main3 ? UI_LABEL() : UI_DIM());
    txt(buf, chX(c), pb + 5, 1);
  }
  if (best) spr.fillTriangle(chX(best), pb + 14, chX(best) - 4, pb + 19, chX(best) + 4, pb + 19, C_ACC());

  // «холмики» реальной ширины (20 / 40 МГц), слабые снизу, «жар» — сила
  std::vector<std::pair<int, uint64_t>> v;
  for (auto& kv : g_aps) if (kv.second.ch >= 1 && kv.second.ch <= CH_MAX) v.push_back({ kv.second.rssiS, kv.first });
  std::sort(v.begin(), v.end());
  for (auto& p : v) {
    const Ap& a = g_aps[p.second];
    uint16_t c = heat(a.rssiS), fill = scale565(c, 18);
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
  for (int i = (int)v.size() - 1, k = 0; i >= 0 && k < 3; i--, k++) {     // подписи трёх сильных
    const Ap& a = g_aps[v[i].second];
    char nm[20], nu[20];
    fitText(ssidOr(a), nm, sizeof(nm), 72, 1);
    upperCopy(nm, nu, sizeof(nu));
    spr.setTextDatum(BC_DATUM);
    spr.setTextColor(C_HOT());
    txt(nu, constrain(chX(apCenter(a)), pl + 30, pr - 30), dbY(a.rssiS) - 2, 1);
  }
  present();
}

// ============================================================
//  ACTIVITY — пакеты в секунду и шум по каналам
// ============================================================
void drawActivity(uint32_t now) {
  int W = spr.width();
  char buf[40];
  float tot = 0, mx = 1;
  for (int c = 1; c <= CH_MAX; c++) { float t = g_ppsMgmt[c] + g_ppsData[c]; tot += t; mx = max(mx, t); }
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d pkt/s", (int)(tot + 0.5f));
  uiHeader("Air activity", buf);
  scanDot();

  int pl = UI_M + 40, pr = W - UI_M - 6, pt = UI_TOP + 2, pb = UI_TOP + 64;
  int bw = (pr - pl) / CH_MAX;
  float smx = sqrtf(mx);
  for (int c = 1; c <= CH_MAX; c++) {
    int x = pl + (c - 1) * bw;
    float t = g_ppsMgmt[c] + g_ppsData[c];
    int h = (int)((pb - pt) * sqrtf(t) / smx);
    int hd = t > 0 ? (int)(h * g_ppsData[c] / t) : 0;
    spr.fillRect(x + 2, pt, bw - 4, pb - pt, tft.color565(10, 18, 26));
    if (h > 0) {                                  // данные — ярко, служебные — тусклее
      uint16_t hc = heatV(0.3f + 0.7f * sqrtf(t) / smx);
      spr.fillRect(x + 2, pb - h, bw - 4, h - hd, scale565(hc, 40));
      spr.fillRect(x + 2, pb - hd, bw - 4, hd, hc);
    }
    if (c == g_hopCh) spr.fillRect(x + 2, pb + 1, bw - 4, 2, C_ACC());
    snprintf(buf, sizeof(buf), "%d", c);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor((c == 1 || c == 6 || c == 11) ? UI_LABEL() : UI_DIM());
    txt(buf, x + bw / 2, pb + 5, 1);
    if (g_noise[c] != 0) {                         // шум: обычно -90..-95; выше -85 — тревога
      int nf = (int)lroundf(g_noise[c]);
      snprintf(buf, sizeof(buf), "%d", -nf);
      spr.setTextColor(nf > -86 ? C_ALERT() : UI_DIM());
      txt(buf, x + bw / 2, pb + 16, 1);
    }
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt("CH", UI_M + 8, pb + 5, 1);
  txt("NOISE", UI_M + 2, pb + 16, 1);
  txt("DATA", UI_M + 10, pt + 2, 1);
  spr.fillRect(UI_M + 2, pt + 2, 5, 7, heatV(0.9f));
  txt("MGMT", UI_M + 10, pt + 14, 1);
  spr.fillRect(UI_M + 2, pt + 14, 5, 7, scale565(heatV(0.9f), 40));

  // общий трафик за 2 минуты
  int gx = UI_M + 4, gy = UI_TOP + 92, gw = W - 2 * UI_M - 8, gh = UI_BOTTOM - gy + 2;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  float hm = 1;
  for (int i = 0; i < g_actHistN; i++) hm = max(hm, g_actHist[i]);
  for (int i = 0; i < g_actHistN; i++) {
    int xx = gx + 2 + (gw - 5) * (i + (ACT_HIST_N - g_actHistN)) / (ACT_HIST_N - 1);
    int h = (int)((gh - 5) * g_actHist[i] / hm);
    spr.drawFastVLine(xx, gy + gh - 2 - h, h, heatV(0.25f + 0.6f * g_actHist[i] / hm));
  }
  snprintf(buf, sizeof(buf), "2 MIN  PEAK %d", (int)hm);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(buf, gx + 5, gy + 3, 1);
  present();

  if (g_ledEvent == 0) {                           // лента — уровень трафика
    int lv = tot < 1 ? 0 : constrain((int)(log10f(tot) * 2.7f), 0, WS2812_N);
    strip.clear();
    for (int i = 0; i < lv; i++) strip.setPixelColor(i, strip.Color(10 + i * 8, 40 + i * 20, 60 + i * 20));
    strip.show();
  }
}

// ============================================================
//  CLIENTS — сколько устройств работает в каждой сети
// ============================================================
void drawClients(uint32_t now) {
  int W = spr.width();
  char buf[48];
  std::map<uint64_t, int> per;
  for (auto& kv : g_stas) per[kv.second.bssid]++;
  std::vector<std::pair<int, uint64_t>> v;
  for (auto& kv : per) v.push_back({ -kv.second, kv.first });
  std::sort(v.begin(), v.end());

  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d devices  %d nets", (int)g_stas.size(), (int)per.size());
  uiHeader("Clients", buf);
  scanDot();

  if (v.empty()) uiEmpty("listening", "devices appear as they send data");
  int rows = 5, rh = 17, y0 = UI_TOP + 2;
  int maxc = v.empty() ? 1 : -v[0].first;
  for (int i = 0; i < rows && i < (int)v.size(); i++) {
    uint64_t b = v[i].second;
    int cnt = -v[i].first, cy = y0 + i * rh + rh / 2;
    auto it = g_aps.find(b);
    const char* nm;
    char vb[24];
    if (it != g_aps.end()) nm = ssidOr(it->second);
    else {
      uint8_t m[6]; for (int k = 0; k < 6; k++) m[k] = (b >> (8 * (5 - k))) & 0xFF;
      const char* ven = ouiName(m);
      snprintf(vb, sizeof(vb), "? %s", ven ? ven : "unknown net");
      nm = vb;
    }
    fitText(nm, buf, sizeof(buf), 124, 2);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(it != g_aps.end() ? C_HOT() : UI_DIM());
    txt(buf, UI_M + 8, cy, 2);
    int bx = 144, bwid = W - bx - UI_M - 28;
    int len = max(4, bwid * cnt / maxc);
    for (int x = 0; x + 3 <= len; x += 5) spr.fillRect(bx + x, cy - 4, 3, 8, heatV(0.4f + 0.6f * cnt / maxc));
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(C_HOT());
    txtNum(cnt, W - UI_M - 8, cy, 2);
  }
  // «ищут Wi-Fi»
  int y = UI_TOP + 2 + rows * rh + 4;
  spr.drawFastHLine(UI_M + 8, y, W - 2 * UI_M - 16, UI_LINE());
  snprintf(buf, sizeof(buf), "%d", (int)g_probes.size());
  uiReadout(UI_M + 8, y + 4, "looking for wi-fi", buf, C_ACC(), 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(UI_DIM());
  txt("LAST 2 MIN, APPROXIMATE:", W - UI_M - 8, y + 6, 1);
  txt("PHONES CHANGE ADDRESSES", W - UI_M - 8, y + 16, 1);
  present();
}

// ============================================================
//  FIND — «горячо-холодно» по выбранной сети
// ============================================================
void drawFind(uint32_t now) {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  uint64_t t = findTarget();
  if (!t) { uiHeader("Target lock"); waiting(); present(); ledsOff(); return; }
  const Ap& a = g_aps[t];
  char sub[32]; snprintf(sub, sizeof(sub), "ch %d  %s", a.ch, encName(a.enc));
  uiHeader("Target lock", sub);

  // средний сигнал за последние ~5 замеров
  int sum = 0, cnt = 0;
  for (int i = g_fSparkN - 1; i >= 0 && i >= g_fSparkN - 5; i--)
    if (g_fSpark[i] > -127) { sum += g_fSpark[i]; cnt++; }
  bool lost = (cnt == 0) || (now - g_fLastHit > 4000);
  int r = cnt ? sum / cnt : a.rssi;
  const char* word = lost ? "NO SIGNAL" : r > -55 ? "EXCELLENT" : r > -67 ? "GOOD" : r > -75 ? "FAIR" : r > -85 ? "WEAK" : "BAD";
  uint16_t col = lost ? UI_DIM() : heat(r);
  int pct = lost ? 0 : constrain((r + 95) * 100 / 60, 0, 100);

  int cx = W / 2, cy = 120;
  drawArcGauge(cx, cy, 72, 9, pct, col, UI_LINE(), C_BG());
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(lost ? UI_DIM() : C_HOT());
  if (lost) txt("--", cx, cy - 22, 6); else txtNum(r, cx, cy - 22, 6);
  if (!lost) {
    float d = powf(10.0f, (-45.0f - r) / 25.0f);    // грубая оценка до точки доступа
    snprintf(buf, sizeof(buf), "DBM  %s  ~%.0f M", word, d < 1 ? 1.0f : d);
  } else snprintf(buf, sizeof(buf), "%s", word);
  spr.setTextColor(UI_LABEL());
  txt(buf, cx, cy + 4, 1);
  char nm[40]; fitText(ssidOr(a), nm, sizeof(nm), 200, 2);
  spr.setTextColor(C_HOT());
  txt(nm, cx, cy + 22, 2);

  // слева: MAX / MIN / AVG; справа: тренд
  int mx = -127, mn = 0, s2 = 0, n = 0;
  for (int i = 0; i < g_fSparkN; i++) if (g_fSpark[i] > -127) {
    mx = max(mx, (int)g_fSpark[i]); mn = min(mn, (int)g_fSpark[i]); s2 += g_fSpark[i]; n++;
  }
  if (n) {
    snprintf(buf, sizeof(buf), "%d", mx); uiReadout(UI_M + 8, UI_TOP + 4, "max", buf, C_HOT(), 2);
    snprintf(buf, sizeof(buf), "%d", mn); uiReadout(UI_M + 8, UI_TOP + 32, "min", buf, C_HOT(), 2);
    snprintf(buf, sizeof(buf), "%d", s2 / n); uiReadout(UI_M + 8, UI_TOP + 60, "avg", buf, C_HOT(), 2);
  }
  int recent = 0, rn = 0, older = 0, on = 0;
  for (int i = g_fSparkN - 1; i >= 0 && i >= g_fSparkN - 5; i--) if (g_fSpark[i] > -127) { recent += g_fSpark[i]; rn++; }
  for (int i = g_fSparkN - 6; i >= 0 && i >= g_fSparkN - 10; i--) if (g_fSpark[i] > -127) { older += g_fSpark[i]; on++; }
  if (rn && on) {
    int dlt = recent / rn - older / on;
    snprintf(buf, sizeof(buf), "%+d dB", dlt);
    int tx = W - UI_M - 64;
    uiReadout(tx, UI_TOP + 4, dlt >= 3 ? "warmer" : dlt <= -3 ? "colder" : "trend", buf, dlt >= 3 ? C_ACC() : C_HOT(), 2);
  }
  uiFooter("click: next network", "side: back");
  present();
  if (!lost && g_wifiTicks) audioGeiger(pct);                     // щелчки чаще — ближе

  if (g_ledEvent == 0) {                           // лента — уровень сигнала
    int lv = pct * WS2812_N / 100;
    strip.clear();
    uint8_t rr = ((col >> 11) & 0x1F) << 3, gg = ((col >> 5) & 0x3F) << 2, bb = (col & 0x1F) << 3;
    for (int i = 0; i < lv; i++) strip.setPixelColor(i, strip.Color(rr / 3, gg / 3, bb / 3));
    strip.show();
  }
}
