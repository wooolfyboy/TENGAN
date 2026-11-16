#include <string>
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
#include "gallery/weather.h"
#include "gallery/astro.h"
#include "gallery/sat.h"
#include "web/web.h"
#include "pc/pc.h"
#include "core/power.h"
#include "core/ui.h"
#include "core/boot.h"
#include "ble/ble.h"
#include "ble/ble_model.h"
#include "wifi/wifi.h"
#include "wifi/wifi_model.h"
#include "subghz/subghz.h"
#include "subghz/subghz_model.h"
#include "subghz/sensors.h"
#include "audio/audio.h"
#include "audio/audio_model.h"
#include "probe/ir.h"
#include "probe/nfc.h"
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
// «чёрный ящик» (в прошивке — core/crumb.cpp)
void crumb(const char*) {}
bool crumbWasCrash() { return false; }
const char* crumbLastStep() { return ""; }
const char* crumbLastReason() { return ""; }
void bleRadioOff() {}
void bleRadioOn() {}
bool bleRadioActive() { return true; }
int  bleEntityIds(uint32_t*, int) { return 0; }
bool subWebUse(int, int) { return true; }
void subWebTick(uint32_t) {}
void subWebStop() {}
bool audioWebUse() { return true; }
void audioWebTick(uint32_t) {}
void audioWebStop() {}
void wifiRadioOff() {}

// ---------- датчики 433 (в прошивке — sens_rx.cpp): посылки собираем сами и кормим настоящий разбор ----------
void sensRxStart() {}
void sensRxStop() {}
void sensRxTick(uint32_t, int) {}
bool sensRxOn() { return true; }
static OokTrain s_tr;
static void trAdd(int p, int g) { if (s_tr.n < OOK_MAXP) { s_tr.p[s_tr.n] = p; s_tr.g[s_tr.n] = g; s_tr.n++; } }
static void trBits(const uint8_t* b, int nbits, int p0, int g0, int p1, int g1) {
  for (int i = 0; i < nbits; i++) { bool one = b[i >> 3] & (0x80 >> (i & 7)); trAdd(one ? p1 : p0, one ? g1 : g0); }
}
static void simNexus(uint8_t id, int ch, float t, int hum) {         // 36 бит PPM, 12 повторов
  int raw = (int)lroundf(t * 10) & 0xFFF;
  uint8_t b[5] = { id, (uint8_t)(0x80 | ((ch - 1) << 4) | (raw >> 8)), (uint8_t)raw, (uint8_t)(0xF0 | (hum >> 4)), (uint8_t)(hum << 4) };
  s_tr.n = 0;
  for (int r = 0; r < 12; r++) { trBits(b, 36, 500, 1000, 500, 2000); trAdd(500, r == 11 ? 30000 : 4000); }
}
static uint8_t simDigest(const uint8_t* m, int bytes) {               // LaCrosse: lfsr 0x31 / 0xf4, отражённый
  uint8_t sum = 0, key = 0xF4;
  for (int k = bytes - 1; k >= 0; --k) for (int i = 0; i < 8; ++i) { if ((m[k] >> i) & 1) sum ^= key; key = (key & 0x80) ? (key << 1) ^ 0x31 : key << 1; }
  return sum;
}
static void simTx141(uint8_t id, int ch, float t, int hum) {         // 40 бит PWM, преамбула 4×833, 12 повторов
  int raw = (int)lroundf(t * 10) + 500;
  uint8_t b[5] = { id, (uint8_t)(((ch - 1) << 4) | (raw >> 8)), (uint8_t)raw, (uint8_t)hum, 0 };
  b[4] = simDigest(b, 4);
  s_tr.n = 0;
  for (int r = 0; r < 12; r++) { for (int k = 0; k < 4; k++) trAdd(833, 833); trBits(b, 40, 208, 417, 417, 208); }
  s_tr.g[s_tr.n - 1] = 30000;
}
static void simBell() {                                               // звонок EV1527: 24 бита, 6 повторов
  uint8_t b[3] = { 0x5C, 0x3A, 0x91 };
  s_tr.n = 0;
  for (int r = 0; r < 6; r++) { trAdd(350, 10850); trBits(b, 24, 350, 1050, 1050, 350); }
  s_tr.g[s_tr.n - 1] = 30000;
}

