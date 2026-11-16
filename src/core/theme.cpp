// ============================================================
//  theme.cpp — дизайн-система «Tactical HUD»
// ============================================================
#include "core/theme.h"
#include "core/perf.h"
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

static uint16_t s_lut[256];
static bool     s_lutOk = false;

uint16_t C(uint8_t r, uint8_t g, uint8_t b) {
  if (!s_lutOk) { for (int i = 0; i < 256; i++) s_lut[i] = ramp(COOL, 6, i / 255.0f); s_lutOk = true; }
  return s_lut[(77 * r + 150 * g + 29 * b) >> 8];    // яркость по восприятию глаза
}
// TC — «буквальный» цвет и C_SEAL — цвет печати 天眼: точки, через которые новая тема сможет
// перекрасить экраны (сейчас тема одна — DENKO, и это просто цвет как есть / акцент)
uint16_t TC(uint8_t r, uint8_t g, uint8_t b) { return tft.color565(r, g, b); }
uint16_t C_BG()    { return tft.color565(5, 8, 13); }
uint16_t C_HOT()   { return tft.color565(235, 250, 255); }
uint16_t C_ALERT() { return tft.color565(255, 122, 26); }
uint16_t C_ACC()   { return tft.color565(62, 230, 255); }
uint16_t C_SEAL()  { return C_ACC(); }

uint16_t heatV(float x) { return ramp(HEAT, 4, x); }
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

// «тусклее»: затемнить (фон почти чёрный)
uint16_t scale565(uint16_t c, int pct) {
  pct = constrain(pct, 0, 100);
  uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return tft.color565(r * pct / 100, g * pct / 100, b * pct / 100);
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

// Вывод на экран только изменившихся строк. Кадр целиком — 320×170×2 = 108 КБ, по шине SPI
// это ~22 мс; на спокойных экранах (меню, настройки, списки) меняются часы, цифры, пара полосок —
// отправляем только эти полосы строк. Строку узнаём по «отпечатку» (хэш FNV по 32-битным словам,
// ~1 мс на кадр), раз в 2 с — полный кадр на всякий случай.
static uint32_t s_rowHash[400];
static uint32_t s_fullMs = 0;
static bool     s_rowsValid = false;
static void pushChanged() {
  const int W = spr.width(), H = spr.height();
  uint16_t* buf = (uint16_t*)spr.getPointer();
  uint32_t now = millis();
  if (!buf || spr.getColorDepth() != 16 || H > 400 || (W & 1) || !s_rowsValid || now - s_fullMs > 2000) {
    if (buf && spr.getColorDepth() == 16 && H <= 400 && !(W & 1)) {     // запомнить отпечатки для следующего кадра
      for (int y = 0; y < H; y++) {
        const uint32_t* p = (const uint32_t*)(buf + y * W); uint32_t h = 2166136261u;
        for (int i = 0; i < W / 2; i++) h = (h ^ p[i]) * 16777619u;
        s_rowHash[y] = h;
      }
      s_rowsValid = true;
    }
    s_fullMs = now;
    spr.pushSprite(0, 0);
    perfRows(H, H);
    return;
  }
  static bool ch[400];
  for (int y = 0; y < H; y++) {
    const uint32_t* p = (const uint32_t*)(buf + y * W); uint32_t h = 2166136261u;
    for (int i = 0; i < W / 2; i++) h = (h ^ p[i]) * 16777619u;
    ch[y] = h != s_rowHash[y]; s_rowHash[y] = h;
  }
  // полосы изменившихся строк; разрывы до 6 строк склеиваем (меньше команд экрану)
  int pushed = 0;
  for (int y = 0; y < H; ) {
    if (!ch[y]) { y++; continue; }
    int y1 = y, gap = 0, end = y;
    while (y1 < H && gap <= 6) { if (ch[y1]) { end = y1; gap = 0; } else gap++; y1++; }
    spr.pushSprite(0, y, 0, y, W, end - y + 1);
    pushed += end - y + 1;
    y = end + 1;
  }
  perfRows(pushed, H);
}

void present() {
  corners();
  if (g_uiPages > 1) {                                // точки-страницы: под линией шапки справа (там всегда свободно)
    int W = spr.width(), x0 = W - UI_M - 4 - g_uiPages * 8 + 2;
    for (int p = 0; p < g_uiPages; p++)
      spr.fillRect(x0 + p * 8, 21, 5, 2, p == g_uiPage ? C_ACC() : UI_LINE());
  }
  pushChanged();
}
