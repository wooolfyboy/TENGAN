// ============================================================
//  gallery.h — Gallery section: pretty things that analyze
//  nothing (hypercube, point torus)
// ============================================================
#pragma once
#include "core/section.h"

extern const Section gallerySection;
void drawTesseract(uint32_t now);
void drawTorus(uint32_t now);
void drawLife(uint32_t now);
void drawPendulum(uint32_t now);
void drawSky(uint32_t now);          // planetarium (gallery_sky.cpp)
void drawGlobe(uint32_t now);        // Earth and satellites
void skyTurn(int dir);  void skyClick();
void globeTurn(int dir); void globeClick();
