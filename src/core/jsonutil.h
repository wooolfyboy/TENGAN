// ============================================================
//  jsonutil.h — quoted JSON string for the device web page:
//  quotes and backslashes are escaped, control chars become spaces.
// ============================================================
#pragma once
#include <Arduino.h>

static inline void jsonStr(String& o, const char* s) {
  o += '"';
  for (; s && *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  o += '"';
}
static inline void jsonStr(String& o, const String& s) { jsonStr(o, s.c_str()); }
