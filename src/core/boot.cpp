// ============================================================
//  boot.cpp — power-on splash (~2.7 s)
//
//  A radar unfolds: crosshair and corners fly out, rings expand, a sweep
//  with afterglow lights up targets, the title "decrypts" on top, then a
//  bright scan line wipes the screen top to bottom, revealing the menu.
// ============================================================
#include "core/boot.h"
#include "core/lang.h"
#include "core/board.h"
#include "core/seal.h"      // 天眼 seal
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/version.h"

static const uint32_t BOOT_MS = 2700;

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float easeOut(float x) { x = clamp01(x); return 1 - (1 - x) * (1 - x) * (1 - x); }
static float seg(uint32_t t, uint32_t a, uint32_t b) { return t <= a ? 0 : t >= b ? 1 : (float)(t - a) / (b - a); }
static uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }

// radar targets: angle (degrees) and distance (fraction of radius)
static const struct { int16_t deg; uint8_t r; } BLIPS[] = {
  { 35, 72 }, { 80, 38 }, { 140, 88 }, { 200, 55 }, { 250, 80 }, { 310, 30 }, { 335, 64 },
};

void bootFrame(uint32_t t) {
  const int W = spr.width(), H = spr.height();
  const int cx = W / 2, cy = H / 2;
  const uint16_t bg = C_BG(), acc = C_ACC(), hot = C_HOT(), dim = UI_DIM();
  spr.fillSprite(bg);

  // 1. crosshair grows from the center
  float g = easeOut(seg(t, 0, 450));
  int hx = (int)(g * W / 2), hy = (int)(g * H / 2);
  spr.drawFastHLine(cx - hx, cy, hx * 2, dim);
  spr.drawFastVLine(cx, cy - hy, hy * 2, dim);
  for (int x = cx - hx; x <= cx + hx; x += 20) spr.drawFastVLine(x, cy - 2, 5, dim);   // ticks

  // 2. rings expand in turn; the outer one has ticks
  const int RING[] = { 20, 40, 60, 80 };
  for (int i = 0; i < 4; i++) {
    float e = easeOut(seg(t, 80 + i * 110, 420 + i * 110));
    if (e > 0) spr.drawCircle(cx, cy, (int)(RING[i] * e), i == 3 ? acc : dim);
  }
  float ticks = seg(t, 500, 800);
  for (int a = 0; a < 360 * ticks; a += 10) {
    float r = a * DEG_TO_RAD;
    int l = (a % 30 == 0) ? 6 : 3;
    spr.drawLine(cx + cosf(r) * 80, cy + sinf(r) * 80, cx + cosf(r) * (80 + l), cy + sinf(r) * (80 + l), a % 30 ? dim : acc);
  }

  // 3. sweep with afterglow + targets it hits
  if (t > 300 && t < 2400) {
    float sweep = (t - 300) * 0.48f;                       // degrees
    float fade = 1 - seg(t, 2050, 2400);
    for (int k = 36; k >= 0; k--) {                          // tail: 36 lines, 2° apart
      float a = (sweep - k * 2) * DEG_TO_RAD;
      float v = (1 - k / 37.0f) * fade;
      spr.drawLine(cx, cy, cx + cosf(a) * 80, cy + sinf(a) * 80, heatV(v * v * 0.85f));
    }
    float a = sweep * DEG_TO_RAD;
    spr.drawLine(cx, cy, cx + cosf(a) * 80, cy + sinf(a) * 80, hot);
    for (auto& b : BLIPS) {
      float since = fmodf(sweep - b.deg + 720, 360);         // degrees since the sweep passed
      if (sweep < b.deg) continue;                           // sweep hasn't reached the target yet
      float v = clamp01(1 - since / 300) * fade;
      if (v <= 0.02f) continue;
      float r = b.deg * DEG_TO_RAD;
      int x = cx + cosf(r) * b.r * 0.8f, y = cy + sinf(r) * b.r * 0.8f;
      spr.fillCircle(x, y, 2, heatV(0.3f + 0.7f * v));
      if (since < 40) spr.drawCircle(x, y, 3 + (int)(since / 6), heatV(v));   // flash on detection
    }
  }
  spr.fillCircle(cx, cy, 2, acc);

  // 4. corners fly from the center to the screen edges
  float c = easeOut(seg(t, 0, 500));
  const int L = 14;
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sy = -1; sy <= 1; sy += 2) {
      int x = cx + (int)(sx * (W / 2 - 3) * c), y = cy + (int)(sy * (H / 2 - 3) * c);
      spr.drawFastHLine(sx < 0 ? x : x - L + 1, y, L, acc);
      spr.drawFastVLine(x, sy < 0 ? y : y - L + 1, L, acc);
    }

  // 5. title bar opens from the center; letters "decrypt"
  float bar = easeOut(seg(t, 1000, 1280));
  if (bar > 0) {
    int bw = (int)(W * 0.86f * bar), bx = cx - bw / 2, by = cy - 33, bh = 66;
    spr.fillRect(bx, by, bw, bh, bg);
    spr.drawFastHLine(bx, by, bw, acc);
    spr.drawFastHLine(bx, by + bh - 1, bw, acc);
    if (bar > 0.98f) {
      spr.drawFastVLine(bx, by, 6, acc);           spr.drawFastVLine(bx + bw - 1, by, 6, acc);
      spr.drawFastVLine(bx, by + bh - 6, 6, acc);  spr.drawFastVLine(bx + bw - 1, by + bh - 6, 6, acc);
    }
  }
  // title + 天眼 seal to its right, the whole block centered
  static const char TITLE[] = DEVICE_NAME;
  const int SEAL_GAP = 14;
  const int titleW = txtW(TITLE, 6), blockX = cx - (titleW + SEAL_GAP + SEAL_W) / 2;
  const int titleCx = blockX + titleW / 2;
  if (t >= 1120) {
    static const char POOL[] = "ABCDEFGHKLMNPRSTVXZ0123456789";
    const int n = sizeof(TITLE) - 1;
    int x = blockX;
    spr.setTextDatum(TL_DATUM);
    for (int i = 0; i < n; i++) {
      char ch[2] = { TITLE[i], 0 };
      int w = txtW(ch, 6);
      uint32_t lockT = 1150 + i * 55;
      if (TITLE[i] != ' ' && t >= 1120 + i * 25) {
        if (t < lockT) {                                   // still cycling random characters
          char r[2] = { POOL[hash32(i * 977 + t / 45) % (sizeof(POOL) - 1)], 0 };
          spr.setTextColor(acc);
          spr.setTextDatum(TC_DATUM);
          txt(r, x + w / 2, cy - 27, 6);
          spr.setTextDatum(TL_DATUM);
        } else {
          spr.setTextColor(t < lockT + 90 ? acc : hot);    // just locked in: highlighted
          txt(ch, x, cy - 27, 6);
        }
      }
      x += w;
    }
  }
  if (t >= 1700) {                                         // subtitle is typed out
    const char* SUB = L("// AIRWAVE ANALYZER", "// АНАЛИЗАТОР ЭФИРА");
    const int SUBN = (int)strlen(SUB);
    int k = min(SUBN, (int)((t - 1700) / 14));
    while (k > 0 && k < SUBN && ((uint8_t)SUB[k] & 0xC0) == 0x80) k--;   // don't split a UTF-8 character
    char buf[64]; memcpy(buf, SUB, k); buf[k] = 0;
    if (((t / 120) & 1) && k < SUBN) strcat(buf, "_");
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(UI_LABEL());
    txt(buf, titleCx, cy + 20, 1);
    spr.setTextDatum(BR_DATUM);                        // firmware version, small in the corner
    spr.setTextColor(UI_DIM());
    txt("v" FW_VERSION, W - 6, H - 4, 1);
  }
  // seal "stamps" with a hit: flash and expanding frame, then settles
  if (t >= 1640) {
    const int sx = blockX + titleW + SEAL_GAP, sy = cy - SEAL_H / 2;
    float k = seg(t, 1640, 1820);                          // 0 = hit, 1 = settled
    uint16_t ink = k < 0.25f ? hot : C_SEAL();
    for (int y = 0; y < SEAL_H; y++)
      for (int xx = 0; xx < SEAL_W; xx++)
        if ((SEAL_BITS[y * (SEAL_W / 8) + xx / 8] >> (7 - xx % 8)) & 1) spr.drawPixel(sx + xx, sy + y, ink);
    if (k < 1) {
      int g = 2 + (int)(k * 14);
      spr.drawRect(sx - g, sy - g, SEAL_W + 2 * g, SEAL_H + 2 * g, scale565(hot, (int)(90 * (1 - k))));
    }
  }

  // 6. bright scan line wipes everything top to bottom
  float wipe = seg(t, 2350, BOOT_MS);
  if (wipe > 0) {
    int y = (int)(easeOut(wipe) * (H + 6));
    spr.fillRect(0, 0, W, min(y, H), bg);
    spr.drawFastHLine(0, y, W, hot);
    spr.drawFastHLine(0, y - 1, W, acc);
    spr.drawFastHLine(0, y + 1, W, acc);
    spr.drawFastHLine(0, y - 3, W, dim);
  }
}

void bootScreen() {
  uint32_t t0 = millis();
  for (;;) {
    uint32_t t = millis() - t0;
    if (t > BOOT_MS) break;
    bootFrame(t);
    spr.pushSprite(0, 0);
  }
}
