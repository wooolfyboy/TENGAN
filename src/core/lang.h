// ============================================================
//  lang.h — язык интерфейса: English / Русский, одна прошивка на оба
// ============================================================
#pragma once
#include <stddef.h>

// Язык выбирается на устройстве (Настройки › Язык; при первом включении — экран выбора).
// Обе строки лежат во флеше, L() выбирает нужную в момент вызова — переключение сразу, без перезагрузки.
//
// ВАЖНО: L() — это выбор во время работы. Не класть L() в static/глобальные таблицы:
// такая таблица заполнится один раз и «замёрзнет» на языке запуска. Для таблиц — LP()/LT():
//   const char* name = LP("Sound", "Звук");   // одна строка-пара во флеше
//   txt(LT(name), …);                         // выбор языка — при показе
extern int g_lang;                           // 0 — English, 1 — Русский (core/settings.cpp)
#define L(en, ru) (g_lang ? (ru) : (en))
#define LANG_CODE (g_lang ? "ru" : "en")
#define LANG_RU (g_lang != 0)

// пара строк для таблиц: "\x01" en "\0" ru — LT() отдаёт нужную половину; обычную строку — как есть
#define LP(en, ru) ("\x01" en "\0" ru)
inline const char* LT(const char* p) {
  if (!p || p[0] != '\x01') return p;
  p++;
  if (!g_lang) return p;
  while (*p) p++;
  return p + 1;
}

// английская половина пары (машинные имена: /api/set, JSON-ключи)
inline const char* LEN(const char* p) { return (p && p[0] == '\x01') ? p + 1 : p; }

// число + слово в нужной форме: PL(n, "device", "devices", "устройство", "устройства", "устройств")
inline const char* plForm(int n, const char* one, const char* few, const char* many) {
  n = n < 0 ? -n : n; int a = n % 100, b = n % 10;
  if (a > 10 && a < 20) return many;
  if (b == 1) return one;
  if (b >= 2 && b <= 4) return few;
  return many;
}
#define PL(n, en1, enN, ru1, ru2, ru5) (g_lang ? plForm((n), ru1, ru2, ru5) : ((n) == 1 ? (en1) : (enN)))

// заглавные буквы с учётом кириллицы (UTF-8): «настройки» -> «НАСТРОЙКИ».
// out — буфер n байт; разрезанная буква в конце не остаётся.
void utf8Upper(const char* s, char* out, size_t n);
void utf8UpperInPlace(char* s);
