// perf.cpp — сводка замеров скорости (см. perf.h)
#include "core/perf.h"
#include <algorithm>

Perf g_perf;
void perfTick(uint32_t now) {
  Perf& P = g_perf;
  if (now - P.t0 < 2000) return;
  uint32_t dt = now - P.t0; P.t0 = now;
  P.loopAvgUs = P.lN ? std::min<uint32_t>(65535, P.lSum / P.lN) : 0; P.loopMaxUs = std::min<uint32_t>(65535, P.lMax);
  P.drawAvgUs = P.dN ? std::min<uint32_t>(65535, P.dSum / P.dN) : 0; P.drawMaxUs = std::min<uint32_t>(65535, P.dMax);
  P.fps = (uint8_t)std::min<uint32_t>(255, P.dN * 1000 / std::max<uint32_t>(1, dt));
  P.pushPct = P.rowsAll ? (uint8_t)(P.rows * 100 / P.rowsAll) : 0;
  P.lSum = P.lMax = P.lN = P.dSum = P.dMax = P.dN = P.rows = P.rowsAll = 0;
}
