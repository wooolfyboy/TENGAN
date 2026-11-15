// ============================================================
//  sat.h — спутники для Gallery › Globe
//
//  Орбиты (TLE) раз в несколько часов скачивает программа на ноутбуке
//  и присылает строкой «TLE\t<имя>\t<строка1>\t<строка2>». Палка
//  запоминает их в памяти настроек и дальше считает сама: орбита Кеплера
//  + вековые поправки от сплюснутости Земли (J2) — для картинки и прогноза
//  пролёта (точность — минуты) этого хватает на дни вперёд.
// ============================================================
#pragma once
#include <Arduino.h>

#define SAT_MAX 4
struct Sat {
  bool   ok = false;
  char   name[16] = "";
  double epochUnix = 0;                  // момент, к которому относятся элементы
  double n0, ndot2;                      // рад/мин, рад/мин²
  double inc, raan0, ecc, argp0, m0;     // радианы
  double a;                              // большая полуось, км
};
extern Sat g_sats[SAT_MAX];
extern int g_nSats;

struct SatPos { float lat, lon, altKm, speedKms; double x, y, z; };   // x,y,z — ECEF, км

void satBegin();                                      // загрузить сохранённые орбиты
void satFromLine(char* line);                         // строка «TLE\t...» с ноутбука
bool satPos(const Sat& s, double unixUtc, SatPos& out);
void satLook(const SatPos& p, float lat, float lon, float& elDeg, float& azDeg);   // как видно из города
float satAgeDays(const Sat& s, double unixUtc);
void satJson(String& o);                              // элементы орбит для планшета

// ближайший пролёт над городом (выше minEl): старт, конец, максимум
struct Pass { bool found; double rise, set; float maxEl; float azRise; };
Pass satNextPass(const Sat& s, double fromUnix, float lat, float lon, float minEl, float hours);
