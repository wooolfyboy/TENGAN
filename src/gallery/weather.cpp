// ============================================================
//  weather.cpp — «Радиопогода»: расчёт (для палки и планшета)
//  и экран Gallery › Weather. Описание — в weather.h.
// ============================================================
#include <algorithm>
#include "core/lang.h"
#include "gallery/weather.h"
#include "gallery/astro.h"
#include "ble/ble.h"
#include "ble/ble_model.h"
#include "subghz/subghz.h"
#include "subghz/subghz_model.h"
#include "subghz/sensors.h"
#include "audio/audio.h"
#include "audio/audio_model.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/timeutil.h"

WxState g_wx;

static const int SUB_MONITOR = G_MONITOR, SUB_433 = 1;   // CC1101: монитор на 433.92 (BANDS[1])

// ---------- источники (только для экрана палки; планшету их включает web.cpp) ----------
static bool s_mine = false, s_subOn = false, s_sndOn = false;
void wxStart() {
  if (s_mine) return;
  s_mine = true;
  bleRadioOn();
  s_subOn = subWebUse(SUB_MONITOR, SUB_433);
  s_sndOn = audioWebUse();
}
void wxStop() {
  if (!s_mine) return;
  s_mine = false;
  if (s_subOn) subWebStop();
  if (s_sndOn) audioWebStop();
  s_subOn = s_sndOn = false;
  bleRadioOff();
}
void wxRadiosTick(uint32_t now) {
  if (s_subOn) subWebTick(now);
  if (s_sndOn) audioWebTick(now);
}

// ---------- расчёт: раз в секунду ----------
#define IDS_MAX 160
static uint32_t s_ids[IDS_MAX]; static int s_idsN = 0;     // устройства прошлой секунды (отсортированы)
static uint8_t  s_turnSec[60], s_burstSec[60], s_loudSec[60];   // за каждую секунду последней минуты
static int      s_sec = 0;
static uint32_t s_lastMs = 0, s_lastEvents = 0, s_histMs = 0, s_condMs = 0;
static bool     s_haveEvents = false;
static float    s_sndSlow = -90;
static const char* s_condCand = "calm";

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float easeTo(float v, float target, float dt, float tau) { return v + (target - v) * (1 - expf(-dt / tau)); }

static int sumOf(const uint8_t* a) { int s = 0; for (int i = 0; i < 60; i++) s += a[i]; return s; }

static const char* condFor(const WxState& w) {
  if (w.clouds < 0.08f && w.rain < 0.05f && w.wind < 0.1f) return "calm";
  if (w.storm > 0.6f) return "storm";
  if (w.rain > 0.35f) return "rain";
  if (w.wind > 0.5f) return "windy";
  if (w.clouds > 0.7f) return "overcast";
  if (w.clouds > 0.35f) return "cloudy";
  return "clear";
}

