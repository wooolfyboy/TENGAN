// ============================================================
//  settings.h — настройки (хранятся в памяти платы, NVS)
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

// Дальность Bluetooth: устройство засчитывается, когда его сглаженный сигнал >= порога,
// и выбывает, только когда упал ниже порога ещё на RANGE_HYST_DB.
struct RangeOpt { int dbm; const char* name; };
static const RangeOpt RANGES[] = {
  { -127, "all" }, { -90, "far" }, { -80, "room" }, { -70, "near" }, { -60, "very near" },
};
static const int N_RANGES = sizeof(RANGES) / sizeof(RANGES[0]);
static const int RANGE_HYST_DB = 5;
static const uint32_t MIN_PKTS = 2;        // одиночный случайный пакет не считается

extern int g_range;                        // индекс в RANGES
extern int g_thresh;                       // порог событий для светодиодов (0 = выкл)
extern int g_ledBright;                    // % яркости ленты
extern int g_scrBright;                    // % яркости экрана

void settingsBegin();                      // загрузить и применить
void applyBrightness();
void settingsAutosave(uint32_t now);       // сохранить через 3 с после изменения
void settingsSaveNow();

extern const Section settingsSection;
