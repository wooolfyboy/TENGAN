// ============================================================
//  pc.h — PC section: laptop load monitor.
//  tools/pc_monitor/pc_monitor.py runs on the laptop and once a second
//  sends a line over USB like
//    PC cpu=23.4 ram=61.2 ramu=9.8 ramt=16.0 c=12,40,5 ... host=LAPTOP
//  The stick parses it and draws the readings.
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

#define PC_CORES 16
#define PC_HIST  60                   // one minute of history (one point per second)

struct PcStat {
  bool     have = false;              // any data received yet
  uint32_t t = 0;                     // when the last arrived (millis)
  float    cpu = 0, freq = 0;         // %, GHz
  int      nCores = 0;
  uint8_t  core[PC_CORES] = {};       // % per core
  float    ram = 0, ramU = 0, ramT = 0;   // %, GB used, GB total
  float    disk = 0, dR = 0, dW = 0;  // % C: full, MB/s read/write
  float    nD = 0, nU = 0;            // KB/s down / up
  int      bat = -1; bool plug = false;   // laptop battery (-1 = none)
  uint32_t up = 0;                    // laptop uptime, s
  char     top[24] = "", host[24] = "";
  int      vol = -1; bool mute = false;  // laptop volume (if the program could read it)
  uint32_t volT = 0;                  // when the volume arrived
  // now playing (Windows system media panel: Spotify, browser YouTube, etc.)
  bool     trk = false;               // is there a track
  bool     playing = false;
  int      pos = -1, dur = -1;        // seconds (-1 = unknown)
  uint32_t trkT = 0;                  // when it arrived (position between updates is extrapolated)
  char     title[96] = "", artist[72] = "", app[24] = "";   // UTF-8 (Cyrillic letters take 2 bytes)
  float    cpuH[PC_HIST] = {}, ramH[PC_HIST] = {};
  int      histN = 0;
};
extern PcStat g_pc;
extern int    g_pcPage;               // 0 overview, 1 cores, 2 process city, 3 music (wheel = volume)
#define PC_PAGES 4
// remote: what and when the stick last sent to the laptop (for screen animation)
extern int      g_pcLastCmd;          // +1 louder, -1 quieter, 2 pause/play, 3 mute
extern uint32_t g_pcLastCmdMs;

void pcPoll(uint32_t now);            // every loop(): read USB
void pcSendLine(const char* s);       // send a line to the laptop program (if running)
bool pcKey(const char* name);         // tablet remote: VOLUP VOLDN MUTE PLAY NEXT PREV -> media key on the laptop
bool pcLive(uint32_t now);            // data is fresh (< 3 s)

void drawPc(uint32_t now);
void drawPcCores(uint32_t now);
void drawPcWaiting(uint32_t now);
void drawPcMedia(uint32_t now);
void drawPcCity(uint32_t now);       // process city (pc_city.cpp)
void pcCityLine(char* s);            // "PP ..." line from the laptop
void pcCityNext();                   // click: spotlight to the next tower
void pcCityJson(String& o);          // towers for the tablet (appends to o)
bool pcCityLive(uint32_t now);
void pcStreamJson(String& o);        // everything about the laptop for the tablet (SSE stream)

extern const Section pcSection;
