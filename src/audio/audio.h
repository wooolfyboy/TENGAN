// ============================================================
//  audio.h — звук: микрофон (раздел Sound) и динамик (звуки интерфейса)
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

void audioBegin();                   // динамик (звуки интерфейса)
void audioClick();                   // щелчок колеса
void audioBeep(uint16_t hz, uint16_t ms);
void audioGeiger(int pct);           // «счётчик Гейгера»: чем ближе (pct 0..100), тем чаще щелчки

extern const Section soundSection;
