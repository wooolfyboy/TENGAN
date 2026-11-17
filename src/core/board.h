// ============================================================
//  board.h — LilyGO T-Embed CC1101 pinout (from LilyGO utilities.h) and shared constants.
// ============================================================
#pragma once

// Device name (splash, main menu, power-off screen); change it here.
#define DEVICE_NAME "TENGEN"            // 天眼 "heavenly eye": sees the invisible
#include <Arduino.h>

#define BOARD_PWR_EN 15     // peripheral power (CC1101, LEDs, ...)
#define PIN_TFT_CS   41
#define PIN_SD_CS    13
#define PIN_LORA_CS  12     // CC1101
#define PIN_NRF_CS   44
#define PIN_ENC_KEY  0      // wheel press
#define PIN_ENC_A    4      // wheel phase A
#define PIN_ENC_B    5      // wheel phase B
#define PIN_USER_KEY 6      // side button
#define PIN_WS2812   14
#define WS2812_N     8

static const uint32_t REDRAW_MS = 66;      // ~15 fps