// ---------- состояние Sub-GHz (в прошивке — subghz.cpp) ----------
int  g_gMode = 0, g_band = 1;
bool g_ccOk = true, g_ccTried = true;
uint8_t g_ccVersion = 0x14;
int8_t  g_spCur[SP_N], g_spPeak[SP_N];
static uint8_t s_wfMem[WF_ROWS][SP_N];
uint8_t (*g_wf)[SP_N] = s_wfMem;
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
bool   g_auFromPc = false;
uint32_t g_powerHold = 0;
int    g_auSrcNow = 0;
int8_t g_auXY[AU_XY][2];
bool   g_auXYStereo = false;
bool audioPcLive(uint32_t) { return g_auFromPc; }
void audioPcFrame(float level, int hz, const uint8_t* bands, const int8_t* wave, const int8_t* xy, bool stereo) {
  g_auFromPc = true; g_au.level = level; g_au.domHz = hz; float e = 0;
  for (int k = 0; k < AU_BANDS; k++) { g_au.bands[k] = bands[k] / 255.0f; g_au.caps[k] = g_au.bands[k] + 0.05f; e += g_au.bands[k]; }
  g_au.energy = e / AU_BANDS;
  for (int i = 0; i < AU_WAVE; i++) g_au.wave[i] = wave[i * 64 / AU_WAVE] * 200;
  memcpy(g_auXY, xy, sizeof(g_auXY)); g_auXYStereo = stereo;
}

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
  static const char* const nm[] = { "Spectrum", "Monitor", "Sensors" };
  static const char* const ds[] = { "band + waterfall", "catch remotes", "thermometers, bells" };
  static const char* const vv[] = { "433 MHz", "3", "3" };
  it.name = nm[i]; it.desc = ds[i]; snprintf(it.value, sizeof(it.value), "%s", vv[i]);
}
const Section bleSection = { "Bluetooth", 80, 150, 240, iconBluetooth, bleSub, noop, noop,
  NUM_MODES, bleInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };
const Section wifiSection = { "Wi-Fi", 70, 210, 130, iconWifi, wifiSub, noop, noop,
  W_NUM, wifiInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };
static void sndSub(char* o, size_t n) { snprintf(o, n, "%d dBfs", (int)g_au.level); }
static void sndInfo(int i, SubItem& it) {
  static const char* const nm[] = { "Bars", "Terrain", "Reactor", "Level", "Scope", "XY" };
  static const char* const ds[] = { "sound spectrum", "3D sound mountains", "pulse animation", "noise meter", "waveform", "oscilloscope XY" };
  static const char* const vv[] = { "1.2 kHz", "", "", "-38 dBfs", "", "" };
  it.name = nm[i]; it.desc = ds[i]; snprintf(it.value, sizeof(it.value), "%s", vv[i]);
}
static void webSub(char* o, size_t n) { snprintf(o, n, "1 online"); }
const Section webSection = { "Web", 90, 200, 230, iconPhone, webSub, noop, noop,
  0, nullptr, nullptr, nullptr, nullptr, noop, noopI, noBack, noopT, noopT };
const Section soundSection = { "Sound", 120, 200, 255, iconMic, sndSub, noop, noop,
  A_NUM, sndInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };
const Section subghzSection = { "Sub-GHz", 240, 120, 90, iconAntenna, subSub, noop, noop,
  G_NUM, subInfo, zero, noopI, noop, noop, noopI, noBack, noopT, noopT };
static void irSub(char* o, size_t n) { if (g_ir.have) snprintf(o, n, "last: %s", g_ir.last.proto); else snprintf(o, n, "38 kHz"); }
static void nfcSub(char* o, size_t n) { snprintf(o, n, "%lu cards", (unsigned long)g_nfc.taps); }
const Section irSection = { "IR", 230, 90, 120, iconRemote, irSub, noop, noop,
  0, nullptr, nullptr, nullptr, nullptr, noop, noopI, noBack, noopT, drawIr };
const Section nfcSection = { "NFC", 120, 230, 160, iconCard, nfcSub, noop, noop,
  0, nullptr, nullptr, nullptr, nullptr, noop, noopI, noBack, noopT, drawNfc };

