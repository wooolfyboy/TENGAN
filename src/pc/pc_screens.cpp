// ============================================================
//  pc_screens.cpp — экраны раздела PC
//
//  ОБЗОР — CPU крупно + график за минуту; справа RAM, диск, сеть, батарея
//  ЯДРА  — загрузка каждого ядра столбиками
//  ЖДЁМ  — если ноутбук ничего не присылает: что сделать по шагам
// ============================================================
#include "core/timeutil.h"
#include "pc/pc.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"

static uint16_t loadCol(float pct) {                 // загрузка: холодно -> горячо, > 90% — тревога
  return pct >= 90 ? C_ALERT() : heatV(0.25f + 0.75f * pct / 100.0f);
}
static void rateStr(float kb, char* out, size_t n) {
  if (kb >= 1000) snprintf(out, n, "%.1f MB/s", kb / 1024.0f);
  else            snprintf(out, n, "%d KB/s", (int)kb);
}
static void upStr(uint32_t s, char* out, size_t n) {
  if (s >= 86400) snprintf(out, n, "up %lud %luh", (unsigned long)(s / 86400), (unsigned long)(s / 3600 % 24));
  else            snprintf(out, n, "up %luh %lum", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
}
static void bar(int x, int y, int w, int h, float pct, uint16_t col) {
  spr.drawRect(x, y, w, h, UI_LINE());
  int f = (int)((w - 2) * constrain(pct, 0.0f, 100.0f) / 100.0f);
  if (f > 0) spr.fillRect(x + 1, y + 1, f, h - 2, col);
}
static void pageDots(int page) {
  int W = spr.width();
  for (int p = 0; p < 2; p++) spr.fillRect(W / 2 - 8 + p * 10, UI_BOTTOM + 2, 6, 2, p == page ? C_ACC() : UI_LINE());
}

void drawPc(uint32_t now) {
  const PcStat& p = g_pc;
  const int W = spr.width();
  char buf[40], a[16], b[16];
  spr.fillSprite(C_BG());
  uint32_t age = msSince(now, p.t) / 1000;
  char hdr[32];
  if (age >= 3) snprintf(hdr, sizeof(hdr), "no data %lus", (unsigned long)age);   // короткая пауза — цифры старые
  uiHeader("PC", age >= 3 ? hdr : (p.host[0] ? p.host : "laptop"), p.cpu >= 90);

  // ---- слева: CPU ----
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_LABEL());
  txt("CPU", UI_M + 6, UI_TOP + 4, 1);
  if (p.freq > 0) { snprintf(buf, sizeof(buf), "%.1f GHZ", p.freq); spr.setTextColor(UI_DIM()); txt(buf, UI_M + 34, UI_TOP + 4, 1); }
  snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(p.cpu));
  spr.setTextColor(loadCol(p.cpu));
  txt(buf, UI_M + 4, UI_TOP + 14, 6);
  // график: CPU (яркий) и RAM (тусклый) за минуту
  const int gx = UI_M + 4, gy = UI_TOP + 70, gw = 140, gh = 54;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  for (int k = 1; k < 4; k++) spr.drawFastHLine(gx + 1, gy + gh * k / 4, gw - 2, UI_LINE());
  auto plot = [&](const float* h, uint16_t col, bool fill) {
    int px = -1, py = 0;
    for (int i = 0; i < p.histN; i++) {
      int x = gx + gw - 2 - (p.histN - 1 - i) * (gw - 3) / (PC_HIST - 1);
      int y = gy + gh - 2 - (int)((gh - 4) * constrain(h[i], 0.0f, 100.0f) / 100.0f);
      if (fill) spr.drawFastVLine(x, y, gy + gh - 1 - y, tft.color565(10, 45, 56));
      if (px >= 0) spr.drawLine(px, py, x, y, col);
      px = x; py = y;
    }
  };
  plot(p.ramH, UI_DIM(), false);
  plot(p.cpuH, C_ACC(), true);
  plot(p.cpuH, C_ACC(), false);
  spr.setTextColor(UI_DIM());
  txt("60 S", gx + 3, gy + 3, 1);

  // ---- справа: RAM, диск, сеть, батарея ----
  const int rx = 164, rw = W - rx - UI_M - 4;
  int y = UI_TOP + 4;
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_LABEL()); txt("RAM", rx, y, 1);
  snprintf(buf, sizeof(buf), "%.1f / %.0f GB", p.ramU, p.ramT);
  spr.setTextDatum(TR_DATUM); spr.setTextColor(C_HOT()); txt(buf, rx + rw, y, 1);
  bar(rx, y + 11, rw, 8, p.ram, loadCol(p.ram));

  y += 28;
  spr.setTextDatum(TL_DATUM); spr.setTextColor(UI_LABEL()); txt("DISK C:", rx, y, 1);
  snprintf(buf, sizeof(buf), "%d%% FULL", (int)p.disk);
  spr.setTextDatum(TR_DATUM); spr.setTextColor(C_HOT()); txt(buf, rx + rw, y, 1);
  bar(rx, y + 11, rw, 8, p.disk, p.disk >= 90 ? C_ALERT() : UI_LABEL());
  snprintf(buf, sizeof(buf), "R %.1f  W %.1f MB/S", p.dR, p.dW);
  spr.setTextDatum(TL_DATUM); spr.setTextColor(UI_DIM()); txt(buf, rx, y + 22, 1);

  y += 38;
  spr.setTextColor(UI_LABEL()); txt("NET", rx, y, 1);
  rateStr(p.nD, a, sizeof(a)); rateStr(p.nU, b, sizeof(b));
  spr.setTextColor(C_HOT());
  snprintf(buf, sizeof(buf), "DN %s", a); txt(buf, rx, y + 11, 1);
  snprintf(buf, sizeof(buf), "UP %s", b); txt(buf, rx, y + 22, 1);

  y += 36;
  if (p.bat >= 0) {
    spr.setTextColor(UI_LABEL()); txt("BATTERY", rx, y, 1);
    snprintf(buf, sizeof(buf), "%d%% %s", p.bat, p.plug ? "PLUGGED" : "");
    spr.setTextDatum(TR_DATUM); spr.setTextColor(p.bat <= 15 && !p.plug ? C_ALERT() : C_HOT()); txt(buf, rx + rw, y, 1);
  }

  char top[40]; snprintf(top, sizeof(top), "top: %s", p.top[0] ? p.top : "-");
  upStr(p.up, buf, sizeof(buf));
  uiFooter(top, buf);
  pageDots(0);
  present();
}

