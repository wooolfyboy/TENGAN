// ============================================================
//  theme.h — внешний вид: цвета, шрифты, эффекты экрана
//
//  Все экраны рисуют цветом через C(r, g, b) и текстом через txt().
//  Тема решает, как это будет выглядеть:
//    PHOSPHOR — зелёный люминофор: любой цвет превращается в оттенок
//               зелёного по своей яркости, шрифт Share Tech Mono,
//               свечение крупного текста и полосы строк, как у старого экрана;
//    CLASSIC  — исходные цвета и встроенные шрифты.
// ============================================================
#pragma once
#include <Arduino.h>

enum { THEME_PHOSPHOR = 0, THEME_CLASSIC = 1, N_THEMES };
extern int g_theme;
const char* themeName(int t);
void themeApply();                 // пересчитать палитру после смены темы

uint16_t C(uint8_t r, uint8_t g, uint8_t b);   // цвет с учётом темы
uint16_t C_BG();                               // фон
uint16_t C_HOT();                              // самый яркий (вместо белого)

// Текст. size: 1 — мелкий пиксельный, 2 — обычный, 4 — крупный, 7 — цифры-«семисегментник».
// Выравнивание и цвет — как обычно через spr.setTextDatum() / spr.setTextColor().
void txt(const char* s, int x, int y, int size);
void txtNum(long n, int x, int y, int size);
int  txtW(const char* s, int size);

// «Жар» — цвет по силе сигнала (вариант D): далёкое тлеет тёмно-зелёным,
// ближе — жёлто-зелёное, рядом — янтарное, вплотную — добела.
// Это настоящие цвета: они не проходят через зелёный фильтр люминофора.
uint16_t heatV(float x);           // x = 0..1
uint16_t heat(int rssi);           // -100 dBm (холодно) .. -40 dBm (горячо)

// Значок-буква (вариант C): буква в рамке; inverted — тёмная буква на светлом (тревога).
void drawGlyph(int x, int y, char ch, uint16_t col, bool inverted);   // (x, y) — центр
static const int GLYPH_W = 9, GLYPH_H = 11;

void present();                    // эффекты темы + вывести кадр на экран
