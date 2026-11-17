// ============================================================
//  battery.h — battery state (BQ27220 + BQ25896, read-only)
// ============================================================
#pragma once
#include <Arduino.h>

struct Bat {
  bool     ok = false;      // fuel gauge answered
  bool     chgOk = false;   // charger answered
  int      soc = 0;         // %
  int      mv = 0;          // mV
  int      ma = 0;          // mA: + charging, - discharging
  float    tempC = 0;
  int      tte = -1;        // minutes to empty (-1 = unknown)
  int      ttf = -1;        // minutes to full
  int      remain = 0, full = 0;   // mAh
  uint8_t  bus = 0;         // power source (0 = no USB)
  uint8_t  chg = 0;         // 0 idle, 1 precharge, 2 charging, 3 done
  uint32_t t = 0;
};
extern Bat g_bat;

void batBegin();
void batTick(uint32_t now);                  // re-reads sensors every 2 s
bool batUsb();                               // USB connected
bool batCharging();                          // charging
uint16_t batColor();
void batSummary(char* out, size_t n);        // one-line power summary
