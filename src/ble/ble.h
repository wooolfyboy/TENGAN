// ============================================================
//  ble.h — раздел Bluetooth (снаружи раздела нужен только этот файл)
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

void bleBegin();                 // NimBLE + постоянное сканирование
void bleUpdate(uint32_t now);    // каждый кадр: свести адреса в устройства (нужно и меню, и ленте)
void bleSample(uint32_t now);    // раз в секунду: история TIMELINE и события для ленты
void bleRadioOn();               // Wi-Fi и Bluetooth делят антенну —
void bleRadioOff();              // работает что-то одно
bool bleRadioActive();           // сканирует ли сейчас (для строки состояния планшета)

void bleJson(String& out);       // устройства и термометры для веб-страницы
void bleDevJson(uint32_t id, String& out);   // подробности одного устройства
void bleStreamJson(String& out); // компактно — для планшета (поток SSE)
void bleLowDuty(bool low);       // веб-режим: слушать реже, делить эфир с Wi-Fi

extern const Section bleSection;
