// ============================================================
//  ir_decode.cpp — разбор ИК-пачек: протокол, адрес, команда, марка.
//
//  На входе — длительности в микросекундах: вспышка, пауза, вспышка, ...
//  (приёмник уже снял несущую 38 кГц). Допуск широкий: дешёвые приёмники
//  удлиняют вспышки на ~100 мкс, а пульты бывают неточными.
// ============================================================
#include "probe/ir.h"

IrModel g_ir;

static bool near(uint32_t v, uint32_t t) {
  return v + 100 >= t * 7 / 10 && v <= t * 13 / 10 + 100;
}

// «расстояние между вспышками»: бит = вспышка mark + пауза one/zero. Начало — с индекса i0.
static bool pulseDist(const uint16_t* d, int n, int i0, int bits, uint32_t mark, uint32_t one, uint32_t zero,
                      bool lsb, uint64_t& v) {
  if (n < i0 + 2 * bits) return false;
  v = 0;
  for (int b = 0; b < bits; b++) {
    if (!near(d[i0 + 2 * b], mark)) return false;
    uint32_t s = d[i0 + 2 * b + 1];
    int bit;
    if (near(s, one)) bit = 1; else if (near(s, zero)) bit = 0; else return false;
    if (lsb) v |= (uint64_t)bit << b; else v = (v << 1) | bit;
  }
  return true;
}

static uint8_t rev8(uint8_t x) { uint8_t r = 0; for (int i = 0; i < 8; i++) if (x & (1 << i)) r |= 0x80 >> i; return r; }
static void setP(IrCode& c, const char* p) { snprintf(c.proto, sizeof(c.proto), "%s", p); }

// Manchester: разложить длительности на «полубиты» по unit мкс (1 — вспышка, 0 — пауза)
static int halves(const uint16_t* d, int n, int i0, uint32_t unit, uint8_t* h, int cap, int maxK) {
  int m = 0;
  for (int i = i0; i < n; i++) {
    int k = (d[i] + unit / 2) / unit;
    if (k < 1 || k > maxK) return -1;
    if (d[i] + 120 < k * unit * 7 / 10 || d[i] > k * unit * 13 / 10 + 150) return -1;
    for (int j = 0; j < k && m < cap; j++) h[m++] = (i - i0) % 2 == 0 ? 1 : 0;
  }
  return m;
}

static bool decodeRc5(const uint16_t* d, int n, IrCode& c) {
  if (n < 9 || !near(d[0], 889) && !near(d[0], 1778)) return false;
  uint8_t h[40]; h[0] = 0;                                 // первая половина стартового бита — пауза (не видна)
  int m = halves(d, n, 0, 889, h + 1, 38, 2);
  if (m < 0) return false;
  m += 1;
  while (m < 28) h[m++] = 0;                               // хвостовая пауза не видна
  if (m != 28) return false;
  uint16_t v = 0;
  for (int b = 0; b < 14; b++) {
    uint8_t a = h[2 * b], z = h[2 * b + 1];
    if (a == z) return false;
    v = (v << 1) | (a == 0 && z == 1 ? 1 : 0);             // RC5: пауза->вспышка = 1
  }
  if (!(v & 0x2000)) return false;                         // S1 всегда 1
  setP(c, "RC5");
  c.toggle = (v >> 11) & 1;
  c.addr = (v >> 6) & 0x1F;
  c.cmd = (v & 0x3F) | (((v >> 12) & 1) ? 0 : 0x40);       // S2 = инверсия 7-го бита команды (RC5X)
  c.bits = 13;
  return true;
}

