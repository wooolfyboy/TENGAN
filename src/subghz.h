// ============================================================
//  subghz.h — раздел Sub-GHz на CC1101 (подключается внутрь main.cpp)
//
//  ТОЛЬКО ПРИЁМ: модуль никогда не переводится в передачу (STX не
//  используется). Мерим силу сигнала (RSSI) на частотах 315 / 433 / 868 / 915 МГц.
//
//  Экраны:
//    SPECTRUM — развёртка по полосе ±2 МГц + «водопад»
//    MONITOR  — слушаем одну частоту и ловим всплески (брелоки, звонки, датчики)
//
//  CC1101 делит линии SPI с экраном. Библиотека экрана настраивает SPI только
//  «на запись», поэтому перед каждым обменом с CC1101 включаем чтение
//  (SET_BUS_READ_MODE), а экран при следующей отрисовке вернёт своё.
// ============================================================
#pragma once
#include <SPI.h>

#define CC_CS    12
#define CC_MISO  10
#define CC_SW1   47     // переключатель антенны: SW1/SW0 = 1/0 315, 1/1 433, 0/1 868/915
#define CC_SW0   48

enum { G_SPECTRUM = 0, G_MONITOR, G_NUM };
int  g_gMode = 0;
bool g_ccOk = false, g_ccTried = false, g_ccOn = false;
uint8_t g_ccVersion = 0;

struct Band { const char* name; float lo, hi, mon; };
static const Band BANDS[] = {
  { "315 MHz", 313.0f, 317.0f, 315.00f },
  { "433 MHz", 431.92f, 435.92f, 433.92f },
  { "868 MHz", 866.3f, 870.3f, 868.30f },
  { "915 MHz", 913.0f, 917.0f, 915.00f },
};
static const int N_BANDS = 4;
int g_band = 1;

SPISettings g_ccSet(4000000, MSBFIRST, SPI_MODE0);
SPIClass* g_ccSpi = nullptr;

// ---------- низкий уровень ----------
static bool ccSel() {
  digitalWrite(CC_CS, LOW);
  uint32_t t = micros();
  while (digitalRead(CC_MISO)) if (micros() - t > 3000) return false;   // ждём готовности
  return true;
}
static void ccBegin() { g_ccSpi->beginTransaction(g_ccSet); SET_BUS_READ_MODE; }
static void ccEnd()   { digitalWrite(CC_CS, HIGH); g_ccSpi->endTransaction(); }

uint8_t ccStrobe(uint8_t s) {
  ccBegin(); ccSel();
  uint8_t st = g_ccSpi->transfer(s);
  ccEnd();
  return st;
}
void ccWrite(uint8_t a, uint8_t v) {
  ccBegin(); ccSel();
  g_ccSpi->transfer(a); g_ccSpi->transfer(v);
  ccEnd();
}
uint8_t ccReadStatus(uint8_t a) {            // регистры состояния 0x30..0x3D
  ccBegin(); ccSel();
  g_ccSpi->transfer(a | 0xC0);
  uint8_t v = g_ccSpi->transfer(0);
  ccEnd();
  return v;
}

int ccRssi() {
  uint8_t r = ccReadStatus(0x34);
  int v = (r >= 128) ? ((int)r - 256) / 2 : r / 2;
  return v - 74;
}

void ccAntenna(float mhz) {
  if (mhz < 350)      { digitalWrite(CC_SW1, HIGH); digitalWrite(CC_SW0, LOW);  }
  else if (mhz < 500) { digitalWrite(CC_SW1, HIGH); digitalWrite(CC_SW0, HIGH); }
  else                { digitalWrite(CC_SW1, LOW);  digitalWrite(CC_SW0, HIGH); }
}

void ccSetFreq(float mhz) {
  uint32_t f = (uint32_t)(mhz * 65536.0f / 26.0f + 0.5f);
  ccWrite(0x0D, (f >> 16) & 0xFF);
  ccWrite(0x0E, (f >> 8) & 0xFF);
  ccWrite(0x0F, f & 0xFF);
  // нижняя часть каждого диапазона требует другой настройки VCO (как в библиотеке ELECHOUSE)
  bool low = (mhz < 322.88f) || (mhz >= 378 && mhz < 430.5f) || (mhz >= 779 && mhz < 861);
  ccWrite(0x2E, low ? 0x0B : 0x09);          // TEST0
}

// ширина полосы приёма: 0x?5 в MDMCFG4; F5 ≈ 58 кГц, A5 ≈ 270 кГц
void ccSetBw(uint8_t mdmcfg4) { ccWrite(0x10, mdmcfg4); }

