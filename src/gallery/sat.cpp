// ============================================================
//  sat.cpp — разбор TLE и упрощённый расчёт орбиты (Кеплер + J2)
// ============================================================
#include "gallery/sat.h"
#include "core/jsonutil.h"
#include "gallery/astro.h"
#include <Preferences.h>

Sat g_sats[SAT_MAX];
int g_nSats = 0;

static const double MU = 398600.4418;          // км³/с²
static const double RE = 6378.137, J2 = 1.08262668e-3;
static const double TWO_PI_ = 2 * PI, D2R = PI / 180.0;

static double fld(const char* s, int from, int len) {      // число из колонок TLE (с 1)
  char b[24]; int n = min(len, 23);
  memcpy(b, s + from - 1, n); b[n] = 0;
  return atof(b);
}
static bool parseTle(const char* name, const char* l1, const char* l2, Sat& s) {
  if (strlen(l1) < 63 || strlen(l2) < 63 || l1[0] != '1' || l2[0] != '2') return false;
  int yy = (int)fld(l1, 19, 2);
  double doy = fld(l1, 21, 12);
  int year = yy < 57 ? 2000 + yy : 1900 + yy;
  // юникс-время начала года
  long days = 0;
  for (int y = 1970; y < year; y++) days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
  s.epochUnix = (days + doy - 1) * 86400.0;
  double ndot2 = fld(l1, 34, 10);                          // (dn/dt)/2, об/сут²
  s.inc   = fld(l2, 9, 8) * D2R;
  s.raan0 = fld(l2, 18, 8) * D2R;
  char e[10] = "0."; memcpy(e + 2, l2 + 26, 7); e[9] = 0;
  s.ecc   = atof(e);
  s.argp0 = fld(l2, 35, 8) * D2R;
  s.m0    = fld(l2, 44, 8) * D2R;
  double revDay = fld(l2, 53, 11);
  s.n0    = revDay * TWO_PI_ / 1440.0;                     // рад/мин
  s.ndot2 = ndot2 * TWO_PI_ / (1440.0 * 1440.0);
  double nSec = s.n0 / 60.0;
  s.a = pow(MU / (nSec * nSec), 1.0 / 3.0);
  strncpy(s.name, name, sizeof(s.name) - 1); s.name[sizeof(s.name) - 1] = 0;
  s.ok = revDay > 0.5 && s.ecc < 0.9;
  return s.ok;
}

// ---------- хранение ----------
static void satSave(int i, const char* name, const char* l1, const char* l2) {
  Preferences p; p.begin("sats", false);
  char k[8];
  snprintf(k, sizeof(k), "n%d", i); p.putString(k, name);
  snprintf(k, sizeof(k), "a%d", i); p.putString(k, l1);
  snprintf(k, sizeof(k), "b%d", i); p.putString(k, l2);
  p.end();
}
void satBegin() {
  Preferences p; p.begin("sats", true);
  g_nSats = 0;
  for (int i = 0; i < SAT_MAX; i++) {
    char k[8];
    snprintf(k, sizeof(k), "n%d", i); String n = p.getString(k, "");
    snprintf(k, sizeof(k), "a%d", i); String a = p.getString(k, "");
    snprintf(k, sizeof(k), "b%d", i); String b = p.getString(k, "");
    if (n.length() && parseTle(n.c_str(), a.c_str(), b.c_str(), g_sats[g_nSats])) g_nSats++;
  }
  p.end();
}

// «TLE\t<имя>\t<строка1>\t<строка2>»
void satFromLine(char* line) {
  char* save = nullptr;
  char* f0 = strtok_r(line, "\t", &save);
  char* name = strtok_r(nullptr, "\t", &save);
  char* l1 = strtok_r(nullptr, "\t", &save);
  char* l2 = strtok_r(nullptr, "\t", &save);
  if (!f0 || !name || !l1 || !l2) return;
  Sat s;
  if (!parseTle(name, l1, l2, s)) return;
  int slot = -1;
  for (int i = 0; i < g_nSats; i++) if (!strcmp(g_sats[i].name, s.name)) slot = i;
  if (slot < 0) { if (g_nSats >= SAT_MAX) return; slot = g_nSats++; }
  bool changed = fabs(g_sats[slot].epochUnix - s.epochUnix) > 1;
  g_sats[slot] = s;
  if (changed) satSave(slot, name, l1, l2);                  // в память — только новые элементы
}

float satAgeDays(const Sat& s, double t) { return (float)((t - s.epochUnix) / 86400.0); }

