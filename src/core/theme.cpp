// ============================================================
//  theme.cpp — дизайн-система «Tactical HUD»
// ============================================================
#include "core/theme.h"
#include "core/ui.h"
#include "core/display.h"
#include "core/fonts/STM14.h"
#include "core/fonts/RAJ24.h"
#include "core/fonts/RAJ44.h"
#include "core/fonts/JBM14.h"   // с кириллицей — для текстов «снаружи» (названия треков)
#include "core/fonts/JBM19.h"
#include "core/settings.h"

struct Stop { float x; uint8_t r, g, b; };
static uint16_t ramp(const Stop* s, int n, float x) {
  x = constrain(x, 0.0f, 1.0f);
  int i = 0;
  while (i < n - 2 && x > s[i + 1].x) i++;
  float k = (x - s[i].x) / (s[i + 1].x - s[i].x);
  return tft.color565(s[i].r + (s[i + 1].r - s[i].r) * k,
                      s[i].g + (s[i + 1].g - s[i].g) * k,
                      s[i].b + (s[i + 1].b - s[i].b) * k);
}

// палитра: яркость исходного цвета -> бирюзово-стальная шкала
static const Stop COOL[] = {
  { 0.00f,   5,   8,  13 }, { 0.25f,  24,  40,  54 }, { 0.50f,  95, 125, 145 },
  { 0.72f, 150, 185, 205 }, { 0.86f,  62, 230, 255 }, { 1.00f, 235, 250, 255 },
};
// «жар»: холодный стальной -> бирюзовый -> белый (без оранжевого: он только для тревоги)
static const Stop HEAT[] = {
  { 0.00f,  14,  30,  46 }, { 0.35f,  40,  95, 130 }, { 0.75f,  62, 230, 255 }, { 1.00f, 235, 250, 255 },
};

// ---- тема SUMI (тушь на бумаге): светлый фон, тёмное — главное ----
// яркость исходного цвета -> оттенок туши: тёмное почти исчезает в бумаге, яркое — чёрная тушь
static const Stop COOL_SUMI[] = {
  { 0.00f, 236, 228, 212 }, { 0.50f, 170, 160, 145 }, { 0.86f, 60, 54, 47 }, { 1.00f, 27, 24, 20 },
};
static const Stop HEAT_SUMI[] = {
  { 0.00f, 222, 212, 194 }, { 0.35f, 160, 150, 135 }, { 0.75f, 74, 67, 59 }, { 1.00f, 27, 24, 20 },
};
static bool sumi() { return g_theme == THEME_SUMI; }

static uint16_t s_lut[256];
static int      s_lutTheme = -1;                      // для какой темы построена таблица

uint16_t C(uint8_t r, uint8_t g, uint8_t b) {
  if (s_lutTheme != g_theme) {
    for (int i = 0; i < 256; i++) s_lut[i] = sumi() ? ramp(COOL_SUMI, 4, i / 255.0f) : ramp(COOL, 6, i / 255.0f);
    s_lutTheme = g_theme;
  }
  return s_lut[(77 * r + 150 * g + 29 * b) >> 8];    // яркость по восприятию глаза
}
// TC — «буквальный» цвет: в TENGAN как есть, в SUMI — оттенок туши по яркости
uint16_t TC(uint8_t r, uint8_t g, uint8_t b) { return sumi() ? C(r, g, b) : tft.color565(r, g, b); }
uint16_t C_BG()    { return sumi() ? tft.color565(236, 228, 212) : tft.color565(5, 8, 13); }
uint16_t C_HOT()   { return sumi() ? tft.color565(27, 24, 20) : tft.color565(235, 250, 255); }
uint16_t C_ALERT() { return sumi() ? tft.color565(194, 58, 43) : tft.color565(255, 122, 26); }
uint16_t C_ACC()   { return sumi() ? tft.color565(27, 24, 20) : tft.color565(62, 230, 255); }
uint16_t C_SEAL()  { return sumi() ? tft.color565(194, 58, 43) : C_ACC(); }   // печать 天眼: киноварь в SUMI

uint16_t heatV(float x) { return sumi() ? ramp(HEAT_SUMI, 4, x) : ramp(HEAT, 4, x); }
uint16_t heat(int rssi) { return heatV((rssi + 100) / 60.0f); }

