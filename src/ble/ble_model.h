// ============================================================
//  ble_model.h — данные раздела Bluetooth, общие для логики
//  (ble_core.cpp) и экранов (ble_screens.cpp). Снаружи раздела
//  используется только ble/ble.h.
// ============================================================
#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include "ble/devtypes.h"
#include "ble/sensors.h"

static const uint32_t REMEMBER_MS = 30000;          // сколько помним молчащий адрес
static const uint32_t SENSOR_REMEMBER_MS = 300000;  // датчики шлют редко — помним 5 минут
static const size_t   MAX_SENSORS = 24;
static const size_t   MAX_DEVS    = 600;            // предохранитель памяти
// склейка смены адреса: старый адрес должен замолчать хотя бы на STALE_MIN
// и не дольше STALE_MAX назад, а сила сигнала — быть похожей
static const uint32_t STALE_MIN   = 1500;
static const uint32_t STALE_MAX   = 12000;
static const int      MERGE_DB    = 10;
// список: «липкий» порядок
static const int      ORDER_HYST_DB = 8;            // меняем местами, только если разница > 8 dB
static const uint32_t ORDER_EVERY   = 2000;         // как часто обновлять порядок
static const uint32_t ORDER_FREEZE  = 4000;         // после касания колеса порядок замирает

enum { M_OVERVIEW = 0, M_SCOPE, M_CITY, M_NEAREST, M_LIST, M_WEATHER, NUM_MODES };
enum { V_PRIV = 0, V_OTHER = 1 };                   // 2.. — бренды из таблицы BRANDS (vendors.h)
enum { A_PUBLIC = 0, A_STATIC, A_ROTATING, A_NONRES };   // виды адресов

// Dev — один MAC-адрес. ent — номер «физического» устройства:
// у нескольких адресов одного телефона (после смены MAC) он общий.
struct Dev {
  uint32_t t;        // когда последний раз слышали
  uint32_t first;    // когда впервые услышали (с учётом склейки)
  uint32_t ent;      // id физического устройства
  uint32_t sig;      // отпечаток рекламы
  uint32_t pkts;     // сколько пакетов поймали
  int8_t   rssi;     // последний сигнал
  int8_t   rssiS;    // сглаженный сигнал (для сортировки)
  int8_t   rmin, rmax;
  uint8_t  cat;      // бренд (V_* / индекс в BRANDS)
  uint8_t  vkind;    // откуда узнали производителя: 0 нет, 1 Company ID, 2 UUID сервиса
  uint8_t  vscore;   // насколько надёжно (3 бренд, 2 прочий, 1 производитель чипа)
  uint16_t vcode;    // сам Company ID или UUID
  uint8_t  type;     // тип устройства (DevType)
  uint8_t  akind;    // вид адреса (A_*)
  bool     rnd;      // адрес случайный и меняющийся
  bool     merged;   // уже стал «предшественником» нового адреса
  bool     inRange;  // ближе порога дальности (с гистерезисом)
  bool     hasCid;
  uint16_t cid;      // Company ID
  uint16_t appearance;
  uint8_t  mac[6];   // адрес (младший байт первым)
  uint8_t  mfr[6];   // первые байты данных производителя
  uint8_t  mfrLen;   // их полная длина
  uint16_t uuids[4]; // сервисы
  uint8_t  nU;
  char     name[20];
};
// Ent — физическое устройство (агрегат по его адресам)
struct Ent {
  uint32_t t, first, pkts;
  int      rssi, rssiS;
  uint8_t  cat, type, nMacs;
  uint8_t  vkind, vscore;
  uint16_t vcode;
  uint64_t key;      // ключ самого свежего адреса
  char     name[20];
  char     label[21]; // подпись, которую дал пользователь с планшета (UTF-8, может быть пустой)
};
// датчик погоды
struct Sensor {
  uint32_t t;
  int8_t   rssi;
  SensorReading r;
  uint8_t  mac[6];
  char     name[20];
};
struct Blip { int rssi; uint32_t h; uint8_t cat; uint8_t type; char label[21]; };   // точка на SCOPE / CITY (label — подпись пользователя)

// --- история для TIMELINE ---
#define HIST_N 150
struct Sample { int count; float base; int8_t ev; };
extern Sample g_hist[HIST_N];
extern int    g_histLen;

// --- состояние раздела ---
extern volatile int g_mode;            // экран раздела (M_*)
extern bool  g_bleActive;              // сканирует ли Bluetooth сейчас
extern int   g_lastCount;              // физических устройств (с учётом дальности)
extern int   g_lastRaw;                // MAC-адресов (без склейки)
extern int   g_sensCount;              // датчиков погоды

// --- список и подробности ---
extern std::vector<uint32_t> g_order;  // порядок строк (id устройств)
extern uint32_t g_sel;                 // выбранное устройство
extern int      g_top;                 // первая видимая строка
extern bool     g_detail;              // открыты подробности
#define SPARK_N 60
extern int8_t   g_spark[SPARK_N];      // история сигнала выбранного (точка раз в 0.5 с)
extern int      g_sparkN;

// ---------- экраны (ble_screens.cpp) ----------
float       estDistance(int rssi);
void drawOverview(int count, int raw, const int* typeCnt);
void drawScope(const std::vector<Blip>& bl);
void drawCity(const std::vector<Blip>& bl, uint32_t now);   // 3D: устройства — башни
void cityTurn(int dir);                                      // колесо: повернуть камеру
void drawNearest(bool have, int rssi, const Ent& best, int count);
void drawList(const std::map<uint32_t, Ent>& ents);
void drawDetail(const Dev& d, const Ent& e, bool inMap, uint32_t now);
void drawWeather(const std::vector<Sensor>& ss, uint32_t now);
