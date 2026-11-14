// ============================================================
//  audio_screens.cpp — экраны раздела Sound
//
//  BARS (спектр) · REACTOR (анимация) · LEVEL (шумомер) · SCOPE (волна)
// ============================================================
#include "audio/audio_model.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"

static uint16_t dim565(uint16_t c, int pct) {
  uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return tft.color565(r * pct / 100, g * pct / 100, b * pct / 100);
}

static void hzStr(float hz, char* out, size_t n) {
  if (hz >= 1000) snprintf(out, n, "%.1f kHz", hz / 1000.0f);
  else            snprintf(out, n, "%d Hz", (int)hz);
}

// ============================================================
//  BARS — 32 полосы с «шапками» пиков и отражением
// ============================================================
void drawAuBars(uint32_t now) {
  int W = spr.width();
  char buf[40], hz[16];
  spr.fillSprite(C_BG());
  hzStr(g_au.domHz, hz, sizeof(hz));
  snprintf(buf, sizeof(buf), "%d dBfs  %s", (int)g_au.level, g_au.energy > 0.15f ? hz : "--");
  uiHeader("Sound spectrum", buf);

  const int x0 = UI_M + 8, base = 112, hMax = 84, step = (W - 2 * x0) / AU_BANDS;
  for (int k = 0; k < AU_BANDS; k++) {
    int x = x0 + k * step, bw = step - 2;
    int h = (int)(g_au.bands[k] * hMax);
    // сегменты по 3 px: внизу холоднее, вверху «горячее»
    for (int y = 0; y + 3 <= h; y += 4) {
      uint16_t c = heatV(0.25f + 0.75f * (float)y / hMax);
      spr.fillRect(x, base - y - 3, bw, 3, c);
      if (y < 28) spr.fillRect(x, base + 3 + y, bw, 2, dim565(c, 22 - y * 3 / 4));   // отражение
    }
    int cy = base - (int)(g_au.caps[k] * hMax) - 5;
    spr.fillRect(x, cy, bw, 2, C_HOT());
  }
  spr.drawFastHLine(x0, base + 1, step * AU_BANDS - 2, UI_LINE());
  // подписи частот
  static const float marks[] = { 100, 500, 1000, 4000 };
  static const char* const lbl[] = { "100", "500", "1K", "4K" };
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(UI_DIM());
  for (int i = 0; i < 4; i++) {
    float pos = logf(marks[i] / 60.0f) / logf(8000.0f / 60.0f);
    txt(lbl[i], x0 + (int)(pos * step * AU_BANDS), UI_BOTTOM - 4, 1);
  }
  uiFooter("wheel: screens", "side: back");
  present();
}

// ============================================================
//  REACTOR — кольцо лучей пульсирует и вращается в такт звуку
// ============================================================
void drawReactor(uint32_t now) {
  int W = spr.width();
  spr.fillSprite(C_BG());
  uiHeader("Reactor", levelWord(g_au.level), g_au.level >= -25);
  int cx = W / 2, cy = 90;
  float e = g_au.energy;
  float rot = now * 0.0004f + e * 0.8f;
  int r0 = 24 + (int)(e * 14);

  // внешнее кольцо с делениями, вращается навстречу
  for (int k = 0; k < 48; k++) {
    float a = -now * 0.0002f + k * 2 * PI / 48;
    int r1 = 70, r2 = (k % 4 == 0) ? 64 : 67;
    spr.drawLine(cx + cosf(a) * r1, cy + sinf(a) * r1, cx + cosf(a) * r2, cy + sinf(a) * r2, UI_LINE());
  }
  // лучи: 64 штуки, спектр зеркально по кругу
  for (int i = 0; i < 64; i++) {
    int band = i < 32 ? i : 63 - i;
    float v = g_au.bands[band];
    float a = rot + i * 2 * PI / 64;
    int len = 3 + (int)(v * 34);
    float ca = cosf(a), sa = sinf(a);
    spr.drawWideLine(cx + ca * (r0 + 3), cy + sa * (r0 + 3), cx + ca * (r0 + 3 + len), cy + sa * (r0 + 3 + len),
                     2.0f, heatV(0.25f + 0.75f * v), C_BG());
  }
  // ядро
  spr.drawCircle(cx, cy, r0, C_ACC());
  spr.drawCircle(cx, cy, r0 - 4, dim565(C_ACC(), 40));
  spr.fillSmoothCircle(cx, cy, max(3, (int)(r0 * 0.35f + e * 8)), heatV(0.5f + 0.5f * e), C_BG());
  // орбиты-точки
  for (int k = 0; k < 3; k++) {
    float a = now * (0.0015f + k * 0.0007f) + k * 2.1f;
    int rr = 48 + k * 8;
    spr.fillCircle(cx + cosf(a) * rr, cy + sinf(a) * rr, 2, k == 0 ? C_ACC() : UI_LABEL());
  }
  // показания по углам
  char buf[24];
  snprintf(buf, sizeof(buf), "%d", (int)g_au.level);
  uiReadout(UI_M + 8, UI_TOP + 4, "dBfs", buf, C_HOT(), 4);
  hzStr(g_au.domHz, buf, sizeof(buf));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(UI_DIM());
  txt("DOMINANT", W - UI_M - 8, UI_TOP + 4, 1);
  spr.setTextColor(e > 0.15f ? C_HOT() : UI_DIM());
  txt(e > 0.15f ? buf : "--", W - UI_M - 8, UI_TOP + 14, 2);
  uiFooter("wheel: screens", "side: back");
  present();
}

