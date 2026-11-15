// ============================================================
//  gallery_sky.cpp — Gallery › Sky (планетарий) и Globe (спутники)
//
//  SKY   — небо над городом прямо сейчас: купол (зенит в центре,
//          горизонт — край круга, север сверху, восток слева, как на
//          звёздной карте), ~1600 звёзд, линии созвездий, Солнце, Луна
//          с фазой, планеты. Колесо крутит купол, нажатие — подписи.
//  GLOBE — Земля с материками, днём и ночью, МКС и другие станции на
//          своих орбитах, след полёта, зона видимости и ближайший
//          пролёт над твоим городом. Колесо крутит Землю, нажатие —
//          следить за МКС / за домом / за следующим спутником.
// ============================================================
#include "gallery/gallery.h"
#include "gallery/astro.h"
#include "gallery/astrodata.h"
#include "gallery/sat.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/settings.h"
#include "core/timeutil.h"

static const float D2R = PI / 180.0f;

static void hhmm(double unixUtc, char* out, size_t n, bool withSec = false) {
  long t = (long)(unixUtc + astroTzOffset());
  long d = ((t % 86400) + 86400) % 86400;
  if (withSec) snprintf(out, n, "%02ld:%02ld:%02ld", d / 3600, d / 60 % 60, d % 60);
  else         snprintf(out, n, "%02ld:%02ld", d / 3600, d / 60 % 60);
}

// ============================================================
//  SKY
// ============================================================
static float s_skyRot = 0, s_skyRotT = 0;     // поворот купола (колесо), плавно
static int   s_skyLabels = 0;                 // 0 созвездия+планеты, 1 яркие звёзды, 2 без подписей

void skyTurn(int dir) { s_skyRotT += dir * 15; }
void skyClick() { s_skyLabels = (s_skyLabels + 1) % 3; }

struct DomeView { int cx, cy, R; float rot; };

// подписи на куполе не должны налезать друг на друга: помним занятые места
static int s_lblN = 0;
static int16_t s_lbl[64][4];
static bool labelFree(int x, int y, int w, int h) {
  for (int i = 0; i < s_lblN; i++)
    if (x < s_lbl[i][0] + s_lbl[i][2] && s_lbl[i][0] < x + w && y < s_lbl[i][1] + s_lbl[i][3] && s_lbl[i][1] < y + h) return false;
  if (s_lblN < 64) { s_lbl[s_lblN][0] = x; s_lbl[s_lblN][1] = y; s_lbl[s_lblN][2] = w; s_lbl[s_lblN][3] = h; s_lblN++; }
  return true;
}
static void label(const char* t, int x, int y) {         // подпись по центру (x, y), если есть место
  int w = txtW(t, 1);
  if (labelFree(x - w / 2 - 1, y - 4, w + 2, 9)) txt(t, x, y, 1);
}
// стереографическая проекция: зенит — центр, горизонт — край; север вверху, восток слева
static bool domeXY(const DomeView& v, float alt, float az, int& x, int& y) {
  if (alt < 0) return false;
  float r = v.R * tanf((90 - alt) * D2R / 2);                 // tan(45°)=1 на горизонте
  float a = (az - v.rot) * D2R;
  x = v.cx - (int)(r * sinf(a));
  y = v.cy - (int)(r * cosf(a));
  return true;
}

