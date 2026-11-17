// ============================================================
//  display.h — screen and shared UI building blocks
// ============================================================
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI    tft;
extern TFT_eSprite spr;            // the whole frame is drawn into a sprite, then pushed to the screen at once

void displayBegin();

// lists (Devices, Networks)
static const int LIST_ROWS = 7, LIST_ROWH = 18, LIST_Y0 = 26;   // grid from core/ui.h

// utilities
extern bool g_fastFrame;     // screen wants frequent frames (3D); reset after each frame
void fitText(const char* src, char* dst, size_t cap, int px, int font);  // truncate with ".."
void drawBars(int x, int y, int rssi, uint16_t col);                     // 4 signal bars
void macStr(const uint8_t* m, char* out);                                // m = least significant byte first
// signal graph (-100..-30 dBm); buf = n points of cap, -127 = gap
void drawSpark(int gx, int gy, int gw, int gh, const int8_t* buf, int n, int cap, uint16_t col);

// submenu
struct SubItem { const char* name; const char* desc; char value[24]; };
void drawSubmenu(const char* title, uint16_t acc, SubItem* it, int n, int sel);

// HUD: panel with cut corners (top right and bottom left)
void hudPanel(int x, int y, int w, int h, int cut, uint16_t col, bool filled, uint16_t bg);
// arc gauge (top semicircle): pct 0..100, ticks every 15°
void drawArcGauge(int cx, int cy, int r, int thick, int pct, uint16_t col, uint16_t track, uint16_t bg);

// main menu icons
void iconBluetooth(int x, int y, uint16_t c, uint16_t bg);
void iconWifi(int x, int y, uint16_t c, uint16_t bg);
void iconGear(int x, int y, uint16_t c, uint16_t bg);
void iconAntenna(int x, int y, uint16_t c, uint16_t bg);
void iconMic(int x, int y, uint16_t c, uint16_t bg);
void iconCube(int x, int y, uint16_t c, uint16_t bg);
void iconPhone(int x, int y, uint16_t c, uint16_t bg);
void iconLaptop(int x, int y, uint16_t c, uint16_t bg);
