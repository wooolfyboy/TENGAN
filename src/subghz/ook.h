// ============================================================
//  ook.h — 433 MHz (OOK) packet parsing: pulse analyzer and
//  decoders for the most common outdoor sensors. Receive/parse only.
//
//  Hardware-independent: the simulator and PC tests (tools/ook_test) build it too.
//  Protocols follow the open rtl_433 docs (formats, checksums); the code is original.
//
//  A packet (OokTrain) is alternating carrier-on (pulse) and silence
//  (gap) in microseconds, as the CC1101 outputs on GDO0.
// ============================================================
#pragma once
#include <stdint.h>
#include <string.h>

#define OOK_MAXP 1024                      // pulses per packet (TX141TH: 12 repeats ≈ 530)

struct OokTrain {
  uint16_t p[OOK_MAXP];                    // pulse, µs
  uint16_t g[OOK_MAXP];                    // gap after the pulse, µs (the last one is long)
  int n = 0;
};

enum : uint8_t { OOK_TH = 0, OOK_T, OOK_REMOTE };   // device type

struct OokMsg {
  const char* proto = "?";                 // «Nexus», «LaCrosse-TX141TH», «Remote» ...
  uint32_t id = 0;                         // sensor id (or remote hash)
  int8_t   ch = 0;                         // channel 1..3 (0 = none)
  uint8_t  kind = OOK_TH;
  float    t = 0;                          // °C
  int16_t  h = -1;                         // %, -1 = none
  int8_t   bat = -1;                       // 1 = ok, 0 = low, -1 = unknown
  uint8_t  btn = 0;                        // remote button (number, not the code)
  uint8_t  row[12] = {};                   // packet bytes (sensors only, for the analyzer)
  uint8_t  rowBits = 0;
};

struct OokInfo {
  int pulses = 0, bits = 0, rows = 0;
  const char* mod = "?";                   // PWM / PPM / Manchester / ?
  uint16_t shortW = 0, longW = 0;          // pulse widths, µs
  uint16_t shortG = 0, longG = 0, syncG = 0;   // gaps, µs
  const char* cls = "noise";               // sensor / remote / rolling / unknown / noise
  const char* proto = "?";
  char hex[25] = "";                       // bytes: sensors ONLY (we never show a remote's code)
  uint16_t hash = 0;                       // packet fingerprint (instead of the code)
};

// find sensors/remotes in a packet; returns how many were written to out
int  ookDecode(const OokTrain& t, OokMsg* out, int maxOut);
// parse the packet shape (always); dec = what ookDecode found
void ookAnalyze(const OokTrain& t, OokInfo& info, const OokMsg* dec, int nDec);
