// ============================================================
//  sim.cpp — симулятор экрана палки на компьютере.
//
//  Компилирует НАСТОЯЩИЙ код рисования (src/core, *_screens.cpp, меню),
//  подставляет правдоподобные данные и сохраняет каждый экран в картинку.
//  Радио, кнопок и железа тут нет. Запуск: tools/sim/build.sh
// ============================================================
#include <map>
#include <vector>
#include "core/display.h"
#include "core/theme.h"
#include "core/menu.h"
#include "core/battery.h"
#include "core/settings.h"
#include "gallery/gallery.h"
#include "core/boot.h"
#include "ble/ble.h"
#include "ble/ble_model.h"
#include "wifi/wifi.h"
#include "wifi/wifi_model.h"
#include "subghz/subghz.h"
#include "subghz/subghz_model.h"
#include "audio/audio.h"
#include "audio/audio_model.h"
#include <esp_wifi.h>
#include <Wire.h>
#include <SPI.h>

SPIClass SPI;
TwoWire Wire;
static uint32_t s_now = 3 * 3600000 + 14 * 60000 + 32000;   // «время работы» 3:14:32
uint32_t millis() { return s_now; }
uint32_t micros() { return s_now * 1000; }

void drawSettings();

// ---------- состояние Bluetooth (в прошивке — ble_core.cpp) ----------
volatile int g_mode = 0;
bool  g_bleActive = true;
int   g_lastCount = 23, g_lastRaw = 31, g_sensCount = 2;
Sample g_hist[HIST_N];
int    g_histLen = 0;
std::vector<uint32_t> g_order;
uint32_t g_sel = 0;
int      g_top = 0;
bool     g_detail = false;
int8_t   g_spark[SPARK_N];
int      g_sparkN = 0;
void bleRadioOff() {}
void bleRadioOn() {}
void wifiRadioOff() {}

// ---------- состояние Sub-GHz (в прошивке — subghz.cpp) ----------
int  g_gMode = 0, g_band = 1;
bool g_ccOk = true, g_ccTried = true;
uint8_t g_ccVersion = 0x14;
int8_t  g_spCur[SP_N], g_spPeak[SP_N];
uint8_t g_wf[WF_ROWS][SP_N];
int     g_wfHead = 0, g_wfCount = 0;
float spFreq(int i) { const Band& b = BANDS[g_band]; return b.lo + (b.hi - b.lo) * i / (SP_N - 1); }
int8_t   g_mon[MON_N];
int      g_monN = 0;
float    g_floor = -101;
bool     g_burst = false;
uint32_t g_events = 3;
Burst    g_ev[EV_N];
int      g_evN = 0;

// ---------- звук (в прошивке — audio.cpp) ----------
AudioView g_au;
bool  g_micOk = true;
MicDiag g_micDiag = { 4, 0, 0, 37, 0, false };
int   g_aMode = 0;
float g_auHist[AU_HIST];
int   g_auHistN = 0;
float g_auMax = -18, g_auAvg = -47;
const char* levelWord(float db) { return db < -55 ? "QUIET" : db < -40 ? "NORMAL" : db < -25 ? "LOUD" : "VERY LOUD"; }
void audioBegin() {}
void audioClick() {}
void audioBeep(uint16_t, uint16_t) {}
void audioGeiger(int) {}

// ---------- разделы для меню (в прошивке — свои файлы) ----------
static void noop() {}
static void noopI(int) {}
static void noopT(uint32_t) {}
static bool noBack() { return false; }
static void bleSub(char* o, size_t n) { snprintf(o, n, "%d devices", g_lastCount); }
static void wifiSub(char* o, size_t n) { snprintf(o, n, "%d nets", (int)g_aps.size()); }
static void subSub(char* o, size_t n) { snprintf(o, n, "%s", BANDS[g_band].name); }
static int  zero() { return 0; }
static void bleInfo(int i, SubItem& it) {
  static const char* const nm[] = { "Overview", "Scope", "City", "Nearest", "Devices", "Weather" };
  static const char* const ds[] = { "count, trend, types", "radar picture", "3D: devices as towers", "closest device", "list + details", "BLE thermometers" };
  static const char* const vv[] = { "23", "phone 3", "23", "-48 dBm", "23", "2" };
  it.name = nm[i]; it.desc = ds[i]; snprintf(it.value, sizeof(it.value), "%s", vv[i]);
}
static void wifiInfo(int i, SubItem& it) {
  static const char* const nm[] = { "Networks", "Channels", "Activity", "Clients", "Find" };
  static const char* const ds[] = { "list + details", "who uses which ch", "packets & noise", "devices in networks", "best signal spot" };
  static const char* const vv[] = { "14", "best 6", "412/s", "9", "Keenetic-48" };
  it.name = nm[i]; it.desc = ds[i]; snprintf(it.value, sizeof(it.value), "%s", vv[i]);
}
static void subInfo(int i, SubItem& it) {
  static const char* const nm[] = { "Spectrum", "Monitor" };
  static const char* const ds[] = { "band + waterfall", "catch remotes" };
  static const char* const vv[] = { "433 MHz", "3" };
  it.name = nm[i]; it.desc = ds[i]; snprintf(it.value, sizeof(it.value), "%s", vv[i]);
}
const Section bleSection = { "Bluetooth", 80, 150, 240, iconBluetooth, bleSub, noop, noop,
  NUM_MODES, bleInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };
