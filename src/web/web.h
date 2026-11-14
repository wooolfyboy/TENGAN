// ============================================================
//  web.h — раздел Web: палка раздаёт свой Wi-Fi и показывает
//  страницу с данными на телефоне или компьютере
// ============================================================
#pragma once
#include "core/section.h"

extern const Section webSection;

// экран (web_screen.cpp) — отдельно, чтобы его видел симулятор
struct WebView { const char* ssid; const char* pass; const char* url; int clients; uint32_t reqs; bool ok; const char* tab; };
void drawWebScreen(const WebView& v, const uint8_t* qr, int qrSize);