void wxUpdate(uint32_t now) {
  if (s_lastMs && msSince(now, s_lastMs) < 1000) return;
  float dt = s_lastMs ? min(5.0f, msSince(now, s_lastMs) / 1000.0f) : 1.0f;
  s_lastMs = now;
  WxState& w = g_wx;
  s_sec = (s_sec + 1) % 60;

  // облака и ветер: устройства Bluetooth и их смена
  static uint32_t cur[IDS_MAX];
  int n = bleRadioActive() ? bleEntityIds(cur, IDS_MAX) : 0;
  std::sort(cur, cur + n);
  int same = 0;
  for (int i = 0, j = 0; i < n && j < s_idsN; ) {
    if (cur[i] == s_ids[j]) { same++; i++; j++; } else if (cur[i] < s_ids[j]) i++; else j++;
  }
  int turn = w.ok ? (n - same) + (s_idsN - same) : 0;      // пришли + ушли (первую секунду не считаем)
  memcpy(s_ids, cur, n * sizeof(uint32_t)); s_idsN = n;
  s_turnSec[s_sec] = (uint8_t)min(turn, 255);
  w.ble = n;
  w.turn = sumOf(s_turnSec);

  // дождь: всплески на 433.92
  w.subOk = g_ccOk && (s_subOn || !s_mine);
  uint32_t ev = g_events;
  int newBursts = (s_haveEvents && ev >= s_lastEvents) ? (int)(ev - s_lastEvents) : 0;
  s_lastEvents = ev; s_haveEvents = true;
  s_burstSec[s_sec] = (uint8_t)min(newBursts, 255);
  w.sub = sumOf(s_burstSec);
  if (newBursts > 0 && g_evN > 0) {
    w.subPk = g_ev[g_evN - 1].peak;
    if (w.subPk > -55) w.thunderMs = now;                    // сильный всплеск — тоже «гром»
  }

  // звук: громкость, громкие пики, гром
  w.sndOk = g_micOk;
  float lv = w.sndOk ? g_au.level : -90;
  w.snd = lv;
  w.pk = max(lv, w.pk - 2.0f * dt);                          // пик с медленным спадом
  bool loud = lv > -25;
  s_loudSec[s_sec] = loud ? 1 : 0;
  if (w.sndOk && lv > -30 && lv - s_sndSlow > 18) w.thunderMs = now;   // резкий скачок громкости
  s_sndSlow = easeTo(s_sndSlow, lv, dt, 6);
  w.thunder = w.thunderMs && msSince(now, w.thunderMs) < 2000;

  // нормированные величины — плавно, без дёрганья
  float cloudsT = 1 - expf(-n / 25.0f);                       // ~0.9 при 60 устройствах
  float windT   = clamp01(w.turn / 30.0f);
  float rainT   = clamp01(w.sub / 6.0f);
  float stormT  = clamp01(rainT * 0.6f + sumOf(s_loudSec) / 20.0f * 0.5f + (w.thunder ? 0.3f : 0));
  w.clouds = easeTo(w.clouds, cloudsT, dt, 8);
  w.wind   = easeTo(w.wind, windT, dt, 6);
  w.rain   = easeTo(w.rain, rainT, dt, 5);
  w.storm  = easeTo(w.storm, stormT, dt, 4);

  // история и барометр: точка раз в 10 с
  if (!s_histMs || msSince(now, s_histMs) >= 10000) {
    s_histMs = now;
    if (w.histN < WX_HIST) w.histN++;
    else memmove(w.hist, w.hist + 1, sizeof(w.hist) - sizeof(w.hist[0]));
    w.hist[w.histN - 1][0] = w.clouds; w.hist[w.histN - 1][1] = w.rain;
  }
  float old = w.hist[0][0];
  w.trend = w.histN > 6 ? max(-1.0f, min(1.0f, (w.clouds - old) * 3)) : 0;

  // за окном: уличный термодатчик 433 МГц (CC1101 в мониторе разбирает посылки)
  { float t; int h; const char* src;
    w.tOk = sensOutside(t, h, src, now);
    if (w.tOk) { w.temp = t; w.hum = h; w.tsrc = src; } else { w.hum = -1; w.tsrc = ""; } }

  // слово погоды меняется, только если новое продержалось 5 с
  const char* c = condFor(w);
  if (c != s_condCand) { s_condCand = c; s_condMs = now; }
  if (!w.ok || msSince(now, s_condMs) >= 5000) w.cond = s_condCand;
  w.ok = true;
}

