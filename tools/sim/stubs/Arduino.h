// Заглушка Arduino для симулятора экрана на компьютере (tools/sim).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <string>
#include <algorithm>
#define PROGMEM
#define pgm_read_byte(a)  (*(const uint8_t*)(a))
#define pgm_read_word(a)  (*(const uint16_t*)(a))
// на ПК указатели 64-битные: поля-указатели читаем целиком
template <class T> static inline uintptr_t pgm_read_dword_f(const T* p) { return (uintptr_t)*p; }
static inline uintptr_t pgm_read_dword_f(const void* p) { return *(const uintptr_t*)p; }   // так библиотека читает указатели
#define pgm_read_dword(a) pgm_read_dword_f(a)
#define pgm_read_ptr(a)   (*(void* const*)(a))
#define IRAM_ATTR
#define ARDUINO_ISR_ATTR
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define F(x) (x)
#define bitRead(v, b) (((v) >> (b)) & 1)
typedef bool boolean;
typedef uint8_t byte;
using std::min; using std::max;
template <class T, class L, class H> static inline T constrain(T x, L lo, H hi) { return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x); }
uint32_t millis();
uint32_t micros();
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}
inline void yield() {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int  digitalRead(int) { return HIGH; }
inline void analogWrite(int, int) {}
#define digitalPinToBitMask(p) (1u << ((p) & 31))
#define digitalPinToPort(p) (0)
#define portOutputRegister(p) ((volatile uint32_t*)0)
inline long random(long lo, long hi) { return lo + rand() % (hi - lo); }
inline long random(long hi) { return rand() % hi; }
inline char* ltoa(long v, char* buf, int base) { if (base == 10) sprintf(buf, "%ld", v); else sprintf(buf, "%lx", v); return buf; }
inline char* ultoa(unsigned long v, char* buf, int base) { if (base == 10) sprintf(buf, "%lu", v); else sprintf(buf, "%lx", v); return buf; }
class __FlashStringHelper;
class String {
 public:
  std::string s;
  String(const char* c = "") : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  const char* c_str() const { return s.c_str(); }
  unsigned length() const { return s.size(); }
  char operator[](unsigned i) const { return s[i]; }
  bool operator==(const char* o) const { return s == o; }
  void toCharArray(char* buf, unsigned n) const { strncpy(buf, s.c_str(), n); if (n) buf[n - 1] = 0; }
};
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  size_t print(const char* s) { size_t n = 0; while (*s) n += write(*s++); return n; }
};
inline uint32_t esp_random() { static uint32_t s = 2463534242u; s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

#include <string>
struct SimSerial {
  std::string buf; size_t pos = 0;
  void begin(unsigned long) {}
  void setRxBufferSize(size_t) {}
  int available() { return (int)(buf.size() - pos); }
  int read() { return pos < buf.size() ? (unsigned char)buf[pos++] : -1; }
  void feed(const char* s) { buf += s; }
  int availableForWrite() { return 64; }
  void print(const char*) {}
  void print(char) {}
};
inline SimSerial Serial;