void drawSky(uint32_t now) {
  const int W = spr.width();
  g_fastFrame = true;
  s_skyRot += (s_skyRotT - s_skyRot) * 0.25f;
  spr.fillSprite(C_BG());
  const City& c = CITIES[g_city];
  bool timeOk = astroTimeKnown();
  double t = astroUnixNow();
  double jd = julianDate(t);

  RaDec sun = sunRaDec(jd);
  AltAz sunA = toAltAz(sun.ra, sun.dec, jd, c.lat, c.lon);
  bool day = sunA.alt > -6, night = sunA.alt < -12;

  DomeView v = { 86, 88, 58, s_skyRot };
  s_lblN = 0;
  spr.fillCircle(v.cx, v.cy, v.R, day ? tft.color565(8, 30, 44) : night ? tft.color565(3, 10, 16) : tft.color565(5, 20, 30));
  for (int alt = 30; alt < 90; alt += 30) {                    // круги высоты 30° и 60°
    int r = (int)(v.R * tanf((90 - alt) * D2R / 2));
    for (int k = 0; k < 72; k++) { float a = k * 5 * D2R; spr.drawPixel(v.cx + (int)(r * cosf(a)), v.cy + (int)(r * sinf(a)), UI_LINE()); }
  }
  spr.drawCircle(v.cx, v.cy, v.R, C_ACC());

  // линии созвездий
  uint16_t clCol = scale565(C_ACC(), day ? 25 : 45);
  for (int k = 0; k < N_CL; k++) {
    int px = 0, py = 0; bool have = false;
    for (int i = CL_START[k]; i < CL_START[k + 1]; i++) {
      AltAz a = toAltAz(CL_PTS[i][0] / 100.0f, (int16_t)CL_PTS[i][1] / 100.0f, jd, c.lat, c.lon);
      int x, y;
      if (domeXY(v, a.alt, a.az, x, y)) { if (have) spr.drawLine(px, py, x, y, clCol); px = x; py = y; have = true; }
      else have = false;
    }
  }
  // звёзды: днём видны только самые яркие (и то — тускло)
  float magLimit = day ? 1.5f : night ? 5.0f : 3.0f;
  for (int i = 0; i < N_STARS; i++) {
    float mag = STARS[i].mag / 10.0f - 2;
    if (mag > magLimit) break;                                   // отсортированы по яркости
    AltAz a = toAltAz(STARS[i].ra / 100.0f, STARS[i].dec / 100.0f, jd, c.lat, c.lon);
    int x, y;
    if (!domeXY(v, a.alt, a.az, x, y)) continue;
    if (mag < 0.5f)      spr.fillCircle(x, y, 2, C_HOT());
    else if (mag < 1.5f) { spr.fillRect(x - 1, y - 1, 3, 3, heatV(0.9f)); spr.drawPixel(x, y, C_HOT()); }
    else if (mag < 2.5f) spr.fillRect(x, y, 2, 2, heatV(0.75f));
    else                 spr.drawPixel(x, y, heatV(max(0.15f, 0.65f - (mag - 2.5f) * 0.2f)));
  }
  // планеты
  int visPl[PL_NUM], nVis = 0; float visAlt[PL_NUM];
  for (int p = 0; p < PL_NUM; p++) {
    RaDec r = planetRaDec(p, jd);
    AltAz a = toAltAz(r.ra, r.dec, jd, c.lat, c.lon);
    int x, y;
    if (!domeXY(v, a.alt, a.az, x, y)) continue;
    visPl[nVis] = p; visAlt[nVis++] = a.alt;
    spr.fillCircle(x, y, 2, C_HOT());
    spr.drawCircle(x, y, 4, C_ACC());
    if (s_skyLabels == 0) {
      static const char* const EN[PL_NUM] = { "MER", "VEN", "MAR", "JUP", "SAT" };
      spr.setTextColor(C_HOT());
      label(EN[p], x, y - 9);
    }
  }
  // подписи: созвездия (латинские сокращения) или яркие звёзды
  spr.setTextDatum(MC_DATUM);
  if (s_skyLabels == 0) {
    spr.setTextColor(scale565(C_ACC(), 70));
    for (int i = 0; i < N_CON_NAMES; i++) {
      AltAz a = toAltAz(CON_NAMES[i].ra / 100.0f, CON_NAMES[i].dec / 100.0f, jd, c.lat, c.lon);
      int x, y;
      if (a.alt > 12 && domeXY(v, a.alt, a.az, x, y)) label(CON_NAMES[i].ab, x, y);
    }
  } else if (s_skyLabels == 1) {
    spr.setTextColor(UI_LABEL());
    for (int i = 0; i < N_STAR_NAMES; i++) {
      const AStar& s = STARS[STAR_NAMES[i].star];
      AltAz a = toAltAz(s.ra / 100.0f, s.dec / 100.0f, jd, c.lat, c.lon);
      int x, y;
      if (a.alt > 5 && domeXY(v, a.alt, a.az, x, y)) label(STAR_NAMES[i].name, x, y - 7);
    }
  }
  // Луна с фазой и Солнце
  float phase; RaDec mo = moonRaDec(jd, &phase);
  AltAz moA = toAltAz(mo.ra, mo.dec, jd, c.lat, c.lon);
  int x, y;
  if (domeXY(v, moA.alt, moA.az, x, y)) {
    spr.fillCircle(x, y, 5, scale565(C_HOT(), 25));
    int lit = (int)(phase * 11) - 5;                            // освещённая часть — полосой
    for (int dx = -5; dx <= 5; dx++) if (dx <= lit) {
      int h = (int)sqrtf(25 - dx * dx);
      spr.drawFastVLine(x + dx, y - h, 2 * h + 1, C_HOT());
    }
  }
  if (domeXY(v, sunA.alt, sunA.az, x, y)) {
    spr.fillCircle(x, y, 5, C_HOT());
    for (int k = 0; k < 8; k++) { float a = k * 45 * D2R; spr.drawLine(x + cosf(a) * 7, y + sinf(a) * 7, x + cosf(a) * 10, y + sinf(a) * 10, C_HOT()); }
  }
  // стороны света (поворачиваются вместе с куполом)
  static const char* const SIDES[4] = { "N", "E", "S", "W" };
  spr.setTextColor(C_ACC());
  for (int k = 0; k < 4; k++) {
    float a = (k * 90 - v.rot) * D2R;
    txt(SIDES[k], v.cx - (int)((v.R + 7) * sinf(a)), v.cy - (int)((v.R + 7) * cosf(a)), 1);
  }

  // ---- справа: время, Солнце, Луна, планеты ----
  const int px = 168;
  char buf[40];
  spr.setTextDatum(TL_DATUM);
  if (timeOk) { hhmm(t, buf, sizeof(buf)); spr.setTextColor(C_HOT()); txt(buf, px, UI_TOP + 2, 4); }
  else { spr.setTextColor(C_ALERT()); txt("TIME?", px, UI_TOP + 2, 4); }
  spr.setTextColor(UI_LABEL());
  txt(timeOk ? "LOCAL TIME" : "CONNECT LAPTOP ONCE", px, UI_TOP + 28, 1);
  snprintf(buf, sizeof(buf), "SUN %+d", (int)lroundf(sunA.alt));
  spr.setTextColor(C_HOT()); txt(buf, px, UI_TOP + 44, 1);
  spr.setTextColor(UI_DIM()); txt(day ? (sunA.alt > 0 ? "DAY" : "TWILIGHT") : night ? "NIGHT" : "TWILIGHT", px + 64, UI_TOP + 44, 1);
  snprintf(buf, sizeof(buf), "MOON %d%%  %+d", (int)lroundf(phase * 100), (int)lroundf(moA.alt));
  spr.setTextColor(C_HOT()); txt(buf, px, UI_TOP + 56, 1);
  int yy = UI_TOP + 70;
  spr.setTextColor(UI_LABEL());
  txt(nVis ? "PLANETS UP, DEG" : "NO PLANETS UP", px, yy, 1);
  for (int i = 0; i < nVis && i < 3; i++) {                // по-русски, шрифтом с кириллицей
    snprintf(buf, sizeof(buf), "%s  %d", PLANET_RU[visPl[i]], (int)lroundf(visAlt[i]));
    spr.setTextColor(C_HOT());
    txt(buf, px, yy + 11 + i * 15, 3);
  }
  if (nVis > 3) { snprintf(buf, sizeof(buf), "+%d", nVis - 3); spr.setTextColor(UI_DIM()); spr.setTextDatum(TR_DATUM); txt(buf, spr.width() - UI_M - 4, yy, 1); spr.setTextDatum(TL_DATUM); }
  snprintf(buf, sizeof(buf), "%s", c.name);
  uiHeader("Sky", buf);
  static const char* const LBL[3] = { "wheel: turn  click: stars", "wheel: turn  click: no labels", "wheel: turn  click: constell." };
  uiFooter(LBL[s_skyLabels], "side: back");
  present();
  (void)now; (void)W;
}