bool ccInit() {
  g_ccSpi = &tft.getSPIinstance();
  pinMode(CC_CS, OUTPUT); digitalWrite(CC_CS, HIGH);
  pinMode(CC_SW1, OUTPUT); pinMode(CC_SW0, OUTPUT);
  ccAntenna(BANDS[g_band].mon);
  // сброс
  ccStrobe(0x30);                              // SRES
  delay(2);
  uint8_t part = ccReadStatus(0x30), ver = ccReadStatus(0x31);
  g_ccVersion = ver;
  if (part != 0x00 || (ver != 0x14 && ver != 0x04 && ver != 0x07)) return false;

  // приём OOK/ASK, без пакетов — нам нужен только уровень сигнала
  static const uint8_t cfg[][2] = {
    {0x00, 0x2E}, {0x01, 0x2E}, {0x02, 0x2E},    // GDO — высокий импеданс (MISO свободен)
    {0x03, 0x47}, {0x08, 0x32}, {0x0B, 0x06}, {0x0C, 0x00},
    {0x10, 0xF5}, {0x11, 0x93}, {0x12, 0x30}, {0x13, 0x22}, {0x14, 0xF8},
    {0x15, 0x15}, {0x16, 0x07}, {0x17, 0x30}, {0x18, 0x18},
    {0x19, 0x16}, {0x1A, 0x6C}, {0x1B, 0x07}, {0x1C, 0x00}, {0x1D, 0x91},
    {0x21, 0x56}, {0x22, 0x11},
    {0x23, 0xE9}, {0x24, 0x2A}, {0x25, 0x00}, {0x26, 0x1F},
    {0x2C, 0x81}, {0x2D, 0x35}, {0x2E, 0x09},
  };
  for (auto& r : cfg) ccWrite(r[0], r[1]);
  ccStrobe(0x36);                              // SIDLE
  return true;
}

void ccSleep() {
  if (!g_ccOk) return;
  ccStrobe(0x36);                              // SIDLE
  ccStrobe(0x39);                              // SPWD — сон
  g_ccOn = false;
}

// ---------- SPECTRUM ----------
#define SP_N 96
#define WF_ROWS 56
int8_t  g_spCur[SP_N], g_spPeak[SP_N];
uint8_t g_wf[WF_ROWS][SP_N];
int     g_wfHead = 0, g_wfCount = 0;
int     g_spIdx = 0;
int8_t  g_spRow[SP_N];

float spFreq(int i) { const Band& b = BANDS[g_band]; return b.lo + (b.hi - b.lo) * i / (SP_N - 1); }

void spReset() {
  for (int i = 0; i < SP_N; i++) { g_spCur[i] = -110; g_spPeak[i] = -110; g_spRow[i] = -110; }
  g_wfHead = 0; g_wfCount = 0; g_spIdx = 0;
}

int ccMeasure(float mhz) {
  ccStrobe(0x36);                              // SIDLE
  ccSetFreq(mhz);
  ccStrobe(0x34);                              // SRX (с автокалибровкой)
  uint32_t t = micros();
  while ((ccReadStatus(0x35) & 0x1F) != 0x0D && micros() - t < 2000) {}   // ждём RX
  delayMicroseconds(250);                      // RSSI устанавливается
  return ccRssi();
}