void wxJson(String& o) {
  const WxState& w = g_wx;
  char b[200];
  snprintf(b, sizeof(b), "{\"ok\":%d,\"cond\":\"%s\",\"clouds\":%.2f,\"wind\":%.2f,\"rain\":%.2f,\"storm\":%.2f,\"thunder\":%d,\"trend\":%.2f,",
           w.ok ? 1 : 0, w.cond, w.clouds, w.wind, w.rain, w.storm, w.thunder ? 1 : 0, w.trend);
  o.reserve(220 + w.histN * 12);
  o = b;
  snprintf(b, sizeof(b), "\"ble\":%d,\"turn\":%.0f,\"sub\":%.0f,\"subPk\":%d,\"subOk\":%d,\"snd\":%.1f,\"pk\":%.1f,\"sndOk\":%d,\"hist\":[",
           w.ble, w.turn, w.sub, w.subPk, w.subOk ? 1 : 0, w.snd, w.pk, w.sndOk ? 1 : 0);
  o += b;
  for (int i = 0; i < w.histN; i++) {
    snprintf(b, sizeof(b), "%s[%.2f,%.2f]", i ? "," : "", w.hist[i][0], w.hist[i][1]);
    o += b;
  }
  o += ']';
  if (w.tOk) {
    snprintf(b, sizeof(b), ",\"temp\":%.1f,\"hum\":", w.temp); o += b;
    if (w.hum >= 0) o += w.hum; else o += "null";
    o += ",\"tsrc\":\""; o += w.tsrc; o += '"';
  } else o += ",\"temp\":null,\"hum\":null,\"tsrc\":\"\"";
  o += '}';
}

// ============================================================
//  Экран палки 320×170: слева небо, справа «метеостанция».
//  Только токены темы (theme.h).
// ============================================================
static int s_view = 0;                                       // 0 — небо, 1 — график
void wxTurn(int) { s_view ^= 1; }

static uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }

static bool isNight() {
  if (!astroTimeKnown()) return false;
  double t = astroUnixNow() + astroTzOffset();
  int h = (int)fmod(t / 3600.0, 24.0);
  return h < 6 || h >= 21;
}

// облако: несколько кругов, верх светлее (светится)
static void cloud(int x, int y, int s, uint16_t fill, uint16_t edge) {
  const int8_t P[][3] = { { 0, 0, 10 }, { 9, -4, 9 }, { 18, 0, 8 }, { -9, 2, 7 }, { 6, 4, 9 } };
  for (auto& p : P) spr.fillCircle(x + p[0] * s / 10, y + p[1] * s / 10, p[2] * s / 10, edge);
  for (auto& p : P) spr.fillCircle(x + p[0] * s / 10, y + p[1] * s / 10 + 1, p[2] * s / 10 - 1, fill);
}

