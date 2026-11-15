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

enum { THEME_TENGAN = 0, THEME_SUMI = 1 };
extern int g_theme;                        // оформление (THEME_*), меняется в General
extern int g_range;                        // индекс в RANGES
extern int g_thresh;                       // порог событий для светодиодов (0 = выкл)
extern int g_ledBright;                    // % яркости ленты
extern int g_scrBright;                    // % яркости экрана
extern int g_micEn;                        // микрофон разрешён (1) / выключен (0)
extern int g_auSrc;
extern int g_city;                         // город для Sky / Globe (индекс в CITIES, gallery/astro.h)
extern int g_tzOff;                        // смещение местного времени, с (приходит с ноутбука)                        // звук для раздела Sound: 0 авто, 1 микрофон, 2 ноутбук
extern int g_auLeds;                       // Sound: светодиоды показывают громкость (по умолчанию выкл)
extern int g_clickEn;                      // щелчки колеса
extern int g_bleTicks;                     // щелчки «ближе-дальше» в Bluetooth
extern int g_wifiTicks;                    // щелчки «ближе-дальше» в Wi-Fi Find
extern int g_wRange;                       // Wi-Fi: скрывать сети слабее (индекс в RANGES)
extern int g_sndIdx;                       // звуки: 0 off, 1 quiet, 2 loud
extern int g_dimIdx;                       // авто-приглушение: индекс в DIM_SECS (0 — выкл)
static const uint16_t DIM_SECS[] = { 0, 30, 60, 300 };
static const int N_DIMS = sizeof(DIM_SECS) / sizeof(DIM_SECS[0]);

void settingsBegin();                      // загрузить и применить
void applyBrightness();
void settingsAutosave(uint32_t now);       // сохранить через 3 с после изменения
void settingsSaveNow();
void settingsTouch();                      // отметить: настройки изменились (сохранить позже)

extern const Section settingsSection;
