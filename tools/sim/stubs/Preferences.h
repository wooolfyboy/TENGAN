#pragma once
#include "Arduino.h"
class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  int  getInt(const char*, int def) { return def; }
  size_t putInt(const char*, int) { return 4; }
};
