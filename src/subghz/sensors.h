// ============================================================
//  sensors.h — 433 MHz sensors: table of what was heard, analyzer of
//  the last packet, "outdoor temperature" for Radio Weather.
//
//  Active while the CC1101 is in Monitor or Sensors mode (receive only):
//  it outputs demodulated OOK on GDO0, an interrupt measures durations
//  (sens_rx.cpp), ook.cpp parses packets.
//  Nothing is saved; everything stays in RAM.
// ============================================================
#pragma once
#include <Arduino.h>
#include "subghz/ook.h"

#define SENS_MAX 16

struct SensRec {
  const char* proto;                   // «Nexus», «LaCrosse-TX141TH», «Remote» ...
  uint32_t id;
  int8_t   ch;
  uint8_t  kind;                       // OOK_TH / OOK_T / OOK_REMOTE
  float    t;                          // °C
  int16_t  h;                          // %, -1 = none
  int8_t   bat;                        // 1 ok, 0 low, -1 unknown
  uint8_t  btn;                        // remote button (number)
  int8_t   rssi;                       // dBm of the last packet
  uint32_t lastMs, firstMs;
  uint16_t n;                          // times heard
};
extern SensRec  g_sens[SENS_MAX];
extern int      g_sensN;

// last packet (analyzer). pt = pulses/gaps for drawing, µs:
// sensors: up to 48 pairs; remotes and unknown: only the start (12 pairs), the code can't be recovered.
#define SENS_PT 48
struct SensLast {
  bool     valid = false;
  OokInfo  info;
  int8_t   rssi = -120;
  uint32_t ms = 0;
  uint16_t pt[SENS_PT * 2];
  uint8_t  ptN = 0;                    // pairs
  uint32_t durUs = 0;                  // duration of the whole packet
};
extern SensLast g_sensLast;
extern uint32_t g_sensGen;             // grows with each new packet (for the tablet stream)
extern uint32_t g_sensPackets;         // packets parsed in total
extern uint32_t g_sensNoise;           // dropped as noise

void sensFeed(const OokTrain& t, int rssi, uint32_t now);           // parse a packet
bool sensOutside(float& t, int& h, const char*& src, uint32_t now); // strongest thermometer within 10 min
int  sensSorted(uint8_t* idx, int max, uint32_t now);               // screen order: thermometers by strength, then remotes
void sensId(const SensRec& s, char* out, size_t n);                 // «nexus-1-0x5a», «remote-a3f1»
void sensJson(String& o, uint32_t now);                             // sens event for the tablet
void sensClear();

// reception (sens_rx.cpp; stubs in the simulator)
void sensRxStart();                    // GDO0 = data, interrupt enabled
void sensRxStop();
void sensRxTick(uint32_t now, int rssi);
bool sensRxOn();
