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
void drawWebOta(const WebView& v, int pct, const char* err);
// планшет подключён: вместо пароля — анимация связи «палка -> планшет»
struct WebLink { int tablets; const char* streams; uint32_t bps; uint32_t sent; };   // streams: "BLE  WX  SND"; bps — байт/с
void drawWebLink(const WebLink& l, uint32_t now);   // загрузка прошивки: -2 ошибка, 0..100

// приложение для компьютера по USB (строки «@…» с того же провода, что монитор ПК)
void webLinkLine(char* s);                         // строка от компьютера (её отдаёт pc.cpp)
void webLinkPump();                                // каждый проход loop(): страница кусками + очередь USB