// ============================================================
//  LEVEL — шумомер: число, шкала с пиком, история за минуту
// ============================================================
void drawLevel(uint32_t now) {
  int W = spr.width();
  char buf[32];
  spr.fillSprite(C_BG());
  bool loud = g_au.level >= -25;
  uiHeader("Noise meter", levelWord(g_au.level), loud);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(loud ? C_ALERT() : C_HOT());
  txtNum((long)lroundf(g_au.level), UI_M + 8, UI_TOP + 2, 6);
  spr.setTextColor(UI_DIM());
  txt("DBFS  (0 = MIC MAXIMUM)", UI_M + 8, UI_TOP + 50, 1);
  snprintf(buf, sizeof(buf), "%d", (int)g_au.peak);
  uiReadout(150, UI_TOP + 4, "peak", buf, C_HOT());
  snprintf(buf, sizeof(buf), "%d", (int)g_auMax);
  uiReadout(204, UI_TOP + 4, "max", buf, C_HOT());
  snprintf(buf, sizeof(buf), "%d", (int)g_auAvg);
  uiReadout(254, UI_TOP + 4, "avg", buf, C_HOT());

  // сегментная шкала -80..0 dBFS; выше -25 — тревога
  int mx = UI_M + 8, mw = W - 2 * mx, my = UI_TOP + 62, segs = 40, sw = mw / segs;
  for (int i = 0; i < segs; i++) {
    float db = -80 + 80.0f * (i + 1) / segs;
    bool on = g_au.level >= db;
    uint16_t c = db > -25 ? C_ALERT() : heatV(0.3f + 0.7f * i / segs);
    spr.fillRect(mx + i * sw, my, sw - 1, 9, on ? c : UI_LINE());
  }
  int px = mx + (int)((constrain(g_au.peak, -80.0f, 0.0f) + 80) / 80 * mw);
  spr.fillRect(px - 1, my - 2, 2, 13, C_HOT());
  spr.setTextColor(UI_DIM());
  spr.setTextDatum(TL_DATUM); txt("-80", mx, my + 12, 1);
  spr.setTextDatum(TC_DATUM); txt("-40", mx + mw / 2, my + 12, 1);
  spr.setTextDatum(TR_DATUM); txt("0", mx + mw, my + 12, 1);

  // история за минуту
  int gx = UI_M + 4, gy = UI_TOP + 86, gw = W - 2 * UI_M - 8, gh = UI_BOTTOM - gy + 2;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  int prevx = -1, prevy = -1;
  for (int i = 0; i < g_auHistN; i++) {
    int x = gx + 2 + (gw - 5) * (i + (AU_HIST - g_auHistN)) / (AU_HIST - 1);
    int y = gy + gh - 3 - (int)((constrain(g_auHist[i], -80.0f, 0.0f) + 80) / 80 * (gh - 6));
    if (prevx >= 0) spr.drawLine(prevx, prevy, x, y, g_auHist[i] >= -25 ? C_ALERT() : C_ACC());
    prevx = x; prevy = y;
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt("1 MIN", gx + 5, gy + 3, 1);
  uiFooter("click: reset max", "side: back");
  present();
}

// ============================================================
//  SCOPE — осциллограф: форма волны с «запуском» по переходу через ноль
// ============================================================
void drawWave(uint32_t now) {
  int W = spr.width();
  char buf[24];
  spr.fillSprite(C_BG());
  hzStr(g_au.domHz, buf, sizeof(buf));
  uiHeader("Waveform", g_au.energy > 0.15f ? buf : "listening");
  int gx = UI_M + 4, gy = UI_TOP, gw = W - 2 * UI_M - 8, gh = UI_BOTTOM - UI_TOP + 2, cy = gy + gh / 2;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  for (int x = gx + 4; x < gx + gw - 4; x += 4) spr.drawPixel(x, cy, UI_LINE());
  for (int k = 1; k < 8; k++) for (int y = gy + 4; y < gy + gh - 4; y += 4) spr.drawPixel(gx + k * gw / 8, y, UI_LINE());

  // запуск: первый переход снизу вверх через ноль — картинка не «плывёт»
  int start = 0;
  for (int i = 1; i < AU_WAVE / 2; i++) if (g_au.wave[i - 1] < 0 && g_au.wave[i] >= 0) { start = i; break; }
  int span = AU_WAVE - AU_WAVE / 2;
  int amp = 200;
  for (int i = 0; i < AU_WAVE; i++) amp = max(amp, abs((int)g_au.wave[i]));
  float k = (gh / 2 - 6) / (float)amp;                         // автоусиление
  int px = -1, py = -1;
  for (int i = 0; i < span; i++) {
    int x = gx + 3 + (gw - 6) * i / (span - 1);
    int y = cy - (int)(g_au.wave[start + i] * k);
    if (px >= 0) {
      spr.drawLine(px, py + 1, x, y + 1, dim565(C_ACC(), 35));  // «свечение»
      spr.drawLine(px, py, x, y, C_ACC());
    }
    px = x; py = y;
  }
  snprintf(buf, sizeof(buf), "AUTO GAIN X%d  RAW %d  %s", max(1, 16000 / amp), g_micDiag.rawPeak, g_micDiag.slot ? "R" : "L");
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(buf, gx + 5, gy + 3, 1);
  uiFooter("wheel: screens", "side: back");
  present();
}

void drawMicMissing() {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  if (g_micDiag.disabled) {
    uiHeader("Sound", "microphone off");
    uiEmpty("microphone off", "turn on: settings > sound");
    uiFooter("", "side: back");
    present();
    return;
  }
  static const char* const stages[] = { "not started", "no free channel", "config rejected",
                                         "start failed", "running" };
  uiHeader("Mic check", g_micDiag.stage == 4 ? "no sound data" : "driver error", true);
  int x = UI_M + 8, y = UI_TOP + 4;
  uiReadout(x, y, "driver", stages[constrain(g_micDiag.stage, 0, 4)], g_micDiag.stage == 4 ? C_HOT() : C_ALERT());
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_micDiag.blocks);
  uiReadout(x, y + 30, "blocks", buf, g_micDiag.blocks ? C_HOT() : C_ALERT());
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_micDiag.timeouts);
  uiReadout(x + 90, y + 30, "timeouts", buf, C_HOT());
  snprintf(buf, sizeof(buf), "%d", g_micDiag.rawPeak);
  uiReadout(x + 180, y + 30, "raw peak", buf, C_HOT());
  uiReadout(x, y + 60, "channel", g_micDiag.slot ? "right" : "left", C_HOT());
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt("PDM MIC: CLK GPIO39, DATA GPIO42, I2S0, 16 KHZ", x, y + 96, 1);
  txt("SEND A PHOTO OF THIS SCREEN TO THE DEVELOPER", x, y + 108, 1);
  uiFooter("", "side: back");
  present();
}
