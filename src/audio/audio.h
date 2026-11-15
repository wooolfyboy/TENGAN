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

// веб-страница (раздел Web)
bool audioWebUse();                  // включить микрофон (если разрешён в настройках)
void audioWebTick(uint32_t now);
void audioWebStop();
void audioJson(String& out);
void audioStreamJson(String& out);   // компактный кадр для планшета (поток SSE)

// звук с ноутбука (строки «PA ...» по USB, разбирает pc.cpp)
void audioPcFrame(float levelDb, int domHz, const uint8_t* bands32, const int8_t* wave64,
                  const int8_t* xy64x2, bool stereo);
bool audioPcLive(uint32_t now);      // кадры с ноута идут (< 0.6 с назад)
