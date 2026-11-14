// ============================================================
//  wifi.h — раздел Wi-Fi (только приём)
// ============================================================
#pragma once
#include "core/section.h"

void wifiRadioOff();             // выключить Wi-Fi (перед включением Bluetooth)

extern const Section wifiSection;
