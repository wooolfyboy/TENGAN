// ============================================================
//  subghz.cpp — раздел Sub-GHz на CC1101
//
//  ТОЛЬКО ПРИЁМ: модуль никогда не переводится в передачу (STX не
//  используется). Мерим силу сигнала (RSSI) на частотах 315 / 433 / 868 / 915 МГц.
//
//  Экраны:
//    SPECTRUM — развёртка по полосе ±2 МГц + «водопад»
//    MONITOR  — слушаем одну частоту и ловим всплески (брелоки, звонки, датчики)
// ============================================================
#include "subghz/subghz.h"
#include "subghz/cc1101.h"
#include "subghz/subghz_model.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"
#include "core/menu.h"


int  g_gMode = 0;
bool g_ccOk = false, g_ccTried = false, g_ccOn = false;

int g_band = 1;


// ---------- SPECTRUM ----------
int8_t  g_spCur[SP_N], g_spPeak[SP_N];
uint8_t g_wf[WF_ROWS][SP_N];
int     g_wfHead = 0, g_wfCount = 0;
int     g_spIdx = 0;
int8_t  g_spRow[SP_N];

float spFreq(int i) { const Band& b = BANDS[g_band]; return b.lo + (b.hi - b.lo) * i / (SP_N - 1); }

static void spReset() {
  for (int i = 0; i < SP_N; i++) { g_spCur[i] = -110; g_spPeak[i] = -110; g_spRow[i] = -110; }
  g_wfHead = 0; g_wfCount = 0; g_spIdx = 0;
}

static void spTick() {
  for (int k = 0; k < 12; k++) {               // ~12 точек за проход цикла (~15 мс)
    int r = ccMeasure(spFreq(g_spIdx));
    r = constrain(r, -120, -10);
    g_spRow[g_spIdx] = r;
    g_spCur[g_spIdx] = r;
    if (r > g_spPeak[g_spIdx]) g_spPeak[g_spIdx] = r;
    if (++g_spIdx >= SP_N) {
      g_spIdx = 0;
      for (int i = 0; i < SP_N; i++) {         // строка водопада
        g_wf[g_wfHead][i] = (uint8_t)constrain(g_spRow[i] + 120, 0, 110);
        if (g_spPeak[i] > -110) g_spPeak[i]--; // пики медленно оседают
      }
      g_wfHead = (g_wfHead + 1) % WF_ROWS;
      if (g_wfCount < WF_ROWS) g_wfCount++;
    }
  }
}




// ---------- MONITOR ----------
int8_t   g_mon[MON_N];
int      g_monN = 0;
float    g_floor = -100;          // уровень шума (медленное среднее)
bool     g_burst = false;
uint32_t g_burstStart = 0, g_burstLastHi = 0;
int      g_burstPeak = -120;
uint32_t g_events = 0;
Burst    g_ev[EV_N];
int      g_evN = 0;

static void monStart() {
  ccStrobe(0x36);
  ccSetBw(0xA5);                               // пошире (~270 кГц) — брелоки «гуляют» по частоте
  ccSetFreq(BANDS[g_band].mon);
  ccStrobe(0x34);                              // SRX
  g_monN = 0; g_burst = false;
}

