// ============================================================
//  leds.h — светодиодная лента (8 × WS2812)
// ============================================================
#pragma once
#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel strip;
extern int      g_ledEvent;        // 0 нет, +1 пришли, -1 ушли (анимация событий Bluetooth)
extern uint32_t g_ledStart;

void ledsBegin();
void ledsEvent(int dir);           // запустить анимацию события
void ledsTick();                   // анимация событий (каждый проход loop)
void ledsOff();
