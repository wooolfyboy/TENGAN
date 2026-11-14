// ============================================================
//  psram.h — большие буферы — во «внешнюю» память (PSRAM, 8 МБ),
//  а не в быструю внутреннюю (320 КБ на всё). Выделяются при первом
//  использовании экрана и дальше живут там же.
// ============================================================
#pragma once
#include <stdlib.h>
#if __has_include(<esp_heap_caps.h>)
#include <esp_heap_caps.h>
template <class T> static inline T* psAlloc(size_t n) {
  void* p = heap_caps_calloc(n, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = calloc(n, sizeof(T));                       // нет PSRAM — обычная память
  return (T*)p;
}
#else
template <class T> static inline T* psAlloc(size_t n) { return (T*)calloc(n, sizeof(T)); }   // симулятор
#endif
