// ============================================================
//  wifi_screens.cpp — экраны раздела Wi-Fi
//
//  NETWORKS (+подробности) · CHANNELS · ACTIVITY · CLIENTS · FIND
// ============================================================
#include <esp_wifi.h>
#include "wifi/wifi_model.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"

// значок защиты сети (вариант C); открытые и WEP — инверсией, как тревога
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

void drawScanDot(int x, int y) {
  if ((g_scanning || g_prom) && (millis() / 300) % 2) spr.fillCircle(x, y, 2, C(70, 210, 130));
}
void drawWaiting(int W, int H) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt(g_wEverScanned ? "no networks" : "scanning...", W / 2, H / 2, 4);
}

void drawNets(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[48];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), C_BG());
  sprintf(buf, "NETWORKS  %d", (int)g_wOrder.size());
  txt(buf, 4, 1, 2);
  drawScanDot(txtW(buf, 2) + 10, 9);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt("wheel: pick  click: info", W - 4, 5, 1);

  if (g_wOrder.empty()) { drawWaiting(W, H); present(); return; }
  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = g_wTop + i;
    if (idx >= (int)g_wOrder.size()) break;
    uint64_t k = g_wOrder[idx];
    auto it = g_aps.find(k);
    if (it == g_aps.end()) continue;
    const Ap& a = it->second;
    int y = LIST_Y0 + i * LIST_ROWH;
    bool sel = (k == g_wSel);
    uint16_t bg = sel ? C(28, 38, 62) : C_BG();
    if (sel) spr.fillRoundRect(0, y, W - 4, LIST_ROWH - 1, 3, bg);
    drawGlyph(8, y + LIST_ROWH / 2 - 1, secGlyph(a.enc), secAlert(a.enc) ? C_HOT() : C(170, 170, 185), secAlert(a.enc));
    fitText(ssidOr(a), buf, sizeof(buf), 146, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(a.ssid[0] ? C_HOT() : C_DIM(), bg);
    txt(buf, 16, y + 1, 2);
    sprintf(buf, "%d", a.ch);
    spr.setTextColor(apColor(k), bg);
    txt(buf, 168, y + 1, 2);
    // метка: открытая > раздача с телефона > поколение
    if (a.enc == WIFI_AUTH_OPEN)  { spr.setTextColor(encColor(a.enc), bg); txt("open", 190, y + 1, 2); }
    else if (a.hotspot)           { spr.setTextColor(C(240, 150, 50), bg); txt("phone", 190, y + 1, 2); }
    else if (a.gen == 6)          { spr.setTextColor(C(60, 205, 205), bg); txt("W6", 190, y + 1, 2); }
    sprintf(buf, "%d", a.rssi);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(now - a.t > 6000 ? C_DIM() : heat(a.rssi), bg);
    txt(buf, 268, y + 1, 2);
    drawBars(274, y + 2, a.rssi, heat(a.rssi));
  }
  int n = (int)g_wOrder.size();
  if (n > LIST_ROWS) {
    int trackH = LIST_ROWS * LIST_ROWH;
    int barH = trackH * LIST_ROWS / n; if (barH < 8) barH = 8;
    int barY = LIST_Y0 + (trackH - barH) * g_wTop / (n - LIST_ROWS);
    spr.fillRect(W - 2, LIST_Y0, 2, trackH, C(30, 30, 38));
    spr.fillRect(W - 2, barY, 2, barH, C_GRAY());
  }
  present();
}

// полуширина сети в «каналах» (канал = 5 МГц): 20 МГц -> 2, 40 МГц -> 4

