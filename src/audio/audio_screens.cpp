// ============================================================
//  audio_screens.cpp — Sound section screens
//
//  BARS (spectrum) · REACTOR (animation) · LEVEL (meter) · SCOPE (waveform)
// ============================================================
#include "audio/audio_model.h"
#include "core/lang.h"
#include "core/timeutil.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/settings.h"
#include "core/ui.h"
#include "core/psram.h"

// top right: sound source MIC / PC (in auto mode: AUTO MIC / AUTO PC)
static const char* src(const char* right) {
  static char buf[56];
  const char* tag = g_auFromPc ? L("PC", "ПК") : L("MIC", "МИК");            // current source (chosen in settings)
  snprintf(buf, sizeof(buf), "%s  %s", tag, right ? right : "");
  return buf;
}

static void hzStr(float hz, char* out, size_t n) {
  if (hz >= 1000) snprintf(out, n, L("%.1f kHz", "%.1f кГц"), hz / 1000.0f);
  else            snprintf(out, n, L("%d Hz", "%d Гц"), (int)hz);
}

// ============================================================
//  BARS — 32 bands with peak caps and reflection
// ============================================================
void drawAuBars(uint32_t now) {
  int W = spr.width();
  char buf[40], hz[16];
  spr.fillSprite(C_BG());
  hzStr(g_au.domHz, hz, sizeof(hz));
  snprintf(buf, sizeof(buf), "%d dBfs  %s", (int)g_au.level, g_au.energy > 0.15f ? hz : "--");
  uiHeader(L("Spectrum", "Спектр"), src(buf));

  const int x0 = UI_M + 8, base = 112, hMax = 84, step = (W - 2 * x0) / AU_BANDS;
  for (int k = 0; k < AU_BANDS; k++) {
    int x = x0 + k * step, bw = step - 2;
    int h = (int)(g_au.bands[k] * hMax);
    // 3 px segments: cooler at the bottom, hotter at the top
    for (int y = 0; y + 3 <= h; y += 4) {
      uint16_t c = heatV(0.25f + 0.75f * (float)y / hMax);
      spr.fillRect(x, base - y - 3, bw, 3, c);
      if (y < 28) spr.fillRect(x, base + 3 + y, bw, 2, scale565(c, 22 - y * 3 / 4));   // reflection
    }
    int cy = base - (int)(g_au.caps[k] * hMax) - 5;
    spr.fillRect(x, cy, bw, 2, C_HOT());
  }
  spr.drawFastHLine(x0, base + 1, step * AU_BANDS - 2, UI_LINE());
  // frequency labels
  static const float marks[] = { 100, 500, 1000, 4000 };
  static const char* const lbl[] = { "100", "500", "1K", "4K" };
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(UI_DIM());
  for (int i = 0; i < 4; i++) {
    float pos = logf(marks[i] / 60.0f) / logf(8000.0f / 60.0f);
    txt(lbl[i], x0 + (int)(pos * step * AU_BANDS), UI_BOTTOM - 4, 1);
  }
  uiFooter(L("wheel: screens", "колесо: экраны"), L("side: back", "кнопка: назад"));
  present();
}

