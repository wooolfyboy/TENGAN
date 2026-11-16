// ============================================================
//  astro.cpp — время, город и положения светил
//  Планеты — по средним элементам орбит JPL (E. M. Standish, 1800–2050),
//  Луна — укороченный ряд из «Астрономических алгоритмов» (Meeus).
// ============================================================
#include "gallery/astro.h"
#include "core/lang.h"
#include <sys/time.h>
#include <time.h>
#include "core/settings.h"

const City CITIES[] = {
  { L("Moscow", "Москва"), 55.756f, 37.617f },        { L("Saint Petersburg", "Санкт-Петербург"), 59.939f, 30.316f },
  { L("Novosibirsk", "Новосибирск"), 55.030f, 82.920f },   { L("Yekaterinburg", "Екатеринбург"), 56.838f, 60.605f },
  { L("Kazan", "Казань"), 55.796f, 49.106f },         { L("Nizhny Novgorod", "Нижний Новгород"), 56.327f, 44.006f },
  { L("Samara", "Самара"), 53.195f, 50.100f },        { L("Krasnodar", "Краснодар"), 45.035f, 38.975f },
  { L("Rostov-on-Don", "Ростов-на-Дону"), 47.222f, 39.718f }, { L("Vladivostok", "Владивосток"), 43.115f, 131.886f },
  { L("Irkutsk", "Иркутск"), 52.287f, 104.305f },      { L("Minsk", "Минск"), 53.902f, 27.562f },
  { L("Kyiv", "Киев"), 50.450f, 30.523f },          { L("Almaty", "Алматы"), 43.238f, 76.946f },
  { L("Tbilisi", "Тбилиси"), 41.716f, 44.783f },       { L("Yerevan", "Ереван"), 40.183f, 44.515f },
  { L("Berlin", "Берлин"), 52.520f, 13.405f },        { L("London", "Лондон"), 51.507f, -0.128f },
  { L("New York", "Нью-Йорк"), 40.713f, -74.006f },     { L("Tokyo", "Токио"), 35.676f, 139.650f },
};
const int N_CITIES = sizeof(CITIES) / sizeof(CITIES[0]);

City g_gpsCity = { "GPS", 0, 0 };
bool g_gpsKnown = false;
const City& curCity() { return (g_city == N_CITIES && g_gpsKnown) ? g_gpsCity : CITIES[g_city < N_CITIES ? g_city : 0]; }
int  cityCount() { return g_gpsKnown ? N_CITIES + 1 : N_CITIES; }
void setGpsCity(float lat, float lon) {
  g_gpsCity.lat = lat; g_gpsCity.lon = lon; g_gpsKnown = true;
  g_city = N_CITIES;
  settingsTouch();                      // сохранится вместе с остальными настройками
}

static const double D2R = PI / 180.0, R2D = 180.0 / PI;
static double norm360(double a) { a = fmod(a, 360.0); return a < 0 ? a + 360.0 : a; }

// ---------- время ----------
static int32_t s_tz = 0;
void astroSetTime(uint32_t unixUtc, int32_t tz) {
  s_tz = tz;
  time_t cur = time(nullptr);
  if (llabs((long long)cur - (long long)unixUtc) > 2) {        // часы чипа сильно ушли — поправить
    struct timeval tv = { (time_t)unixUtc, 0 };
    settimeofday(&tv, nullptr);
  }
  if (g_tzOff != tz) { g_tzOff = tz; settingsTouch(); }       // запомнить (для «без ноутбука»)
}
bool astroTimeKnown() { return time(nullptr) > 1700000000; }
double astroUnixNow() {
  struct timeval tv; gettimeofday(&tv, nullptr);
  return tv.tv_sec + tv.tv_usec / 1e6;
}
int32_t astroTzOffset() { return s_tz ? s_tz : g_tzOff; }

double julianDate(double unixSec) { return unixSec / 86400.0 + 2440587.5; }
double gmstDeg(double jd) {
  double d = jd - 2451545.0, T = d / 36525.0;
  return norm360(280.46061837 + 360.98564736629 * d + 0.000387933 * T * T);
}

AltAz toAltAz(float ra, float dec, double jd, float lat, float lon) {
  double H = norm360(gmstDeg(jd) + lon - ra) * D2R;
  double d = dec * D2R, p = lat * D2R;
  double sa = sin(p) * sin(d) + cos(p) * cos(d) * cos(H);
  double alt = asin(constrain(sa, -1.0, 1.0));
  double az = atan2(-cos(d) * sin(H), sin(d) * cos(p) - cos(d) * sin(p) * cos(H));
  AltAz r = { (float)(alt * R2D), (float)norm360(az * R2D) };
  return r;
}

