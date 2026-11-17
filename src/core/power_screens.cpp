// ============================================================
//  power_screens.cpp — power screens: "keep holding" countdown ring and
//  the power-off animation. Split from power.cpp (hardware) so the
//  simulator can use them.
// ============================================================
#include "core/power.h"
#include "core/lang.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/settings.h"
#include "core/ui.h"

// arc around a circle (TFT_eSPI: 0° at the bottom, clockwise); may wrap past 360°
static void arcWrap(int cx, int cy, int r, int ir, int a0, int a1, uint16_t col, bool smooth) {
  if (a1 <= a0) return;
  if (a1 <= 360) { spr.drawArc(cx, cy, r, ir, a0, a1, col, C_BG(), smooth); return; }
  spr.drawArc(cx, cy, r, ir, a0, 360, col, C_BG(), smooth);
  spr.drawArc(cx, cy, r, ir, 0, min(a1 - 360, 360), col, C_BG(), smooth);
}

void drawPowerHold() {
  const int W = spr.width(), H = spr.height(), cx = W / 2, cy = H / 2 - 4;
  uint32_t p = g_powerHold > PWR_OFF_MS ? PWR_OFF_MS : g_powerHold;
  float k = constrain((float)(p - PWR_SHOW_MS) / (PWR_OFF_MS - PWR_SHOW_MS), 0.0f, 1.0f);
  spr.fillSprite(C_BG());
  // countdown ring: fills clockwise while the button is held
  const int R = 46;
  spr.drawArc(cx, cy, R, R - 7, 0, 360, UI_LINE(), C_BG(), false);
  if (k > 0) arcWrap(cx, cy, R, R - 7, 180, 180 + (int)(360 * k), k > 0.85f ? C_HOT() : C_ACC(), true);   // from 12 o'clock, clockwise
  for (int a = 0; a < 360; a += 30) {                  // ticks around the circle
    float rad = a * DEG_TO_RAD;
    spr.drawLine(cx + cosf(rad) * (R + 4), cy + sinf(rad) * (R + 4), cx + cosf(rad) * (R + 8), cy + sinf(rad) * (R + 8), a / 30 <= (int)(k * 12) ? C_ACC() : UI_LINE());
  }
  // power icon: arc with a gap at the top and a vertical bar
  uint16_t ic = k > 0.85f ? C_HOT() : C_ACC();
  arcWrap(cx, cy, 18, 14, 215, 505, ic, true);         // arc with a gap at the top
  spr.fillRoundRect(cx - 2, cy - 22, 5, 20, 2, ic);
  // labels
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_ACC());
  txt(L("// POWER", "// ПИТАНИЕ"), UI_M + 4, 8, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(UI_LABEL());
  char buf[16]; snprintf(buf, sizeof(buf), "%d%%", (int)(k * 100));
  txt(buf, W - UI_M - 4, 10, 1);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(UI_LABEL());
  txt(L("KEEP HOLDING THE SIDE BUTTON", "ДЕРЖИТЕ БОКОВУЮ КНОПКУ"), cx, H - 18, 1);
  present();
}

// ---------- power-off animation (~1.4 s): old-TV style ----------
// radar rings collapse to the center, the picture squeezes into a bright line,
// the line into a dot, the dot fades. shutFrame(t) draws the frame at t (simulator too).
void shutFrame(uint32_t t) {
  const int W = spr.width(), H = spr.height(), cx = W / 2, cy = H / 2;
  spr.fillSprite(C_BG());
  if (t < 650) {                                       // 1. rings converge, text flickers
    float k = t / 650.0f;
    for (int i = 0; i < 4; i++) {
      int r = (int)((20 + i * 20) * (1 - k * k));
      if (r > 1) spr.drawCircle(cx, cy, r, i == 3 ? C_ACC() : UI_DIM());
    }
    for (int a = 0; a < 360; a += 30) {                // ray ticks retract too
      float rad = a * DEG_TO_RAD, r0 = 86 * (1 - k), r1 = r0 + 6;
      spr.drawLine(cx + cosf(rad) * r0, cy + sinf(rad) * r0, cx + cosf(rad) * r1, cy + sinf(rad) * r1, UI_DIM());
    }
    spr.fillCircle(cx, cy, 2 + (int)(k * 4), C_HOT());
    if ((t / 90) % 4 != 0) {
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(C_ACC());
      txt(L("SHUTTING DOWN", "ВЫКЛЮЧЕНИЕ"), cx, cy + 40, 1);
      spr.setTextColor(UI_LABEL());
      txt(DEVICE_NAME, cx, cy - 42, 1);
    }
  } else if (t < 900) {                                // 2. screen squeezes into a glowing line
    float k = (t - 650) / 250.0f;
    int h = max(2, (int)(H * (1 - k) * (1 - k)));
    spr.fillRect(0, cy - h / 2, W, h, scale565(C_ACC(), 35 + (int)(k * 30)));
    spr.fillRect(0, cy - max(1, h / 6), W, max(2, h / 3), C_HOT());
  } else if (t < 1150) {                               // 3. line shrinks to a dot
    float k = (t - 900) / 250.0f;
    int w = max(4, (int)(W * (1 - k) * (1 - k)));
    spr.fillRect(cx - w / 2, cy - 1, w, 3, C_HOT());
    spr.drawFastHLine(cx - w / 2 - 6, cy, w + 12, C_ACC());
  } else {                                             // 4. dot fades
    float k = min(1.0f, (t - 1150) / 250.0f);
    int r = max(1, (int)(4 * (1 - k)));
    spr.fillCircle(cx, cy, r + 2, scale565(C_ACC(), (int)(60 * (1 - k))));
    spr.fillCircle(cx, cy, r, scale565(C_HOT(), (int)(100 * (1 - k))));
  }
}

