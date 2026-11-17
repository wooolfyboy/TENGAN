// ============================================================
//  psram.h — big buffers go to external PSRAM (8 MB), not the fast internal RAM (320 KB total).
//  Allocated on first use of a screen and kept.
// ============================================================
#pragma once
#include <stdlib.h>
#if __has_include(<esp_heap_caps.h>)
#include <esp_heap_caps.h>
template <class T> static inline T* psAlloc(size_t n) {
  void* p = heap_caps_calloc(n, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = calloc(n, sizeof(T));                       // no PSRAM: plain heap
  return (T*)p;
}
#else
template <class T> static inline T* psAlloc(size_t n) { return (T*)calloc(n, sizeof(T)); }   // simulator
#endif