void drawNetDetail(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[64], tmp[48];
  auto it = g_aps.find(g_wSel);
  if (it == g_aps.end()) { g_wDetail = false; drawNets(now); return; }
  const Ap& a = it->second;

  fitText(ssidOr(a), tmp, sizeof(tmp), W - 70, 4);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(a.ssid[0] ? C_HOT() : C_DIM(), C_BG());
  txt(tmp, 4, 2, 4);
  int pos = -1;
  for (size_t i = 0; i < g_wOrder.size(); i++) if (g_wOrder[i] == g_wSel) pos = (int)i;
  if (pos >= 0) {
    sprintf(buf, "%d/%d", pos + 1, (int)g_wOrder.size());
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), C_BG());
    txt(buf, W - 4, 6, 2);
  }

  // строка 2: защита · канал · ширина · поколение
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(encColor(a.enc), C_BG());
  txt(encName(a.enc), 4, 30, 2);
  int x = 4 + txtW(encName(a.enc), 2) + 10;
  sprintf(buf, "ch %d  %d MHz  %s", a.ch, a.sec ? 40 : 20, genName(a.gen));
  spr.setTextColor(apColor(g_wSel), C_BG());
  txt(buf, x, 30, 2);

  // левая колонка
  int sameCh = 0, near = 0;
  for (auto& kv : g_aps) {
    if (kv.first == g_wSel) continue;
    float d = fabsf(apCenter(kv.second) - apCenter(a));
    if (d < 0.5f) sameCh++; else if (d < apHalf(kv.second) + apHalf(a)) near++;
  }
  int y = 50; const int LH = 15;
  uint32_t ago = (now - a.t) / 1000;
  spr.setTextColor(C_GRAY(), C_BG());
  if (ago >= 6) sprintf(buf, "signal %d (%lus ago)", a.rssi, (unsigned long)ago);
  else          sprintf(buf, "signal %d dBm", a.rssi);
  txt(buf, 4, y, 2); y += LH;
  sprintf(buf, "range  %d .. %d", a.rmin, a.rmax);
  txt(buf, 4, y, 2); y += LH;
  uint32_t secs = (now - a.first) / 1000;
  sprintf(buf, "seen   %lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  txt(buf, 4, y, 2); y += LH;
  sprintf(buf, "rivals %d same, %d near", sameCh, near);
  txt(buf, 4, y, 2); y += LH;
  int cl = clientsOf(g_wSel);
  if (cl > 0) sprintf(buf, "devices %d", cl);
  else        sprintf(buf, "devices ? (see CLIENTS)");
  txt(buf, 4, y, 2);

  // график сигнала (~2 мин)
  int gx = 176, gy = 50, gw = W - gx - 4, gh = 70;
  spr.drawRect(gx, gy, gw, gh, C(40, 40, 50));
  spr.setTextColor(C_DIM(), C_BG());
  txt("signal, 2 min", gx + 3, gy + 3, 1);
  drawSpark(gx, gy, gw, gh, g_wSpark, g_wSparkN, W_SPARK_N, heat(a.rssi));

  // низ: производитель и BSSID
  const uint8_t* b = a.bssid;
  bool local = b[0] & 0x02;
  if (a.hotspot)      sprintf(buf, "MAKER %s  -> phone hotspot", a.vendor ? a.vendor : "?");
  else if (a.vendor)  sprintf(buf, "MAKER %s", a.vendor);
  else if (local)     sprintf(buf, "MAKER hidden (random MAC: hotspot or extra network)");
  else                sprintf(buf, "MAKER unknown");
  spr.setTextColor(a.hotspot ? C(240, 150, 50) : C_GRAY(), C_BG());
  txt(buf, 4, 130, 1);
  sprintf(buf, "BSSID %02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  spr.setTextColor(C_GRAY(), C_BG());
  txt(buf, 4, 141, 1);
  if (a.enc == WIFI_AUTH_OPEN) {
    spr.setTextColor(C(235, 80, 80), C_BG());
    txt("open network: traffic is not encrypted", 4, 152, 1);
  }
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt("click: back", W - 4, H - 1, 1);
  present();
}


void drawChannels(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[40];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), C_BG());
  txt("CHANNELS", 4, 1, 2);
  drawScanDot(78, 9);

  int pl = 24, pr = W - 8, pt = 22, pb = H - 22;
  auto chX = [&](float ch) { return pl + (int)((ch - 1) * (pr - pl) / (CH_MAX - 1)); };
  auto dbY = [&](int db) { int c = constrain(db, -100, -30); return pb - (c + 100) * (pb - pt) / 70; };

  for (int db = -90; db <= -40; db += 10) {
    int y = dbY(db);
    spr.drawFastHLine(pl, y, pr - pl, C(28, 28, 34));
    sprintf(buf, "%d", db);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(C(70, 70, 80), C_BG());
    txt(buf, pl - 2, y, 1);
  }
  spr.drawFastHLine(pl, pb, pr - pl, C(60, 60, 70));
  for (int c = 1; c <= CH_MAX; c++) {
    bool main3 = (c == 1 || c == 6 || c == 11);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(main3 ? C_GRAY() : C(70, 70, 80), C_BG());
    sprintf(buf, "%d", c);
    txt(buf, chX(c), pb + 3, main3 ? 2 : 1);
  }

  // «холмики»: ширина по реальной полосе сети (20 или 40 МГц), слабые снизу
  std::vector<std::pair<int, uint64_t>> v;
  for (auto& kv : g_aps) if (kv.second.ch >= 1 && kv.second.ch <= CH_MAX) v.push_back({ kv.second.rssiS, kv.first });
  std::sort(v.begin(), v.end());
  for (auto& p : v) {
    const Ap& a = g_aps[p.second];
    uint16_t c = heat(a.rssiS);                 // «жар»: сильная сеть — раскалённый холмик
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
    spr.setTextColor(apColor(v[i].second), C_BG());
    txt(nm, constrain(chX(apCenter(a)), pl + 20, pr - 20), dbY(a.rssiS) - 2, 1);
  }

  if (!g_aps.empty()) {
    sprintf(buf, "best channel: %d", bestChannel());
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C(70, 210, 130), C_BG());
    txt(buf, W - 4, 1, 2);
  } else {
    drawWaiting(W, H);
  }
  present();
}