void spTick() {
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

// цвет водопада: -110 (чёрный) -> синий -> бирюзовый -> жёлтый -> красный (-40)
uint16_t wfColor(int v) {        // v = dBm + 120
  float x = constrain((v - 10) / 70.0f, 0.0f, 1.0f);
  uint8_t r, g, b;
  if (x < 0.25f)      { float k = x / 0.25f; r = 0; g = 0; b = (uint8_t)(40 + 160 * k); }
  else if (x < 0.5f)  { float k = (x - 0.25f) / 0.25f; r = 0; g = (uint8_t)(200 * k); b = 200; }
  else if (x < 0.75f) { float k = (x - 0.5f) / 0.25f; r = (uint8_t)(240 * k); g = 200 + (uint8_t)(40 * k); b = (uint8_t)(200 * (1 - k)); }
  else                { float k = (x - 0.75f) / 0.25f; r = 240; g = (uint8_t)(240 * (1 - k)); b = 0; }
  return tft.color565(r, g, b);
}

void drawSpectrum() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[40];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("SPECTRUM", 4, 1, 2);
  spr.setTextColor(tft.color565(200, 160, 90), TFT_BLACK);
  spr.drawString(BANDS[g_band].name, 84, 1, 2);

  int pl = 4, pr = W - 4, pt = 20, pb = 78;
  auto px = [&](int i) { return pl + i * (pr - pl) / (SP_N - 1); };
  auto py = [&](int db) { int c = constrain(db, -110, -40); return pb - (c + 110) * (pb - pt) / 70; };
  spr.drawRect(pl, pt, pr - pl + 1, pb - pt + 1, tft.color565(35, 35, 45));
  for (int db = -100; db <= -50; db += 10) spr.drawFastHLine(pl + 1, py(db), pr - pl - 1, tft.color565(22, 22, 28));
  // пики — тускло, текущий уровень — ярко
  for (int i = 1; i < SP_N; i++) spr.drawLine(px(i - 1), py(g_spPeak[i - 1]), px(i), py(g_spPeak[i]), tft.color565(90, 70, 40));
  for (int i = 0; i < SP_N; i++) {
    int x = px(i), y = py(g_spCur[i]);
    spr.drawFastVLine(x, y, pb - y, tft.color565(25, 60, 50));
  }
  for (int i = 1; i < SP_N; i++) spr.drawLine(px(i - 1), py(g_spCur[i - 1]), px(i), py(g_spCur[i]), tft.color565(90, 230, 160));
  // маркер максимума
  int mi = 0;
  for (int i = 1; i < SP_N; i++) if (g_spCur[i] > g_spCur[mi]) mi = i;
  spr.drawFastVLine(px(mi), pt + 1, pb - pt - 1, tft.color565(240, 200, 70));
  sprintf(buf, "%.2f MHz %d dBm", spFreq(mi), g_spCur[mi]);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(tft.color565(240, 200, 70), TFT_BLACK);
  spr.drawString(buf, W - 4, 5, 1);

  // водопад: новые строки сверху
  int wt = pb + 3, wh = H - 14 - wt;
  for (int r = 0; r < g_wfCount && r < wh; r++) {
    int row = (g_wfHead - 1 - r + WF_ROWS) % WF_ROWS;
    for (int i = 0; i < SP_N; i++) {
      int x0 = px(i), x1 = (i + 1 < SP_N) ? px(i + 1) : pr + 1;
      spr.drawFastHLine(x0, wt + r, x1 - x0, wfColor(g_wf[row][i]));
    }
  }
  // ось частот
  spr.setTextColor(C_DIM(), TFT_BLACK);
  sprintf(buf, "%.2f", BANDS[g_band].lo);
  spr.setTextDatum(BL_DATUM); spr.drawString(buf, pl, H - 1, 1);
  sprintf(buf, "%.2f", (BANDS[g_band].lo + BANDS[g_band].hi) / 2);
  spr.setTextDatum(BC_DATUM); spr.drawString(buf, W / 2, H - 1, 1);
  sprintf(buf, "%.2f", BANDS[g_band].hi);
  spr.setTextDatum(BR_DATUM); spr.drawString(buf, pr, H - 1, 1);
  spr.pushSprite(0, 0);
}

// ---------- MONITOR ----------
#define MON_N 300
int8_t   g_mon[MON_N];
int      g_monN = 0;
float    g_floor = -100;          // уровень шума (медленное среднее)
bool     g_burst = false;
uint32_t g_burstStart = 0, g_burstLastHi = 0;
int      g_burstPeak = -120;
uint32_t g_events = 0;
struct Burst { uint32_t t; uint16_t durMs; int8_t peak; };
#define EV_N 6
Burst    g_ev[EV_N];
int      g_evN = 0;
static const int BURST_ON_DB = 12, BURST_OFF_DB = 7;

void monStart() {
  ccStrobe(0x36);
  ccSetBw(0xA5);                               // пошире (~270 кГц) — брелоки «гуляют» по частоте
  ccSetFreq(BANDS[g_band].mon);
  ccStrobe(0x34);                              // SRX
  g_monN = 0; g_burst = false;
}

