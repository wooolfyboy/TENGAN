// ============================================================
//  theme.h — "Tactical HUD" design system: colors, fonts, effects
//
//  COLOR — each color has one meaning:
//    background — dark blue (C_BG)
//    accent     — teal: selection, main value, active (C_ACC)
//    value      — near white: numbers and names (C_HOT)
//    label      — steel: field names (UI_LABEL, core/ui.h)
//    secondary  — dim steel: hints, axes (UI_DIM)
//    lines      — dark steel: frames, grid (UI_LINE)
//    alert      — orange, ONLY for alerts: tags, open networks,
//                 low battery, noisy channel (C_ALERT)
//    "heat"     — signal strength: steel -> teal -> white (heat)
//  Any other C(r, g, b) is mapped onto the teal-steel ramp by brightness,
//  so screens can't accidentally get off-palette colors.
//
//  FONTS (txt size): 1 = 6x8 pixel font for labels (UPPERCASE),
//  2 = Share Tech Mono 14 px for text, 4 = Rajdhani 24 px for titles
//  and big values, 6 = Rajdhani 44 px for the screen's main number,
//  7 = built-in seven-segment.
// ============================================================
#pragma once
#include <Arduino.h>

uint16_t C(uint8_t r, uint8_t g, uint8_t b);   // color mapped onto the palette
uint16_t scale565(uint16_t c, int pct);         // same color dimmer (toward background): pct% (100 = unchanged)
uint16_t C_BG();
uint16_t C_HOT();
uint16_t C_ALERT();
uint16_t C_ACC();
uint16_t C_SEAL();                              // 天眼 seal color (currently = accent)
uint16_t TC(uint8_t r, uint8_t g, uint8_t b);   // literal color, no palette mapping
uint16_t C_SELBG();                             // selected card/row background (KURENAI: white plate)
uint16_t C_SELFG();                             // text on selection (KURENAI: black)
uint16_t C_SELACC();                            // accent on selection (big number, icon)
bool themeKurenai();                            // KURENAI 紅 theme (black/white/red)
void themeChanged();                            // theme changed: recompute palette

// size: 1 JBM11 small, 2 JBM12 text, 4 title (Exo 2 / Oswald), 6 big digits (Exo 2 / Oswald), 7 seven-segment;
//       3 JBM14, 5 JBM19. All fonts: Latin + Cyrillic (UTF-8)
void txt(const char* s, int x, int y, int size);
void txtNum(long n, int x, int y, int size);
int  txtW(const char* s, int size);

uint16_t heatV(float x);           // x = 0..1
uint16_t heat(int rssi);           // -100 dBm (cold) .. -40 dBm (hot)

// Letter icon: a letter in a frame; inverted = dark letter on light (alert).
void drawGlyph(int x, int y, char ch, uint16_t col, bool inverted);   // (x, y) = center
static const int GLYPH_W = 9, GLYPH_H = 11;
// Radar target marker: diamond; alert = filled orange diamond with a ring.
void drawMarker(int x, int y, uint16_t col, bool alert);

void present();                    // corner brackets + push the frame to the screen