void drawActivity(uint32_t now) {
  int W = spr.width();
  spr.fillSprite(C_BG());
  char buf[40];
  float tot = 0, mx = 1;
  for (int c = 1; c <= CH_MAX; c++) {
    float t = g_ppsMgmt[c] + g_ppsData[c];
    tot += t; if (t > mx) mx = t;
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), C_BG());
  txt("ACTIVITY", 4, 1, 2);
  drawScanDot(76, 9);
  sprintf(buf, "%d pkt/s", (int)(tot + 0.5f));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_HOT(), C_BG());
  txt(buf, W - 4, 1, 2);

  // столбики: данные (зелёные) + маячки/служебные (синие), шкала — корень
  int pl = 28, pr = W - 4, pt = 20, pb = 92;
  int bw = (pr - pl) / CH_MAX;
  float smx = sqrtf(mx);
  for (int c = 1; c <= CH_MAX; c++) {
    int x = pl + (c - 1) * bw;
    float t = g_ppsMgmt[c] + g_ppsData[c];
    int h = (int)((pb - pt) * sqrtf(t) / smx);
    int hd = t > 0 ? (int)(h * g_ppsData[c] / t) : 0;
    spr.fillRect(x + 2, pt, bw - 4, pb - pt, C(22, 22, 28));
    if (h > 0) {                                // «жар» по загрузке канала; служебные — тусклее
      uint16_t hc = heatV(sqrtf(t) / smx);
      spr.fillRect(x + 2, pb - h, bw - 4, h - hd, dimColor(hc, 45));
      spr.fillRect(x + 2, pb - hd, bw - 4, hd, hc);
    }
    if (c == g_hopCh) spr.fillRect(x + 2, pb + 1, bw - 4, 2, C_HOT());
    bool main3 = (c == 1 || c == 6 || c == 11);
    sprintf(buf, "%d", c);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(main3 ? C_GRAY() : C(70, 70, 80), C_BG());
    txt(buf, x + bw / 2, pb + 4, 1);
    // шум канала: обычно ~ -95..-90; выше -85 — что-то сильно шумит
    if (g_noise[c] != 0) {
      int nf = (int)lroundf(g_noise[c]);
      uint16_t col = nf > -80 ? C(235, 80, 80) : nf > -88 ? C(240, 150, 50) : C(90, 90, 104);
      sprintf(buf, "%d", -nf);
      spr.setTextColor(col, C_BG());
      txt(buf, x + bw / 2, pb + 14, 1);
    }
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt("ch", 4, pb + 4, 1);
  txt("-dBm", 0, pb + 14, 1);
  txt("noise", 0, pb + 23, 1);
  spr.fillRect(pl + 40, pb + 26, 6, 6, heatV(0.8f));
  txt("data", pl + 49, pb + 25, 1);
  spr.fillRect(pl + 82, pb + 26, 6, 6, dimColor(heatV(0.8f), 45));
  txt("beacons/service", pl + 91, pb + 25, 1);

  // история общего трафика ~2 мин
  int gx = 4, gy = 128, gw = W - 8, gh = 38;
  spr.drawRect(gx, gy, gw, gh, C(40, 40, 50));
  float hm = 1;
  for (int i = 0; i < g_actHistN; i++) if (g_actHist[i] > hm) hm = g_actHist[i];
  for (int i = 0; i < g_actHistN; i++) {
    int xx = gx + 1 + (gw - 3) * (i + (ACT_HIST_N - g_actHistN)) / (ACT_HIST_N - 1);
    int h = (int)((gh - 3) * g_actHist[i] / hm);
    spr.drawFastVLine(xx, gy + gh - 1 - h, h, C(70, 170, 120));
  }
  sprintf(buf, "2 min, peak %d", (int)hm);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt(buf, gx + gw - 3, gy + 3, 1);
  present();

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
  spr.fillSprite(C_BG());
  char buf[48];
  // считаем устройства по сетям
  std::map<uint64_t, int> per;
  for (auto& kv : g_stas) per[kv.second.bssid]++;
  std::vector<std::pair<int, uint64_t>> v;
  for (auto& kv : per) v.push_back({ -kv.second, kv.first });
  std::sort(v.begin(), v.end());

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), C_BG());
  txt("CLIENTS", 4, 1, 2);
  drawScanDot(66, 9);
  sprintf(buf, "%d devices in %d nets", (int)g_stas.size(), (int)per.size());
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_HOT(), C_BG());
  txt(buf, W - 4, 1, 2);

  int rows = 6, rh = 19, y0 = 22;
  int maxc = v.empty() ? 1 : -v[0].first;
  if (v.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_DIM(), C_BG());
    txt("listening...", W / 2, 70, 4);
    txt("devices appear as they send data", W / 2, 96, 2);
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
      const char* ven = ouiName(m);
      snprintf(vb, sizeof(vb), "? %s", ven ? ven : "unknown net");
      nm = vb;
    }
    uint16_t c = heatV(0.35f + 0.65f * cnt / maxc);   // больше устройств — «горячее»
    fitText(nm, buf, sizeof(buf), 120, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(it != g_aps.end() ? C_HOT() : C_DIM(), C_BG());
    txt(buf, 4, y, 2);
    int bx = 130, bwid = W - bx - 40;
    int len = bwid * cnt / maxc; if (len < 2) len = 2;
    spr.fillRoundRect(bx, y + 4, len, 9, 3, c);
    sprintf(buf, "%d", cnt);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_HOT(), C_BG());
    txt(buf, W - 4, y, 2);
  }

  spr.drawFastHLine(4, H - 34, W - 8, C(35, 35, 45));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C(240, 200, 70), C_BG());
  sprintf(buf, "searching for Wi-Fi: %d", (int)g_probes.size());
  txt(buf, 4, H - 30, 2);
  spr.setTextColor(C_DIM(), C_BG());
  txt("last 2 min; phones change addresses, so it's approximate", 4, H - 11, 1);
  present();
}