// ---------- ИК: синтетические пачки разных пультов (проверка разбора) ----------
static std::vector<uint16_t> irDist(uint32_t hm, uint32_t hs, const std::vector<uint8_t>& bytes, int bits, uint32_t mk, uint32_t one, uint32_t zero, bool lsb) {
  std::vector<uint16_t> d = { (uint16_t)hm, (uint16_t)hs };
  for (int b = 0; b < bits; b++) {
    int bit = lsb ? (bytes[b / 8] >> (b % 8)) & 1 : (bytes[b / 8] >> (7 - b % 8)) & 1;
    d.push_back(mk + 40); d.push_back(bit ? one - 40 : zero - 40);   // приёмник удлиняет вспышки
  }
  d.push_back(mk);
  return d;
}
static std::vector<uint16_t> runs(const std::vector<uint8_t>& h, uint32_t unit) {   // полубиты -> длительности (с первой вспышки)
  std::vector<uint16_t> d; int i = 0;
  while (i < (int)h.size() && h[i] == 0) i++;
  while (i < (int)h.size()) { int j = i; while (j < (int)h.size() && h[j] == h[i]) j++; d.push_back((j - i) * unit); i = j; }
  if (d.size() % 2 == 0) d.pop_back();                      // хвостовая пауза не видна
  return d;
}
static void irTestAll(bool verbose) {
  std::vector<std::vector<uint16_t>> f;
  f.push_back(irDist(9000, 4500, { 0x04, 0xFB, 0x08, 0xF7 }, 32, 560, 1690, 560, true));        // LG TV, power
  f.push_back(irDist(4500, 4500, { 0x07, 0x07, 0x02, 0xFD }, 32, 560, 1690, 560, true));        // Samsung TV
  { std::vector<uint16_t> d = { 2400, 600 }; uint32_t v = 0x15 | (1 << 7);                       // Sony TV 12 бит
    for (int b = 0; b < 12; b++) { d.push_back((v >> b) & 1 ? 1200 : 600); if (b < 11) d.push_back(600); } f.push_back(d); }
  { std::vector<uint8_t> h; uint16_t v = 0x3000 | (0 << 6) | 0x0C;                              // RC5 Philips TV, cmd 12
    for (int b = 13; b >= 0; b--) { int bit = (v >> b) & 1; h.push_back(bit ? 0 : 1); h.push_back(bit ? 1 : 0); }
    f.push_back(runs(h, 889)); }
  { std::vector<uint8_t> h = { 1, 1, 1, 1, 1, 1, 0, 0 };                                         // RC6: лидер 6+2 полубитов по 444
    h.push_back(1); h.push_back(0);                                                               // старт 1
    for (int b = 0; b < 3; b++) { h.push_back(0); h.push_back(1); }                               // режим 0
    h.push_back(0); h.push_back(0); h.push_back(1); h.push_back(1);                               // прицеп (toggle 0)
    uint16_t v = 0x000C;
    for (int b = 15; b >= 0; b--) { int bit = (v >> b) & 1; h.push_back(bit); h.push_back(!bit); }
    auto d = runs(h, 444); d[0] = 2666; d[1] = 889; f.push_back(d); }
  f.push_back(irDist(3456, 1728, { 0x02, 0x20, 0xE0, 0x04, 0x3D, 0xBD }, 48, 432, 1296, 432, true));   // Panasonic
  f.push_back(irDist(8500, 4250, { 0x88, 0x00, 0x34, 0x70 }, 28, 550, 1600, 550, false));          // LG кондиционер
  { std::vector<uint16_t> d = { 3100, 1600 }; for (int i = 0; i < 139; i++) d.push_back(i % 2 ? (i % 7 < 3 ? 1150 : 400) : 450); f.push_back(d); }   // «кондиционер»
  f.push_back(irDist(9000, 4500, { 0xEE, 0x87, 0x0C, 0x5A }, 32, 560, 1690, 560, true));          // Apple
  for (auto& d : f) {
    IrCode c;
    bool ok = irDecode(d.data(), (int)d.size(), c);
    if (verbose) printf("ir: n=%3d -> %s %-9s addr=%lX cmd=%lX bits=%d brand=%s\n", (int)d.size(), ok ? "ok" : "--", c.proto,
                        (unsigned long)c.addr, (unsigned long)c.cmd, c.bits, ok ? irBrand(c) : "");
  }
  for (size_t i = 0; i < f.size(); i++) { s_now += 1500; irFeed(f[i].data(), (int)f[i].size(), s_now); }
  s_now += 1500; irFeed(f[0].data(), (int)f[0].size(), s_now);                                   // снова LG — последняя
  { uint16_t r[] = { 9000, 2250, 560 }; s_now += 110; irFeed(r, 3, s_now); }                     // держат кнопку
}

