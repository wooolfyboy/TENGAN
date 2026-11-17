// ============================================================
//  weather.h — "Radio weather" (Gallery › Weather and the tablet)
//
//  The surrounding airwaves as weather. Sources are only those that don't
//  interfere with each other: Bluetooth (2.4 GHz), CC1101 at 433.92 (monitor) and
//  the microphone. Wi-Fi is not scanned. Receive only.
//    clouds   = number of Bluetooth devices around
//    wind     = devices that came/left in a minute
//    rain     = 433.92 bursts per minute (remotes, sensors)
//    storm    = rain + loud sound peaks; thunder = a sharp peak
//    outside  = temperature from a 433 MHz outdoor sensor (strongest in 10 min)
//  Everything is smoothed: looks matter more than accuracy.
// ============================================================
#pragma once
#include <Arduino.h>

#define WX_HIST 60                     // history: 60 points x 10 s = 10 minutes

struct WxState {
  bool  ok = false;                    // data present (computing)
  float clouds = 0, wind = 0, rain = 0, storm = 0;   // 0..1, smoothed
  bool  thunder = false;               // sharp sound or strong burst in the last 2 s
  float trend = 0;                     // "barometer" -1..1: clouds rising / falling over 10 minutes
  int   ble = 0;                       // devices now
  float turn = 0;                      // came + left per minute
  float sub = 0;                       // 433.92 bursts per minute
  int   subPk = -127;                  // peak of the last burst, dBm
  bool  subOk = false;                 // CC1101 running
  float snd = -90, pk = -90;           // level and recent peak, dBFS
  bool  sndOk = false;                 // microphone running
  const char* cond = "calm";           // calm / clear / cloudy / overcast / windy / rain / storm
  float hist[WX_HIST][2] = {};         // clouds, rain
  int   histN = 0;
  uint32_t thunderMs = 0;              // time of the last thunder (for the flash)
  bool  tOk = false;                   // outdoor thermometer heard (433 MHz, sensors.h)
  float temp = 0;                      // °C
  int   hum = -1;                      // %, -1 = sensor has no humidity
  const char* tsrc = "";              // sensor protocol (Nexus, LaCrosse-TX141TH ...)
};
extern WxState g_wx;

void wxStart();                        // Gallery › Weather: turn on the sources (BLE, CC1101 433 monitor, microphone)
void wxStop();                         // turn off what was turned on
void wxRadiosTick(uint32_t now);       // feed CC1101 and the microphone (when the sources are enabled here)
void wxUpdate(uint32_t now);           // recompute the weather (once a second; decides internally)
void wxJson(String& o);                // for the tablet (SSE stream, wx event)

void drawWeather(uint32_t now);        // stick screen
void wxTurn(int dir);                  // wheel: sky / graph
