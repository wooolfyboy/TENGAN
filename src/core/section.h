// ============================================================
//  section.h — a main-menu "section" (Bluetooth, Wi-Fi, Sub-GHz, settings).
//
//  Each section describes itself with one function table; the menu
//  (core/menu.cpp) treats all sections alike: card, submenu, input, drawing.
//  New section = new file with such a table + one line in menu.cpp.
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/display.h"

struct Section {
  const char* title;                         // card title
  uint8_t r, g, b;                           // section color
  void (*icon)(int x, int y, uint16_t c, uint16_t bg);
  void (*subtitle)(char* out, size_t n);     // live caption under the card

  void (*enter)();                           // entered from the main menu
  void (*leave)();                           // left to the main menu

  int  nScreens;                             // 0 = no submenu (straight to the screen)
  void (*screenInfo)(int i, SubItem& it);    // submenu row: name, description, number
  int  (*screenNow)();                       // currently selected screen
  void (*open)(int screen);                  // submenu -> screen
  void (*close)();                           // screen -> submenu

  void (*click)();                           // wheel click on a screen
  void (*rotate)(int dir);                   // wheel step on a screen
  bool (*back)();                            // side button on a screen: true = handled here
  void (*tick)(uint32_t now);                // every loop() pass while the section is open
  void (*draw)(uint32_t now);                // draw the current screen
};