// ============================================================
//  REACTOR — a ring of rays pulses and rotates to the sound
// ============================================================
void drawReactor(uint32_t now) {
  int W = spr.width();
  spr.fillSprite(C_BG());
  uiHeader(L("Reactor", "Реактор"), src(levelWord(g_au.level)), g_au.level >= -25);
  int cx = W / 2, cy = 90;
  float e = g_au.energy;
  float rot = now * 0.0004f + e * 0.8f;
  int r0 = 24 + (int)(e * 14);

  // outer ring with ticks, rotates the opposite way
  for (int k = 0; k < 48; k++) {
    float a = -now * 0.0002f + k * 2 * PI / 48;
    int r1 = 70, r2 = (k % 4 == 0) ? 64 : 67;
    spr.drawLine(cx + cosf(a) * r1, cy + sinf(a) * r1, cx + cosf(a) * r2, cy + sinf(a) * r2, UI_LINE());
  }
  // rays: 64 of them, spectrum mirrored around the circle
  for (int i = 0; i < 64; i++) {
    int band = i < 32 ? i : 63 - i;
    float v = g_au.bands[band];
    float a = rot + i * 2 * PI / 64;
    int len = 3 + (int)(v * 34);
    float ca = cosf(a), sa = sinf(a);
    spr.drawWideLine(cx + ca * (r0 + 3), cy + sa * (r0 + 3), cx + ca * (r0 + 3 + len), cy + sa * (r0 + 3 + len),
                     2.0f, heatV(0.25f + 0.75f * v), C_BG());
  }
  // core
  spr.drawCircle(cx, cy, r0, C_ACC());
  spr.drawCircle(cx, cy, r0 - 4, scale565(C_ACC(), 40));
  spr.fillSmoothCircle(cx, cy, max(3, (int)(r0 * 0.35f + e * 8)), heatV(0.5f + 0.5f * e), C_BG());
  // orbit dots
  for (int k = 0; k < 3; k++) {
    float a = now * (0.0015f + k * 0.0007f) + k * 2.1f;
    int rr = 48 + k * 8;
    spr.fillCircle(cx + cosf(a) * rr, cy + sinf(a) * rr, 2, k == 0 ? C_ACC() : UI_LABEL());
  }
  // corner readouts
  char buf[24];
  snprintf(buf, sizeof(buf), "%d", (int)g_au.level);
  uiReadout(UI_M + 8, UI_TOP + 4, "dBfs", buf, C_HOT(), 4);
  hzStr(g_au.domHz, buf, sizeof(buf));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(UI_DIM());
  txt(L("DOMINANT", "ОСНОВНАЯ"), W - UI_M - 8, UI_TOP + 4, 1);
  spr.setTextColor(e > 0.15f ? C_HOT() : UI_DIM());
  txt(e > 0.15f ? buf : "--", W - UI_M - 8, UI_TOP + 14, 2);
  uiFooter(L("wheel: screens", "колесо: экраны"), L("side: back", "кнопка: назад"));
  present();
}

// ============================================================
//  LEVEL — meter: number, scale with peak, one-minute history
// ============================================================
void drawLevel(uint32_t now) {
  int W = spr.width();
  char buf[32];
  spr.fillSprite(C_BG());
  bool loud = g_au.level >= -25;
  uiHeader(L("Noise meter", "Шумомер"), src(levelWord(g_au.level)), loud);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(loud ? C_ALERT() : C_HOT());
  txtNum((long)lroundf(g_au.level), UI_M + 8, UI_TOP + 2, 6);
  spr.setTextColor(UI_DIM());
  txt(L("DBFS  (0 = MIC MAXIMUM)", "ДБFS  (0 = МАКСИМУМ МИКРОФОНА)"), UI_M + 8, UI_TOP + 50, 1);
  snprintf(buf, sizeof(buf), "%d", (int)g_au.peak);
  uiReadout(150, UI_TOP + 4, L("peak", "пик"), buf, C_HOT());
  snprintf(buf, sizeof(buf), "%d", (int)g_auMax);
  uiReadout(204, UI_TOP + 4, L("max", "макс"), buf, C_HOT());
  snprintf(buf, sizeof(buf), "%d", (int)g_auAvg);
  uiReadout(254, UI_TOP + 4, L("avg", "сред"), buf, C_HOT());

  // segmented scale -80..0 dBFS; above -25 = alarm
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

  // one-minute history
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
  txt(L("1 MIN", "1 МИН"), gx + 5, gy + 3, 1);
  uiFooter(L("wheel: screens  ·  click: reset max", "колесо: экраны · нажать: сброс"), L("side: back", "кнопка: назад"));
  present();
}

// ============================================================
//  SCOPE — oscilloscope: waveform triggered on a zero crossing
// ============================================================
void drawWave(uint32_t now) {
  int W = spr.width();
  char buf[40];                                  // "AUTO GAIN X16000  RAW 32767  L" — 30 characters
  spr.fillSprite(C_BG());
  hzStr(g_au.domHz, buf, sizeof(buf));
  uiHeader(L("Waveform", "Волна"), src(g_au.energy > 0.15f ? buf : L("listening", "слушаю")));
  int gx = UI_M + 4, gy = UI_TOP, gw = W - 2 * UI_M - 8, gh = UI_BOTTOM - UI_TOP + 2, cy = gy + gh / 2;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  for (int x = gx + 4; x < gx + gw - 4; x += 4) spr.drawPixel(x, cy, UI_LINE());
  for (int k = 1; k < 8; k++) for (int y = gy + 4; y < gy + gh - 4; y += 4) spr.drawPixel(gx + k * gw / 8, y, UI_LINE());

  // trigger: first rising zero crossing, so the picture doesn't drift
  int start = 0;
  for (int i = 1; i < AU_WAVE / 2; i++) if (g_au.wave[i - 1] < 0 && g_au.wave[i] >= 0) { start = i; break; }
  int span = AU_WAVE - AU_WAVE / 2;
  int amp = 200;
  for (int i = 0; i < AU_WAVE; i++) amp = max(amp, abs((int)g_au.wave[i]));
  float k = (gh / 2 - 6) / (float)amp;                         // auto-gain
  int px = -1, py = -1;
  for (int i = 0; i < span; i++) {
    int x = gx + 3 + (gw - 6) * i / (span - 1);
    int y = cy - (int)(g_au.wave[start + i] * k);
    if (px >= 0) {
      spr.drawLine(px, py + 1, x, y + 1, scale565(C_ACC(), 35));  // glow
      spr.drawLine(px, py, x, y, C_ACC());
    }
    px = x; py = y;
  }
  snprintf(buf, sizeof(buf), L("AUTO GAIN X%d", "АВТОУСИЛЕНИЕ X%d"), max(1, 16000 / amp));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(buf, gx + 5, gy + 3, 1);
  uiFooter(L("wheel: screens", "колесо: экраны"), L("side: back", "кнопка: назад"));
  present();
}