static bool decodeRc6(const uint16_t* d, int n, IrCode& c) {
  if (n < 10 || !near(d[0], 2666) || !near(d[1], 889)) return false;
  uint8_t h[90];
  int m = halves(d, n, 2, 444, h, 88, 6);
  if (m < 0) return false;
  if (m < 12 + 31) return false;
  if (h[0] != 1 || h[1] != 0) return false;                // стартовый бит «1»: вспышка->пауза
  int mode = 0;
  for (int b = 0; b < 3; b++) { uint8_t a = h[2 + 2 * b], z = h[3 + 2 * b]; if (a == z) return false; mode = (mode << 1) | a; }
  if (h[8] != h[9] || h[10] != h[11] || h[8] == h[10]) return false;   // бит-«прицеп» двойной ширины
  c.toggle = h[8];
  int rem = m - 12, bits = (rem + 1) / 2;
  if (bits >= 32) bits = 32; else if (bits >= 24) bits = 24; else if (bits >= 20) bits = 20; else if (bits >= 16) bits = 16; else return false;
  while (m < 12 + 2 * bits) h[m++] = 0;
  uint32_t v = 0;
  for (int b = 0; b < bits; b++) { uint8_t a = h[12 + 2 * b], z = h[13 + 2 * b]; if (a == z) return false; v = (v << 1) | a; }
  snprintf(c.proto, sizeof(c.proto), mode ? "RC6-%d" : "RC6", mode);
  if (bits == 32) { c.addr = v >> 16; c.cmd = v & 0x7FFF; if (mode == 6) c.toggle = (v >> 15) & 1; }   // MCE: бит 15 — toggle
  else            { c.addr = v >> 8; c.cmd = v & 0xFF; }
  c.bits = bits;
  return true;
}

static bool decodeSony(const uint16_t* d, int n, IrCode& c) {
  if (n < 23 || !near(d[0], 2400) || !near(d[1], 600)) return false;
  int bits = n / 2;                                        // вспышек после заголовка (последняя пауза не видна)
  if (bits != 12 && bits != 15 && bits != 20) return false;
  uint32_t v = 0;
  for (int b = 0; b < bits; b++) {
    uint32_t mk = d[2 + 2 * b];
    if (2 + 2 * b + 1 < n && !near(d[3 + 2 * b], 600)) return false;
    int bit = near(mk, 1200) ? 1 : near(mk, 600) ? 0 : -1;
    if (bit < 0) return false;
    v |= (uint32_t)bit << b;
  }
  snprintf(c.proto, sizeof(c.proto), "SONY%d", bits);
  c.cmd = v & 0x7F; c.addr = v >> 7; c.bits = bits;
  return true;
}

static uint32_t rawHash(const uint16_t* d, int n) {        // «отпечаток» неизвестной пачки
  uint32_t h = 2166136261u;
  for (int i = 0; i < n; i++) { h ^= (d[i] + 150) / 300; h *= 16777619u; }
  return h;
}

bool irDecode(const uint16_t* d, int n, IrCode& c) {
  memset(&c, 0, sizeof(c));
  if (n < 3) return false;
  uint64_t v;
  // NEC и родня: заголовок 9 мс / 4.5 мс
  if (near(d[0], 9000) && near(d[1], 2250) && n <= 4) { setP(c, "NEC"); c.rep = true; return true; }   // «держат кнопку»
  if (near(d[0], 8800) && near(d[1], 4400)) {
    int bits = (n - 3) / 2;
    if (bits >= 32 && pulseDist(d, n, 2, 32, 560, 1690, 560, true, v)) {
      uint8_t b0 = v, b1 = v >> 8, b2 = v >> 16, b3 = v >> 24;
      if ((uint8_t)~b0 == b1) { setP(c, "NEC"); c.addr = b0; } else { setP(c, "NEC-EXT"); c.addr = b0 | b1 << 8; }
      c.cmd = ((uint8_t)~b2 == b3) ? b2 : (b2 | b3 << 8);
      c.bits = 32; return true;
    }
    if (bits == 28 && pulseDist(d, n, 2, 28, 550, 1600, 550, false, v)) {
      setP(c, "LG"); c.addr = (v >> 20) & 0xFF; c.cmd = (v >> 4) & 0xFFFF; c.bits = 28; return true;
    }
    if (bits == 16 && pulseDist(d, n, 2, 16, 525, 1575, 525, true, v)) {
      setP(c, "JVC"); c.addr = v & 0xFF; c.cmd = (v >> 8) & 0xFF; c.bits = 16; return true;
    }
  }
  if (near(d[0], 4500) && near(d[1], 4500) && pulseDist(d, n, 2, 32, 560, 1690, 560, true, v)) {
    setP(c, "SAMSUNG"); c.addr = v & 0xFF; c.cmd = (v >> 16) & 0xFF; c.bits = 32; return true;
  }
  if (near(d[0], 3456) && near(d[1], 1728) && pulseDist(d, n, 2, 48, 432, 1296, 432, true, v)) {
    setP(c, "KASEIKYO");
    uint16_t vendor = v & 0xFFFF;
    c.addr = vendor; c.cmd = (uint32_t)((v >> 32) & 0xFF) | (uint32_t)(((v >> 20) & 0xFFF) << 8);
    c.bits = 48; return true;
  }
  if (decodeSony(d, n, c)) return true;
  if (decodeRc6(d, n, c)) return true;
  if (decodeRc5(d, n, c)) return true;
  // не узнали: «сырая» пачка с отпечатком (одна и та же кнопка — один отпечаток)
  if (n < 7) return false;                                 // 3 вспышки и меньше — скорее помеха
  setP(c, "RAW"); c.addr = 0; c.cmd = rawHash(d, n); c.bits = (n + 1) / 2;
  return true;
}

