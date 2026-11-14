// ============================================================
//  menu.h — навигация: главное меню -> подменю раздела -> экран
// ============================================================
#pragma once
#include "core/section.h"

void navBegin();
void navClick();                   // нажатие колеса
void navRotate(int dir);           // шаг колеса
void navBack();                    // короткое нажатие боковой кнопки: на шаг назад
void navToSub();                   // с экрана — обратно в подменю
void navTick(uint32_t now);        // каждый проход loop()
void navDraw(uint32_t now);        // нарисовать кадр

bool navActive(const Section* s);  // открыт ли этот раздел
bool navInSub();                   // показано ли подменю
