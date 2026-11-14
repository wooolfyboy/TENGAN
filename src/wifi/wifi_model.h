// ============================================================
//  wifi_model.h — данные раздела Wi-Fi, общие для логики (wifi.cpp)
//  и экранов (wifi_screens.cpp). Снаружи раздела нужен только wifi.h.
// ============================================================
#pragma once
#include <Arduino.h>
#include <map>
#include <vector>

enum { W_NETS = 0, W_CHANNELS, W_ACTIVITY, W_CLIENTS, W_FIND, W_NUM };
static const int CH_MAX = 13;                 // каналы 2.4 ГГц (1..13)

// точка доступа
struct Ap {
  uint8_t  bssid[6];
  char     ssid[33];
  int8_t   rssi, rssiS, rmin, rmax;
  uint8_t  ch, enc;
  int8_t   sec;        // второй канал (40 МГц): +1 выше, -1 ниже, 0 — обычные 20 МГц
  uint8_t  gen;        // 6 = Wi-Fi 6 (ax), 4 = Wi-Fi 4 (n), 3 = 11g, 1 = 11b
  bool     hotspot;    // похоже на раздачу с телефона
  const char* vendor;  // производитель по OUI (или nullptr)
  uint32_t t, first;
};
struct Sta { uint64_t bssid; uint32_t t; };   // устройство в сети (только для счёта)

extern int  g_wMode;
extern bool g_wEverScanned, g_scanning, g_prom;
extern std::map<uint64_t, Ap> g_aps;
extern std::map<uint64_t, Sta> g_stas;
extern std::map<uint64_t, uint32_t> g_probes;

// список и подробности
extern std::vector<uint64_t> g_wOrder;
extern uint64_t g_wSel;
extern int      g_wTop;
extern bool     g_wDetail;
#define W_SPARK_N 60
extern int8_t   g_wSpark[W_SPARK_N];
extern int      g_wSparkN;
// FIND
#define F_SPARK_N 100
extern int8_t   g_fSpark[F_SPARK_N];
extern int      g_fSparkN;
extern uint32_t g_fLastHit;
// ACTIVITY
extern int      g_hopCh;
extern float    g_ppsMgmt[CH_MAX + 1], g_ppsData[CH_MAX + 1], g_noise[CH_MAX + 1];
#define ACT_HIST_N 120
extern float    g_actHist[ACT_HIST_N];
extern int      g_actHistN;

// помощники (wifi.cpp)
uint64_t    macKey(const uint8_t* m);
const char* encName(uint8_t e);
const char* genName(uint8_t g);
uint16_t    dimColor(uint16_t c, int pct);
const char* ssidOr(const Ap& a);                 // имя или "(hidden)"
float       apCenter(const Ap& a);               // центр полосы, в каналах
float       apHalf(const Ap& a);                 // полуширина: 20 МГц -> 2, 40 МГц -> 4
float       interference(const Ap& a, int k);    // насколько сеть мешает каналу k (0..1)
int         bestChannel();                       // лучший из 1 / 6 / 11
int         clientsOf(uint64_t bssid);
uint64_t    findTarget();                        // сеть-цель для FIND
const char* ouiName(const uint8_t* mac);         // производитель по MAC (или nullptr)

// экраны (wifi_screens.cpp)
void drawNets(uint32_t now);
void drawNetDetail(uint32_t now);
void drawChannels(uint32_t now);
void drawActivity(uint32_t now);
void drawClients(uint32_t now);
void drawFind(uint32_t now);
