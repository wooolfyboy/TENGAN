// ============================================================
//  lang.h — UI language: English / Russian, one firmware for both
// ============================================================
#pragma once
#include <stddef.h>

// Language is chosen on the device (Settings > Language; a picker on first boot).
// Both strings live in flash and L() picks one at call time, so switching is instant.
//
// IMPORTANT: L() is evaluated at run time. Do not put it in static/global tables: the table
// would freeze in the boot language. For tables use LP()/LT():
//   const char* name = LP("Sound", "Звук");   // one string pair in flash
//   txt(LT(name), ...);                       // language picked when drawn
extern int g_lang;                           // 0 = English, 1 = Russian (core/settings.cpp)
#define L(en, ru) (g_lang ? (ru) : (en))
#define LANG_CODE (g_lang ? "ru" : "en")
#define LANG_RU (g_lang != 0)

// string pair for tables: "\x01" en "\0" ru; LT() returns the right half, a plain string as is
#define LP(en, ru) ("\x01" en "\0" ru)
inline const char* LT(const char* p) {
  if (!p || p[0] != '\x01') return p;
  p++;
  if (!g_lang) return p;
  while (*p) p++;
  return p + 1;
}

// English half of a pair (machine names: /api/set, JSON keys)
inline const char* LEN(const char* p) { return (p && p[0] == '\x01') ? p + 1 : p; }

// number + word in the right form (Russian has 3 plural forms): PL(n, "device", "devices", "устройство", "устройства", "устройств")
inline const char* plForm(int n, const char* one, const char* few, const char* many) {
  n = n < 0 ? -n : n; int a = n % 100, b = n % 10;
  if (a > 10 && a < 20) return many;
  if (b == 1) return one;
  if (b >= 2 && b <= 4) return few;
  return many;
}
#define PL(n, en1, enN, ru1, ru2, ru5) (g_lang ? plForm((n), ru1, ru2, ru5) : ((n) == 1 ? (en1) : (enN)))

// Uppercase with Cyrillic support (UTF-8). out is an n-byte buffer; a cut-off letter is never left at the end.
void utf8Upper(const char* s, char* out, size_t n);
void utf8UpperInPlace(char* s);