SkyFrame skyFrame(double jd, float lat, float lon) {
  SkyFrame f;
  f.lst = (float)norm360(gmstDeg(jd) + lon);           // местное звёздное время, градусы
  f.sinLat = sinf(lat * (float)D2R); f.cosLat = cosf(lat * (float)D2R);
  return f;
}
AltAz toAltAzF(const SkyFrame& f, float ra, float dec) {
  const float d2r = (float)D2R, r2d = (float)R2D;
  float H = (f.lst - ra) * d2r, d = dec * d2r;
  float sd = sinf(d), cd = cosf(d), cH = cosf(H);
  float sa = f.sinLat * sd + f.cosLat * cd * cH;
  float alt = asinf(sa < -1 ? -1 : sa > 1 ? 1 : sa);
  float az = atan2f(-cd * sinf(H), sd * f.cosLat - cd * f.sinLat * cH) * r2d;
  if (az < 0) az += 360;
  AltAz r = { alt * r2d, az };
  return r;
}

// ---------- эклиптика -> экватор ----------
static RaDec eclToEq(double lonDeg, double latDeg) {
  const double eps = 23.43928 * D2R;
  double l = lonDeg * D2R, b = latDeg * D2R;
  double x = cos(b) * cos(l);
  double y = cos(eps) * cos(b) * sin(l) - sin(eps) * sin(b);
  double z = sin(eps) * cos(b) * sin(l) + cos(eps) * sin(b);
  RaDec r = { (float)norm360(atan2(y, x) * R2D), (float)(asin(z) * R2D) };
  return r;
}

// ---------- планеты: элементы J2000 и их скорости за столетие ----------
struct Elem { double a, da, e, de, I, dI, L, dL, w, dw, N, dN; };
static const Elem EL[] = {
  { 0.38709927, 0.00000037, 0.20563593, 0.00001906, 7.00497902, -0.00594749, 252.25032350, 149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081 },
  { 0.72333566, 0.00000390, 0.00677672, -0.00004107, 3.39467605, -0.00078890, 181.97909950, 58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418 },
  { 1.52371034, 0.00001847, 0.09339410, 0.00007882, 1.84969142, -0.00813131, -4.55343205, 19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343 },
  { 5.20288700, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714, 34.39644051, 3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106 },
  { 9.53667594, -0.00125060, 0.05386179, -0.00050991, 2.48599187, 0.00193609, 49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794 },
};
static const Elem EARTH = { 1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166, 35999.37244981, 102.93768193, 0.32327364, 0.0, 0.0 };
const char* const PLANET_RU[PL_NUM] = { L("Mercury", "Меркурий"), L("Venus", "Венера"), L("Mars", "Марс"), L("Jupiter", "Юпитер"), L("Saturn", "Сатурн") };   // имена на языке прошивки

static void helio(const Elem& el, double T, double& x, double& y, double& z) {
  double a = el.a + el.da * T, e = el.e + el.de * T, I = (el.I + el.dI * T) * D2R;
  double L = el.L + el.dL * T, w = el.w + el.dw * T, N = (el.N + el.dN * T) * D2R;
  double M = norm360(L - w) * D2R, om = (w * D2R) - N;     // аргумент перицентра
  double E = M;
  for (int k = 0; k < 8; k++) E = E - (E - e * sin(E) - M) / (1 - e * cos(E));
  double xp = a * (cos(E) - e), yp = a * sqrt(1 - e * e) * sin(E);
  double co = cos(om), so = sin(om), cN = cos(N), sN = sin(N), cI = cos(I), sI = sin(I);
  x = (co * cN - so * sN * cI) * xp + (-so * cN - co * sN * cI) * yp;
  y = (co * sN + so * cN * cI) * xp + (-so * sN + co * cN * cI) * yp;
  z = (so * sI) * xp + (co * sI) * yp;
}

RaDec planetRaDec(int p, double jd) {
  double T = (jd - 2451545.0) / 36525.0, x, y, z, ex, ey, ez;
  helio(EL[p], T, x, y, z);
  helio(EARTH, T, ex, ey, ez);
  x -= ex; y -= ey; z -= ez;
  return eclToEq(atan2(y, x) * R2D, atan2(z, sqrt(x * x + y * y)) * R2D);
}

RaDec sunRaDec(double jd) {
  double T = (jd - 2451545.0) / 36525.0, ex, ey, ez;
  helio(EARTH, T, ex, ey, ez);
  return eclToEq(atan2(-ey, -ex) * R2D, 0);
}

RaDec moonRaDec(double jd, float* phase01) {
  double d = jd - 2451545.0;
  double L = norm360(218.316 + 13.176396 * d), M = norm360(134.963 + 13.064993 * d) * D2R;
  double F = norm360(93.272 + 13.229350 * d) * D2R, D = norm360(297.850 + 12.190749 * d) * D2R;
  double Ms = norm360(357.529 + 0.98560028 * d) * D2R;
  double lon = L + 6.289 * sin(M) + 1.274 * sin(2 * D - M) + 0.658 * sin(2 * D) - 0.186 * sin(Ms) + 0.214 * sin(2 * M);
  double lat = 5.128 * sin(F);
  if (phase01) *phase01 = (float)((1 - cos(D)) / 2);           // освещённая доля по элонгации
  return eclToEq(lon, lat);
}
