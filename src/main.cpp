// ============================================================
//  BLE RADAR  —  визуальный сканер Bluetooth для LilyGO T-Embed CC1101
//
//  Режим 1 (RADAR):     сколько BLE-устройств вокруг (число + кольцо).
//  Режим 2 (PROXIMITY): ближайшее устройство: шкала, ~метры, dBm, вендор.
//  Режим 3 (SCOPE):     радар-скоуп. Радиус = реальный (RSSI), угол = по MAC.
//                       Цвет блипа = вендор (Apple/MS/Samsung/Google/...).
//  Кнопка (энкодер или боковая) листает режимы.
//
//  Весь код: Claude. Распиновка — из репозитория LilyGO.
// ============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------- пины ----------
#define BOARD_PWR_EN 15
#define PIN_TFT_CS   41
#define PIN_SD_CS    13
#define PIN_LORA_CS  12
#define PIN_NRF_CS   44
#define PIN_ENC_KEY  0
#define PIN_ENC_A    4    // энкодер фаза A
#define PIN_ENC_B    5    // энкодер фаза B
#define PIN_USER_KEY 6
#define PIN_WS2812   14
#define WS2812_N     8

static const uint32_t REMEMBER_MS = 30000;
static const uint32_t REDRAW_MS   = 66;

TFT_eSPI    tft;
TFT_eSprite spr = TFT_eSprite(&tft);

// категории вендоров
enum { V_PRIV = 0, V_APPLE, V_MS, V_SAMSUNG, V_GOOGLE, V_OTHER };

struct Dev  { uint32_t t; int rssi; uint8_t cat; };
struct Blip { int rssi; uint32_t h; uint8_t cat; };
std::map<std::string, Dev> g_seen;
SemaphoreHandle_t g_lock;

volatile int g_mode = 0;
int g_sweep = 0;

// --- настраиваемый порог события (крутилка) ---
int      g_thresh = 7;
uint32_t g_encActiveMs = 0;

// --- история для TIMELINE ---
#define HIST_N 150
struct Sample { int count; float base; int8_t ev; };
Sample   g_hist[HIST_N];
int      g_histLen = 0;
float    g_base = -1.0f;
volatile int g_lastCount = 0;
uint32_t g_lastSampleMs = 0;
uint32_t g_cooldownMs = 0;

// --- светодиоды на события ---
Adafruit_NeoPixel strip(WS2812_N, PIN_WS2812, NEO_GRB + NEO_KHZ800);
int      g_ledEvent = 0;        // 0 нет, +1 пришли, -1 ушли
uint32_t g_ledStart = 0;
const uint32_t LED_DUR = 1200;

// ---------- вендоры: цвет / буква / имя ----------
uint16_t vendorColor(uint8_t c) {
  switch (c) {
    case V_APPLE:   return tft.color565(240, 240, 240);
    case V_MS:      return tft.color565(80, 150, 240);
    case V_SAMSUNG: return tft.color565(60, 205, 205);
    case V_GOOGLE:  return tft.color565(240, 200, 70);
    case V_OTHER:   return tft.color565(240, 150, 50);
    default:        return tft.color565(120, 120, 130); // private
  }
}
const char* vendorLetter(uint8_t c) {
  switch (c) {
    case V_APPLE: return "A"; case V_MS: return "M"; case V_SAMSUNG: return "S";
    case V_GOOGLE: return "G"; case V_OTHER: return "?"; default: return ".";
  }
}
const char* vendorName(uint8_t c) {
  switch (c) {
    case V_APPLE: return "Apple"; case V_MS: return "Microsoft"; case V_SAMSUNG: return "Samsung";
    case V_GOOGLE: return "Google"; case V_OTHER: return "other"; default: return "private";
  }
}
uint8_t catFromCompanyId(uint16_t cid) {
  switch (cid) {
    case 0x004C: return V_APPLE;
    case 0x0006: return V_MS;
    case 0x0075: return V_SAMSUNG;
    case 0x00E0: return V_GOOGLE;
    default:     return V_OTHER;
  }
}