// ---------- текст ----------
static const GFXfont* fontFor(int size) {
  switch (size) {
    case 2: return &STM14;
    case 4: return &RAJ24;
    case 6: return &RAJ44;
    case 3: return &JBM14;                           // кириллица, мелкий
    case 5: return &JBM19;                           // кириллица, крупный
    default: return nullptr;                         // 1 и 7 — встроенные
  }
}

// «тусклее»: смешать с фоном (в TENGAN фон почти чёрный — то же, что затемнить; в SUMI — к бумаге)
uint16_t scale565(uint16_t c, int pct) {
  pct = constrain(pct, 0, 100);
  uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  if (!sumi()) return tft.color565(r * pct / 100, g * pct / 100, b * pct / 100);
  const int br = 236, bgc = 228, bb = 212;
  return tft.color565(br + (r - br) * pct / 100, bgc + (g - bgc) * pct / 100, bb + (b - bb) * pct / 100);
}

void txt(const char* s, int x, int y, int size) {
  const GFXfont* f = fontFor(size);
  if (f) { spr.setFreeFont(f); spr.drawString(s, x, y); spr.setFreeFont(nullptr); }
  else   { spr.setFreeFont(nullptr); spr.drawString(s, x, y, size); }
}

void txtNum(long n, int x, int y, int size) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", n);
  txt(buf, x, y, size);
}

int txtW(const char* s, int size) {
  const GFXfont* f = fontFor(size);
  if (!f) return spr.textWidth(s, size);
  spr.setFreeFont(f);
  int w = spr.textWidth(s);
  spr.setFreeFont(nullptr);
  return w;
}

// ---------- значки ----------
void drawGlyph(int x, int y, char ch, uint16_t col, bool inverted) {
  int x0 = x - GLYPH_W / 2, y0 = y - GLYPH_H / 2;
  char s[2] = { ch, 0 };
  spr.setFreeFont(nullptr);
  spr.setTextDatum(MC_DATUM);
  if (inverted) {
    spr.fillRect(x0, y0, GLYPH_W, GLYPH_H, col);
    spr.setTextColor(C_BG());
  } else {
    spr.fillRect(x0, y0, GLYPH_W, GLYPH_H, C_BG());
    spr.drawRect(x0, y0, GLYPH_W, GLYPH_H, col);
    spr.setTextColor(col);
  }
  spr.drawString(s, x + 1, y + 1, 1);
}

void drawMarker(int x, int y, uint16_t col, bool alert) {
  if (alert) {
    uint16_t a = C_ALERT();
    spr.fillTriangle(x, y - 6, x + 6, y, x, y + 6, a);
    spr.fillTriangle(x, y - 6, x - 6, y, x, y + 6, a);
    spr.drawCircle(x, y, 9, a);
    return;
  }
  spr.drawLine(x, y - 4, x + 4, y, col); spr.drawLine(x + 4, y, x, y + 4, col);
  spr.drawLine(x, y + 4, x - 4, y, col); spr.drawLine(x - 4, y, x, y - 4, col);
  spr.drawPixel(x, y, col);
}

// ---------- кадр ----------
// уголки-скобки по краям экрана — «рамка прицела»
static void corners() {
  int W = spr.width(), H = spr.height(), L = 10;
  uint16_t c = C_ACC();
  spr.drawFastHLine(0, 0, L, c);         spr.drawFastVLine(0, 0, L, c);
  spr.drawFastHLine(W - L, 0, L, c);     spr.drawFastVLine(W - 1, 0, L, c);
  spr.drawFastHLine(0, H - 1, L, c);     spr.drawFastVLine(0, H - L, L, c);
  spr.drawFastHLine(W - L, H - 1, L, c); spr.drawFastVLine(W - 1, H - L, L, c);
}

int g_uiPages = 0, g_uiPage = 0;

void present() {
  corners();
  if (g_uiPages > 1) {                                // точки-страницы: под линией шапки справа (там всегда свободно)
    int W = spr.width(), x0 = W - UI_M - 4 - g_uiPages * 8 + 2;
    for (int p = 0; p < g_uiPages; p++)
      spr.fillRect(x0 + p * 8, 21, 5, 2, p == g_uiPage ? C_ACC() : UI_LINE());
  }
  spr.pushSprite(0, 0);
}