// ---------- вывод ----------
static const char* s_outDir = "out";
static std::vector<Blip> blipsAnim;
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

extern int g_theme;
int main(int argc, char** argv) {
  if (argc > 1) s_outDir = argv[1];
  spr.setColorDepth(16);
  spr.createSprite(320, 170);
  settingsBegin();
  if (getenv("SIM_THEME")) g_theme = atoi(getenv("SIM_THEME"));   // 0 — DENKO, 1 — SUMI (тушь)
  g_bat.ok = true; g_bat.soc = 87; g_bat.mv = 3980; g_bat.ma = -132; g_bat.tte = 250; g_bat.chgOk = true;
  fillBle(); fillWifi(); fillSub();
  { String j; settingsJson(j);                            // то же, что палка отдаёт планшету по /api/settings
    FILE* f = fopen((std::string(getenv("SIM_OUT") ? getenv("SIM_OUT") : "tools/sim/out") + "/settings.json").c_str(), "w");
    if (f) { fputs(j.c_str(), f); fclose(f); }
    String r; settingsSetKey("ledbr", "70", r); printf("set ledbr: %s\n", r.c_str());
    settingsSetKey("theme", "sumi", r); printf("set theme: %s\n", r.c_str()); settingsSetKey("theme", "denko", r);
    settingsSetKey("nope", "1", r); printf("set nope: %s\n", r.c_str()); }

  { const uint32_t ts[] = { 250, 600, 1000, 1300, 1680, 1950, 2200 };
    for (int i = 0; i < 7; i++) { char nm[32]; snprintf(nm, sizeof nm, "00_boot_%04u", (unsigned)ts[i]); bootFrame(ts[i]); snap(nm); } }
  navBegin(); navDraw(s_now);                    snap("01_menu");
  g_powerHold = 1500; drawPowerHold();           snap("08_power_hold"); g_powerHold = 0;
  { const uint32_t ts[] = { 300, 760, 1000, 1250, 1400 };
    for (int i = 0; i < 5; i++) { char nm[32]; snprintf(nm, sizeof nm, "09_power_off_%04u", (unsigned)ts[i]); shutFrame(ts[i]); spr.pushSprite(0, 0); snap(nm); } }
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
  blipsAnim = blips;
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
  g_uiPages = 6; g_uiPage = 5;
  drawWeather(sens, s_now);                                              snap("17_ble_weather");
  g_uiPages = 0;

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
  g_uiPages = 5; g_uiPage = 4;
  drawFind(s_now);           snap("25_wifi_find");
  g_uiPages = 0;

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
  g_uiPages = 6; g_uiPage = 0;
  drawAuBars(s_now);         snap("40_sound_bars");
  g_uiPages = 0;
  for (int f = 0; f < 30; f++) {                        // «горы»: прокрутить историю
    for (int k = 0; k < AU_BANDS; k++) g_au.bands[k] = constrain(0.15f + 0.7f * expf(-(k - 4 - (f % 7)) * (k - 4 - (f % 7)) / 12.0f) * (0.5f + 0.5f * sinf(f * 0.5f + k * 0.2f)) + noise(k + f) * 0.1f, 0.0f, 1.0f);
    drawTerrain(s_now + f * 50);
  }
  snap("40b_sound_terrain");
  drawReactor(s_now);        snap("41_sound_reactor");
  drawLevel(s_now);          snap("42_sound_level");
  drawWave(s_now);           snap("43_sound_scope");
  drawXY(s_now);             snap("44b_sound_xy_mic");
  { FILE* f = fopen("/tmp/claude-0/-home-user-lilygostuff/c5b88f80-222b-5b59-9742-a02655fa5cbd/scratchpad/pa_line.txt", "r");
    if (f) { static char ln[1200]; size_t n = fread(ln, 1, sizeof(ln) - 1, f); ln[n] = 0; fclose(f);
      Serial.feed(ln); pcPoll(0);
      for (int k = 0; k < 4; k++) drawXY(s_now);
      snap("45b_sound_xy_pc"); drawAuBars(s_now); snap("45c_sound_bars_pc"); g_auFromPc = false; } }
  g_au.level = -21; g_au.peak = -18;
  drawLevel(s_now);          snap("42b_sound_level_loud");
  drawMicMissing();          snap("44_mic_check");
  g_micDiag.disabled = true; drawMicMissing(); snap("45_mic_off");

  for (int f = 0; f < 40; f++) drawTesseract(s_now + f * 33);   snap("50_gallery_tesseract");
  for (int f = 0; f < 10; f++) drawTorus(s_now + 1500 + f * 33); snap("51_gallery_torus");
  for (int f = 0; f < 60; f++) drawLife(s_now + 2000 + f * 120); snap("52_gallery_life");
  for (int f = 0; f < 160; f++) drawPendulum(s_now + 9000 + f * 33); snap("53_gallery_pendulum");
  { char l1[] = "TLE\tISS\t1 25544U 98067A   26318.50000000  .00016717  00000+0  30472-3 0  9990\t2 25544  51.6400 120.0000 0003000  90.0000 270.0000 15.50000000400000";
    char l2[] = "TLE\tTiangong\t1 48274U 21035A   26318.40000000  .00020000  00000+0  25000-3 0  9990\t2 48274  41.4700 210.0000 0005000  30.0000 330.0000 15.60000000300000";
    satFromLine(l1); satFromLine(l2);
    astroSetTime((uint32_t)astroUnixNow(), 10800);
    for (int f = 0; f < 3; f++) drawSky(s_now + f * 40);   snap("54_gallery_sky");
    skyClick(); drawSky(s_now);                           snap("54b_gallery_sky_stars"); skyClick(); skyClick();
    for (int f = 0; f < 60; f++) drawGlobe(s_now + f * 40); snap("55_gallery_globe"); }
  { // радиопогода: подставные значения (расчёт на палке — wxUpdate)
    WxState& w = g_wx; w.ok = true; w.cond = "rain"; w.clouds = 0.62f; w.wind = 0.35f; w.rain = 0.55f; w.storm = 0.4f;
    w.ble = 31; w.turn = 9; w.sub = 3; w.subPk = -61; w.subOk = true; w.snd = -38; w.pk = -24; w.sndOk = true; w.trend = 0.5f;
    w.histN = WX_HIST; for (int i = 0; i < WX_HIST; i++) { w.hist[i][0] = 0.3f + 0.3f * i / WX_HIST + 0.05f * sinf(i * 0.5f); w.hist[i][1] = (i % 13 < 3) ? 0.6f : 0.08f; }
    drawWeather(s_now);                                  snap("56_gallery_weather");
    w.cond = "storm"; w.storm = 0.8f; w.rain = 0.9f; w.thunder = true; w.thunderMs = s_now - 120; w.clouds = 0.85f;
    drawWeather(s_now);                                  snap("56b_gallery_weather_storm");
    w.cond = "clear"; w.storm = 0; w.rain = 0; w.thunder = false; w.thunderMs = 0; w.clouds = 0.2f; w.wind = 0.1f;
    drawWeather(s_now);                                  snap("56c_gallery_weather_clear");
    wxTurn(1); drawWeather(s_now);                       snap("56d_gallery_weather_chart"); wxTurn(1); }
  { uint8_t qr[29 * 29]; for (int i = 0; i < 29 * 29; i++) qr[i] = (noise(i) > 0.5f) || ((i % 29 < 7 || i % 29 > 21) && (i / 29 < 7) && !((i % 29 == 1 || i % 29 == 5 || i % 29 == 23 || i % 29 == 27) && i / 29 > 0 && i / 29 < 6));
    WebView v = { "TENGAN-3F2A", "48213907", "192.168.4.1", 1, 214, true, "sub-ghz" };
    drawWebScreen(v, qr, 29); snap("60_web"); }
  { WebLink l = { 1, "BLE  WX  ", 3400, 0 };              // планшет подключён: живая картинка по теме
    char nm[32];
    for (int f = 0; f < 48; f++) { drawWebLink(l, 2000 + f * 70); snprintf(nm, sizeof nm, "anim_link_%03d", f); snap(nm); } }
  { // тема с планшета: SUMI -> DENKO посреди анимации связи (воспроизведение падения)
    WebLink l = { 1, "BLE  ", 3000, 0 }; String o;
    settingsSetKey("theme", "sumi", o); printf("set sumi: %s\n", o.c_str());
    for (int f = 0; f < 5; f++) drawWebLink(l, 9000 + f * 70);
    settingsSetKey("theme", "denko", o); printf("set denko: %s\n", o.c_str());
    for (int f = 0; f < 5; f++) drawWebLink(l, 9500 + f * 70);
    snap("zz_theme_flip"); printf("theme flip ok\n"); }
  { // IR и NFC (разбор — настоящий код, данные синтетические)
    g_ir.on = g_ir.ok = true;
    drawIr(s_now);                                       snap("80_ir_idle");
    uint32_t keep = s_now;
    irTestAll(true);
    drawIr(s_now + 120);                                 snap("81_ir_lg");
    drawIr(s_now + 2000);                                snap("81b_ir_settled");
    s_now = keep;
    g_nfc.state = NFC_SCAN;
    drawNfc(s_now + 900);                                snap("82_nfc_idle");
    NfcCard c; memset(&c, 0, sizeof c);
    uint8_t u1[4] = { 0x3A, 0x91, 0x0C, 0x55 }; memcpy(c.uid, u1, 4); c.uidLen = 4; c.atqa = 0x0004; c.sak = 0x08;
    nfcClassify(c, -1); nfcCommit(c, s_now - 60000);
    NfcCard t; memset(&t, 0, sizeof t);
    uint8_t u2[7] = { 0x04, 0xA2, 0x5B, 0x1A, 0x9C, 0x70, 0x80 }; memcpy(t.uid, u2, 7); t.uidLen = 7; t.atqa = 0x0044; t.sak = 0x00;
    const char url[] = "tengan.dev/hello";
    uint8_t mem[64] = { 0x03, 0, 0xD1, 0x01, (uint8_t)(sizeof(url)), 'U', 0x04 };
    memcpy(mem + 7, url, sizeof(url) - 1);
    int p = 7 + sizeof(url) - 1;
    const char txtr[] = "\x02" "ruПривет, палка";
    mem[p++] = 0x51; mem[p++] = 0x01; mem[p++] = (uint8_t)(sizeof(txtr) - 1); mem[p++] = 'T';
    memcpy(mem + p, txtr, sizeof(txtr) - 1); p += sizeof(txtr) - 1;
    mem[1] = (uint8_t)(p - 2); mem[p++] = 0xFE;
    mem[2] = 0x91;                                           // первая запись: MB, SR, TNF=1 (не последняя)
    nfcParseNdef(mem, p, t); nfcClassify(t, 0x0F); nfcCommit(t, s_now - 200);
    printf("nfc: %s / %s / ndef=%d [%s] [%s]\n", t.fam, t.hint, t.nNdef, t.ndef[0], t.nNdef > 1 ? t.ndef[1] : "");
    g_nfc.lastSeen = s_now;
    drawNfc(s_now);                                      snap("83_nfc_tag");
    NfcCard b; memset(&b, 0, sizeof b);
    uint8_t u3[4] = { 0x08, 0x4F, 0x21, 0x9E }; memcpy(b.uid, u3, 4); b.uidLen = 4; b.atqa = 0x0004; b.sak = 0x20; b.isoDep = true; b.bank = true;
    nfcClassify(b, -1); nfcCommit(b, s_now - 300); g_nfc.lastSeen = s_now;
    drawNfc(s_now);                                      snap("84_nfc_bank");
    g_nfc.present = false; drawNfc(s_now + 14000);       snap("84b_nfc_bank_gone");
    { String j; char u[24]; nfcUidMasked(b, u, sizeof u, true); printf("nfc uid: %s\n", u); }
    g_nfc.state = NFC_NOREADER; drawNfc(s_now);          snap("85_nfc_noreader"); g_nfc.state = NFC_SCAN; }
  drawPcWaiting(s_now);      snap("70_pc_waiting");
  for (int i = 0; i < 60; i++) {                        // минута «настоящих» строк с ноутбука
    char ln[300];
    float cpu = 18 + 14 * sinf(i * 0.25f) + noise(i) * 10 + (i > 44 ? 40 : 0);
    snprintf(ln, sizeof ln, "PC cpu=%.1f freq=2.80 c=%d,%d,%d,%d,%d,%d,%d,%d ram=%.1f ramu=%.1f ramt=16.0 dsk=71 dr=1.2 dw=0.4 nd=%d nu=46 up=11520 top=chrome host=WOLF-LAPTOP bat=78 plug=0\n",
             cpu, (int)cpu + 10, (int)cpu - 5, 90, 12, 33, 8, 61, 4, 58 + i * 0.05f, 9.4f + i * 0.01f, 1240 + i * 10);
    Serial.feed(ln); pcPoll(0);
  }
  g_pc.t = s_now;
  drawPc(s_now);             snap("71_pc_overview");
  drawPcCores(s_now);        snap("72_pc_cores");
  Serial.feed("PP\tchrome,384,2840,1\tCode,121,1240,0\tTelegram,45,410,0\tsvchost,38,620,0\tDiscord,72,780,0\tYandexMusic,64,520,0\t"
              "explorer,12,210,0\tMsMpEng,95,380,0\tdwm,52,160,0\tpython,31,120,0\tSteam,8,340,0\tSearchHost,4,180,0\tOneDrive,6,90,0\t"
              "audiodg,18,40,0\tTeams,22,690,0\tNVIDIA,9,150,0\tgame,612,3100,0\n");
  for (int f = 0; f < 50; f++) { pcPoll(0); drawPcCity(s_now + f * 33); }
  snap("72b_pc_city");
  pcCityNext(); drawPcCity(s_now + 2000); snap("72c_pc_city_spot");
  Serial.feed("PV vol=42 mute=0\n"); pcPoll(0); g_pc.volT = s_now;
  g_pcLastCmd = 1; g_pcLastCmdMs = s_now - 120;
  Serial.feed("PM\t1\t63\t215\tSpotify\tГруппа крови (Remastered 2024 Edition)\tКино\n"); pcPoll(0); g_pc.trkT = s_now;
  g_pc.trkT = 0; g_pcLastCmdMs = 0; g_pc.volT = 0; drawPcMedia(300); snap("73_pc_media");   // бегущая строка — с начала
  Serial.feed("PM\n"); pcPoll(0); g_pc.trkT = s_now;
  drawPcMedia(s_now);        snap("74_pc_media_idle");
  g_uiPages = 2; g_uiPage = 0;
  drawSpectrum();            snap("30_subghz_spectrum");
  g_uiPages = 0;
  drawMonitor(s_now);        snap("31_subghz_monitor");
  drawSensors(s_now);        snap("32a_subghz_sensors_empty");
  simBell();                 sensFeed(s_tr, -71, s_now - 95000);
  simTx141(0x9b, 1, 21.4f, 38); sensFeed(s_tr, -78, s_now - 41000);
  simNexus(0x5a, 1, 4.2f, 81);  sensFeed(s_tr, -62, s_now - 12000);
  simNexus(0x2c, 3, -1.6f, 0);  sensFeed(s_tr, -88, s_now - 700000);
  printf("sensors: %d decoded\n", g_sensN);
  { String o; sensJson(o, s_now); printf("sens json: %s\n", o.c_str()); }
  drawSensors(s_now - 11000);  snap("32b_subghz_sensors");
  simBell();                 sensFeed(s_tr, -66, s_now - 400);
  drawSensors(s_now);        snap("32c_subghz_sensors_bell");
  { WxState& w = g_wx; w.thunder = false; w.thunderMs = 0; w.cond = "cloudy"; w.clouds = 0.45f; w.rain = 0.1f; w.storm = 0.1f;
    float t; int h; const char* src;
    w.tOk = sensOutside(t, h, src, s_now); if (w.tOk) { w.temp = t; w.hum = h; w.tsrc = src; }
    drawWeather(s_now);      snap("56e_gallery_weather_temp");
    String o; wxJson(o); printf("wx json tail: %s\n", o.c_str() + (o.length() > 60 ? o.length() - 60 : 0)); }
  if (getenv("SIM_ANIM")) {                              // кадры для гифок README (tools/make_docs.py)
    char nm[48];
    for (int f = 0; f * 40 <= 2700; f++) { bootFrame(f * 40); snprintf(nm, sizeof nm, "anim_boot_%03d", f); snap(nm); }
    for (int f = 0; f < 60; f++) { drawCity(blipsAnim, s_now + 4000 + f * 50); snprintf(nm, sizeof nm, "anim_city_%03d", f); snap(nm); }
    for (int f = 0; f < 60; f++) { drawTesseract(s_now + 9000 + f * 40); snprintf(nm, sizeof nm, "anim_tess_%03d", f); snap(nm); }
    for (int f = 0; f < 36; f++) { shutFrame(f * 40); spr.pushSprite(0, 0); snprintf(nm, sizeof nm, "anim_off_%03d", f); snap(nm); }
  }
  return 0;
}
