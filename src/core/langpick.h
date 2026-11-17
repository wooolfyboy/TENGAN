// langpick.h — first-boot language picker screen (while g_langAsk)
#pragma once
#include <Arduino.h>

void langPickRotate(int dir);   // wheel: English <-> Russian (applied live)
void langPickClick();           // click: save the choice, go to the main menu
void langPickDraw(uint32_t now);