static void drawSkyView(uint32_t now, int x0, int y0, int w, int h) {
  const WxState& W = g_wx;
  const uint16_t bg = C_BG(), line = UI_LINE();
  spr.drawRect(x0, y0, w, h, line);
  spr.setViewport(x0 + 1, y0 + 1, w - 2, h - 2, false);   // обрезка по рамке неба, координаты прежние
  float t = now / 1000.0f;
  bool night = isNight();
  int horizon = y0 + h - 14;

  // вспышка молнии: короткая подсветка всего неба
  bool flash = W.thunder && ((now / 70) % 3 != 0) && msSince(now, W.thunderMs) < 600;
  if (flash) spr.fillRect(x0 + 1, y0 + 1, w - 2, h - 2, scale565(C_HOT(), 22));

  // светило: солнце в ясную погоду, луна и звёзды — ночью и в штиль
  if (W.clouds < 0.55f) {
    int sx = x0 + w - 34, sy = y0 + 26;
    if (night) {
      for (int i = 0; i < 26; i++) {                          // звёзды мерцают
        uint32_t r = hash32(i * 7919);
        int px = x0 + 4 + r % (w - 8), py = y0 + 4 + (r >> 10) % (h - 30);
        if (((now / 400) + i) % 5) spr.drawPixel(px, py, (r >> 20) & 1 ? UI_LABEL() : UI_DIM());
      }
      spr.fillCircle(sx, sy, 10, UI_LABEL());
      spr.fillCircle(sx + 5, sy - 3, 9, bg);                 // серп
    } else {
      int r = 9 + (int)(2 * sinf(t * 1.3f));
      for (int a = 0; a < 360; a += 30) {                    // лучи
        float rad = (a + t * 12) * DEG_TO_RAD;
        spr.drawLine(sx + cosf(rad) * (r + 3), sy + sinf(rad) * (r + 3), sx + cosf(rad) * (r + 8), sy + sinf(rad) * (r + 8), C_ACC());
      }
      spr.fillCircle(sx, sy, r, C_SEAL());
    }
  }

  // облака: число — по облачности, скорость — по ветру
  int nc = (int)(W.clouds * 9 + 0.5f);
  float speed = 4 + W.wind * 40;
  uint16_t cFill = scale565(UI_LABEL(), 35 + (int)(W.storm * 30)), cEdge = UI_LABEL();
  for (int i = 0; i < nc; i++) {
    uint32_t r = hash32(i * 2654435761u + 17);
    int s = 10 + r % 9, yy = y0 + 18 + (r >> 8) % (h / 2 - 4);
    float span = w + 70;
    int xx = x0 - 35 + (int)fmodf((r >> 16) % (int)span + t * speed * (0.7f + (r % 5) * 0.12f), span);
    cloud(xx, yy, s, cFill, cEdge);
  }

  // дождь: косые штрихи, наклон — от ветра
  int drops = (int)(W.rain * 70);
  float slant = 2 + W.wind * 8;
  for (int i = 0; i < drops; i++) {
    uint32_t r = hash32(i * 40503u + 7);
    int fall = (int)fmodf((r >> 12) % h + t * (70 + (r % 30)), (float)(h - 10));
    int px = x0 + 2 + (int)fmodf((r % w) + fall * slant / 10.0f, (float)(w - 4)), py = y0 + 30 + fall % (horizon - y0 - 30);
    spr.drawLine(px, py, px - (int)slant / 2, py + 5, C_ACC());
  }

  // молния
  if (W.thunder && msSince(now, W.thunderMs) < 450) {
    uint32_t r = hash32(W.thunderMs);
    int lx = x0 + 20 + r % (w - 40), ly = y0 + 22;
    for (int k = 0; k < 5; k++) {
      int nx = lx + (int)((hash32(r + k) % 15) - 7), ny = ly + (horizon - y0 - 22) / 5;
      spr.drawLine(lx, ly, nx, ny, C_HOT()); spr.drawLine(lx + 1, ly, nx + 1, ny, C_HOT());
      lx = nx; ly = ny;
    }
  }

  // за окном: температура с уличного датчика — крупно в левом верхнем углу неба
  if (W.tOk) {
    char tb[16];
    snprintf(tb, sizeof(tb), "%+.1f", W.temp);
    int tx = x0 + 6, ty = y0 + 5;
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_HOT());
    txt(tb, tx, ty, 4);
    int dx = tx + txtW(tb, 4) + 3;
    spr.drawCircle(dx, ty + 4, 3, C_HOT()); spr.drawCircle(dx, ty + 4, 2, C_HOT());
    char sb[28];
    char src[16]; strncpy(src, W.tsrc, sizeof(src)); src[sizeof(src) - 1] = 0;
    { char* d = strchr(src, '-'); if (d) *d = 0; utf8UpperInPlace(src); }
    if (W.hum >= 0) snprintf(sb, sizeof(sb), "%d%%  %s", W.hum, src); else snprintf(sb, sizeof(sb), "%s", src);
    spr.setTextColor(UI_LABEL());
    txt(sb, tx + 1, ty + 26, 1);
  }

  // земля: линия горизонта и «трава» — громкость колышет её
  spr.drawFastHLine(x0 + 1, horizon, w - 2, UI_DIM());
  float sway = W.sndOk ? constrain((W.snd + 60) / 40.0f, 0.0f, 1.0f) : 0;
  for (int gx = x0 + 3; gx < x0 + w - 3; gx += 4) {
    float a = sinf(t * 2 + gx * 0.3f) * (1 + sway * 3) + W.wind * 3;
    int gh = 3 + (hash32(gx) % 5);
    spr.drawLine(gx, horizon, gx + (int)a, horizon - gh, UI_LINE());
  }
  spr.resetViewport();
}

