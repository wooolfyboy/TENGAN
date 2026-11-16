// ============================================================
//  serout.cpp — очередь записи в USB (см. serout.h)
//  Кольцевой буфер 64 КБ во внешней памяти (PSRAM): строки ложатся целиком,
//  в порт уходят кусками по мере места в его буфере — без ожидания.
// ============================================================
#include <Arduino.h>
#include <string.h>
#include "core/psram.h"
#include "link/serout.h"

static const size_t CAP = 64 * 1024;      // всё, что может ждать отправки
static const size_t DROP_AT = 12 * 1024;  // «можно выкинуть» — уже при такой очереди
static char*  s_buf = nullptr;
static size_t s_head = 0, s_len = 0;      // начало и длина данных в кольце
static bool   s_open = false;             // строка собирается (begin…end)

static bool ensure() { if (!s_buf) s_buf = psAlloc<char>(CAP); return s_buf != nullptr; }
static void put(const char* s, size_t n) {
  size_t tail = (s_head + s_len) % CAP;
  size_t a = n < CAP - tail ? n : CAP - tail;
  memcpy(s_buf + tail, s, a);
  if (n > a) memcpy(s_buf, s + a, n - a);
  s_len += n;
}

bool serOutBegin(size_t total, bool droppable) {
  if (!ensure() || s_open) return false;
  if (droppable && s_len > DROP_AT) return false;
  if (s_len + total + 1 > CAP) return false;
  s_open = true; return true;
}
void serOutPart(const char* s, size_t n) { if (s_open) put(s, n); }
void serOutEnd() { if (!s_open) return; put("\n", 1); s_open = false; }

bool serOutLine(const char* s, size_t n, bool droppable) {
  if (!serOutBegin(n, droppable)) return false;
  serOutPart(s, n); serOutEnd(); return true;
}
bool serOutLine(const char* s) { return serOutLine(s, strlen(s), false); }
size_t serOutQueued() { return s_len; }

void serOutPump() {
  if (!s_len || s_open) return;
  for (int guard = 0; guard < 8 && s_len; guard++) {
    int room = Serial.availableForWrite();
    if (room <= 0) return;                 // компьютер не успевает — позже
    size_t n = s_len < (size_t)room ? s_len : (size_t)room;
    if (n > CAP - s_head) n = CAP - s_head;     // до конца кольца, остальное — следующим куском
    size_t w = Serial.write((const uint8_t*)(s_buf + s_head), n);
    if (!w) return;
    s_head = (s_head + w) % CAP; s_len -= w;
  }
}
