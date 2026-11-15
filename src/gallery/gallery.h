// ============================================================
//  gallery.h — раздел Gallery: красивые штуки, которые ничего
//  не анализируют (гиперкуб, тор из точек)
// ============================================================
#pragma once
#include "core/section.h"

extern const Section gallerySection;
void drawTesseract(uint32_t now);
void drawTorus(uint32_t now);
void drawLife(uint32_t now);
void drawPendulum(uint32_t now);
void drawSky(uint32_t now);          // планетарий (gallery_sky.cpp)
void drawGlobe(uint32_t now);        // Земля и спутники
void skyTurn(int dir);  void skyClick();
void globeTurn(int dir); void globeClick();