void monTick(uint32_t now) {
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

void drawMonitor(uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("MONITOR", 4, 1, 2);
  sprintf(buf, "%.2f MHz", BANDS[g_band].mon);
  spr.setTextColor(tft.color565(200, 160, 90), TFT_BLACK);
  spr.drawString(buf, 76, 1, 2);
  int cur = g_monN ? g_mon[g_monN - 1] : -120;
  sprintf(buf, "%d dBm", cur);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(g_burst ? tft.color565(240, 200, 70) : TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W - 4, 1, 2);

  // лента сигнала (~6 с), линия шума и порог срабатывания
  int gx = 4, gy = 20, gw = W - 8, gh = 70;
  spr.drawRect(gx, gy, gw, gh, tft.color565(40, 40, 50));
  auto yy = [&](int db) { int c = constrain(db, -115, -35); return gy + gh - 2 - (c + 115) * (gh - 4) / 80; };
  spr.drawFastHLine(gx + 1, yy((int)g_floor), gw - 2, tft.color565(60, 60, 75));
  spr.drawFastHLine(gx + 1, yy((int)g_floor + BURST_ON_DB), gw - 2, tft.color565(90, 70, 30));
  for (int i = 0; i < g_monN; i++) {
    int x = gx + 1 + (gw - 3) * (i + (MON_N - g_monN)) / (MON_N - 1);
    int y = yy(g_mon[i]);
    bool hi = g_mon[i] > g_floor + BURST_ON_DB;
    spr.drawFastVLine(x, y, gy + gh - 2 - y, hi ? tft.color565(240, 200, 70) : tft.color565(50, 120, 100));
  }
  sprintf(buf, "noise %d dBm", (int)g_floor);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString(buf, gx + 3, gy + 3, 1);

  // журнал всплесков
  sprintf(buf, "bursts: %lu", (unsigned long)g_events);
  spr.setTextColor(tft.color565(240, 200, 70), TFT_BLACK);
  spr.drawString(buf, 4, 96, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("click: next band", W - 4, 100, 1);
  for (int i = 0; i < g_evN && i < 4; i++) {
    const Burst& b = g_ev[g_evN - 1 - i];
    uint32_t ago = (now - b.t) / 1000;
    if (ago < 60) sprintf(buf, "%lus ago", (unsigned long)ago);
    else if (ago < 3600) sprintf(buf, "%lum ago", (unsigned long)(ago / 60));
    else sprintf(buf, "%luh ago", (unsigned long)(ago / 3600));
    int y = 116 + i * 13;
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_GRAY(), TFT_BLACK);
    spr.drawString(buf, 4, y, 1);
    sprintf(buf, "%.1f s", b.durMs / 1000.0f);
    spr.drawString(buf, 90, y, 1);
    sprintf(buf, "%d dBm", b.peak);
    spr.drawString(buf, 150, y, 1);
    int len = constrain((b.peak + 110) * 2, 2, 100);
    spr.fillRect(W - 4 - len, y + 1, len, 6, tft.color565(240, 200, 70));
  }
  if (g_evN == 0) {
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString("press a car key / gate remote / doorbell nearby", 4, 120, 1);
  }
  spr.pushSprite(0, 0);
}

// ---------- раздел целиком ----------
void subEnter() {
  if (!g_ccTried) { g_ccTried = true; g_ccOk = ccInit(); spReset(); }
  if (!g_ccOk) return;
  ccAntenna(BANDS[g_band].mon);
  ccStrobe(0x36);
  g_ccOn = true;
}
void subLeave() { ccSleep(); strip.clear(); strip.show(); }

void subStartMode() {
  if (!g_ccOk) return;
  ccAntenna(BANDS[g_band].mon);
  if (g_gMode == G_SPECTRUM) { ccSetBw(0xF5); spReset(); }
  else monStart();
  strip.clear(); strip.show();
}
void subOpenScreen(int m) { g_gMode = m; subStartMode(); }
void subCloseScreen() { if (g_ccOk) ccStrobe(0x36); strip.clear(); strip.show(); }
int  subScreenNow() { return g_gMode; }
void subRotate(int dir) { g_gMode = (g_gMode + dir + G_NUM) % G_NUM; subStartMode(); }
void subClick() { g_band = (g_band + 1) % N_BANDS; subStartMode(); }

// вызывается на каждом проходе loop(), пока открыт экран раздела
void subTick(uint32_t now) {
  if (!g_ccOk || !g_ccOn) return;
  if (g_gMode == G_SPECTRUM) spTick(); else monTick(now);
}

void drawCcMissing() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(tft.color565(235, 80, 80), TFT_BLACK);
  spr.drawString("CC1101 not found", W / 2, H / 2 - 12, 4);
  char buf[40];
  sprintf(buf, "chip answered 0x%02X", g_ccVersion);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString(buf, W / 2, H / 2 + 16, 2);
  spr.pushSprite(0, 0);
}

void subFrame(uint32_t now) {
  if (!g_ccOk) { drawCcMissing(); return; }
  if (g_gMode == G_SPECTRUM) drawSpectrum(); else drawMonitor(now);
}

void subSubFrame() {
  SubItem it[G_NUM] = {
    { "Spectrum", "band + waterfall",  "" },
    { "Monitor",  "catch remotes",     "" },
  };
  if (!g_ccOk && g_ccTried) { strcpy(it[0].value, "no chip"); strcpy(it[1].value, "no chip"); }
  else {
    strcpy(it[0].value, BANDS[g_band].name);
    if (g_events) sprintf(it[1].value, "%lu", (unsigned long)g_events); else strcpy(it[1].value, "-");
  }
  drawSubmenu("SUB-GHZ", tft.color565(200, 160, 90), it, G_NUM);
}
