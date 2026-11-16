// ============================================================
//  lang.h — язык прошивки (выбирается при сборке)
//
//  Две сборки: английская (по умолчанию, env t-embed-cc1101) и русская
//  (env t-embed-cc1101-ru, -DTENGAN_LANG_RU=1). Строки интерфейса пишутся
//  как L("English", "Русский") — в прошивку попадает только одна из них,
//  памяти это не стоит.
// ============================================================
#pragma once

#ifdef TENGAN_LANG_RU
  #define L(en, ru) (ru)
  #define LANG_CODE "ru"
  #define LANG_RU 1
#else
  #define L(en, ru) (en)
  #define LANG_CODE "en"
  #define LANG_RU 0
#endif

// число + слово в нужной форме: PL(n, "device", "devices", "устройство", "устройства", "устройств")
inline const char* plForm(int n, const char* one, const char* few, const char* many) {
  n = n < 0 ? -n : n; int a = n % 100, b = n % 10;
  if (a > 10 && a < 20) return many;
  if (b == 1) return one;
  if (b >= 2 && b <= 4) return few;
  return many;
}
#if LANG_RU
  #define PL(n, en1, enN, ru1, ru2, ru5) plForm((n), ru1, ru2, ru5)
#else
  #define PL(n, en1, enN, ru1, ru2, ru5) ((n) == 1 ? (en1) : (enN))
#endif

// заглавные буквы с учётом кириллицы (UTF-8): «настройки» -> «НАСТРОЙКИ».
// out — буфер n байт; разрезанная буква в конце не остаётся.
void utf8Upper(const char* s, char* out, size_t n);
void utf8UpperInPlace(char* s);
