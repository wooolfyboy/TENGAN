// ============================================================
//  timeutil.h — elapsed time without the unsigned-wrap trap.
//  now is taken at the start of a loop() pass, but t may be stamped LATER (a USB line or a BLE
//  packet from another task). Then now - t on uint32_t is ~4 billion, not -3 ms, and "long ago"
//  fires spuriously. msSince() returns 0 in that case.
// ============================================================
#pragma once
#include <stdint.h>

static inline uint32_t msSince(uint32_t now, uint32_t t) {
  int32_t d = (int32_t)(now - t);
  return d > 0 ? (uint32_t)d : 0;
}
