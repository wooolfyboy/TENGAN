// ============================================================
//  battery.h — заряд батареи (BQ27220 + BQ25896, только чтение)
// ============================================================
#pragma once
#include <Arduino.h>

struct Bat {
  bool     ok = false;      // датчик заряда ответил
  bool     chgOk = false;   // контроллер зарядки ответил
  int      soc = 0;         // %
  int      mv = 0;          // мВ
  int      ma = 0;          // мА: + заряд, − разряд
  float    tempC = 0;
  int      tte = -1;        // минут до разряда (-1 = неизвестно)
  int      ttf = -1;        // минут до полного заряда
  int      remain = 0, full = 0;   // мА·ч
  uint8_t  bus = 0;         // источник питания (0 = нет USB)
  uint8_t  chg = 0;         // 0 не заряжается, 1 предзаряд, 2 заряд, 3 заряжено
  uint32_t t = 0;
};
extern Bat g_bat;

void batBegin();
void batTick(uint32_t now);                  // раз в 2 с перечитывает датчики
bool batUsb();                               // подключён USB
bool batCharging();                          // идёт зарядка
uint16_t batColor();
void drawBatteryIcon(int x, int y);          // (x, y) — правый верхний угол
void batSummary(char* out, size_t n);        // одна строка о питании
