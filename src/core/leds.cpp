// ============================================================
//  leds.cpp — светодиодная лента
// ============================================================
#include "core/leds.h"
#include "core/board.h"

Adafruit_NeoPixel strip(WS2812_N, PIN_WS2812, NEO_GRB + NEO_KHZ800);
int      g_ledEvent = 0;
uint32_t g_ledStart = 0;
static const uint32_t LED_DUR = 1200;

void ledsBegin() { strip.begin(); strip.clear(); strip.show(); }
void ledsOff()   { strip.clear(); strip.show(); }
void ledsEvent(int dir) { g_ledEvent = dir; g_ledStart = millis(); }

// ---------- анимация светодиодной ленты ----------
void ledsTick() {
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
