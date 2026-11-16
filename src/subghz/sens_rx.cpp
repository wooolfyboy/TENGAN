// ============================================================
//  sens_rx.cpp — приём посылок 433 МГц для датчиков (только приём).
//
//  CC1101 в асинхронном режиме OOK выдаёт демодулированный сигнал
//  на GDO0 (GPIO 3): 1 — несущая есть, 0 — тишина. Прерывание по
//  каждому фронту пишет длительность в кольцевой буфер; основной цикл
//  собирает из них посылку (импульс + пауза) и, когда наступила
//  тишина 20 мс, отдаёт её в sensFeed (ook.cpp разбирает).
//
//  Передача не используется: GDO0 — вход, STX никогда не подаётся.
// ============================================================
#include <soc/gpio_reg.h>
#include "subghz/sensors.h"
#include "subghz/cc1101.h"
#include "core/crumb.h"

#define CC_GDO0 3                          // BOARD_LORA_IO0 (официальная распиновка LilyGO)
#define RING 2048                          // фронтов в очереди (~0.5 с плотной посылки)
#define TRAIN_END_US 20000                 // тишина дольше — посылка закончилась
#define GLITCH_US 40                       // импульсы и паузы короче — помеха, склеиваем

static volatile uint32_t s_ring[RING];     // бит 31 — уровень закончившегося отрезка, остальное — мкс
static volatile uint16_t s_w = 0, s_r = 0;
static volatile uint32_t s_lastEdge = 0;
static volatile bool     s_over = false;
static bool s_on = false;

static void IRAM_ATTR gdoIsr() {
  uint32_t t = micros();
  uint32_t lvl = (REG_READ(GPIO_IN_REG) >> CC_GDO0) & 1;   // новый уровень
  uint32_t d = t - s_lastEdge;
  s_lastEdge = t;
  uint16_t w = s_w, nw = (w + 1) & (RING - 1);
  if (nw == s_r) { s_over = true; return; }
  s_ring[w] = (d & 0x7FFFFFFF) | (lvl ? 0 : 0x80000000u);   // закончился отрезок противоположного уровня
  s_w = nw;
}

// ---------- сборка посылки ----------
static OokTrain s_train;
static bool     s_havePulse = false;        // импульс записан, ждём паузу после него
static uint16_t s_pulse = 0;
static bool     s_full = false;             // посылку оборвал переполненный буфер (не тишина)

static void cleanup(OokTrain& T) {          // склеить помехи короче GLITCH_US
  int o = 0;
  for (int i = 0; i < T.n; i++) {
    if (T.p[i] < GLITCH_US && o > 0) {       // короткий импульс — часть паузы
      uint32_t g = (uint32_t)T.g[o - 1] + T.p[i] + T.g[i];
      T.g[o - 1] = g > 65535 ? 65535 : g;
      continue;
    }
    T.p[o] = T.p[i]; T.g[o] = T.g[i];
    if (T.g[o] < GLITCH_US && i + 1 < T.n) { // короткая пауза — часть импульса
      uint32_t p = (uint32_t)T.p[o] + T.g[o] + T.p[i + 1];
      T.p[o] = p > 65535 ? 65535 : p; T.g[o] = T.g[i + 1];
      i++;
    }
    o++;
  }
  T.n = o;
}

static int s_rssi = -120;
static void finish(uint32_t now) {
  if (s_train.n >= 8) {
    cleanup(s_train);
    crumb("sens feed");
    if (s_full) g_sensNoise++;               // сплошной поток без тишины — шум эфира, не посылка
    else sensFeed(s_train, s_rssi, now);
  }
  s_train.n = 0; s_havePulse = false; s_full = false;
}

static void segment(bool high, uint32_t us, uint32_t now) {
  uint16_t d = us > 65535 ? 65535 : (uint16_t)us;
  if (high) { s_pulse = d; s_havePulse = true; return; }
  if (!s_havePulse) return;                  // тишина до первого импульса
  s_havePulse = false;
  if (s_train.n < OOK_MAXP) { s_train.p[s_train.n] = s_pulse; s_train.g[s_train.n] = d; s_train.n++; }
  if (us > TRAIN_END_US) finish(now);
  else if (s_train.n >= OOK_MAXP) { s_full = true; finish(now); }
}

void sensRxStart() {
  if (s_on) return;
  pinMode(CC_GDO0, INPUT);
  ccWrite(0x02, 0x0D);                       // IOCFG0: GDO0 = асинхронные данные (демодулированный OOK)
  ccWrite(0x08, 0x32);                       // PKTCTRL0: асинхронный последовательный режим, без пакетов
  ccWrite(0x12, 0x30);                       // MDMCFG2: ASK/OOK, без синхрослова
  ccWrite(0x1B, 0x07); ccWrite(0x1C, 0x00); ccWrite(0x1D, 0x91);   // AGC под OOK (порог 42 дБ, без заморозки)
  s_train.n = 0; s_havePulse = false; s_full = false;
  s_r = s_w; s_over = false;
  s_lastEdge = micros();
  attachInterrupt(digitalPinToInterrupt(CC_GDO0), gdoIsr, CHANGE);
  s_on = true;
}

void sensRxStop() {
  if (!s_on) return;
  detachInterrupt(digitalPinToInterrupt(CC_GDO0));
  ccWrite(0x02, 0x2E);                       // GDO0 снова в высоком импедансе
  s_on = false;
  s_train.n = 0; s_havePulse = false;
}

bool sensRxOn() { return s_on; }

void sensRxTick(uint32_t now, int rssi) {
  if (!s_on) return;
  if (rssi > s_rssi || s_train.n == 0) s_rssi = rssi;          // пик за посылку
  if (s_over) { s_over = false; s_r = s_w; s_train.n = 0; s_havePulse = false; g_sensNoise++; return; }
  int guard = RING;
  while (s_r != s_w && guard--) {
    uint32_t v = s_ring[s_r];
    s_r = (s_r + 1) & (RING - 1);
    segment(v & 0x80000000u, v & 0x7FFFFFFF, now);
  }
  // тишина после последнего импульса: посылку закрываем по времени (следующего фронта может не быть)
  if (s_havePulse || s_train.n) {
    uint32_t quiet = micros() - s_lastEdge;
    bool low = !((REG_READ(GPIO_IN_REG) >> CC_GDO0) & 1);
    if (low && quiet > TRAIN_END_US) {
      if (s_havePulse) segment(false, 65535, now);
      else finish(now);
      s_rssi = -120;
    }
  }
}