void drawMicMissing() {
  char buf[48];
  spr.fillSprite(C_BG());
  if (g_micDiag.disabled) {
    uiHeader(L("Sound", "Звук"), L("microphone off", "микрофон выключен"));
    uiEmpty(L("microphone off", "микрофон выключен"), L("turn it on in Settings > Sound", "включите: Настройки > Звук"));
    uiFooter("", L("side: back", "кнопка: назад"));
    present();
    return;
  }
  const char* const stages[] = { L("not started", "не запущен"), L("no free channel", "нет канала"), L("config rejected", "отказ настроек"),
                                         L("start failed", "не стартовал"), L("running", "работает") };
  uiHeader(L("Microphone", "Микрофон"), g_micDiag.stage == 4 ? L("no sound data", "нет данных") : L("driver error", "ошибка драйвера"), true);
  int x = UI_M + 8, y = UI_TOP + 4;
  uiReadout(x, y, L("driver", "драйвер"), stages[constrain(g_micDiag.stage, 0, 4)], g_micDiag.stage == 4 ? C_HOT() : C_ALERT());
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_micDiag.blocks);
  uiReadout(x, y + 30, L("blocks", "блоки"), buf, g_micDiag.blocks ? C_HOT() : C_ALERT());
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_micDiag.timeouts);
  uiReadout(x + 90, y + 30, L("timeouts", "таймауты"), buf, C_HOT());
  snprintf(buf, sizeof(buf), "%d", g_micDiag.rawPeak);
  uiReadout(x + 180, y + 30, L("raw peak", "пик"), buf, C_HOT());
  uiReadout(x, y + 60, L("channel", "канал"), g_micDiag.slot ? L("right", "правый") : L("left", "левый"), C_HOT());
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(L("THE MICROPHONE IS NOT RESPONDING", "МИКРОФОН НЕ ОТВЕЧАЕТ"), x, y + 96, 1);
  txt(L("RESTART THE DEVICE", "ПЕРЕЗАПУСТИТЕ УСТРОЙСТВО"), x, y + 108, 1);
  uiFooter("", L("side: back", "кнопка: назад"));
  present();
}

// ============================================================
//  TERRAIN — 3D sound mountains. Each frame the spectrum becomes a new ridge
//  in front, older ones drift to the horizon. Bass on the left, treble on the right.
//  The ridge is smoothed and self-normalized (loud sound doesn't turn it into a wall),
//  with rocky noise added. Mountains are filled: dark slopes, light peaks, white snow caps.
// ============================================================
#define TR_ROWS 24
#define TR_COLS 52
static float    (*s_trH)[TR_COLS] = nullptr;   // heights (0 = newest ridge); ~5 KB in external memory
static int      s_trHead = 0;
static uint32_t s_trLast = 0, s_trSeq = 0;
static float    s_trTop = 0.3f;            // auto-gain just for the mountains

static float rock(uint32_t row, int col) {   // "rockiness": noise tied to the ridge
  uint32_t x = row * 73856093u ^ (uint32_t)col * 19349663u;
  x ^= x >> 13; x *= 0x5bd1e995; x ^= x >> 15;
  return (x & 0xFFFF) / 65535.0f;
}