// ============================================================
//  GLOBE
// ============================================================
static float s_gLon = 30, s_gLat = 25, s_gLonOff = 0, s_gLonOffT = 0;
static int   s_gFollow = 0;                    // 0 — первый спутник (МКС), 1 — дом, 2.. — остальные спутники
static Pass  s_pass;
static double s_passAt = 0;

void globeTurn(int dir) { s_gLonOffT += dir * 20; }
void globeClick() {
  s_gFollow = (s_gFollow + 1) % (g_nSats > 1 ? g_nSats + 1 : 2);
  s_gLonOffT = s_gLonOff = 0;
}

struct GlobeView { int cx, cy, R; float lat0, lon0; float sl, cl; };
static bool globeXY(const GlobeView& g, float lat, float lon, int& x, int& y) {
  float la = lat * D2R, dl = (lon - g.lon0) * D2R;
  float cosc = g.sl * sinf(la) + g.cl * cosf(la) * cosf(dl);
  if (cosc < 0) return false;                                  // обратная сторона Земли
  x = g.cx + (int)(g.R * cosf(la) * sinf(dl));
  y = g.cy - (int)(g.R * (g.cl * sinf(la) - g.sl * cosf(la) * cosf(dl)));
  return true;
}

void drawGlobe(uint32_t now) {
  g_fastFrame = true;
  spr.fillSprite(C_BG());
  const City& c = CITIES[g_city];
  bool timeOk = astroTimeKnown();
  double t = astroUnixNow();
  double jd = julianDate(t);

  // за чем следим
  SatPos sp[SAT_MAX]; bool spOk[SAT_MAX] = {};
  for (int i = 0; i < g_nSats; i++) spOk[i] = timeOk && satPos(g_sats[i], t, sp[i]);
  int satIdx = s_gFollow == 0 ? 0 : s_gFollow >= 2 ? s_gFollow - 1 : -1;
  float tLat = c.lat, tLon = c.lon;
  if (satIdx >= 0 && satIdx < g_nSats && spOk[satIdx]) { tLat = sp[satIdx].lat; tLon = sp[satIdx].lon; }
  s_gLonOff += (s_gLonOffT - s_gLonOff) * 0.2f;
  float wantLon = tLon + s_gLonOff, wantLat = constrain(tLat * 0.6f + 10, -50.0f, 60.0f);
  float dLon = fmodf(wantLon - s_gLon + 540, 360) - 180;
  s_gLon += dLon * 0.08f; s_gLat += (wantLat - s_gLat) * 0.08f;

  GlobeView g = { 86, 90, 60, s_gLat, s_gLon, sinf(s_gLat * D2R), cosf(s_gLat * D2R) };
  // день и ночь: по каждой точке диска — освещена ли она Солнцем
  RaDec sun = sunRaDec(jd);
  float sLon = sun.ra - (float)gmstDeg(jd), sLat = sun.dec;      // подсолнечная точка
  float sx = cosf(sLat * D2R) * cosf(sLon * D2R), sy = cosf(sLat * D2R) * sinf(sLon * D2R), sz = sinf(sLat * D2R);
  float cl0 = cosf(g.lon0 * D2R), sl0 = sinf(g.lon0 * D2R);
  const uint16_t dayC = tft.color565(10, 38, 54), nightC = tft.color565(3, 11, 17), twiC = tft.color565(6, 22, 32);
  for (int yy = -g.R; yy <= g.R; yy++) {
    int half = (int)sqrtf((float)(g.R * g.R - yy * yy));
    for (int xx = -half; xx <= half; xx++) {
      // точка на сфере в системе «камеры» -> земные координаты
      float X = xx / (float)g.R, Y = -yy / (float)g.R, Z = sqrtf(max(0.0f, 1 - X * X - Y * Y));
      float ex = -X * sl0 + (-Y * g.sl + Z * g.cl) * cl0;
      float ey =  X * cl0 + (-Y * g.sl + Z * g.cl) * sl0;
      float ez =  Y * g.cl + Z * g.sl;
      float d = ex * sx + ey * sy + ez * sz;
      spr.drawPixel(g.cx + xx, g.cy + yy, timeOk ? (d > 0.05f ? dayC : d > -0.05f ? twiC : nightC) : twiC);
    }
  }
  spr.drawCircle(g.cx, g.cy, g.R, C_ACC());
  // сетка 30°
  for (int la = -60; la <= 60; la += 30)
    for (int lo = -180; lo < 180; lo += 4) { int x, y; if (globeXY(g, la, lo, x, y)) spr.drawPixel(x, y, UI_LINE()); }
  for (int lo = -180; lo < 180; lo += 30)
    for (int la = -88; la <= 88; la += 4) { int x, y; if (globeXY(g, la, lo, x, y)) spr.drawPixel(x, y, UI_LINE()); }
  // материки
  for (int k = 0; k < N_LAND; k++) {
    int px = 0, py = 0; bool have = false;
    for (int i = LAND_START[k]; i < LAND_START[k + 1]; i++) {
      int x, y;
      if (globeXY(g, LAND_PTS[i][1] / 100.0f, LAND_PTS[i][0] / 100.0f, x, y)) { if (have) spr.drawLine(px, py, x, y, C_ACC()); px = x; py = y; have = true; }
      else have = false;
    }
  }
  // дом
  int hx, hy;
  if (globeXY(g, c.lat, c.lon, hx, hy)) {
    int pr = 3 + (int)((now / 150) % 4);
    spr.drawCircle(hx, hy, pr, scale565(C_HOT(), 70));
    spr.fillCircle(hx, hy, 2, C_HOT());
  }
  // спутники: след (−30 … +90 мин), зона видимости МКС, метки
  for (int i = 0; i < g_nSats; i++) {
    if (!spOk[i]) continue;
    bool main = (i == (satIdx >= 0 ? satIdx : 0));
    int px = 0, py = 0; bool have = false;
    for (int m = -30; m <= 90; m += 2) {
      SatPos q; if (!satPos(g_sats[i], t + m * 60, q)) break;
      int x, y;
      if (globeXY(g, q.lat, q.lon, x, y)) {
        if (have) spr.drawLine(px, py, x, y, m < 0 ? scale565(C_ACC(), 35) : (main ? C_ACC() : scale565(C_ACC(), 55)));
        px = x; py = y; have = true;
      } else have = false;
    }
    if (main) {                                                  // откуда спутник виден выше 10°
      float el = 10 * D2R, h = sp[i].altKm, lam = acosf(6378.0f * cosf(el) / (6378.0f + h)) - el;
      float la0 = sp[i].lat * D2R, lo0 = sp[i].lon * D2R;
      int fx = 0, fy = 0; bool fh = false;
      for (int k = 0; k <= 48; k++) {
        float b = k * 7.5f * D2R;
        float la = asinf(sinf(la0) * cosf(lam) + cosf(la0) * sinf(lam) * cosf(b));
        float lo = lo0 + atan2f(sinf(b) * sinf(lam) * cosf(la0), cosf(lam) - sinf(la0) * sinf(la));
        int x, y;
        if (globeXY(g, la / D2R, lo / D2R, x, y)) { if (fh && (k % 2)) spr.drawLine(fx, fy, x, y, scale565(C_HOT(), 60)); fx = x; fy = y; fh = true; }
        else fh = false;
      }
    }
    int x, y;
    if (globeXY(g, sp[i].lat, sp[i].lon, x, y)) {
      spr.fillRect(x - 2, y - 2, 5, 5, main ? C_HOT() : C_ACC());
      spr.drawFastHLine(x - 6, y, 13, main ? C_HOT() : C_ACC());
      spr.setTextDatum(BL_DATUM);
      spr.setTextColor(main ? C_HOT() : UI_LABEL());
      txt(g_sats[i].name, x + 5, y - 3, 1);
    }
  }

  // ---- справа: данные спутника и ближайший пролёт ----
  const int px = 160;
  char buf[48], b2[16];
  spr.setTextDatum(TL_DATUM);
  int si = satIdx >= 0 ? satIdx : 0;
  if (!timeOk) {
    spr.setTextColor(C_ALERT()); txt("TIME UNKNOWN", px, UI_TOP + 4, 2);
    spr.setTextColor(UI_LABEL()); txt("CONNECT THE LAPTOP ONCE:", px, UI_TOP + 26, 1);
    txt("RUN start_pc_monitor.bat", px, UI_TOP + 38, 1);
  } else if (g_nSats == 0) {
    spr.setTextColor(C_ALERT()); txt("NO ORBITS", px, UI_TOP + 4, 2);
    spr.setTextColor(UI_LABEL()); txt("LAPTOP DOWNLOADS THEM:", px, UI_TOP + 26, 1);
    txt("RUN start_pc_monitor.bat", px, UI_TOP + 38, 1);
    txt("(NEEDS INTERNET ONCE)", px, UI_TOP + 50, 1);
  } else if (spOk[si]) {
    const SatPos& p = sp[si];
    spr.setTextColor(C_HOT()); txt(g_sats[si].name, px, UI_TOP + 2, 4);
    snprintf(buf, sizeof(buf), "ALT %d KM   %.2f KM/S", (int)p.altKm, p.speedKms);
    spr.setTextColor(UI_LABEL()); txt(buf, px, UI_TOP + 28, 1);
    snprintf(buf, sizeof(buf), "%.1f%c  %.1f%c", fabsf(p.lat), p.lat >= 0 ? 'N' : 'S', fabsf(p.lon), p.lon >= 0 ? 'E' : 'W');
    txt(buf, px, UI_TOP + 40, 1);
    // ближайший пролёт над городом: пересчёт раз в минуту (или если сменили спутник/город)
    static int lastSi = -1, lastCity = -1;
    if (t - s_passAt > 60 || lastSi != si || lastCity != g_city || (s_pass.found && t > s_pass.set)) {
      s_pass = satNextPass(g_sats[si], t, c.lat, c.lon, 10, 24);
      s_passAt = t; lastSi = si; lastCity = g_city;
    }
    float el, az; satLook(p, c.lat, c.lon, el, az);
    static const char* const DIR8[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    if (el >= 10) {
      spr.setTextColor(C_HOT()); txt("OVERHEAD NOW", px, UI_TOP + 58, 2);
      snprintf(buf, sizeof(buf), "LOOK %s, %d DEG UP", DIR8[((int)(az + 22.5f) / 45) % 8], (int)el);
      spr.setTextColor(C_ACC()); txt(buf, px, UI_TOP + 76, 1);
    } else if (s_pass.found) {
      spr.setTextColor(UI_LABEL()); txt("NEXT PASS OVER YOU", px, UI_TOP + 58, 1);
      hhmm(s_pass.rise, b2, sizeof(b2));
      long in = (long)(s_pass.rise - t);
      snprintf(buf, sizeof(buf), "%s", b2);
      spr.setTextColor(C_HOT()); txt(buf, px, UI_TOP + 70, 4);
      snprintf(buf, sizeof(buf), "IN %ldH %02ldM", in / 3600, in / 60 % 60);
      spr.setTextColor(C_ACC()); txt(buf, px, UI_TOP + 96, 1);
      snprintf(buf, sizeof(buf), "UP TO %d DEG, FROM %s", (int)s_pass.maxEl, DIR8[((int)(s_pass.azRise + 22.5f) / 45) % 8]);
      txt(buf, px, UI_TOP + 106, 1);
    } else {
      spr.setTextColor(UI_LABEL()); txt("NO PASS IN 24 H", px, UI_TOP + 58, 1);
    }
    float age = satAgeDays(g_sats[si], t);
    snprintf(buf, sizeof(buf), "ORBIT DATA %.1f DAYS OLD", age);
    spr.setTextColor(age > 7 ? C_ALERT() : UI_DIM()); txt(buf, px, UI_TOP + 120, 1);
  }
  const char* follow = s_gFollow == 1 ? "home" : (satIdx >= 0 && satIdx < g_nSats ? g_sats[satIdx].name : "ISS");
  snprintf(buf, sizeof(buf), "following %s", follow);
  uiHeader("Globe", buf);
  uiFooter("wheel: turn  click: follow", "side: back");
  present();
}
