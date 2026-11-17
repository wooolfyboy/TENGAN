// ============================================================
//  vendors.h — Bluetooth device vendors
// ============================================================
#pragma once
#include <Arduino.h>
#include "ble/devtypes.h"

// Major brands: on-screen name, colour, key (first word of the name in the Bluetooth DB)
struct Brand { const char* label; const char* key; uint8_t r, g, b; };
static const Brand BRANDS[] = {
  { "private",   "",          120, 120, 130 },   // 0  V_PRIV
  { "other",     "",          175, 145, 105 },   // 1  V_OTHER
  { "Apple",     "apple",     240, 240, 240 },
  { "Microsoft", "microsoft",  80, 150, 240 },
  { "Samsung",   "samsung",    60, 205, 205 },
  { "Google",    "google",    240, 200,  70 },
  { "Xiaomi",    "xiaomi",    255, 120,  30 },
  { "Huawei",    "huawei",    230,  60,  60 },
  { "Honor",     "honor",     100, 130, 255 },
  { "OnePlus",   "oneplus",   245,  50,  70 },
  { "Oppo",      "oppo",       40, 180,  90 },
  { "Realme",    "realme",    250, 220,   0 },
  { "vivo",      "vivo",       70, 110, 245 },
  { "Motorola",  "motorola",  110, 110, 220 },
  { "Sony",      "sony",      190, 120, 240 },
  { "LG",        "lg",        220,  60, 140 },
  { "Amazfit",   "huami",     150, 230,  80 },
  { "Garmin",    "garmin",     60, 160, 210 },
  { "Bose",      "bose",      200, 170, 130 },
  { "JBL",       "harman",    255,  90,  40 },
  { "Amazon",    "amazon",    255, 165,  60 },
  { "Meta",      "meta",       70, 110, 250 },
  { "Tile",      "tile",       60, 200, 240 },
  { "Nothing",   "nothing",   210, 210, 210 },
  { "Anker",     "anker",      70, 170, 230 },
  { "Logitech",  "logitech",    0, 190, 170 },
};
static const int N_BRANDS = sizeof(BRANDS) / sizeof(BRANDS[0]);

struct Vend { uint8_t cat, kind, score; uint16_t code; };

Vend        resolveVendor(const AdvInfo& ai);   // best candidate from Company ID and service UUIDs
const char* vendorLabel(uint8_t cat, uint8_t kind, uint16_t code);
const char* btUuidOwner(uint16_t uuid);         // whose "signature" UUID it is (or nullptr)
