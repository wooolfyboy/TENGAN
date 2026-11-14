// ============================================================
//  subghz.h — раздел Sub-GHz (CC1101, только приём)
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

extern const Section subghzSection;

// веб-страница (раздел Web)
bool subWebUse(int mode, int band);     // включить приём в нужном режиме/полосе
void subWebTick(uint32_t now);
void subWebStop();
void subJson(String& out);
