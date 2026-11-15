// ============================================================
//  TENGAN — анализатор радиоэфира для LilyGO T-Embed CC1101
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
#include "pc/pc.h"
#include "gallery/sat.h"
#include <driver/gpio.h>
#include <esp_ota_ops.h>
#include "audio/audio.h"

// Самый ранний момент, где можно что-то сделать (ещё до setup): гасим подсветку,
// чтобы не было видно «мусора» на экране, пока он не инициализирован.
extern "C" void initVariant() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
}
// Ещё раньше: глобальный конструктор запускается до всей инициализации Arduino.
__attribute__((constructor(101))) static void earlyBacklightOff() {
  gpio_set_direction((gpio_num_t)TFT_BL, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)TFT_BL, 0);
}

// Откат прошивки: после обновления по Wi-Fi новая прошивка считается «пробной».
// Подтверждаем её сами, когда она проработала 15 с; если раньше зависла или
// перезагрузилась — загрузчик вернёт старую.
extern "C" bool verifyRollbackLater() { return true; }
static void confirmFirmware(uint32_t now) {
  static bool done = false;
  if (done || now < 15000) return;
  done = true;
  esp_ota_mark_app_valid_cancel_rollback();      // после прошивки по USB просто ничего не делает
}

void setup() {
  checkWakeHold();      // включение после «выключения»: только удержанием боковой кнопки

  // общая шина SPI: все устройства — «не выбраны», пока не начнём с ними работать
  pinMode(BOARD_PWR_EN, OUTPUT); digitalWrite(BOARD_PWR_EN, HIGH);
  pinMode(PIN_TFT_CS,  OUTPUT);  digitalWrite(PIN_TFT_CS,  HIGH);
  pinMode(PIN_SD_CS,   OUTPUT);  digitalWrite(PIN_SD_CS,   HIGH);
  pinMode(PIN_LORA_CS, OUTPUT);  digitalWrite(PIN_LORA_CS, HIGH);
  pinMode(PIN_NRF_CS,  OUTPUT);  digitalWrite(PIN_NRF_CS,  HIGH);

  Serial.setRxBufferSize(2048);
  Serial.begin(115200);  // USB: сюда пишет программа ноутбука (раздел PC)
  inputBegin();
  displayBegin();
  ledsBegin();
  settingsBegin();
  batBegin();
  bootScreen();          // анимация включения
  bleBegin();
  audioBegin();          // динамик: звуки интерфейса
  satBegin();            // сохранённые орбиты спутников (Gallery › Globe)
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
  pcPoll(now);                        // строки с ноутбука по USB (раздел PC)
  navTick(now);                       // работа открытого раздела (сканы, развёртки)

  powerIdleTick(now);
  confirmFirmware(now);
  // в приглушённом режиме кадры в 4 раза реже — меньше работы процессору и шине
  uint32_t frame = powerIdle() ? REDRAW_MS * 4 : (g_fastFrame ? 33 : REDRAW_MS);
  if (now - lastDraw >= frame) {
    lastDraw = now;
    bleUpdate(now);                   // нужно всегда: меню показывает число устройств, лента — события
    g_fastFrame = false;              // экран сам попросит снова, если ему нужно
    if (g_powerHold) drawPowerHold();
    else             navDraw(now);
    settingsAutosave(now);
  }
  delay(5);
}
