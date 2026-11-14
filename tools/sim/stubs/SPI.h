#pragma once
#include "Arduino.h"
#define SPI_MODE0 0
#define MSBFIRST 1
struct SPISettings { SPISettings(uint32_t = 0, uint8_t = 0, uint8_t = 0) {} };
class SPIClass {
 public:
  SPIClass(int = 0) {}
  void begin(int = -1, int = -1, int = -1, int = -1) {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t) { return 0; }
  uint16_t transfer16(uint16_t) { return 0; }
  void transfer(void*, size_t) {}
  void write(uint8_t) {}
  void write16(uint16_t) {}
  void writeBytes(const uint8_t*, uint32_t) {}
  void writePixels(const void*, uint32_t) {}
  void setFrequency(uint32_t) {}
};
extern SPIClass SPI;
