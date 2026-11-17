// ============================================================
//  boot.h — power-on animation
// ============================================================
#pragma once
#include <Arduino.h>

void bootScreen();               // play the whole splash (~2.7 s)
void bootFrame(uint32_t t);      // draw the frame at t ms (into the sprite, no push)