// RSSI -> ~метры (грубая оценка по модели затухания)
float estDistance(int rssi) {
  const float MEASURED = -59.0f; // ожидаемый RSSI на 1 м
  const float N = 2.5f;          // показатель затухания (помещение)
  float d = powf(10.0f, (MEASURED - (float)rssi) / (10.0f * N));
  if (d < 0.1f) d = 0.1f;
  if (d > 99.0f) d = 99.0f;
  return d;
}

// ---------- RADAR цвета ----------
uint16_t colorForCount(int n) {
  if (n <= 2)  return tft.color565(70, 200, 130);
  if (n <= 6)  return tft.color565(235, 205, 70);
  if (n <= 12) return tft.color565(240, 150, 50);
  return tft.color565(235, 80, 80);
}
const char* labelForCount(int n) {
  if (n <= 2) return "calm"; if (n <= 6) return "busy";
  if (n <= 12) return "crowded"; return "packed";
}

// ---------- колбэк сканера (NimBLE 2.x) ----------
class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    std::string mac = d->getAddress().toString();
    int rssi = d->getRSSI();
    uint8_t cat = V_PRIV;
    std::string md = d->getManufacturerData();
    if (md.size() >= 2) {
      uint16_t cid = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
      cat = catFromCompanyId(cid);
    }
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
      g_seen[mac] = { millis(), rssi, cat };
      xSemaphoreGive(g_lock);
    }
  }
};

// ---------- (настройка порога пока отключена; колесо листает режимы) ----------
void drawThresh() {}

// ---------- RADAR ----------
void drawRadar(int count) {
  int W = spr.width(), H = spr.height();
  uint16_t theme = colorForCount(count);
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("BLE RADAR", W / 2, 6, 4);
  int cx = W / 2, cy = H / 2 + 8, R = 54, slots = 24;
  for (int i = 0; i < slots; i++) {
    float a = ((float)i / slots) * 2.0f * PI - (PI / 2.0f);
    int x = cx + (int)(cos(a) * R), y = cy + (int)(sin(a) * R);
    uint16_t c = (i < count) ? theme : tft.color565(40, 40, 48);
    spr.fillCircle(x, y, 3, c);
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawNumber(count, cx, cy, 7);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(theme, TFT_BLACK);
  spr.drawString(labelForCount(count), W / 2, H - 6, 4);
  drawThresh();
  spr.pushSprite(0, 0);
}

// ---------- PROXIMITY ----------
void drawProximity(bool have, int rssi, uint8_t cat, int count) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("PROXIMITY", W / 2, 4, 4);

  if (!have) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString("no devices", W / 2, H / 2, 4);
    drawThresh();
    spr.pushSprite(0, 0);
    return;
  }

  int pct = (rssi + 95) * 100 / 50;
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  uint16_t col; const char* word;
  if (pct > 66)      { col = tft.color565(235, 80, 80);  word = "CLOSE"; }
  else if (pct > 33) { col = tft.color565(240, 185, 60); word = "NEAR";  }
  else               { col = tft.color565(70, 200, 130); word = "FAR";   }

  int bw = W - 60, bh = 26, bx = 30, by = H / 2 - 14;
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(col, TFT_BLACK);
  spr.drawString(word, W / 2, by - 6, 4);
  spr.drawRoundRect(bx, by, bw, bh, 4, tft.color565(70, 70, 80));
  int fillw = (bw - 4) * pct / 100;
  if (fillw > 0) spr.fillRoundRect(bx + 2, by + 2, fillw, bh - 4, 3, col);

  char buf[48];
  float d = estDistance(rssi);
  if (d < 10.0f) sprintf(buf, "~%.1f m  %d dBm", d, rssi);
  else           sprintf(buf, "~%.0f m  %d dBm", d, rssi);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W / 2, by + bh + 8, 4);

  sprintf(buf, "%s   (%d near)", vendorName(cat), count);
  spr.setTextColor(vendorColor(cat), TFT_BLACK);
  spr.drawString(buf, W / 2, by + bh + 34, 2);
  drawThresh();
  spr.pushSprite(0, 0);
}

