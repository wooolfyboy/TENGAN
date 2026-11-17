// ============================================================
//  perf.h — cheap main-loop timing (stream st, field "lp"). Always on: two micros() calls per pass.
// ============================================================
#pragma once
#include <Arduino.h>

struct Perf {
  // result for the last ~2 s (updated in perfTick)
  uint16_t loopAvgUs = 0, loopMaxUs = 0;   // one loop() pass without delay()
  uint16_t drawAvgUs = 0, drawMaxUs = 0;   // frame draw + screen push
  uint8_t  fps = 0;                        // frames per second on the device screen
  uint8_t  pushPct = 0;                    // share of screen rows actually sent (the rest were unchanged)
  // accumulators
  uint32_t lSum = 0, lMax = 0, lN = 0, dSum = 0, dMax = 0, dN = 0, rows = 0, rowsAll = 0, t0 = 0;
};
extern Perf g_perf;

inline void perfLoop(uint32_t us) { g_perf.lSum += us; g_perf.lN++; if (us > g_perf.lMax) g_perf.lMax = us; }
inline void perfDraw(uint32_t us) { g_perf.dSum += us; g_perf.dN++; if (us > g_perf.dMax) g_perf.dMax = us; }
inline void perfRows(uint32_t pushed, uint32_t all) { g_perf.rows += pushed; g_perf.rowsAll += all; }
void perfTick(uint32_t now);               // every ~2 s folds accumulators into the result