void drawFind(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[48];
  uint64_t t = findTarget();
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), C_BG());
  txt("FIND", 4, 1, 2);
  drawScanDot(40, 9);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt("click: next network", W - 4, 5, 1);
  if (!t) { drawWaiting(W, H); present(); ledsOff(); return; }
  const Ap& a = g_aps[t];

  fitText(ssidOr(a), buf, sizeof(buf), W - 16, 4);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C_HOT(), C_BG());
  txt(buf, W / 2, 18, 4);

  // средний сигнал за последние ~5 замеров (так меньше прыгает)
  int sum = 0, cnt = 0;
  for (int i = g_fSparkN - 1; i >= 0 && i >= g_fSparkN - 5; i--)
    if (g_fSpark[i] > -127) { sum += g_fSpark[i]; cnt++; }
  bool lost = (cnt == 0) || (now - g_fLastHit > 4000);
  int r = cnt ? sum / cnt : a.rssi;
  const char* word; uint16_t col;
  if (lost)          { word = "no signal"; col = C(120, 120, 130); }
  else if (r > -55)  { word = "excellent"; col = heat(r); }
  else if (r > -67)  { word = "good";      col = heat(r); }
  else if (r > -75)  { word = "fair";      col = heat(r); }
  else if (r > -85)  { word = "weak";      col = heat(r); }
  else               { word = "bad";       col = heat(r); }

  // крупное число + оценка
  sprintf(buf, "%d", lost ? 0 : -r);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(col, C_BG());
  if (!lost) {
    spr.fillRect(14, 68, 14, 5, col);              // знак «минус»
    txt(buf, 32, 70, 7);
  }
  spr.setTextDatum(ML_DATUM);
  txt(word, 150, 60, 4);
  spr.setTextColor(C_DIM(), C_BG());
  sprintf(buf, "dBm   ch %d", a.ch);
  txt(buf, 150, 84, 2);

  // шкала
  int pct = lost ? 0 : constrain((r + 95) * 100 / 60, 0, 100);
  int bx = 8, by = 102, bw = W - 16, bh = 12;
  spr.drawRoundRect(bx, by, bw, bh, 4, C(60, 60, 70));
  if (pct > 0) spr.fillRoundRect(bx + 2, by + 2, (bw - 4) * pct / 100, bh - 4, 3, col);

  // график
  int gx = 8, gy = 120, gw = W - 16, gh = 46;
  spr.drawRect(gx, gy, gw, gh, C(40, 40, 50));
  drawSpark(gx, gy, gw, gh, g_fSpark, g_fSparkN, F_SPARK_N, col);
  present();

  if (g_ledEvent == 0) {                       // лента — уровень сигнала
    int lv = pct * WS2812_N / 100;
    strip.clear();
    uint8_t rr = ((col >> 11) & 0x1F) << 3, gg = ((col >> 5) & 0x3F) << 2, bb = (col & 0x1F) << 3;
    for (int i = 0; i < lv; i++) strip.setPixelColor(i, strip.Color(rr / 3, gg / 3, bb / 3));
    strip.show();
  }
}
