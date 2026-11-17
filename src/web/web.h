// ============================================================
//  web.h — Web section: the device runs its own Wi-Fi AP and serves a data page.
// ============================================================
#pragma once
#include "core/section.h"

extern const Section webSection;

// screen (web_screen.cpp) is separate so the simulator can use it
struct WebView { const char* ssid; const char* pass; const char* url; int clients; uint32_t reqs; bool ok; const char* tab; };
void drawWebScreen(const WebView& v, const uint8_t* qr, int qrSize);
void drawWebOta(const WebView& v, int pct, const char* err);
// tablet connected: link animation instead of the password
struct WebLink { int tablets; const char* streams; uint32_t bps; uint32_t sent; };   // streams: "BLE  WX  SND"; bps in bytes/s
void drawWebLink(const WebLink& l, uint32_t now);   // firmware upload: -2 error, 0..100

// PC app over USB ("@..." lines on the same wire as the PC monitor)
void webLinkLine(char* s);                         // line from the PC (passed in by pc.cpp)
void webLinkPump();                                // every loop(): page in chunks + USB queue
