// ============================================================
//  ui.h — design-system components (grid, header, footer, frames)
//
//  GRID of the 320x170 screen:
//    header  y 0..19    — "// TITLE" on the left, short status on the right
//    content y 24..152  — 8 px margins left and right
//    footer  y 156..169 — control hints (small, dim)
//  Spacing step is 4 px. Main number of a screen is size 6, one per screen.
// ============================================================
#pragma once
#include <Arduino.h>

static const int UI_M = 8;                 // margins
static const int UI_TOP = 24;              // content top
static const int UI_BOTTOM = 152;          // content bottom
static const int UI_FOOT_Y = 167;          // footer baseline

uint16_t UI_LABEL();                       // steel: labels
uint16_t UI_DIM();                         // dim: hints, axes
uint16_t UI_LINE();                        // dark: frames, grid
uint16_t UI_PANEL();                       // selected row / panel background

// header: "// TITLE" (uppercase) and status on the right; fading line below
void uiHeader(const char* title, const char* right = nullptr, bool rightAlert = false);
// footer: control hints left and right
void uiFooter(const char* left, const char* right = nullptr);
// crosshair frame: corners only (for graphs)
void uiFrame(int x, int y, int w, int h, uint16_t col);
// label (small, uppercase) + value below it
void uiReadout(int x, int y, const char* label, const char* value, uint16_t valCol, int valSize = 2);
// empty screen: big phrase and explanation
void uiEmpty(const char* msg, const char* sub = nullptr);
// page dots above the footer: screens in the section and the open one.
// Set by the menu before drawing a screen (or by the section); drawn by present().
extern int g_uiPages, g_uiPage;
// list row highlight (cut-corner panel + marker on the left)
void uiRowHighlight(int y, int h);
