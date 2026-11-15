// ============================================================
//  pc.h — раздел PC: монитор нагрузки ноутбука.
//  На ноуте работает tools/pc_monitor/pc_monitor.py: раз в секунду
//  шлёт по USB строку вида
//    PC cpu=23.4 ram=61.2 ramu=9.8 ramt=16.0 c=12,40,5 ... host=LAPTOP
//  Палка разбирает её и рисует показатели.
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

#define PC_CORES 16
#define PC_HIST  60                   // история за минуту (точка в секунду)

struct PcStat {
  bool     have = false;              // приходили ли данные вообще
  uint32_t t = 0;                     // когда пришли последние (millis)
  float    cpu = 0, freq = 0;         // %, ГГц
  int      nCores = 0;
  uint8_t  core[PC_CORES] = {};       // % по ядрам
  float    ram = 0, ramU = 0, ramT = 0;   // %, ГБ занято, ГБ всего
  float    disk = 0, dR = 0, dW = 0;  // % заполнен C:, МБ/с чтение/запись
  float    nD = 0, nU = 0;            // КБ/с скачивание / отдача
  int      bat = -1; bool plug = false;   // батарея ноута (-1 — нет)
  uint32_t up = 0;                    // время работы ноута, с
  char     top[24] = "", host[24] = "";
  int      vol = -1; bool mute = false;  // громкость ноута (если программа смогла её узнать)
  uint32_t volT = 0;                  // когда пришла громкость
  // что играет (Windows: системная панель «Сейчас играет» — Spotify, YouTube в браузере и т.д.)
  bool     trk = false;               // есть ли трек
  bool     playing = false;
  int      pos = -1, dur = -1;        // секунды (-1 — неизвестно)
  uint32_t trkT = 0;                  // когда пришло (позицию между обновлениями досчитываем сами)
  char     title[96] = "", artist[72] = "", app[24] = "";   // UTF-8 (русские буквы — по 2 байта)
  float    cpuH[PC_HIST] = {}, ramH[PC_HIST] = {};
  int      histN = 0;
};
extern PcStat g_pc;
extern int    g_pcPage;               // 0 — обзор, 1 — ядра, 2 — город процессов, 3 — музыка (колесо = громкость)
#define PC_PAGES 4
// пульт: что и когда палка последний раз отправила ноуту (для анимации экрана)
extern int      g_pcLastCmd;          // +1 громче, -1 тише, 2 пауза/играть, 3 без звука
extern uint32_t g_pcLastCmdMs;

void pcPoll(uint32_t now);            // каждый проход loop(): читать USB
void pcSendLine(const char* s);       // отправить строку программе на ноуте (если она запущена)
bool pcLive(uint32_t now);            // данные свежие (< 3 с)

void drawPc(uint32_t now);
void drawPcCores(uint32_t now);
void drawPcWaiting(uint32_t now);
void drawPcMedia(uint32_t now);
void drawPcCity(uint32_t now);       // город процессов (pc_city.cpp)
void pcCityLine(char* s);            // строка «PP ...» с ноутбука
void pcCityNext();                   // нажатие: прожектор на следующую башню
void pcCityJson(String& o);          // башни для планшета (дописывает в o)
bool pcCityLive(uint32_t now);
void pcStreamJson(String& o);        // всё про ноутбук для планшета (поток SSE)

extern const Section pcSection;
