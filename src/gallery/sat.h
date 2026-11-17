// ============================================================
//  sat.h — satellites for Gallery › Globe
//
//  The laptop program downloads orbits (TLE) every few hours
//  and sends them as "TLE\t<name>\t<line1>\t<line2>". The stick
//  stores them in settings memory and computes on its own: a Kepler orbit
//  + secular J2 (Earth oblateness) corrections; enough for the picture and pass
//  prediction (accuracy: minutes) for days ahead.
// ============================================================
#pragma once
#include <Arduino.h>

#define SAT_MAX 4
struct Sat {
  bool   ok = false;
  char   name[16] = "";
  double epochUnix = 0;                  // epoch the elements refer to
  double n0, ndot2;                      // rad/min, rad/min²
  double inc, raan0, ecc, argp0, m0;     // radians
  double a;                              // semi-major axis, km
};
extern Sat g_sats[SAT_MAX];
extern int g_nSats;

struct SatPos { float lat, lon, altKm, speedKms; double x, y, z; };   // x,y,z — ECEF, km

void satBegin();                                      // load saved orbits
void satFromLine(char* line);                         // "TLE\t..." line from the laptop
bool satPos(const Sat& s, double unixUtc, SatPos& out);
void satLook(const SatPos& p, float lat, float lon, float& elDeg, float& azDeg);   // as seen from the city
float satAgeDays(const Sat& s, double unixUtc);
void satJson(String& o);                              // orbit elements for the tablet

// next pass over the city (above minEl): start, end, peak
struct Pass { bool found; double rise, set; float maxEl; float azRise; };
Pass satNextPass(const Sat& s, double fromUnix, float lat, float lon, float minEl, float hours);
