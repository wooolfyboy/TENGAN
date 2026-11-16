// ============================================================
//  perf.h — лёгкий замер скорости основного цикла (для планшета: поток st, поле "lp").
//  Считается всегда, стоит копейки: два вызова micros() на проход цикла.
// ============================================================
#pragma once
#include <Arduino.h>

struct Perf {
  // итог за последние ~2 с (обновляется в perfTick)
  uint16_t loopAvgUs = 0, loopMaxUs = 0;   // работа одного прохода loop() без delay()
  uint16_t drawAvgUs = 0, drawMaxUs = 0;   // рисование кадра + вывод на экран
  uint8_t  fps = 0;                        // кадров в секунду на экране палки
  uint8_t  pushPct = 0;                    // какую долю строк экрана реально отправили (остальные не менялись)
  // накопители
  uint32_t lSum = 0, lMax = 0, lN = 0, dSum = 0, dMax = 0, dN = 0, rows = 0, rowsAll = 0, t0 = 0;
};
extern Perf g_perf;

inline void perfLoop(uint32_t us) { g_perf.lSum += us; g_perf.lN++; if (us > g_perf.lMax) g_perf.lMax = us; }
inline void perfDraw(uint32_t us) { g_perf.dSum += us; g_perf.dN++; if (us > g_perf.dMax) g_perf.dMax = us; }
inline void perfRows(uint32_t pushed, uint32_t all) { g_perf.rows += pushed; g_perf.rowsAll += all; }
void perfTick(uint32_t now);               // раз в ~2 с сводит накопители в итог