// ---------- «чей пульт»: по адресу и протоколу. Только честные «вероятно». ----------
const char* irBrand(const IrCode& c) {
  const char* p = c.proto;
  if (!strcmp(p, "NEC")) {
    switch (c.addr) {
      case 0x04: return "LG TV";
      case 0x02: return "Toshiba TV";
      case 0x00: return "no-name: LED strip, fan, box";
      case 0x7A: case 0x7E: return "Yamaha audio";
    }
    return "NEC remote (many brands)";
  }
  if (!strcmp(p, "NEC-EXT")) {
    if (c.addr == 0x87EE) return "Apple remote";
    if (c.addr == 0xEF00) return "RGB LED strip / lamp";
    return "NEC remote (many brands)";
  }
  if (!strcmp(p, "SAMSUNG")) return c.addr == 0x07 ? "Samsung TV" : "Samsung";
  if (!strncmp(p, "SONY", 4)) {
    if (c.bits == 12 && c.addr == 1) return "Sony TV";
    if (c.bits == 20) return "Sony Blu-ray / AV";
    return "Sony";
  }
  if (!strcmp(p, "RC5")) {
    switch (c.addr) { case 0: return "Philips TV"; case 5: return "Philips VCR"; case 16: case 17: return "Philips / Marantz audio"; case 20: return "Philips CD"; }
    return "Philips-style (RC5)";
  }
  if (!strcmp(p, "RC6")) return c.addr == 0 ? "Philips TV" : "Philips-style (RC6)";
  if (!strncmp(p, "RC6-6", 5)) return c.bits == 32 && c.addr == 0x800F ? "Windows Media Center" : "set-top box (RC6-6)";
  if (!strcmp(p, "LG")) return "LG (air con / TV)";
  if (!strcmp(p, "JVC")) return "JVC";
  if (!strcmp(p, "KASEIKYO")) {
    switch (c.addr) { case 0x2002: return "Panasonic"; case 0x3254: return "Denon"; case 0xCB23: return "Mitsubishi";
                      case 0x5AAA: return "Sharp"; case 0x0103: return "JVC"; }
    return "Kaseikyo (Japanese AV)";
  }
  if (!strcmp(p, "RAW")) return c.bits >= 70 ? "long frame: air conditioner?" : "unknown remote";
  return "unknown";
}

// ---------- модель: история, повторы ----------
bool irFeed(const uint16_t* d, int n, uint32_t now) {
  IrCode c;
  if (!irDecode(d, n, c)) return false;
  IrModel& m = g_ir;
  m.frames++;
  if (n > 4 || !c.rep) {                                   // повтор NEC — короткий, картинку пачки не затираем
    m.rawN = min(n, IR_RAW_MAX);
    memcpy(m.raw, d, m.rawN * sizeof(uint16_t));
  }
  if (c.rep) {                                             // NEC «держат»: та же кнопка, что была
    if (!m.have || strncmp(m.last.proto, "NEC", 3)) return false;
    IrCode r = m.last; r.rep = true; r.t = now; c = r;
  } else {
    bool rc = !strncmp(c.proto, "RC", 2);
    bool same = m.have && !strcmp(c.proto, m.last.proto) && c.addr == m.last.addr && c.cmd == m.last.cmd &&
                (now - m.last.t) < (rc ? 400u : 250u) && (!rc || c.toggle == m.last.toggle);
    c.rep = same;                                          // Sony шлёт 3 раза подряд, RC5/6 — тот же toggle
    c.brand = irBrand(c);
    c.t = now;
  }
  if (!c.rep) {
    memmove(m.hist + 1, m.hist, sizeof(IrCode) * (IR_HIST - 1));
    m.hist[0] = c;
    if (m.histN < IR_HIST) m.histN++;
    m.presses++;
  } else if (m.histN) m.hist[0].t = now;
  m.last = c; m.have = true;
  m.gen++;
  (void)rev8;
  return true;
}
