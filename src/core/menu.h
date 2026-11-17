// ============================================================
//  menu.h — navigation: main menu -> section submenu -> screen
// ============================================================
#pragma once
#include "core/section.h"

void navBegin();
void navClick();                   // wheel click
void navRotate(int dir);           // wheel step
void navBack();                    // short side-button press: one step back
void navToSub();                   // from a screen back to the submenu
void navOpen(const Section* s);    // open a section from outside (desktop app)
void navTick(uint32_t now);        // every loop() pass
void navDraw(uint32_t now);        // draw a frame

bool navActive(const Section* s);  // is this section open
bool navInSub();                   // is the submenu shown