void drawPcCores(uint32_t now) {
  const PcStat& p = g_pc;
  const int W = spr.width();
  char buf[40];
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d cores  %d%%", p.nCores, (int)lroundf(p.cpu));
  uiHeader("Cores", buf, p.cpu >= 90);
  int n = max(1, p.nCores);
  const int x0 = UI_M + 6, x1 = W - UI_M - 6, top = UI_TOP + 8, bot = UI_BOTTOM - 14;
  int slot = (x1 - x0) / n, bw = max(4, slot - (n > 8 ? 3 : 6));
  for (int k = 25; k < 100; k += 25) {
    int yy = bot - (bot - top) * k / 100;
    for (int x = x0; x < x1; x += 4) spr.drawPixel(x, yy, UI_LINE());
  }
  spr.setTextDatum(TC_DATUM);
  for (int i = 0; i < p.nCores; i++) {
    int x = x0 + i * slot + (slot - bw) / 2;
    int h = (bot - top) * p.core[i] / 100;
    spr.drawRect(x, top, bw, bot - top, UI_LINE());
    if (h > 0) spr.fillRect(x + 1, bot - h, bw - 2, h, loadCol(p.core[i]));
    spr.setTextColor(UI_DIM());
    snprintf(buf, sizeof(buf), "%d", i + 1);
    txt(buf, x + bw / 2, bot + 3, 1);
  }
  uiFooter("wheel: page", "side: back");
  pageDots(1);
  present();
  (void)now;
}

void drawPcWaiting(uint32_t now) {
  const int W = spr.width();
  spr.fillSprite(C_BG());
  bool lost = g_pc.have;
  uiHeader("PC", lost ? "laptop stopped" : "waiting for laptop", lost);
  spr.setTextDatum(TL_DATUM);
  int x = UI_M + 10, y = UI_TOP + 10;
  spr.setTextColor(C_ACC());  txt("1", x, y, 4);
  spr.setTextColor(C_HOT());  txt("PLUG THIS DEVICE INTO THE LAPTOP (USB)", x + 22, y + 6, 1);
  y += 34;
  spr.setTextColor(C_ACC());  txt("2", x, y, 4);
  spr.setTextColor(C_HOT());  txt("ON THE LAPTOP DOUBLE-CLICK", x + 22, y + 2, 1);
  spr.setTextColor(C_ACC());  txt("start_pc_monitor.bat", x + 22, y + 13, 1);
  y += 34;
  spr.setTextColor(C_ACC());  txt("3", x, y, 4);
  spr.setTextColor(C_HOT());  txt("WAIT 5 SECONDS - NUMBERS APPEAR HERE", x + 22, y + 6, 1);
  // «пульс ожидания»
  int k = (now / 300) % 4;
  for (int i = 0; i < 3; i++) spr.fillRect(W - UI_M - 30 + i * 8, UI_TOP + 112, 5, 5, i < k ? C_ACC() : UI_LINE());
  uiFooter("", "side: back");
  present();
}
