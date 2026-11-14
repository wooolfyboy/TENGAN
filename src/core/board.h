// ============================================================
//  board.h — распиновка LilyGO T-Embed CC1101 (по utilities.h из
//  официального репозитория LilyGO) и общие константы.
// ============================================================
#pragma once
#include <Arduino.h>

#define BOARD_PWR_EN 15     // питание периферии (CC1101, лента, ...)
#define PIN_TFT_CS   41
#define PIN_SD_CS    13
#define PIN_LORA_CS  12     // CC1101
#define PIN_NRF_CS   44
#define PIN_ENC_KEY  0      // нажатие колеса
#define PIN_ENC_A    4      // колесо, фаза A
#define PIN_ENC_B    5      // колесо, фаза B
#define PIN_USER_KEY 6      // боковая кнопка
#define PIN_WS2812   14
#define WS2812_N     8

static const uint32_t REDRAW_MS = 66;      // ~15 кадров в секунду
