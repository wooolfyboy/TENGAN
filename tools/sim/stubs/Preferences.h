#pragma once
#include "Arduino.h"
class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  int  getInt(const char*, int def) { return def; }
  size_t putInt(const char*, int) { return 4; }
  bool getBool(const char*, bool def) { return def; }
  size_t putBool(const char*, bool) { return 1; }
  float getFloat(const char*, float def) { return def; }
  size_t putFloat(const char*, float) { return 4; }
  size_t getBytes(const char*, void*, size_t) { return 0; }
  size_t putBytes(const char*, const void*, size_t n) { return n; }
  String getString(const char*, const String& def = String()) { return def; }
  size_t putString(const char*, const char* v) { return v ? strlen(v) : 0; }
};
