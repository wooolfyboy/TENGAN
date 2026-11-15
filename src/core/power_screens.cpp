// ============================================================
//  power_screens.cpp — экраны питания: «держи кнопку» (кольцо-отсчёт)
//  и анимация выключения. Отдельно от power.cpp (там железо), чтобы
//  их видел симулятор.
// ============================================================
#include "core/power.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/settings.h"
#include "core/ui.h"

// дуга по кругу (TFT_eSPI: 0° — снизу, по часовой); умеет переходить через 360°
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
  // кольцо-отсчёт: заполняется по часовой, пока держишь кнопку
  const int R = 46;
  spr.drawArc(cx, cy, R, R - 7, 0, 360, UI_LINE(), C_BG(), false);
  if (k > 0) arcWrap(cx, cy, R, R - 7, 180, 180 + (int)(360 * k), k > 0.85f ? C_HOT() : C_ACC(), true);   // от 12 часов по часовой
  for (int a = 0; a < 360; a += 30) {                  // риски по кругу
    float rad = a * DEG_TO_RAD;
    spr.drawLine(cx + cosf(rad) * (R + 4), cy + sinf(rad) * (R + 4), cx + cosf(rad) * (R + 8), cy + sinf(rad) * (R + 8), a / 30 <= (int)(k * 12) ? C_ACC() : UI_LINE());
  }
  // значок питания: дуга с разрывом сверху и вертикальная черта
  uint16_t ic = k > 0.85f ? C_HOT() : C_ACC();
  arcWrap(cx, cy, 18, 14, 215, 505, ic, true);         // дуга с разрывом сверху
  spr.fillRoundRect(cx - 2, cy - 22, 5, 20, 2, ic);
  // подписи
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_ACC());
  txt("// POWER", UI_M + 4, 8, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(UI_LABEL());
  char buf[16]; snprintf(buf, sizeof(buf), "%d%%", (int)(k * 100));
  txt(buf, W - UI_M - 4, 10, 1);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(UI_LABEL());
  txt("KEEP HOLDING THE SIDE BUTTON", cx, H - 18, 1);
  present();
}

// ---------- анимация выключения (~1.4 с): как у старого телевизора ----------
// кольца радара схлопываются к центру, картинка сжимается в яркую полосу,
// полоса — в точку, точка гаснет. shutFrame(t) рисует кадр в момент t (для симулятора тоже).
void shutFrame(uint32_t t) {
  const int W = spr.width(), H = spr.height(), cx = W / 2, cy = H / 2;
  spr.fillSprite(C_BG());
  if (t < 650) {                                       // 1. кольца сходятся, надпись мерцает
    float k = t / 650.0f;
    for (int i = 0; i < 4; i++) {
      int r = (int)((20 + i * 20) * (1 - k * k));
      if (r > 1) spr.drawCircle(cx, cy, r, i == 3 ? C_ACC() : UI_DIM());
    }
    for (int a = 0; a < 360; a += 30) {                // лучи-риски тоже втягиваются
      float rad = a * DEG_TO_RAD, r0 = 86 * (1 - k), r1 = r0 + 6;
      spr.drawLine(cx + cosf(rad) * r0, cy + sinf(rad) * r0, cx + cosf(rad) * r1, cy + sinf(rad) * r1, UI_DIM());
    }
    spr.fillCircle(cx, cy, 2 + (int)(k * 4), C_HOT());
    if ((t / 90) % 4 != 0) {
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(C_ACC());
      txt("SHUTTING DOWN", cx, cy + 40, 1);
      spr.setTextColor(UI_LABEL());
      txt(DEVICE_NAME, cx, cy - 42, 1);
    }
  } else if (g_theme == THEME_SUMI) {                  // SUMI: вместо CRT — капля туши растекается и заливает бумагу
    float k = min(1.0f, (t - 650) / 700.0f), e = k * k * (3 - 2 * k);
    int R = (int)(e * 200);
    uint16_t ink = C_ACC();
    if (R > 0) {
      spr.fillCircle(cx, cy, R, ink);
      for (int i = 0; i < 9; i++) {                    // неровный край: «подтёки» по кругу
        float a = i * 0.698f + 0.4f, rr = R * (0.18f + 0.07f * ((i * 37) % 5));
        spr.fillCircle(cx + (int)(cosf(a) * R * 0.92f), cy + (int)(sinf(a) * R * 0.92f), (int)rr, ink);
      }
    }
    if (k < 0.5f) spr.fillCircle(cx, cy, 3, C_SEAL());   // красная точка-печать в центре, пока не утонула
  } else if (t < 900) {                                // 2. экран сжимается в светящуюся полосу
    float k = (t - 650) / 250.0f;
    int h = max(2, (int)(H * (1 - k) * (1 - k)));
    spr.fillRect(0, cy - h / 2, W, h, scale565(C_ACC(), 35 + (int)(k * 30)));
    spr.fillRect(0, cy - max(1, h / 6), W, max(2, h / 3), C_HOT());
  } else if (t < 1150) {                               // 3. полоса стягивается в точку
    float k = (t - 900) / 250.0f;
    int w = max(4, (int)(W * (1 - k) * (1 - k)));
    spr.fillRect(cx - w / 2, cy - 1, w, 3, C_HOT());
    spr.drawFastHLine(cx - w / 2 - 6, cy, w + 12, C_ACC());
  } else {                                             // 4. точка гаснет
    float k = min(1.0f, (t - 1150) / 250.0f);
    int r = max(1, (int)(4 * (1 - k)));
    spr.fillCircle(cx, cy, r + 2, scale565(C_ACC(), (int)(60 * (1 - k))));
    spr.fillCircle(cx, cy, r, scale565(C_HOT(), (int)(100 * (1 - k))));
  }
}

