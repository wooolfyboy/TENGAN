// langpick.h — экран выбора языка при первом включении (пока g_langAsk)
#pragma once
#include <Arduino.h>

void langPickRotate(int dir);   // колесо: English <-> Русский (язык сразу «примеряется»)
void langPickClick();           // нажатие: сохранить выбор, дальше — главное меню
void langPickDraw(uint32_t now);
