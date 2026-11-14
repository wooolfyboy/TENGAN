// ============================================================
//  RADIO RADAR — анализатор радиоэфира для LilyGO T-Embed CC1101
//
//  Только приём: Bluetooth, Wi-Fi (2.4 ГГц) и Sub-GHz (CC1101).
//  Ничего не передаём, ни к чему не подключаемся.
//
//  Где что лежит:
//    core/    — экран, меню, кнопки, питание, батарея, настройки, лента
//    ble/     — раздел Bluetooth (+ базы типов, производителей, датчиков)
//    wifi/    — раздел Wi-Fi (+ база производителей сетевых устройств)
//    subghz/  — раздел Sub-GHz (драйвер CC1101 + экраны)
//
//  Весь код: Claude. Распиновка — из репозитория LilyGO.
// ============================================================
#include <Arduino.h>
#include "core/board.h"
#include "core/display.h"
#include "core/input.h"
#include "core/menu.h"
#include "core/power.h"
#include "core/battery.h"
#include "core/settings.h"
#include "core/leds.h"
#include "ble/ble.h"
#include "core/boot.h"

void setup() {
  checkWakeHold();      // включение после «выключения»: только удержанием боковой кнопки

  // общая шина SPI: все устройства — «не выбраны», пока не начнём с ними работать
  pinMode(BOARD_PWR_EN, OUTPUT); digitalWrite(BOARD_PWR_EN, HIGH);
  pinMode(PIN_TFT_CS,  OUTPUT);  digitalWrite(PIN_TFT_CS,  HIGH);
  pinMode(PIN_SD_CS,   OUTPUT);  digitalWrite(PIN_SD_CS,   HIGH);
  pinMode(PIN_LORA_CS, OUTPUT);  digitalWrite(PIN_LORA_CS, HIGH);
  pinMode(PIN_NRF_CS,  OUTPUT);  digitalWrite(PIN_NRF_CS,  HIGH);

  inputBegin();
  displayBegin();
  ledsBegin();
  settingsBegin();
  batBegin();
  bootScreen();          // заставка-самопроверка
  bleBegin();
  navBegin();
  navDraw(millis());
}

void loop() {
  static uint32_t lastDraw = 0;
  inputPoll();
  uint32_t now = millis();
  bleSample(now);
  ledsTick();
  batTick(now);
  navTick(now);                       // работа открытого раздела (сканы, развёртки)

  powerIdleTick(now);
  // в приглушённом режиме кадры в 4 раза реже — меньше работы процессору и шине
  if (now - lastDraw >= (powerIdle() ? REDRAW_MS * 4 : REDRAW_MS)) {
    lastDraw = now;
    bleUpdate(now);                   // нужно всегда: меню показывает число устройств, лента — события
    if (g_powerHold) drawPowerHold();
    else             navDraw(now);
    settingsAutosave(now);
  }
  delay(5);
}