// ---------- SCOPE ----------
void drawScope(const std::vector<Blip>& bl) {
  int W = spr.width(), H = spr.height();
  int cx = W / 2 + 26, cy = H / 2;     // радар сдвинут вправо, слева место под легенду
  int maxR = (H / 2) - 8;
  spr.fillSprite(TFT_BLACK);
  uint16_t ring = tft.color565(0, 70, 32);
  for (int k = 1; k <= 3; k++) spr.drawCircle(cx, cy, maxR * k / 3, ring);
  spr.drawLine(cx - maxR, cy, cx + maxR, cy, ring);
  spr.drawLine(cx, cy - maxR, cx, cy + maxR, ring);

  for (int t = 0; t < 40; t++) {
    float a = (g_sweep - t) * PI / 180.0f;
    int g = 140 - t * 3; if (g < 0) g = 0;
    spr.drawLine(cx, cy, cx + (int)(cos(a) * maxR), cy + (int)(sin(a) * maxR),
                 tft.color565(0, g, g / 2));
  }

  // блипы — только цветные точки, без букв
  for (size_t i = 0; i < bl.size(); i++) {
    float ang = (bl[i].h % 360) * PI / 180.0f;
    int pct = (bl[i].rssi + 95) * 100 / 50;
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    int r = maxR - (maxR - 8) * pct / 100;
    int x = cx + (int)(cos(ang) * r), y = cy + (int)(sin(ang) * r);
    spr.fillCircle(x, y, 3, vendorColor(bl[i].cat));
  }

  // компактная легенда слева (точка + короткая метка)
  const uint8_t order[5] = { V_APPLE, V_MS, V_SAMSUNG, V_GOOGLE, V_OTHER };
  const char*   lbl[5]   = { "Apple", "MS", "Sams", "Googl", "other" };
  spr.setTextDatum(ML_DATUM);
  for (int i = 0; i < 5; i++) {
    int y = 20 + i * 27;
    spr.fillCircle(7, y, 3, vendorColor(order[i]));
    spr.setTextColor(vendorColor(order[i]), TFT_BLACK);
    spr.drawString(lbl[i], 14, y, 1);
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(0, 170, 80), TFT_BLACK);
  spr.drawString("SCOPE", cx, 2, 2);
  char buf[16]; sprintf(buf, "%d", (int)bl.size());
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(tft.color565(90, 255, 150), TFT_BLACK);
  spr.drawString(buf, W - 4, 2, 4);

  drawThresh();
  spr.pushSprite(0, 0);
  g_sweep = (g_sweep + 6) % 360;
}

// ---------- сэмплер истории (раз в секунду) + детектор событий ±7 ----------
void sampleTick() {
  uint32_t now = millis();
  if (now - g_lastSampleMs < 1000) return;
  g_lastSampleMs = now;
  int c = g_lastCount;

  if (g_base < 0) g_base = c;                     // инициализация
  int8_t ev = 0;
  float delta = c - g_base;
  if (now >= g_cooldownMs) {
    if (delta >= g_thresh)       { ev = +1; g_base = c; g_cooldownMs = now + 3000; }
    else if (delta <= -g_thresh) { ev = -1; g_base = c; g_cooldownMs = now + 3000; }
  }
  if (ev != 0) { g_ledEvent = ev; g_ledStart = now; }   // запустить анимацию ленты
  g_base += 0.15f * (c - g_base);                 // дрейф базовой линии (EMA)

  Sample s = { c, g_base, ev };
  if (g_histLen < HIST_N) g_hist[g_histLen++] = s;
  else { memmove(&g_hist[0], &g_hist[1], sizeof(Sample) * (HIST_N - 1)); g_hist[HIST_N - 1] = s; }
}

