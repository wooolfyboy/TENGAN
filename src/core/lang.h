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

// заглавные буквы с учётом кириллицы (UTF-8): «настройки» -> «НАСТРОЙКИ».
// out — буфер n байт; разрезанная буква в конце не остаётся.
void utf8Upper(const char* s, char* out, size_t n);
void utf8UpperInPlace(char* s);