// ---------- положение ----------
bool satPos(const Sat& s, double t, SatPos& out) {
  if (!s.ok) return false;
  double tm = (t - s.epochUnix) / 60.0;                      // минуты от эпохи
  double cosi = cos(s.inc), sini = sin(s.inc);
  double p = s.a * (1 - s.ecc * s.ecc);
  double k = 1.5 * J2 * (RE / p) * (RE / p) * s.n0;
  double raan = s.raan0 - k * cosi * tm;
  double argp = s.argp0 + k * (2 - 2.5 * sini * sini) * tm;
  double M = fmod(s.m0 + s.n0 * tm + s.ndot2 * tm * tm, TWO_PI_);
  double E = M;
  for (int i = 0; i < 10; i++) E -= (E - s.ecc * sin(E) - M) / (1 - s.ecc * cos(E));
  double nu = 2 * atan2(sqrt(1 + s.ecc) * sin(E / 2), sqrt(1 - s.ecc) * cos(E / 2));
  double r = s.a * (1 - s.ecc * cos(E));
  double u = argp + nu;
  // инерциальная система (ECI)
  double cu = cos(u), su = sin(u), cO = cos(raan), sO = sin(raan);
  double x = r * (cO * cu - sO * su * cosi);
  double y = r * (sO * cu + cO * su * cosi);
  double z = r * (su * sini);
  // поворот Земли: ECI -> ECEF
  double g = gmstDeg(julianDate(t)) * D2R;
  double xe = x * cos(g) + y * sin(g), ye = -x * sin(g) + y * cos(g);
  out.x = xe; out.y = ye; out.z = z;
  double rr = sqrt(xe * xe + ye * ye + z * z);
  out.lat = (float)(asin(z / rr) / D2R);
  out.lon = (float)(atan2(ye, xe) / D2R);
  out.altKm = (float)(rr - RE);
  out.speedKms = (float)sqrt(MU * (2 / r - 1 / s.a));
  return true;
}

void satLook(const SatPos& p, float lat, float lon, float& el, float& az) {
  double la = lat * D2R, lo = lon * D2R;
  double ox = RE * cos(la) * cos(lo), oy = RE * cos(la) * sin(lo), oz = RE * sin(la);
  double rx = p.x - ox, ry = p.y - oy, rz = p.z - oz;
  // топоцентрические оси: восток, север, вверх
  double e = -sin(lo) * rx + cos(lo) * ry;
  double n = -sin(la) * cos(lo) * rx - sin(la) * sin(lo) * ry + cos(la) * rz;
  double u = cos(la) * cos(lo) * rx + cos(la) * sin(lo) * ry + sin(la) * rz;
  double rng = sqrt(rx * rx + ry * ry + rz * rz);
  el = (float)(asin(u / rng) / D2R);
  az = (float)(fmod(atan2(e, n) / D2R + 360.0, 360.0));
}

Pass satNextPass(const Sat& s, double from, float lat, float lon, float minEl, float hours) {
  Pass ps = { false, 0, 0, 0, 0 };
  SatPos p; float el, az;
  double end = from + hours * 3600;
  bool up = false;
  for (double t = from; t < end; t += 30) {
    if (!satPos(s, t, p)) return ps;
    satLook(p, lat, lon, el, az);
    if (el >= minEl) {
      if (!up) {                                           // уточнить момент восхода шагом 2 с
        double tr = t;
        for (double q = t - 30; q <= t; q += 2) { satPos(s, q, p); float e2, a2; satLook(p, lat, lon, e2, a2); if (e2 >= minEl) { tr = q; ps.azRise = a2; break; } }
        if (t == from) { ps.azRise = az; tr = from; }
        ps.rise = tr; up = true; ps.found = true;
      }
      if (el > ps.maxEl) ps.maxEl = el;
    } else if (up) { ps.set = t; return ps; }
  }
  if (up) ps.set = end;
  return ps;
}

// для планшета: элементы орбит, дальше считает браузер (тот же Кеплер + J2)
// [[имя,эпоха_unix,n0,ndot2,inc,raan0,ecc,argp0,m0,a],...]
void satJson(String& o) {
  o += '[';
  for (int i = 0; i < g_nSats; i++) {
    const Sat& s = g_sats[i];
    if (i) o += ',';
    o += '['; jsonStr(o, s.name); o += ','; o += String(s.epochUnix, 0);
    const double v[] = { s.n0, s.ndot2, s.inc, s.raan0, s.ecc, s.argp0, s.m0, s.a };
    for (double x : v) { char b[24]; snprintf(b, sizeof(b), ",%.10g", x); o += b; }
    o += ']';
  }
  o += ']';
}
