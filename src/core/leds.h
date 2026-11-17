// ============================================================
//  leds.h — LED strip (8 x WS2812)
// ============================================================
#pragma once
#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel strip;
extern int      g_ledEvent;        // 0 none, +1 arrived, -1 left (Bluetooth event animation)
extern uint32_t g_ledStart;

void ledsBegin();
void ledsEvent(int dir);           // start the event animation
void ledsTick();                   // animation step (every loop pass)
void ledsOff();
