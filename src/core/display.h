// ============================================================
//  display.h — экран и общие «кирпичики» интерфейса
// ============================================================
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI    tft;
extern TFT_eSprite spr;            // весь кадр рисуется в спрайт, потом одним махом на экран

void displayBegin();

// списки (Devices, Networks)
static const int LIST_ROWS = 7, LIST_ROWH = 18, LIST_Y0 = 26;   // сетка core/ui.h

// утилиты
extern bool g_fastFrame;     // экран просит частые кадры (3D); сбрасывается после кадра
void fitText(const char* src, char* dst, size_t cap, int px, int font);  // обрезать с ".."
void drawBars(int x, int y, int rssi, uint16_t col);                     // 4 столбика сигнала
void macStr(const uint8_t* m, char* out);                                // m — младший байт первым
// график сигнала (-100..-30 dBm); buf — n точек из cap, -127 = разрыв
void drawSpark(int gx, int gy, int gw, int gh, const int8_t* buf, int n, int cap, uint16_t col);

// подменю
struct SubItem { const char* name; const char* desc; char value[24]; };
void drawSubmenu(const char* title, uint16_t acc, SubItem* it, int n, int sel);

// HUD: панель со скошенными углами (правый верхний и левый нижний)
void hudPanel(int x, int y, int w, int h, int cut, uint16_t col, bool filled, uint16_t bg);
// дуга-шкала (полукруг сверху): pct 0..100, деления каждые 15°
void drawArcGauge(int cx, int cy, int r, int thick, int pct, uint16_t col, uint16_t track, uint16_t bg);

// иконки главного меню
void iconBluetooth(int x, int y, uint16_t c, uint16_t bg);
void iconWifi(int x, int y, uint16_t c, uint16_t bg);
void iconGear(int x, int y, uint16_t c, uint16_t bg);
void iconAntenna(int x, int y, uint16_t c, uint16_t bg);
void iconMic(int x, int y, uint16_t c, uint16_t bg);
void iconCube(int x, int y, uint16_t c, uint16_t bg);
void iconPhone(int x, int y, uint16_t c, uint16_t bg);
void iconLaptop(int x, int y, uint16_t c, uint16_t bg);
