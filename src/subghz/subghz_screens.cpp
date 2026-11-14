// ============================================================
//  subghz_screens.cpp — экраны раздела Sub-GHz: SPECTRUM · MONITOR
//  Все экраны собраны из компонентов core/ui.h по общей сетке.
// ============================================================
#include "subghz/subghz_model.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"

// «водопад»: v = dBm + 120; -110 почти чёрный … -40 белый
static uint16_t wfColor(int v) {
  float x = constrain((v - 10) / 70.0f, 0.0f, 1.0f);
  if (x < 0.08f) return C_BG();
  return heatV(x);
}

// ============================================================
//  SPECTRUM — развёртка ±2 МГц и «водопад»
// ============================================================
void drawSpectrum() {
  int W = spr.width();
  char buf[40];
  spr.fillSprite(C_BG());
  int mi = 0;
  for (int i = 1; i < SP_N; i++) if (g_spCur[i] > g_spCur[mi]) mi = i;
  snprintf(buf, sizeof(buf), "peak %.2f  %d dBm", spFreq(mi), g_spCur[mi]);
  char title[24]; snprintf(title, sizeof(title), "Spectrum %s", BANDS[g_band].name);
  uiHeader(title, buf, g_spCur[mi] > -60);

  int pl = UI_M + 4, pr = W - UI_M - 4, pt = UI_TOP, pb = UI_TOP + 54;
  auto px = [&](int i) { return pl + i * (pr - pl) / (SP_N - 1); };
  auto py = [&](int db) { int c = constrain(db, -110, -40); return pb - (c + 110) * (pb - pt) / 70; };
  uiFrame(pl, pt, pr - pl + 1, pb - pt + 1, UI_LINE());
  for (int db = -100; db <= -50; db += 10)
    for (int x = pl + 4; x < pr - 3; x += 4) spr.drawPixel(x, py(db), UI_LINE());
  // пики — тускло, текущий уровень — ярко и «раскаляется» по силе
  for (int i = 1; i < SP_N; i++) spr.drawLine(px(i - 1), py(g_spPeak[i - 1]), px(i), py(g_spPeak[i]), UI_DIM());
  for (int i = 0; i < SP_N; i++) {
    int x = px(i), y = py(g_spCur[i]);
    spr.drawFastVLine(x, y, pb - y, scale565(heat(g_spCur[i] + 10), 25));
  }
  for (int i = 1; i < SP_N; i++)
    spr.drawLine(px(i - 1), py(g_spCur[i - 1]), px(i), py(g_spCur[i]), heat(max(g_spCur[i - 1], g_spCur[i]) + 10));
  // маркер максимума
  int mx = px(mi), my = py(g_spCur[mi]);
  for (int y = pt + 2; y < pb; y += 3) spr.drawPixel(mx, y, C_ACC());
  spr.fillTriangle(mx, my - 3, mx - 4, my - 9, mx + 4, my - 9, C_ACC());

  // «водопад»: новые строки сверху
  int wt = pb + 4, wh = UI_BOTTOM - wt - 8;
  for (int r = 0; r < g_wfCount && r < wh; r++) {
    int row = (g_wfHead - 1 - r + WF_ROWS) % WF_ROWS;
    for (int i = 0; i < SP_N; i++) {
      int x0 = px(i), x1 = (i + 1 < SP_N) ? px(i + 1) : pr + 1;
      if (g_wf) spr.drawFastHLine(x0, wt + r, x1 - x0, wfColor(g_wf[row][i]));
    }
  }
  // ось частот
  spr.setTextColor(UI_DIM());
  snprintf(buf, sizeof(buf), "%.2f", BANDS[g_band].lo);
  spr.setTextDatum(TL_DATUM); txt(buf, pl, UI_BOTTOM - 6, 1);
  snprintf(buf, sizeof(buf), "%.2f MHZ", (BANDS[g_band].lo + BANDS[g_band].hi) / 2);
  spr.setTextDatum(TC_DATUM); txt(buf, W / 2, UI_BOTTOM - 6, 1);
  snprintf(buf, sizeof(buf), "%.2f", BANDS[g_band].hi);
  spr.setTextDatum(TR_DATUM); txt(buf, pr, UI_BOTTOM - 6, 1);
  uiFooter("click: next band", "side: back");
  present();
}

