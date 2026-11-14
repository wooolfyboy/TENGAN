// ============================================================
//  input.cpp — колесо (на прерываниях) и кнопки
// ============================================================
#include <atomic>
#include <soc/gpio_reg.h>
#include "core/input.h"
#include "core/board.h"
#include "core/menu.h"
#include "core/power.h"

// --- колесо: квадратурный декодер в прерывании ---
// таблица переходов (предыдущее состояние AB << 2 | новое) -> -1 / 0 / +1;
// 4 перехода = один «щелчок». Дребезг даёт недопустимые переходы (0) и гасится сам.
static const int8_t TBL[16] = { 0,-1, 1, 0,  1, 0, 0,-1, -1, 0, 0, 1,  0, 1,-1, 0 };
static volatile uint8_t s_prev = 3;
static volatile int8_t  s_accum = 0;
static std::atomic<int> s_steps{0};

static void IRAM_ATTR encIsr() {
  uint32_t in = REG_READ(GPIO_IN_REG);
  uint8_t cur = (((in >> PIN_ENC_A) & 1) << 1) | ((in >> PIN_ENC_B) & 1);
  if (cur == s_prev) return;
  s_accum += TBL[(s_prev << 2) | cur];
  s_prev = cur;
  if (s_accum >= 4)       { s_steps++; s_accum = 0; }
  else if (s_accum <= -4) { s_steps--; s_accum = 0; }
}

void inputBegin() {
  pinMode(PIN_ENC_KEY,  INPUT_PULLUP);
  pinMode(PIN_ENC_A,    INPUT_PULLUP);
  pinMode(PIN_ENC_B,    INPUT_PULLUP);
  pinMode(PIN_USER_KEY, INPUT_PULLUP);
  s_prev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encIsr, CHANGE);
}

// Боковая кнопка: коротко — на шаг назад, удержание 2 с — выключить.
// Нажатие колеса — «выбрать» (что именно — решает раздел).
void inputPoll() {
  static int lastEnc = HIGH;
  static uint32_t lastEncMs = 0;
  static bool userDown = false;
  static uint32_t userDownMs = 0;
  int enc  = digitalRead(PIN_ENC_KEY);
  int user = digitalRead(PIN_USER_KEY);
  uint32_t now = millis();

  if (enc == LOW && lastEnc == HIGH && now - lastEncMs > 250) {
    navClick();
    lastEncMs = now;
  }
  lastEnc = enc;

  int st = s_steps.exchange(0);
  for (; st > 0; st--) navRotate(+1);
  for (; st < 0; st++) navRotate(-1);

  if (g_ignoreUser) {                    // после включения ждём, пока отпустят
    if (user == HIGH) g_ignoreUser = false;
    return;
  }
  if (user == LOW && !userDown) { userDown = true; userDownMs = now; }
  if (userDown && user == LOW) {
    uint32_t held = now - userDownMs;
    g_powerHold = held >= PWR_SHOW_MS ? held : 0;
    if (held >= PWR_OFF_MS) powerOff();
  }
  if (userDown && user == HIGH) {
    uint32_t held = now - userDownMs;
    userDown = false;
    g_powerHold = 0;
    if (held >= 30 && held < PWR_SHOW_MS) navBack();   // короткое нажатие (30 мс — от дребезга)
  }
}