static void monTick(uint32_t now) {
  int mx = -127;
  for (int k = 0; k < 8; k++) {                // несколько замеров подряд, берём максимум
    int r = ccRssi();
    if (r > mx) mx = r;
    delayMicroseconds(300);
  }
  if (g_monN < MON_N) g_mon[g_monN++] = mx;
  else { memmove(g_mon, g_mon + 1, MON_N - 1); g_mon[MON_N - 1] = mx; }

  if (!g_burst) {
    g_floor = g_floor * 0.98f + mx * 0.02f;    // шум считаем только вне всплесков
    if (mx > g_floor + BURST_ON_DB) {
      g_burst = true; g_burstStart = now; g_burstLastHi = now; g_burstPeak = mx;
      strip.clear();                            // вспышка ленты
      for (int i = 0; i < WS2812_N; i++) strip.setPixelColor(i, strip.Color(40, 30, 0));
      strip.show();
    }
  } else {
    if (mx > g_floor + BURST_OFF_DB) { g_burstLastHi = now; if (mx > g_burstPeak) g_burstPeak = mx; }
    if (now - g_burstLastHi > 150) {           // тишина 150 мс — всплеск закончился
      g_burst = false;
      g_events++;
      Burst b = { g_burstStart, (uint16_t)min<uint32_t>(g_burstLastHi - g_burstStart, 65000), (int8_t)g_burstPeak };
      if (g_evN < EV_N) g_ev[g_evN++] = b;
      else { memmove(g_ev, g_ev + 1, sizeof(Burst) * (EV_N - 1)); g_ev[EV_N - 1] = b; }
      strip.clear(); strip.show();
    }
  }
}


// ---------- раздел целиком ----------
static void subEnter() {
  if (!g_ccTried) { g_ccTried = true; g_ccOk = ccInit(); spReset(); }
  if (!g_ccOk) return;
  ccAntenna(BANDS[g_band].mon);
  ccStrobe(0x36);
  g_ccOn = true;
}
static void subLeave() { if (g_ccOk) ccSleep(); g_ccOn = false; ledsOff(); }

static void subStartMode() {
  if (!g_ccOk) return;
  ccAntenna(BANDS[g_band].mon);
  if (g_gMode == G_SPECTRUM) { ccSetBw(0xF5); spReset(); }
  else monStart();
  strip.clear(); strip.show();
}
static void subOpenScreen(int m) { g_gMode = m; subStartMode(); }
static void subCloseScreen() { if (g_ccOk) ccStrobe(0x36); strip.clear(); strip.show(); }
static int  subScreenNow() { return g_gMode; }
static void subRotate(int dir) {
  static uint32_t last = 0;
  if (millis() - last < 120) return;          // экраны не проскакивают при быстром вращении
  last = millis();
  g_gMode = (g_gMode + dir + G_NUM) % G_NUM;
  subStartMode();
}
static void subClick() { g_band = (g_band + 1) % N_BANDS; subStartMode(); }

// вызывается на каждом проходе loop(), пока открыт экран раздела
static void subTick(uint32_t now) {
  if (!g_ccOk || !g_ccOn) return;
  if (g_gMode == G_SPECTRUM) spTick(); else monTick(now);
}


static void subFrame(uint32_t now) {
  if (!g_ccOk) { drawCcMissing(); return; }
  if (g_gMode == G_SPECTRUM) drawSpectrum(); else drawMonitor(now);
}

static void subScreenInfo(int i, SubItem& it) {
  static const char* const names[G_NUM] = { "Spectrum", "Monitor" };
  static const char* const descs[G_NUM] = { "band + waterfall", "catch remotes" };
  it.name = names[i]; it.desc = descs[i];
  if (!g_ccOk && g_ccTried) { strcpy(it.value, "no chip"); return; }
  if (i == G_SPECTRUM) strcpy(it.value, BANDS[g_band].name);
  else if (g_events)   sprintf(it.value, "%lu", (unsigned long)g_events);
  else                 strcpy(it.value, "-");
}

static void subSubtitle(char* out, size_t n) {
  snprintf(out, n, "%s", (g_ccTried && !g_ccOk) ? "no chip" : BANDS[g_band].name);
}

static void subTickFn(uint32_t now) { if (!navInSub()) subTick(now); }
static bool subBackFn() { return false; }

const Section subghzSection = {
  "Sub-GHz", 240, 120, 90, iconAntenna, subSubtitle,
  subEnter, subLeave,
  G_NUM, subScreenInfo, subScreenNow, subOpenScreen, subCloseScreen,
  subClick, subRotate, subBackFn, subTickFn, subFrame,
};