// ============================================================
//  MONITOR — одна частота, ловим всплески
// ============================================================
void drawMonitor(uint32_t now) {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  int cur = g_monN ? g_mon[g_monN - 1] : -120;
  snprintf(buf, sizeof(buf), "%d dBm  %s", cur, g_burst ? "signal!" : "quiet");
  char title[24]; snprintf(title, sizeof(title), "Monitor %.2f", BANDS[g_band].mon);
  uiHeader(title, buf, g_burst);

  // лента сигнала (~6 с), линия шума и порог срабатывания
  int gx = UI_M + 4, gy = UI_TOP, gw = W - 2 * UI_M - 8, gh = 62;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  auto yy = [&](int db) { int c = constrain(db, -115, -35); return gy + gh - 3 - (c + 115) * (gh - 6) / 80; };
  for (int x = gx + 4; x < gx + gw - 4; x += 4) spr.drawPixel(x, yy((int)g_floor + BURST_ON_DB), UI_DIM());
  for (int i = 0; i < g_monN; i++) {
    int x = gx + 2 + (gw - 5) * (i + (MON_N - g_monN)) / (MON_N - 1);
    int y = yy(g_mon[i]);
    bool hi = g_mon[i] > g_floor + BURST_ON_DB;
    spr.drawFastVLine(x, y, gy + gh - 3 - y, hi ? heat(g_mon[i] + 10) : UI_LINE());
  }
  snprintf(buf, sizeof(buf), "NOISE %d DBM   - - TRIGGER", (int)g_floor);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(buf, gx + 5, gy + 3, 1);

  // журнал всплесков
  int ly = gy + gh + 6;
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_events);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_HOT());
  txt(buf, UI_M + 8, ly, 6);
  spr.setTextColor(UI_DIM());
  txt("BURSTS", UI_M + 8, ly + 46, 1);
  int tx = 96;
  spr.setTextColor(UI_DIM());
  txt("WHEN", tx, ly, 1); txt("LENGTH", tx + 70, ly, 1); txt("PEAK", tx + 130, ly, 1);
  for (int i = 0; i < g_evN && i < 4; i++) {
    const Burst& b = g_ev[g_evN - 1 - i];
    uint32_t ago = (now - b.t) / 1000;
    if (ago < 60)        snprintf(buf, sizeof(buf), "%lus ago", (unsigned long)ago);
    else if (ago < 3600) snprintf(buf, sizeof(buf), "%lum ago", (unsigned long)(ago / 60));
    else                 snprintf(buf, sizeof(buf), "%luh ago", (unsigned long)(ago / 3600));
    int y = ly + 11 + i * 12;
    spr.setTextColor(i == 0 ? C_HOT() : UI_LABEL());
    txt(buf, tx, y, 1);
    snprintf(buf, sizeof(buf), "%.1f s", b.durMs / 1000.0f); txt(buf, tx + 70, y, 1);
    snprintf(buf, sizeof(buf), "%d", b.peak); txt(buf, tx + 130, y, 1);
    int len = constrain((b.peak + 110) * 1, 2, 60);
    spr.fillRect(W - UI_M - 8 - len, y + 1, len, 5, heat(b.peak + 10));
  }
  if (g_evN == 0) {
    spr.setTextColor(UI_DIM());
    txt("PRESS A CAR KEY OR GATE REMOTE NEARBY", tx, ly + 14, 1);
  }
  uiFooter("click: next band", "side: back");
  present();
}

// ============================================================
void drawCcMissing() {
  char buf[40];
  spr.fillSprite(C_BG());
  uiHeader("Sub-GHz", "radio not found", true);
  snprintf(buf, sizeof(buf), "chip answered 0x%02X", g_ccVersion);
  uiEmpty("CC1101 not found", buf);
  uiFooter("", "side: back");
  present();
}