// ---------- TIMELINE: число устройств во времени ----------
void drawTimeline() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);

  int plotL = 6, plotR = W - 6, plotT = 22, plotB = H - 22;
  int plotW = plotR - plotL, plotH = plotB - plotT;

  int peak = 1, nowv = 0;
  for (int i = 0; i < g_histLen; i++) if (g_hist[i].count > peak) peak = g_hist[i].count;
  if (g_histLen > 0) nowv = g_hist[g_histLen - 1].count;
  int ymax = peak + peak / 6 + 1;                 // запас сверху

  spr.drawRect(plotL, plotT, plotW, plotH, tft.color565(40, 40, 48));
  float step = (HIST_N > 1) ? (float)plotW / (HIST_N - 1) : plotW;

  // площадь + линия
  int prevx = 0, prevy = 0;
  for (int i = 0; i < g_histLen; i++) {
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    int y = plotB - (int)((long)g_hist[i].count * plotH / ymax);
    spr.drawFastVLine(x, y, plotB - y, tft.color565(15, 55, 35));
    if (i > 0) spr.drawLine(prevx, prevy, x, y, tft.color565(70, 220, 130));
    prevx = x; prevy = y;
  }

  // базовая линия (сглаженная) — тускло
  for (int i = 1; i < g_histLen; i++) {
    int x0 = plotR - (int)((g_histLen - 1 - (i - 1)) * step);
    int x1 = plotR - (int)((g_histLen - 1 - i) * step);
    int y0 = plotB - (int)(g_hist[i - 1].base * plotH / ymax);
    int y1 = plotB - (int)(g_hist[i].base * plotH / ymax);
    spr.drawLine(x0, y0, x1, y1, tft.color565(95, 95, 45));
  }

  // метки событий: ▲ пришли (зел), ▼ ушли (оранж)
  for (int i = 0; i < g_histLen; i++) {
    if (g_hist[i].ev == 0) continue;
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    if (g_hist[i].ev > 0)
      spr.fillTriangle(x, plotT + 2, x - 4, plotT + 10, x + 4, plotT + 10, tft.color565(70, 220, 130));
    else
      spr.fillTriangle(x - 4, plotT + 2, x + 4, plotT + 2, x, plotT + 10, tft.color565(240, 150, 50));
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("TIMELINE", W / 2, 4, 2);

  char buf[48];
  sprintf(buf, "now %d   peak %d", nowv, peak);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(tft.color565(70, 220, 130), TFT_BLACK);
  spr.drawString(buf, W / 2, H - 4, 2);

  drawThresh();
  spr.pushSprite(0, 0);
}

// ---------- анимация светодиодной ленты ----------
void updateLeds() {
  if (g_ledEvent == 0) return;
  uint32_t t = millis() - g_ledStart;
  if (t >= LED_DUR) { strip.clear(); strip.show(); g_ledEvent = 0; return; }
  float p = (float)t / LED_DUR;                 // 0..1
  strip.clear();
  if (g_ledEvent > 0) {
    // зелёная волна снизу вверх с хвостом
    int head = (int)(p * (WS2812_N + 4));
    for (int i = 0; i < WS2812_N; i++) {
      int d = head - i;
      if (d >= 0 && d < 4) {
        int b = 220 - d * 60; if (b < 0) b = 0;
        strip.setPixelColor(i, strip.Color(0, b, b / 3));
      }
    }
  } else {
    // оранжевая вспышка с общим спадом
    int b = (int)(220 * (1.0f - p));
    for (int i = 0; i < WS2812_N; i++)
      strip.setPixelColor(i, strip.Color(b, b * 2 / 5, 0));
  }
  strip.show();
}

// ---------- кнопки ----------
int g_lastBtn = HIGH; uint32_t g_lastBtnMs = 0;
void pollButtons() {
  int b = (digitalRead(PIN_ENC_KEY) == LOW || digitalRead(PIN_USER_KEY) == LOW) ? LOW : HIGH;
  uint32_t now = millis();
  if (b == LOW && g_lastBtn == HIGH && now - g_lastBtnMs > 250) {
    g_mode = (g_mode + 1) % 4;
    g_lastBtnMs = now;
  }
  g_lastBtn = b;
}

