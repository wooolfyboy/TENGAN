// ============================================================
//  ui.cpp — компоненты дизайн-системы
// ============================================================
#include "core/ui.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/settings.h"

static bool sumi() { return g_theme == THEME_SUMI; }
uint16_t UI_LABEL() { return sumi() ? tft.color565(74, 67, 59)    : tft.color565(150, 175, 192); }
uint16_t UI_DIM()   { return sumi() ? tft.color565(138, 129, 116) : tft.color565(90, 112, 128); }
uint16_t UI_LINE()  { return sumi() ? tft.color565(201, 191, 174) : tft.color565(32, 50, 64); }
uint16_t UI_PANEL() { return sumi() ? tft.color565(222, 212, 194) : tft.color565(14, 42, 52); }

static void upper(const char* s, char* out, size_t n) {
  size_t i = 0;
  for (; s[i] && i < n - 1; i++) out[i] = (char)toupper((unsigned char)s[i]);
  out[i] = 0;
}

void uiHeader(const char* title, const char* right, bool rightAlert) {
  int W = spr.width();
  char t[40], u[36];
  upper(title, u, sizeof(u));
  snprintf(t, sizeof(t), "// %s", u);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_ACC());
  txt(t, UI_M + 4, 3, 2);
  if (right && right[0]) {
    char r0[48], r[48]; upper(right, r0, sizeof(r0));
    int room = W - UI_M - 4 - (UI_M + 4 + txtW(t, 2)) - 10;   // сколько места справа от заголовка
    fitText(r0, r, sizeof(r), max(room, 12), 1);               // не наезжать на заголовок
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(rightAlert ? C_ALERT() : UI_LABEL());
    txt(r, W - UI_M - 4, 6, 1);
  }
  // линия под шапкой, угасающая вправо
  uint16_t a = C_ACC();
  for (int x = UI_M; x < W - UI_M; x++) {
    float k = 1.0f - 0.85f * (x - UI_M) / (float)(W - 2 * UI_M);
    spr.drawPixel(x, 19, scale565(a, (int)(k * 100)));
  }
}

void uiFooter(const char* left, const char* right) {
  int W = spr.width();
  char t[60];
  spr.setTextColor(UI_DIM());
  if (left && left[0]) {
    upper(left, t, sizeof(t));
    spr.setTextDatum(BL_DATUM);
    txt(t, UI_M + 8, UI_FOOT_Y, 1);
  }
  if (right && right[0]) {
    upper(right, t, sizeof(t));
    spr.setTextDatum(BR_DATUM);
    txt(t, W - UI_M - 8, UI_FOOT_Y, 1);
  }
}

void uiFrame(int x, int y, int w, int h, uint16_t col) {
  const int L = 6;
  spr.drawFastHLine(x, y, L, col);             spr.drawFastVLine(x, y, L, col);
  spr.drawFastHLine(x + w - L, y, L, col);     spr.drawFastVLine(x + w - 1, y, L, col);
  spr.drawFastHLine(x, y + h - 1, L, col);     spr.drawFastVLine(x, y + h - L, L, col);
  spr.drawFastHLine(x + w - L, y + h - 1, L, col); spr.drawFastVLine(x + w - 1, y + h - L, L, col);
}

void uiReadout(int x, int y, const char* label, const char* value, uint16_t valCol, int valSize) {
  char l[32]; upper(label, l, sizeof(l));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(l, x, y, 1);
  spr.setTextColor(valCol);
  txt(value, x, y + 10, valSize);
}

void uiEmpty(const char* msg, const char* sub) {
  int W = spr.width();
  char m[40]; upper(msg, m, sizeof(m));
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(UI_LABEL());
  txt(m, W / 2, 80, 4);
  if (sub) {
    char s[60]; upper(sub, s, sizeof(s));
    spr.setTextColor(UI_DIM());
    txt(s, W / 2, 104, 1);
  }
}

void uiRowHighlight(int y, int h) {
  int W = spr.width();
  hudPanel(UI_M - 4, y, W - 2 * UI_M + 8, h, 6, UI_PANEL(), true, C_BG());
  spr.fillRect(UI_M - 4, y + 2, 2, h - 8, C_ACC());
}
