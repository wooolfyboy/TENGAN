// ============================================================
//  subghz.h — Sub-GHz section (CC1101, receive only)
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

extern const Section subghzSection;

// web page (Web section)
bool subWebUse(int mode, int band);     // enable receive in the given mode/band
void subWebTick(uint32_t now);
void subWebStop();
void subJson(String& out);
