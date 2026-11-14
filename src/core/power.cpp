// ============================================================
//  power.cpp — выключение / включение боковой кнопкой
// ============================================================
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include "core/power.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"
#include "core/settings.h"

uint32_t g_powerHold = 0;
static uint32_t s_lastTouch = 0;
static bool     s_dimmed = false;

void powerTouch() {
  s_lastTouch = millis();
  if (s_dimmed) { s_dimmed = false; applyBrightness(); }
}
bool powerIdle() { return s_dimmed; }
void powerIdleTick(uint32_t now) {
  uint32_t limit = DIM_SECS[g_dimIdx] * 1000UL;
  if (!limit) { if (s_dimmed) powerTouch(); return; }  // выключено в настройках
  if (!s_dimmed && now - s_lastTouch > limit) {
    s_dimmed = true;
    analogWrite(TFT_BL, g_scrBright * 255 / 100 / 4);   // четверть яркости
  }
}
bool     g_ignoreUser = false;

void sleepNow() {
  // ждём, пока кнопку отпустят, иначе плата тут же проснётся
  while (digitalRead(PIN_USER_KEY) == LOW) delay(10);
  delay(50);
  pinMode(TFT_BL, OUTPUT);       digitalWrite(TFT_BL, LOW);
  pinMode(BOARD_PWR_EN, OUTPUT); digitalWrite(BOARD_PWR_EN, LOW);   // обесточить периферию
  gpio_hold_en((gpio_num_t)TFT_BL);
  gpio_hold_en((gpio_num_t)BOARD_PWR_EN);
  gpio_deep_sleep_hold_en();
  rtc_gpio_pullup_en((gpio_num_t)PIN_USER_KEY);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_USER_KEY);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_USER_KEY, 0);       // проснуться по нажатию
  esp_deep_sleep_start();
}

void powerOff() {
  settingsSaveNow();
  int W = tft.width(), H = tft.height();
  spr.fillSprite(C_BG());
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C(120, 120, 135), C_BG());
  txt("bye", W / 2, H / 2, 4);
  present();
  strip.clear(); strip.show();
  delay(400);
  tft.writecommand(0x28);   // дисплей: выкл
  tft.writecommand(0x10);   // дисплей: сон
  sleepNow();
}

// Вызывается в самом начале setup(): если нас разбудила кнопка — требуем
// удержания PWR_ON_MS, иначе (случайное касание) засыпаем обратно.
void checkWakeHold() {
  gpio_hold_dis((gpio_num_t)TFT_BL);
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, LOW);   // подсветка выкл, пока экран не очищен
  gpio_hold_dis((gpio_num_t)BOARD_PWR_EN);
  gpio_deep_sleep_hold_dis();
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;   // обычное включение
  rtc_gpio_deinit((gpio_num_t)PIN_USER_KEY);
  pinMode(PIN_USER_KEY, INPUT_PULLUP);
  uint32_t t0 = millis();
  while (digitalRead(PIN_USER_KEY) == LOW && millis() - t0 < PWR_ON_MS) delay(10);
  if (millis() - t0 < PWR_ON_MS) sleepNow();     // отпустили раньше — не включаемся
  g_ignoreUser = true;                           // кнопку ещё держат — не считать нажатием
}

void drawPowerHold() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_HOT(), C_BG());
  txt("power off?", W / 2, H / 2 - 24, 4);
  int bw = W - 80, bx = 40, by = H / 2 + 4, bh = 14;
  uint32_t p = g_powerHold > PWR_OFF_MS ? PWR_OFF_MS : g_powerHold;
  int fill = (bw - 4) * (int)(p - PWR_SHOW_MS) / (int)(PWR_OFF_MS - PWR_SHOW_MS);
  if (fill < 0) fill = 0;
  spr.drawRoundRect(bx, by, bw, bh, 4, C(70, 70, 80));
  if (fill > 0) spr.fillRoundRect(bx + 2, by + 2, fill, bh - 4, 3, C(235, 80, 80));
  spr.setTextColor(C(110, 110, 125), C_BG());
  txt("keep holding the side button", W / 2, by + bh + 16, 2);
  present();
}