static void drawChartView(int x0, int y0, int w, int h) {
  const WxState& W = g_wx;
  spr.drawRect(x0, y0, w, h, UI_LINE());
  for (int k = 1; k < 4; k++) spr.drawFastHLine(x0 + 1, y0 + h * k / 4, w - 2, UI_LINE());
  spr.setTextDatum(TL_DATUM); spr.setTextColor(UI_DIM());
  txt("10 MIN", x0 + 3, y0 + 3, 1);
  if (W.histN < 2) { spr.setTextColor(UI_LABEL()); txt("COLLECTING...", x0 + 30, y0 + h / 2 - 4, 1); return; }
  int px = 0, pyC = 0, pyR = 0;
  for (int i = 0; i < W.histN; i++) {
    int x = x0 + 2 + (w - 4) * i / (WX_HIST - 1);
    int yc = y0 + h - 2 - (int)(W.hist[i][0] * (h - 6)), yr = y0 + h - 2 - (int)(W.hist[i][1] * (h - 6));
    if (i) {
      spr.drawLine(px, pyC, x, yc, C_ACC());
      spr.drawLine(px, pyR, x, yr, C_ALERT());
    }
    px = x; pyC = yc; pyR = yr;
  }
  spr.setTextColor(C_ACC()); txt("CLOUDS", x0 + w - 70, y0 + 3, 1);
  spr.setTextColor(C_ALERT()); txt("RAIN", x0 + w - 28, y0 + 3, 1);
}

void drawWeather(uint32_t now) {
  const WxState& W = g_wx;
  spr.fillSprite(C_BG());
  char cond[16]; strncpy(cond, W.cond, sizeof(cond)); cond[sizeof(cond) - 1] = 0;
  uiHeader("Radio weather", W.ok ? cond : "starting", W.storm > 0.6f);

  const int lx = UI_M, ly = UI_TOP + 2, lw = 186, lh = UI_BOTTOM - UI_TOP - 2;
  if (s_view == 0) drawSkyView(now, lx, ly, lw, lh); else drawChartView(lx, ly, lw, lh);

  // метеостанция справа
  int x = lx + lw + 8, y = UI_TOP + 2;
  char v[24];
  snprintf(v, sizeof(v), "%d dev", W.ble);
  uiReadout(x, y, "clouds", v, C_HOT(), 2);           y += 30;
  snprintf(v, sizeof(v), "%.0f/min", W.turn);
  uiReadout(x, y, "wind", v, C_HOT(), 2);             y += 30;
  if (W.subOk) snprintf(v, sizeof(v), "%.0f/min", W.sub); else strcpy(v, "no radio");
  uiReadout(x, y, "rain 433", v, W.rain > 0.35f ? C_ALERT() : C_HOT(), 2);   y += 30;
  if (W.sndOk) snprintf(v, sizeof(v), "%.0f dBFS", W.pk); else strcpy(v, "mic off");
  uiReadout(x, y, "thunder", v, W.thunder ? C_ALERT() : C_HOT(), 2);

  // барометр: стрелка вверх/вниз по тренду облачности
  int bx = x + 100, by = UI_TOP + 12;
  int dy = (int)(-W.trend * 7);
  spr.drawLine(bx, by + 7, bx, by - 7, UI_LINE());
  spr.fillTriangle(bx - 4, by + dy + (dy <= 0 ? 4 : -4), bx + 4, by + dy + (dy <= 0 ? 4 : -4), bx, by + dy + (dy <= 0 ? -3 : 3),
                   fabsf(W.trend) > 0.3f ? C_ACC() : UI_DIM());

  uiFooter(s_view == 0 ? "wheel: chart" : "wheel: sky", "side: back");
  present();
}
