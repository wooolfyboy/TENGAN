// ============================================================
//  sensors.h — датчики 433 МГц: таблица услышанного, анализатор
//  последней посылки, «температура за окном» для Радиопогоды.
//
//  Работает, пока CC1101 в режиме Monitor или Sensors (только приём):
//  CC1101 отдаёт демодулированный OOK на GDO0, прерывание меряет
//  длительности (sens_rx.cpp), ook.cpp разбирает посылки.
//  Ничего не сохраняется — всё в оперативной памяти.
// ============================================================
#pragma once
#include <Arduino.h>
#include "subghz/ook.h"

#define SENS_MAX 16

struct SensRec {
  const char* proto;                   // «Nexus», «LaCrosse-TX141TH», «Remote» ...
  uint32_t id;
  int8_t   ch;
  uint8_t  kind;                       // OOK_TH / OOK_T / OOK_REMOTE
  float    t;                          // °C
  int16_t  h;                          // %, -1 — нет
  int8_t   bat;                        // 1 ок, 0 разряжена, -1 неизвестно
  uint8_t  btn;                        // кнопка пульта (номер)
  int8_t   rssi;                       // dBm последней посылки
  uint32_t lastMs, firstMs;
  uint16_t n;                          // сколько раз слышали
};
extern SensRec  g_sens[SENS_MAX];
extern int      g_sensN;

// последняя посылка (анализатор). pt — импульсы/паузы для рисунка, мкс:
// у датчиков до 48 пар, у пультов и неизвестного — только начало (12 пар): код не восстановить.
#define SENS_PT 48
struct SensLast {
  bool     valid = false;
  OokInfo  info;
  int8_t   rssi = -120;
  uint32_t ms = 0;
  uint16_t pt[SENS_PT * 2];
  uint8_t  ptN = 0;                    // пар
  uint32_t durUs = 0;                  // длительность всей посылки
};
extern SensLast g_sensLast;
extern uint32_t g_sensGen;             // растёт на каждой новой посылке (для потока планшета)
extern uint32_t g_sensPackets;         // посылок разобрано всего
extern uint32_t g_sensNoise;           // отброшено как шум

void sensFeed(const OokTrain& t, int rssi, uint32_t now);           // разобрать посылку
bool sensOutside(float& t, int& h, const char*& src, uint32_t now); // самый сильный термодатчик за 10 мин
int  sensSorted(uint8_t* idx, int max, uint32_t now);               // порядок для экрана: термодатчики по силе, потом пульты
void sensId(const SensRec& s, char* out, size_t n);                 // «nexus-1-0x5a», «remote-a3f1»
void sensJson(String& o, uint32_t now);                             // событие sens для планшета
void sensClear();

// приём (sens_rx.cpp; в симуляторе — заглушки)
void sensRxStart();                    // GDO0 = данные, прерывание включено
void sensRxStop();
void sensRxTick(uint32_t now, int rssi);
bool sensRxOn();