const Section wifiSection = { "Wi-Fi", 70, 210, 130, iconWifi, wifiSub, noop, noop,
  W_NUM, wifiInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };
static void sndSub(char* o, size_t n) { snprintf(o, n, "%d dBfs", (int)g_au.level); }
static void sndInfo(int i, SubItem& it) {
  static const char* const nm[] = { "Bars", "Terrain", "Reactor", "Level", "Scope" };
  static const char* const ds[] = { "sound spectrum", "3D sound mountains", "pulse animation", "noise meter", "waveform" };
  static const char* const vv[] = { "1.2 kHz", "", "", "-38 dBfs", "" };
  it.name = nm[i]; it.desc = ds[i]; snprintf(it.value, sizeof(it.value), "%s", vv[i]);
}
const Section soundSection = { "Sound", 120, 200, 255, iconMic, sndSub, noop, noop,
  A_NUM, sndInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };
const Section subghzSection = { "Sub-GHz", 240, 120, 90, iconAntenna, subSub, noop, noop,
  G_NUM, subInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };

// ---------- вывод ----------
static const char* s_outDir = "out";
static void snap(const char* name) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.ppm", s_outDir, name);
  FILE* f = fopen(path, "wb");
  if (!f) { perror(path); return; }
  int W = spr.width(), H = spr.height();
  fprintf(f, "P6 %d %d 255\n", W, H);
  uint16_t* p = (uint16_t*)spr.getPointer();
  for (int i = 0; i < W * H; i++) {
    uint16_t v = p[i]; v = (v >> 8) | (v << 8);
    unsigned char rgb[3] = { (unsigned char)(((v >> 11) & 31) * 255 / 31), (unsigned char)(((v >> 5) & 63) * 255 / 63),
                             (unsigned char)((v & 31) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("  %s\n", name);
}

// ---------- данные ----------
static float noise(int i) { return ((i * 2654435761u) >> 24) / 255.0f; }

static Ent mkEnt(const char* name, uint8_t type, uint8_t cat, int rssi, uint16_t cid) {
  Ent e; memset(&e, 0, sizeof(e));
  snprintf(e.name, sizeof(e.name), "%s", name);
  e.type = type; e.cat = cat; e.rssi = e.rssiS = rssi; e.vkind = cid ? 1 : 0; e.vcode = cid; e.vscore = 3;
  e.t = s_now - 400; e.first = s_now - 312000; e.pkts = 1834; e.nMacs = 2;
  return e;
}

static std::map<uint32_t, Ent> g_ents;

static void fillBle() {
  struct { const char* n; uint8_t t, c; int r; uint16_t cid; } list[] = {
    { "Galaxy Buds2", T_AUDIO, 4, -48, 0x0075 }, { "", T_PHONE, 2, -55, 0x004C }, { "", T_TRACKER, 2, -58, 0x004C },
    { "Mi Band 8", T_WATCH, 6, -61, 0x038F }, { "", T_PHONE, 4, -66, 0x0075 }, { "LE_WH-1000XM4", T_AUDIO, 14, -70, 0x012D },
    { "", T_UNKNOWN, 0, -74, 0 }, { "[TV] Samsung Q7", T_TV, 4, -79, 0x0075 }, { "", T_UNKNOWN, 1, -83, 0x0059 },
    { "MX Master 3", T_INPUT, 25, -86, 0x01DA }, { "", T_PHONE, 2, -90, 0x004C },
  };
  uint32_t id = 1;
  for (auto& d : list) { g_ents[id] = mkEnt(d.n, d.t, d.c, d.r, d.cid); g_order.push_back(id); id++; }
  g_sel = 2; g_top = 0;
  for (int i = 0; i < HIST_N; i++) {
    int c = 18 + (int)(6 * sinf(i / 14.0f)) + (int)(noise(i) * 3) + (i > 110 ? 5 : 0);
    g_hist[i] = { c, (float)c - 1, (int8_t)(i == 112 ? 1 : (i == 60 ? -1 : 0)) };
  }
  g_histLen = HIST_N;
  for (int i = 0; i < SPARK_N; i++) g_spark[i] = (int8_t)(-62 + 6 * sinf(i / 6.0f) + noise(i) * 4);
  g_sparkN = SPARK_N;
}

static void fillWifi() {
  struct { const char* s; int ch, rssi; uint8_t enc; int8_t sec; uint8_t gen; bool hot; const char* v; } list[] = {
    { "Keenetic-48", 6, -41, WIFI_AUTH_WPA2_PSK, 0, 6, false, "Keenetic" }, { "HomeNet", 1, -52, WIFI_AUTH_WPA2_PSK, 0, 4, false, "TP-Link" },
    { "TP-Link_5G_EXT", 11, -60, WIFI_AUTH_WPA2_WPA3_PSK, -1, 6, false, "TP-Link" }, { "iPhone (Alex)", 6, -63, WIFI_AUTH_WPA2_PSK, 0, 6, true, "Apple" },
    { "MGTS_GPON_2F4A", 9, -68, WIFI_AUTH_WPA2_PSK, 1, 4, false, "ZTE" }, { "Free_WiFi", 1, -71, WIFI_AUTH_OPEN, 0, 4, false, "Huawei" },
    { "", 11, -76, WIFI_AUTH_WPA2_PSK, 0, 4, false, nullptr }, { "DIRECT-2B-HP M28", 13, -80, WIFI_AUTH_WPA2_PSK, 0, 4, false, "HP" },
    { "Rostelecom_7781", 3, -84, WIFI_AUTH_WPA_WPA2_PSK, 0, 4, false, "Sercomm" }, { "old_router", 6, -88, WIFI_AUTH_WEP, 0, 3, false, "D-Link" },
  };
  uint64_t k = 0x10;
  for (auto& d : list) {
    Ap a; memset(&a, 0, sizeof(a));
    snprintf(a.ssid, sizeof(a.ssid), "%s", d.s);
    a.ch = d.ch; a.rssi = a.rssiS = d.rssi; a.rmin = d.rssi - 9; a.rmax = d.rssi + 5; a.enc = d.enc; a.sec = d.sec;
    a.gen = d.gen; a.hotspot = d.hot; a.vendor = d.v; a.t = s_now - 900; a.first = s_now - 754000;
    uint8_t b[6] = { 0x50, 0xFF, 0x20, 0x1A, (uint8_t)(k >> 8), (uint8_t)k }; memcpy(a.bssid, b, 6);
    g_aps[k] = a; g_wOrder.push_back(k); k += 0x11;
  }
  g_wSel = g_wOrder[0]; g_wEverScanned = true;
  for (int i = 0; i < W_SPARK_N; i++) g_wSpark[i] = (int8_t)(-44 + 4 * sinf(i / 5.0f) + noise(i + 7) * 3);
  g_wSparkN = W_SPARK_N;
  for (int i = 0; i < F_SPARK_N; i++) g_fSpark[i] = (int8_t)(-70 + i * 0.22f + noise(i) * 4);
  g_fSparkN = F_SPARK_N; g_fLastHit = s_now - 100;
  float load[14] = { 0, 160, 20, 30, 10, 15, 420, 40, 25, 90, 15, 220, 10, 35 };
  for (int c = 1; c <= CH_MAX; c++) {
    g_ppsData[c] = load[c] * 0.65f; g_ppsMgmt[c] = load[c] * 0.35f + 8;
    g_noise[c] = -95 + (c == 9 ? 9 : 0) + noise(c) * 2;
  }
  g_hopCh = 7;
  for (int i = 0; i < ACT_HIST_N; i++) g_actHist[i] = 900 + 300 * sinf(i / 9.0f) + noise(i) * 200 + (i > 100 ? 500 : 0);
  g_actHistN = ACT_HIST_N;
  int per[] = { 5, 3, 2, 1, 1 };
  uint64_t sta = 0x9000;
  for (int i = 0; i < 5; i++) for (int j = 0; j < per[i]; j++) g_stas[sta++] = { g_wOrder[i], s_now };
  g_stas[sta++] = { 0x3C5AB4000001ull, s_now };
  for (int i = 0; i < 17; i++) g_probes[0xA000 + i] = s_now;
}

static void fillSub() {
  for (int i = 0; i < SP_N; i++) {
    float x = i - SP_N / 2.0f;
    float v = -104 + noise(i) * 6 + 58 * expf(-x * x / 8.0f) + 30 * expf(-(i - 22) * (i - 22) / 4.0f);
    g_spCur[i] = (int8_t)v; g_spPeak[i] = (int8_t)(v + 4 + noise(i + 3) * 6);
  }
  for (int r = 0; r < WF_ROWS; r++)
    for (int i = 0; i < SP_N; i++) {
      float x = i - SP_N / 2.0f;
      bool on = (r % 9) < 3, on2 = (r % 13) < 2;
      float v = -106 + noise(i * 31 + r) * 7 + (on ? 55 * expf(-x * x / 6.0f) : 0) + (on2 ? 32 * expf(-(i - 22) * (i - 22) / 3.0f) : 0);
      g_wf[r][i] = (uint8_t)constrain((int)v + 120, 0, 110);
    }
  g_wfHead = 0; g_wfCount = WF_ROWS;
  for (int i = 0; i < MON_N; i++) {
    float v = -101 + noise(i) * 4;
    if ((i > 200 && i < 225) || (i > 260 && i < 270)) v = -52 + noise(i) * 6;
    g_mon[i] = (int8_t)v;
  }
  g_monN = MON_N;
  g_ev[0] = { s_now - 2 * 3600000, 380, -61 }; g_ev[1] = { s_now - 95000, 420, -49 }; g_ev[2] = { s_now - 4000, 210, -55 };
  g_evN = 3;
}

int main(int argc, char** argv) {
  if (argc > 1) s_outDir = argv[1];
  spr.setColorDepth(16);
  spr.createSprite(320, 170);
  settingsBegin();
  g_bat.ok = true; g_bat.soc = 87; g_bat.mv = 3980; g_bat.ma = -132; g_bat.tte = 250; g_bat.chgOk = true;
  fillBle(); fillWifi(); fillSub();

  { const uint32_t ts[] = { 250, 600, 1000, 1300, 1600, 1950, 2200 };
    for (int i = 0; i < 7; i++) { char nm[32]; snprintf(nm, sizeof nm, "00_boot_%04u", (unsigned)ts[i]); bootFrame(ts[i]); snap(nm); } }
  navBegin(); navDraw(s_now);                    snap("01_menu");
  navClick(); navDraw(s_now);                    snap("02_ble_sub");
  navBack();
  navRotate(+1); navClick(); navDraw(s_now);     snap("03_wifi_sub");
  navBack();
  navRotate(+1); navClick(); navDraw(s_now);     snap("04_subghz_sub");
  navBack();
  navRotate(+1); navClick(); navDraw(s_now);     snap("05_sound_sub");
  navBack();
  navRotate(+1); navClick(); navDraw(s_now);     snap("06_settings");
  { const char* pg[] = { "06a_set_general", "06b_set_ble", "06c_set_wifi", "06d_set_subghz", "06e_set_sound" };
    for (int p = 0; p < 5; p++) {
      if (p) navRotate(+1);
      navClick(); if (p == 0) for (int k = 0; k < 5; k++) navClick(); navDraw(s_now); snap(pg[p]); navBack();
    } }
  navBack();

  std::vector<Blip> blips;
  int typeCnt[T_NUM] = {0};
  Ent best = g_ents.begin()->second;
  for (auto& kv : g_ents) {
    blips.push_back({ kv.second.rssi, kv.first * 2654435761u, kv.second.cat, kv.second.type });
    typeCnt[kv.second.type]++;
  }
  g_mode = M_OVERVIEW; drawOverview((int)g_ents.size(), g_lastRaw, typeCnt); snap("10_ble_overview");
  g_mode = M_SCOPE;    drawScope(blips);                                snap("12_ble_scope");
  g_mode = M_CITY;     for (int f = 0; f < 30; f++) drawCity(blips, s_now + f * 33); snap("13_ble_city");
  g_mode = M_NEAREST;  drawNearest(true, -48, best, g_lastCount);       snap("11_ble_nearest");
  Ent tr = g_ents[3];
  drawNearest(true, -58, tr, g_lastCount);                              snap("11b_ble_nearest_tracker");
  g_mode = M_LIST;     drawList(g_ents);                                snap("15_ble_devices");
  Dev d; memset(&d, 0, sizeof(d));
  Ent& sel = g_ents[g_sel];
  d.rmin = -71; d.rmax = -49; d.akind = A_ROTATING; d.hasCid = true; d.cid = 0x004C; d.mfrLen = 25;
  uint8_t mac[6] = { 0x3A, 0x91, 0x0C, 0x55, 0x12, 0x7E }; memcpy(d.mac, mac, 6);
  uint8_t mf[6] = { 0x12, 0x19, 0x10, 0xA4, 0x3B, 0x77 }; memcpy(d.mfr, mf, 6);
  d.nU = 1; d.uuids[0] = 0xFD44; d.type = sel.type;
  drawDetail(d, sel, true, s_now);                                       snap("16_ble_detail");
  std::vector<Sensor> sens(2);
  memset(sens.data(), 0, sizeof(Sensor) * 2);
  sens[0].t = s_now - 12000; sens[0].r.hasT = sens[0].r.hasH = sens[0].r.hasB = true;
  sens[0].r.temp = 22.4f; sens[0].r.hum = 41; sens[0].r.batt = 87; sens[0].r.model = "PVVX";
  snprintf(sens[0].name, sizeof(sens[0].name), "ATC_A1B2C3");
  sens[1].t = s_now - 95000; sens[1].r.hasT = sens[1].r.hasH = sens[1].r.hasP = true;
  sens[1].r.temp = -3.8f; sens[1].r.hum = 78; sens[1].r.press = 1012; sens[1].r.model = "BTHome";
  uint8_t m2[6] = { 1, 2, 3, 4, 0xC4, 0x38 }; memcpy(sens[1].mac, m2, 6);
  drawWeather(sens, s_now);                                              snap("17_ble_weather");

  drawNets(s_now);           snap("20_wifi_networks");
  g_wDetail = true;
  drawNetDetail(s_now);      snap("21_wifi_detail");
  g_wDetail = false;
  drawChannels(s_now);       snap("22_wifi_channels");
  g_prom = true;
  drawActivity(s_now);       snap("23_wifi_activity");
  drawClients(s_now);        snap("24_wifi_clients");
  g_prom = false;
  g_wMode = W_FIND;
  drawFind(s_now);           snap("25_wifi_find");

  // звук: «голос» с пиком около 1.2 кГц и гармониками
  g_au.level = -38; g_au.peak = -31; g_au.domHz = 1187; g_au.energy = 0.45f;
  for (int k = 0; k < AU_BANDS; k++) {
    float x = k / (float)AU_BANDS;
    float v = 0.25f + 0.55f * expf(-(k - 17) * (k - 17) / 18.0f) + 0.35f * expf(-(k - 9) * (k - 9) / 10.0f) - 0.2f * x + noise(k) * 0.15f;
    g_au.bands[k] = constrain(v, 0.02f, 1.0f); g_au.caps[k] = min(1.0f, g_au.bands[k] + 0.06f + noise(k + 5) * 0.08f);
  }
  for (int i = 0; i < AU_WAVE; i++) g_au.wave[i] = (int16_t)(6000 * sinf(i * 0.37f) + 2500 * sinf(i * 1.11f + 1) + noise(i) * 900 - 450);
  for (int i = 0; i < AU_HIST; i++) g_auHist[i] = -52 + 8 * sinf(i / 7.0f) + noise(i) * 6 + (i > 90 && i < 100 ? 30 : 0);
  g_auHistN = AU_HIST;
  drawAuBars(s_now);         snap("40_sound_bars");
  for (int f = 0; f < 30; f++) {                        // «горы»: прокрутить историю
    for (int k = 0; k < AU_BANDS; k++) g_au.bands[k] = constrain(0.15f + 0.7f * expf(-(k - 4 - (f % 7)) * (k - 4 - (f % 7)) / 12.0f) * (0.5f + 0.5f * sinf(f * 0.5f + k * 0.2f)) + noise(k + f) * 0.1f, 0.0f, 1.0f);
    drawTerrain(s_now + f * 50);
  }
  snap("40b_sound_terrain");
  drawReactor(s_now);        snap("41_sound_reactor");
  drawLevel(s_now);          snap("42_sound_level");
  drawWave(s_now);           snap("43_sound_scope");
  g_au.level = -21; g_au.peak = -18;
  drawLevel(s_now);          snap("42b_sound_level_loud");
  drawMicMissing();          snap("44_mic_check");
  g_micDiag.disabled = true; drawMicMissing(); snap("45_mic_off");

  for (int f = 0; f < 40; f++) drawTesseract(s_now + f * 33);   snap("50_gallery_tesseract");
  for (int f = 0; f < 10; f++) drawTorus(s_now + 1500 + f * 33); snap("51_gallery_torus");
  drawSpectrum();            snap("30_subghz_spectrum");
  drawMonitor(s_now);        snap("31_subghz_monitor");
  return 0;
}