// ---------- энкодер: прокрутка листает режимы ----------
void pollEncoder() {
  static uint8_t prev = 3;       // исходное 0b11 (оба HIGH)
  static int8_t  accum = 0;
  static uint32_t lastStepMs = 0;
  static const int8_t tbl[16] = { 0,-1, 1, 0,  1, 0, 0,-1, -1, 0, 0, 1,  0, 1,-1, 0 };
  uint8_t a = digitalRead(PIN_ENC_A);
  uint8_t b = digitalRead(PIN_ENC_B);
  uint8_t cur = (a << 1) | b;
  if (cur == prev) return;
  accum += tbl[(prev << 2) | cur];
  prev = cur;
  if (accum >= 4 || accum <= -4) {
    uint32_t now = millis();
    if (now - lastStepMs > 120) {               // мягкая защита от дребезга
      if (accum >= 4) g_mode = (g_mode + 1) % 4;
      else            g_mode = (g_mode + 3) % 4;
      lastStepMs = now;
    }
    accum = 0;
  }
}

void setup() {
  pinMode(BOARD_PWR_EN, OUTPUT); digitalWrite(BOARD_PWR_EN, HIGH);
  pinMode(PIN_TFT_CS,  OUTPUT);  digitalWrite(PIN_TFT_CS,  HIGH);
  pinMode(PIN_SD_CS,   OUTPUT);  digitalWrite(PIN_SD_CS,   HIGH);
  pinMode(PIN_LORA_CS, OUTPUT);  digitalWrite(PIN_LORA_CS, HIGH);
  pinMode(PIN_NRF_CS,  OUTPUT);  digitalWrite(PIN_NRF_CS,  HIGH);
  pinMode(PIN_ENC_KEY,  INPUT_PULLUP);
  pinMode(PIN_ENC_A,    INPUT_PULLUP);
  pinMode(PIN_ENC_B,    INPUT_PULLUP);
  pinMode(PIN_USER_KEY, INPUT_PULLUP);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);

  strip.begin();
  strip.setBrightness(120);
  strip.clear();
  strip.show();

  spr.setColorDepth(16);
  if (!spr.createSprite(tft.width(), tft.height())) {
    spr.setColorDepth(8);
    spr.createSprite(tft.width(), tft.height());
  }

  g_lock = xSemaphoreCreateMutex();
  NimBLEDevice::init("");
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(new ScanCB(), true);
  s->setActiveScan(true);
  s->setInterval(100);
  s->setWindow(99);
  s->setMaxResults(0);
  s->start(0, false);

  drawRadar(0);
}

uint32_t g_lastDraw = 0;

void loop() {
  pollButtons();
  pollEncoder();
  sampleTick();
  updateLeds();
  uint32_t now = millis();
  if (now - g_lastDraw >= REDRAW_MS) {
    g_lastDraw = now;
    int count = 0; bool have = false; int bestRssi = -127; uint8_t bestCat = V_PRIV;
    std::vector<Blip> snap;
    if (xSemaphoreTake(g_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
      for (auto it = g_seen.begin(); it != g_seen.end(); ) {
        if (now - it->second.t > REMEMBER_MS) {
          it = g_seen.erase(it);
        } else {
          if (it->second.rssi > bestRssi) { bestRssi = it->second.rssi; bestCat = it->second.cat; have = true; }
          if (g_mode == 2) {
            uint32_t h = (uint32_t)std::hash<std::string>{}(it->first);
            snap.push_back({ it->second.rssi, h, it->second.cat });
          }
          ++it;
        }
      }
      count = (int)g_seen.size();
      xSemaphoreGive(g_lock);
    }
    g_lastCount = count;
    if (g_mode == 0)      drawRadar(count);
    else if (g_mode == 1) drawProximity(have, bestRssi, bestCat, count);
    else if (g_mode == 2) drawScope(snap);
    else                  drawTimeline();
  }
  delay(10);
}
