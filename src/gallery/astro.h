// ============================================================
//  astro.h — time, place and sky for Gallery › Sky and Globe
//
//  Time: exact UTC comes from the laptop (PC line, ts= field) and is
//  written to the chip clock, which keeps running while "off"
//  as long as the battery lasts. City: Settings › General › City.
// ============================================================
#pragma once
#include <Arduino.h>

struct City { const char* name; float lat, lon; };
extern const City CITIES[];
extern const int  N_CITIES;
// "GPS" is a special city at index N_CITIES: the tablet app sends coordinates (/api/loc).
// Listed only when the coordinates are known.
extern City g_gpsCity;
extern bool g_gpsKnown;
const City& curCity();                  // current city for Sky / Globe (GPS included)
int  cityCount();                       // number of options in settings (N_CITIES or N_CITIES + 1)
void setGpsCity(float lat, float lon);  // coordinates came from the tablet: store and select "GPS"

// time
void   astroSetTime(uint32_t unixUtc, int32_t tzOffsetSec);   // from the laptop
bool   astroTimeKnown();
double astroUnixNow();                 // UTC seconds (fractional)
int32_t astroTzOffset();               // local time offset, s

// celestial coordinates
double julianDate(double unixSec);
double gmstDeg(double jd);             // Greenwich sidereal time, degrees
struct RaDec { float ra, dec; };       // degrees
struct AltAz { float alt, az; };       // degrees; az: 0 = north, 90 = east
AltAz  toAltAz(float raDeg, float decDeg, double jd, float latDeg, float lonDeg);
// fast path for hundreds of stars per frame: sidereal time and latitude are computed once (skyFrame),
// then float only (ESP32-S3 has hardware float; double is software and slow)
struct SkyFrame { float lst, sinLat, cosLat; };
SkyFrame skyFrame(double jd, float latDeg, float lonDeg);
AltAz  toAltAzF(const SkyFrame& f, float raDeg, float decDeg);

// Sun, Moon, planets (accuracy: fractions of a degree, enough for the picture)
RaDec  sunRaDec(double jd);
RaDec  moonRaDec(double jd, float* phase01);    // phase01: 0 new moon .. 1 full moon
enum { PL_MERCURY, PL_VENUS, PL_MARS, PL_JUPITER, PL_SATURN, PL_NUM };
RaDec  planetRaDec(int p, double jd);
extern const char* const PLANET_RU[PL_NUM];
