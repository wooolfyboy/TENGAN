// ============================================================
//  input.h — wheel (interrupt-driven) and buttons.
//  Wheel steps are not lost even if the main loop is busy (e.g. a CC1101 sweep).
// ============================================================
#pragma once

void inputBegin();
void inputPoll();      // dispatches presses and wheel steps to navigation
