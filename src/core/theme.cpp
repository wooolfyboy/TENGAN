// ============================================================
//  theme.cpp — внешний вид: цвета, шрифты, эффекты экрана
// ============================================================
#include <math.h>
#include "core/theme.h"
#include "core/display.h"
#include "core/fonts/STM14.h"
#include "core/fonts/STM26.h"

int g_theme = THEME_PHOSPHOR;

const char* themeName(int t) { return t == THEME_CLASSIC ? "classic" : "phosphor"; }

// Палитра люминофора: яркость 0..255 -> оттенок зелёного. Самые яркие места
// «перегорают» в бело-зелёный, как настоящий люминофор.
static uint16_t s_lut[256];
static bool     s_lutReady = false;

static void buildLut() {
  for (int i = 0; i < 256; i++) {
    float v = powf(i / 255.0f, 0.85f);           // чуть приподнять тёмные тона
    float v3 = v * v * v;
    uint8_t r = (uint8_t)(255 * 0.75f * v3);
    uint8_t g = (uint8_t)(255 * v);
    uint8_t b = (uint8_t)(255 * (0.40f * v + 0.20f * v3));
    s_lut[i] = tft.color565(r, g, b);
  }
  s_lutReady = true;
}

void themeApply() { if (!s_lutReady) buildLut(); }

uint16_t C(uint8_t r, uint8_t g, uint8_t b) {
  if (g_theme == THEME_CLASSIC) return tft.color565(r, g, b);
  if (!s_lutReady) buildLut();
  return s_lut[(77 * r + 150 * g + 29 * b) >> 8];   // яркость по восприятию глаза
}
uint16_t C_BG()  { return g_theme == THEME_CLASSIC ? TFT_BLACK : tft.color565(0, 6, 2); }
uint16_t C_HOT() { return C(255, 255, 255); }

// ---------- текст ----------
static const GFXfont* fontFor(int size) {
  if (g_theme == THEME_CLASSIC) return nullptr;
  if (size == 2) return &STM14;
  if (size == 4) return &STM26;
  return nullptr;                                  // 1 и 7 — встроенные (пиксельный и цифровой)
}

static uint16_t dim565(uint16_t c, int pct) {
  uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return tft.color565(r * pct / 100, g * pct / 100, b * pct / 100);
}

static void drawOne(const char* s, int x, int y, int size, const GFXfont* f) {
  if (f) { spr.setFreeFont(f); spr.drawString(s, x, y); spr.setFreeFont(nullptr); }
  else   { spr.setFreeFont(nullptr); spr.drawString(s, x, y, size); }
}

void txt(const char* s, int x, int y, int size) {
  const GFXfont* f = fontFor(size);
  if (g_theme == THEME_PHOSPHOR && size >= 4) {
    // свечение: тусклый «ореол» на пиксель вокруг крупного текста
    uint32_t fg = spr.textcolor, bg = spr.textbgcolor;
    spr.setTextColor(dim565(fg, 30));              // без фона — ореол не затирает соседей
    drawOne(s, x - 1, y, size, f);
    drawOne(s, x + 1, y, size, f);
    drawOne(s, x, y - 1, size, f);
    drawOne(s, x, y + 1, size, f);
    spr.textcolor = fg; spr.textbgcolor = bg;      // вернуть цвет как был
  }
  drawOne(s, x, y, size, f);
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

// ---------- «жар» ----------
struct Stop { float x; uint8_t r, g, b; };
static const Stop HEAT_PHOSPHOR[] = {
  { 0.00f,   4,  34,  14 }, { 0.30f,  31, 156,  84 }, { 0.55f, 200, 255,  74 },
  { 0.80f, 255, 174,  46 }, { 1.00f, 255, 243, 214 },
};
static const Stop HEAT_CLASSIC[] = {
  { 0.00f,  40,  60, 120 }, { 0.30f,  70, 200, 130 }, { 0.55f, 240, 220,  70 },
  { 0.80f, 240, 140,  50 }, { 1.00f, 235,  70,  60 },
};

uint16_t heatV(float x) {
  const Stop* s = g_theme == THEME_CLASSIC ? HEAT_CLASSIC : HEAT_PHOSPHOR;
  x = constrain(x, 0.0f, 1.0f);
  int i = 0;
  while (i < 3 && x > s[i + 1].x) i++;
  float k = (x - s[i].x) / (s[i + 1].x - s[i].x);
  return tft.color565(s[i].r + (s[i + 1].r - s[i].r) * k,
                      s[i].g + (s[i + 1].g - s[i].g) * k,
                      s[i].b + (s[i + 1].b - s[i].b) * k);
}
uint16_t heat(int rssi) { return heatV((rssi + 100) / 60.0f); }

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

// ---------- кадр ----------
// Полосы строк: каждая вторая строка темнее, как на старом экране.
static void scanlines() {
  if (spr.getColorDepth() != 16) return;
  uint16_t* p = (uint16_t*)spr.getPointer();
  if (!p) return;
  int W = spr.width(), H = spr.height();
  for (int y = 1; y < H; y += 2) {
    uint16_t* row = p + y * W;
    for (int x = 0; x < W; x++) {
      uint16_t v = row[x];
      if (!v) continue;
      v = (v >> 8) | (v << 8);                     // в спрайте байты переставлены
      uint16_t r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
      r = r * 5 / 8; g = g * 5 / 8; b = b * 5 / 8;   // ~60 % яркости
      v = (r << 11) | (g << 5) | b;
      row[x] = (v >> 8) | (v << 8);
    }
  }
}

void present() {
  if (g_theme == THEME_PHOSPHOR) scanlines();
  spr.pushSprite(0, 0);
}
