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
