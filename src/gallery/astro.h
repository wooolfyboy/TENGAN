// ============================================================
//  astro.h — время, место и небо для Gallery › Sky и Globe
//
//  Время: точное UTC приходит с ноутбука (строка PC, поле ts=) и
//  записывается в часы чипа — они идут и в «выключенном» состоянии,
//  пока жива батарея. Город — Settings › General › City.
// ============================================================
#pragma once
#include <Arduino.h>

struct City { const char* name; float lat, lon; };
extern const City CITIES[];
extern const int  N_CITIES;
// «GPS» — особый город с индексом N_CITIES: координаты присылает приложение планшета (/api/loc).
// Есть в списке, только когда координаты известны.
extern City g_gpsCity;
extern bool g_gpsKnown;
const City& curCity();                  // текущий город для Sky / Globe (с учётом GPS)
int  cityCount();                       // сколько вариантов в настройках (N_CITIES или N_CITIES + 1)
void setGpsCity(float lat, float lon);  // пришли координаты с планшета: запомнить и выбрать «GPS»

// время
void   astroSetTime(uint32_t unixUtc, int32_t tzOffsetSec);   // пришло с ноутбука
bool   astroTimeKnown();
double astroUnixNow();                 // секунды UTC (с долями)
int32_t astroTzOffset();               // смещение местного времени, с

// небесные координаты
double julianDate(double unixSec);
double gmstDeg(double jd);             // звёздное время по Гринвичу, градусы
struct RaDec { float ra, dec; };       // градусы
struct AltAz { float alt, az; };       // градусы; az: 0 — север, 90 — восток
AltAz  toAltAz(float raDeg, float decDeg, double jd, float latDeg, float lonDeg);
// быстрый вариант для сотен звёзд за кадр: звёздное время и широта считаются один раз (skyFrame),
// дальше — только float (у ESP32-S3 аппаратный float, а double считается программно и медленно)
struct SkyFrame { float lst, sinLat, cosLat; };
SkyFrame skyFrame(double jd, float latDeg, float lonDeg);
AltAz  toAltAzF(const SkyFrame& f, float raDeg, float decDeg);

// Солнце, Луна, планеты (точность — доли градуса, для картинки хватает)
RaDec  sunRaDec(double jd);
RaDec  moonRaDec(double jd, float* phase01);    // phase01: 0 новолуние .. 1 полнолуние
enum { PL_MERCURY, PL_VENUS, PL_MARS, PL_JUPITER, PL_SATURN, PL_NUM };
RaDec  planetRaDec(int p, double jd);
extern const char* const PLANET_RU[PL_NUM];