static void terrainPush() {
  float raw[TR_COLS], sm[TR_COLS];
  for (int c = 0; c < TR_COLS; c++) {                  // bands -> columns (interpolated)
    float k = c * (AU_BANDS - 1) / (float)(TR_COLS - 1);
    int k0 = (int)k; float f = k - k0;
    float b = g_au.bands[k0] * (1 - f) + g_au.bands[min(k0 + 1, AU_BANDS - 1)] * f;
    raw[c] = b * b;                                    // contrast: quiet bands lower
  }
  for (int pass = 0; pass < 2; pass++) {               // smooth: sharp spikes -> slopes
    for (int c = 0; c < TR_COLS; c++)
      sm[c] = (raw[max(c - 1, 0)] + 2 * raw[c] + raw[min(c + 1, TR_COLS - 1)]) * 0.25f;
    memcpy(raw, sm, sizeof(raw));
  }
  float mx = 0;
  for (int c = 0; c < TR_COLS; c++) mx = max(mx, raw[c]);
  s_trTop = max(mx, s_trTop * 0.985f);                 // fast up, slow down
  s_trTop = max(s_trTop, 0.12f);                       // don't amplify silence
  float loud = constrain(g_au.energy * 1.6f, 0.0f, 1.0f);
  s_trHead = (s_trHead + TR_ROWS - 1) % TR_ROWS;
  const float* prev = s_trH[(s_trHead + 1) % TR_ROWS];
  float* row = s_trH[s_trHead];
  s_trSeq++;
  for (int c = 0; c < TR_COLS; c++) {
    float v = raw[c] / s_trTop;                        // ridge shape, 0..1
    v = 1.0f - expf(-2.2f * v);                        // soft ceiling
    float edge = sinf(PI * (c + 0.5f) / TR_COLS);      // foothills toward the edges
    float h = v * (0.35f + 0.65f * loud) * (0.45f + 0.55f * edge);
    h += 0.05f + 0.07f * rock(s_trSeq, c) * (0.3f + h);   // rocks and roughness
    row[c] = prev[c] * 0.35f + h * 0.65f;               // smoothing over time
  }
}

void drawTerrain(uint32_t now) {
  if (!s_trH) s_trH = (float(*)[TR_COLS])psAlloc<float>(TR_ROWS * TR_COLS);
  if (!s_trH) return;
  const int W = spr.width();
  g_fastFrame = true;
  if (msSince(now, s_trLast) >= 45) { s_trLast = now; terrainPush(); }
  char buf[40], hz[16];
  spr.fillSprite(C_BG());
  hzStr(g_au.domHz, hz, sizeof(hz));
  snprintf(buf, sizeof(buf), "%d dBfs  %s", (int)g_au.level, g_au.energy > 0.15f ? hz : "--");

  const float F = 120, camH = 1.25f, hy = 44;
  const int cx = W / 2;
  auto proj = [&](float x, float y, float z, int& sx, int& sy) {
    sx = cx + (int)(x * F / z);
    sy = (int)(hy + (camH - y) * F / z);
  };
  // sky: the "sun" on the horizon glows with loudness
  int sunR = 12 + (int)(g_au.energy * 8);
  // glowing disc in the accent color
  uint16_t sunC = scale565(C_ACC(), 20 + (int)(g_au.energy * 35));
  spr.fillCircle(cx + 60, (int)hy - 2, sunR, sunC);
  spr.drawFastHLine(0, (int)hy, W, UI_LINE());

  // far ridge to near: each a filled silhouette with a light crest
  for (int r = TR_ROWS - 1; r >= 0; r--) {
    const float* h = s_trH[(s_trHead + r) % TR_ROWS];
    float z = 1.55f + r * 0.16f;
    int fade = 100 - r * 80 / TR_ROWS;                 // farther = dimmer (haze)
    int px = 0, py = 0, pbx = 0, pby = 0;
    float ph = 0;
    for (int c = 0; c < TR_COLS; c++) {
      float x = (c - (TR_COLS - 1) / 2.0f) / ((TR_COLS - 1) / 2.0f) * 2.6f;
      int sx, sy, bx, by;
      proj(x, h[c] * 1.15f, z, sx, sy);
      proj(x, 0, z, bx, by);
      if (c > 0) {
        float hm = (h[c] + ph) * 0.5f;
        uint16_t body = scale565(heatV(0.12f + 0.5f * hm), fade * 45 / 100);   // slope
        spr.fillTriangle(px, py, sx, sy, pbx, pby, body);
        spr.fillTriangle(sx, sy, bx, by, pbx, pby, body);
        uint16_t ridge = hm > 0.62f ? C_HOT() : heatV(0.3f + 0.7f * hm);   // high = "snow"
        spr.drawLine(px, py, sx, sy, scale565(ridge, fade));
      }
      px = sx; py = sy; pbx = bx; pby = by; ph = h[c];
    }
  }
  uiHeader(L("Terrain", "Рельеф"), src(buf), g_au.level >= -25);
  uiFooter(L("wheel: screens", "колесо: экраны"), L("side: back", "кнопка: назад"));
  present();
}

