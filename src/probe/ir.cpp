// ============================================================
//  ir.cpp — ИК-приёмник через RMT, раздел IR и JSON для планшета.
//  Работает, только пока раздел открыт или планшет подписан на "ir".
// ============================================================
#include "probe/ir.h"
#include "core/lang.h"
#include "core/jsonutil.h"
#include "core/crumb.h"
#include "core/board.h"

#define IR_RX_PIN 1                   // BOARD_IR_RX (utilities.h LilyGO)
#define IR_TX_PIN 2                   // ИК-светодиод: держим выключенным
#define IR_SYMS   192                 // символов RMT на пачку (4 блока памяти по 48)

static rmt_data_t s_buf[IR_SYMS];
static size_t     s_n = IR_SYMS;
static bool       s_armed = false;

static void arm() {
  s_n = IR_SYMS;
  s_armed = rmtReadAsync(IR_RX_PIN, s_buf, &s_n);
}

bool irStart() {
  if (g_ir.on) return g_ir.ok;
  crumb("ir start");
  pinMode(IR_TX_PIN, OUTPUT); digitalWrite(IR_TX_PIN, LOW);      // передатчик выключен
  bool ok = rmtInit(IR_RX_PIN, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_4, 1000000) ||
            rmtInit(IR_RX_PIN, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_2, 1000000);   // 1 тик = 1 мкс
  if (ok) {
    rmtSetRxMinThreshold(IR_RX_PIN, 3);                          // короче 3 мкс — помеха
    rmtSetRxMaxThreshold(IR_RX_PIN, 12000);                      // тишина 12 мс = конец пачки
    arm();
  }
  g_ir.ok = ok; g_ir.on = true;
  return ok;
}

void irStop() {
  if (!g_ir.on) return;
  crumb("ir stop");
  if (g_ir.ok) rmtDeinit(IR_RX_PIN);
  s_armed = false;
  g_ir.on = false;
}

void irTick(uint32_t now) {
  if (!g_ir.on || !g_ir.ok) return;
  if (!s_armed) { arm(); return; }
  if (!rmtReceiveCompleted(IR_RX_PIN)) return;
  crumb("ir frame");
  // символ RMT = два отрезка (уровень + длительность). Приёмник: 0 — вспышка, 1 — пауза.
  static uint16_t d[IR_RAW_MAX * 2];
  int n = 0;
  size_t ns = min(s_n, (size_t)IR_SYMS);
  for (size_t i = 0; i < ns && n < (int)(sizeof(d) / sizeof(d[0])); i++) {
    const rmt_data_t& s = s_buf[i];
    uint16_t dur[2] = { (uint16_t)s.duration0, (uint16_t)s.duration1 };
    uint8_t  lv[2]  = { (uint8_t)s.level0, (uint8_t)s.level1 };
    for (int k = 0; k < 2 && n < (int)(sizeof(d) / sizeof(d[0])); k++) {
      if (!dur[k]) continue;
      bool mark = lv[k] == 0;
      if (n == 0 && !mark) continue;                         // начало — только со вспышки
      if ((n % 2 == 0) != mark) { if (n) d[n - 1] += dur[k]; continue; }   // два одинаковых уровня подряд — склеить
      d[n++] = dur[k];
    }
  }
  if (n && n % 2 == 0 && d[n - 1] > 10000) n--;              // хвостовая тишина — не пауза
  if (n >= 3) irFeed(d, min(n, IR_RAW_MAX), now);
  arm();
}

// {"p":"NEC","addr":4,"cmd":8,"bits":32,"brand":"LG TV","rep":0,"raw":[9000,4500,...],"t":123456}
void irJson(String& o) {
  const IrModel& m = g_ir;
  o.reserve(420);
  if (!m.have) { o = "{\"p\":\"\",\"ok\":"; o += m.ok ? 1 : 0; o += '}'; return; }
  const IrCode& c = m.last;
  o = "{\"p\":"; jsonStr(o, c.proto);
  o += ",\"addr\":"; o += c.addr; o += ",\"cmd\":"; o += c.cmd; o += ",\"bits\":"; o += c.bits;
  o += ",\"brand\":"; jsonStr(o, c.brand ? c.brand : "");
  o += ",\"rep\":"; o += c.rep ? 1 : 0;
  o += ",\"raw\":[";
  for (int i = 0; i < m.rawN && i < 64; i++) { if (i) o += ','; o += m.raw[i]; }
  o += "],\"t\":"; o += c.t; o += '}';
}

// ---------- раздел ----------
static void irSubtitle(char* out, size_t n) {
  if (g_ir.have) snprintf(out, n, L("last: %s", "пульт: %s"), g_ir.last.proto);
  else           snprintf(out, n, "38 kHz");
}
static void irEnter() { irStart(); }
static void irLeave() { irStop(); }
static void irClick() { g_ir.histN = 0; g_ir.have = false; g_ir.rawN = 0; }   // нажатие — очистить
static void irRotate(int) {}

const Section irSection = {
  L("IR", "ИК"), 230, 90, 120, iconRemote, irSubtitle,
  irEnter, irLeave,
  0, nullptr, nullptr, nullptr, nullptr,
  irClick, irRotate, nullptr, irTick, drawIr,
};
