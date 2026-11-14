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
