// ============================================================
//  ui.h — компоненты дизайн-системы (сетка, шапка, подвал, рамки)
//
//  СЕТКА экрана 320×170:
//    шапка    y 0..19   — «// НАЗВАНИЕ» слева, справа короткий статус
//    контент  y 24..152 — поля 8 px слева и справа
//    подвал   y 156..169 — подсказки управления (мелко, тускло)
//  Шаг отступов — 4 px. Главное число экрана — size 6, по одному на экран.
// ============================================================
#pragma once
#include <Arduino.h>

static const int UI_M = 8;                 // поля
static const int UI_TOP = 24;              // начало контента
static const int UI_BOTTOM = 152;          // конец контента
static const int UI_FOOT_Y = 167;          // базовая линия подвала

uint16_t UI_LABEL();                       // стальной — подписи
uint16_t UI_DIM();                         // тусклый — подсказки, оси
uint16_t UI_LINE();                        // тёмный — рамки, сетка
uint16_t UI_PANEL();                       // фон выбранной строки / панели

// шапка: «// TITLE» (заглавными) и статус справа; под ней угасающая линия
void uiHeader(const char* title, const char* right = nullptr, bool rightAlert = false);
// подвал: подсказки управления слева и справа
void uiFooter(const char* left, const char* right = nullptr);
// рамка-прицел: только уголки (для графиков)
void uiFrame(int x, int y, int w, int h, uint16_t col);
// подпись (мелко, заглавными) + значение под ней
void uiReadout(int x, int y, const char* label, const char* value, uint16_t valCol, int valSize = 2);
// пустой экран: крупная фраза и пояснение
void uiEmpty(const char* msg, const char* sub = nullptr);
// точки-страницы над подвалом: сколько экранов в разделе и какой открыт.
// Ставит меню перед рисованием экрана (или сам раздел); рисует present().
extern int g_uiPages, g_uiPage;
// выделение строки списка (скошенная панель + маркер слева)
void uiRowHighlight(int y, int h);
