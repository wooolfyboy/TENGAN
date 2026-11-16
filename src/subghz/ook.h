// ============================================================
//  ook.h — разбор посылок 433 МГц (OOK): анализатор импульсов и
//  декодеры самых частых уличных датчиков. Только приём и разбор.
//
//  Код не зависит от железа: его же собирают симулятор и тесты на
//  компьютере (tools/ook_test). Протоколы описаны по открытой
//  документации проекта rtl_433 (форматы, контрольные суммы), код свой.
//
//  Посылка (OokTrain) — чередование «есть несущая» (импульс) и «тишина»
//  (пауза) в микросекундах, как их выдаёт CC1101 на выводе GDO0.
// ============================================================
#pragma once
#include <stdint.h>
#include <string.h>

#define OOK_MAXP 1024                      // импульсов в одной посылке (TX141TH: 12 повторов ≈ 530)

struct OokTrain {
  uint16_t p[OOK_MAXP];                    // импульс, мкс
  uint16_t g[OOK_MAXP];                    // пауза после импульса, мкс (у последнего — длинная)
  int n = 0;
};

enum : uint8_t { OOK_TH = 0, OOK_T, OOK_REMOTE };   // что за устройство

struct OokMsg {
  const char* proto = "?";                 // «Nexus», «LaCrosse-TX141TH», «Remote» ...
  uint32_t id = 0;                         // номер датчика (или хеш пульта)
  int8_t   ch = 0;                         // канал 1..3 (0 — нет)
  uint8_t  kind = OOK_TH;
  float    t = 0;                          // °C
  int16_t  h = -1;                         // %, -1 — нет
  int8_t   bat = -1;                       // 1 — ок, 0 — разряжена, -1 — неизвестно
  uint8_t  btn = 0;                        // кнопка пульта (номер, не код)
  uint8_t  row[12] = {};                   // байты посылки (только датчики — для анализатора)
  uint8_t  rowBits = 0;
};

struct OokInfo {
  int pulses = 0, bits = 0, rows = 0;
  const char* mod = "?";                   // PWM / PPM / Manchester / ?
  uint16_t shortW = 0, longW = 0;          // длительности импульсов, мкс
  uint16_t shortG = 0, longG = 0, syncG = 0;   // паузы, мкс
  const char* cls = "noise";               // sensor / remote / rolling / unknown / noise
  const char* proto = "?";
  char hex[25] = "";                       // байты — ТОЛЬКО у датчиков (у пультов кода не показываем)
  uint16_t hash = 0;                       // отпечаток посылки (вместо кода)
};

// найти датчики/пульты в посылке; возвращает сколько записано в out
int  ookDecode(const OokTrain& t, OokMsg* out, int maxOut);
// разобрать форму посылки (всегда); dec — то, что нашёл ookDecode
void ookAnalyze(const OokTrain& t, OokInfo& info, const OokMsg* dec, int nDec);
