// ============================================================
//  ir.h — раздел IR: «чей это пульт?»
//
//  Только приём. ИК-приёмник платы (GPIO1) читается периферией RMT:
//  она сама меряет длительности вспышек и пауз, а мы угадываем протокол
//  (NEC, Samsung, Sony, RC5, RC6, LG, JVC, Panasonic/Kaseikyo) и, по
//  адресу, вероятную марку. Неизвестное показываем как «сырую» пачку.
//  ИК-светодиод (GPIO2) не трогаем — палка ничего не передаёт.
//
//  ir_decode.cpp — разбор (чистый код, собирается и в симуляторе),
//  ir.cpp        — железо (RMT), раздел меню, JSON для планшета,
//  ir_screens.cpp — экран.
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

#define IR_RAW_MAX  128               // длительностей в последней пачке (мкс; вспышка, пауза, ...)
#define IR_HIST     8

struct IrCode {
  char     proto[12];                 // "NEC", "NEC-EXT", "SAMSUNG", "SONY12", "RC5", "RC6", "LG", "JVC", "KASEIKYO", "RAW"
  uint32_t addr, cmd;
  uint8_t  bits;
  bool     rep;                       // кнопку держат (повтор)
  uint8_t  toggle;                    // RC5/RC6: бит «новое нажатие»
  const char* brand;                  // «вероятно чей» (строка из таблицы, не nullptr)
  uint32_t t;                         // millis()
};

struct IrModel {
  bool     ok = false;                // приёмник запущен
  bool     on = false;                // слушаем сейчас
  uint32_t frames = 0;                // всего пачек
  uint32_t gen = 0;                   // растёт на каждую разобранную пачку (для планшета)
  IrCode   last = {};
  bool     have = false;
  IrCode   hist[IR_HIST];             // [0] — самая свежая (повторы не копятся)
  int      histN = 0;
  uint16_t raw[IR_RAW_MAX];           // последняя пачка: вспышка, пауза, вспышка...
  int      rawN = 0;
  uint32_t presses = 0;               // нажатий (без повторов)
};
extern IrModel g_ir;

// разобрать пачку длительностей (мкс, начиная со вспышки) и положить в модель
bool irFeed(const uint16_t* d, int n, uint32_t now);
// разбор без модели: true — что-то узнали (в т.ч. RAW)
bool irDecode(const uint16_t* d, int n, IrCode& out);
const char* irBrand(const IrCode& c);

bool irStart();                       // включить приёмник (раздел открыт или планшет смотрит)
void irStop();
void irTick(uint32_t now);
void irJson(String& o);               // последняя пачка для планшета (событие SSE "ir")

void drawIr(uint32_t now);
void iconRemote(int x, int y, uint16_t c, uint16_t bg);

extern const Section irSection;