// ============================================================
//  XY — X-Y oscilloscope: the left channel moves the beam horizontally,
//  the right vertically. Mono draws a diagonal, stereo draws shapes.
//  With the mic (mono): phase portrait, X = signal, Y = same signal slightly later.
//  Phosphor: previous frames fade gradually.
// ============================================================
#define XY_TRAIL 4
static int16_t s_xyHist[XY_TRAIL][AU_XY][2];
static int     s_xyHead = 0, s_xyN = 0;

void drawXY(uint32_t now) {
  const int W = spr.width();
  g_fastFrame = true;
  spr.fillSprite(C_BG());
  const int side = UI_BOTTOM - UI_TOP - 2, x0 = (W - side) / 2, y0 = UI_TOP + 2;
  const int cx = x0 + side / 2, cy = y0 + side / 2, half = side / 2 - 4;
  uiFrame(x0, y0, side, side, UI_LINE());
  for (int k = 1; k < 8; k++) {                         // dot grid
    for (int t = 4; t < side - 4; t += 4) {
      spr.drawPixel(x0 + k * side / 8, y0 + t, UI_LINE());
      spr.drawPixel(x0 + t, y0 + k * side / 8, UI_LINE());
    }
  }
  // this frame's points (in screen coordinates)
  int16_t (&cur)[AU_XY][2] = s_xyHist[s_xyHead];
  for (int i = 0; i < AU_XY; i++) {
    int x, y;
    if (g_auFromPc) { x = g_auXY[i][0]; y = g_auXY[i][1]; }
    else {                                              // mic: phase portrait from the waveform
      int a = i * (AU_WAVE - 12) / AU_XY;
      x = g_au.wave[a] >> 8; y = g_au.wave[a + 10] >> 8;
    }
    cur[i][0] = cx + x * half / 128;
    cur[i][1] = cy - y * half / 128;
  }
  s_xyHead = (s_xyHead + 1) % XY_TRAIL;
  if (s_xyN < XY_TRAIL) s_xyN++;
  // old frames to new: dimmer -> brighter
  for (int f = s_xyN - 1; f >= 0; f--) {
    int slot = (s_xyHead - 1 - f + XY_TRAIL * 2) % XY_TRAIL;
    int16_t (&pts)[AU_XY][2] = s_xyHist[slot];
    uint16_t col = f == 0 ? C_HOT() : scale565(C_ACC(), 70 - f * 18);
    for (int i = 1; i < AU_XY; i++) spr.drawLine(pts[i - 1][0], pts[i - 1][1], pts[i][0], pts[i][1], col);
    if (f == 0) for (int i = 1; i < AU_XY; i++) spr.drawLine(pts[i - 1][0] + 1, pts[i - 1][1], pts[i][0] + 1, pts[i][1], scale565(C_ACC(), 60));
  }
  // axis labels
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(g_auFromPc ? L("L", "Л") : L("NOW", "СЕЙЧАС"), x0 + side + 6, cy - 4, 1);
  txt(g_auFromPc ? L("R", "П") : L("LATER", "ПОЗЖЕ"), x0 - 30, y0 + 2, 1);
  char buf[24];
  snprintf(buf, sizeof(buf), "%d dBfs", (int)g_au.level);
  spr.setTextColor(UI_LABEL());
  txt(buf, UI_M + 6, UI_BOTTOM - 12, 1);
  uiHeader(L("XY scope", "Осциллограф XY"), src(g_auFromPc ? (g_auXYStereo ? L("stereo", "стерео") : L("mono", "моно")) : L("mic phase", "фаза микрофона")));
  uiFooter(L("wheel: screens", "колесо: экраны"), L("side: back", "кнопка: назад"));
  present();
  (void)now;
}

// "laptop" selected but no audio arrives
void drawPcAudioWait(uint32_t now) {
  spr.fillSprite(C_BG());
  uiHeader(L("Sound", "Звук"), src(L("waiting", "ожидание")));
  uiEmpty(L("waiting for computer audio", "жду звук с компьютера"), L("run TENGAN Desktop on the computer", "запустите TENGAN Desktop на компьютере"));
  uiFooter(L("wheel: screens", "колесо: экраны"), L("side: back", "кнопка: назад"));
  present();
  (void)now;
}
