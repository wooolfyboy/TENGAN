// ============================================================
//  wifi.h — Wi-Fi section (receive-only)
// ============================================================
#pragma once
#include "core/section.h"

void wifiRadioOff();             // turn Wi-Fi off (before enabling Bluetooth)

extern const Section wifiSection;
