// ============================================================
//  subghz_model.h — данные раздела Sub-GHz, общие для логики
//  (subghz.cpp) и экранов (subghz_screens.cpp)
// ============================================================
#pragma once
#include <Arduino.h>

enum { G_SPECTRUM = 0, G_MONITOR, G_NUM };

struct Band { const char* name; float lo, hi, mon; };   // полоса развёртки и частота монитора
static const Band BANDS[] = {
  { "315 MHz", 313.0f, 317.0f, 315.00f },
  { "433 MHz", 431.92f, 435.92f, 433.92f },
  { "868 MHz", 866.3f, 870.3f, 868.30f },
  { "915 MHz", 913.0f, 917.0f, 915.00f },
};
static const int N_BANDS = 4;

extern int  g_gMode, g_band;
extern bool g_ccOk, g_ccTried;
extern uint8_t g_ccVersion;

// SPECTRUM
#define SP_N 96
#define WF_ROWS 56
extern int8_t  g_spCur[SP_N], g_spPeak[SP_N];
extern uint8_t (*g_wf)[SP_N];                // «водопад»: dBm + 120 (WF_ROWS строк, во внешней памяти)
extern int     g_wfHead, g_wfCount;
float spFreq(int i);

// MONITOR
#define MON_N 300
extern int8_t   g_mon[MON_N];
extern int      g_monN;
extern float    g_floor;                     // уровень шума
extern bool     g_burst;
extern uint32_t g_events;
struct Burst { uint32_t t; uint16_t durMs; int8_t peak; };
#define EV_N 6
extern Burst    g_ev[EV_N];
extern int      g_evN;
static const int BURST_ON_DB = 12, BURST_OFF_DB = 7;

// экраны (subghz_screens.cpp)
void drawSpectrum();
void drawMonitor(uint32_t now);
void drawCcMissing();
