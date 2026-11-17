// ============================================================
//  ble.h — Bluetooth section (the only file needed from outside it)
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

void bleBegin();                 // NimBLE + continuous scan
void bleUpdate(uint32_t now);    // every frame: merge addresses into devices (menu and feed)
void bleSample(uint32_t now);    // once a second: TIMELINE history and feed events
void bleRadioOn();               // Wi-Fi and Bluetooth share one antenna:
void bleRadioOff();              // only one runs at a time
bool bleLabelSet(uint32_t id, const char* name, bool& stable);   // tablet label ("" = remove); stable = address does not rotate
bool bleRadioActive();           // is scanning now (tablet status line)
int  bleEntityIds(uint32_t* out, int max);   // ids of devices on air now (radio weather)

void bleJson(String& out);       // devices and thermometers for the web page
void bleDevJson(uint32_t id, String& out);   // details of one device
void bleStreamJson(String& out); // compact, for the tablet (SSE stream)
void bleLowDuty(bool low);       // web mode: scan less often, share the air with Wi-Fi

extern const Section bleSection;
