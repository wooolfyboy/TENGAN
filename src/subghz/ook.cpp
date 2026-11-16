// ============================================================
//  ook.cpp — декодеры датчиков 433 МГц и анализатор импульсов.
//  Описание — в ook.h. Нарезка битов повторяет правила rtl_433
//  (PPM: бит по длине паузы; PWM: бит по длине импульса), чтобы
//  записи из его набора тестов разбирались так же.
// ============================================================
#include "subghz/ook.h"
#include <stdlib.h>
#include <stdio.h>

// ---------- биты: строки (повторы посылки) ----------
#define OB_ROWS  48
#define OB_BYTES 12                        // до 96 бит в строке (длиннее нам не нужно)
struct OokBits {
  uint8_t  b[OB_ROWS][OB_BYTES];
  uint16_t len[OB_ROWS];
  int rows;
  void clear() { memset(this, 0, sizeof(*this)); }
  void add(int bit) {
    if (rows == 0) rows = 1;
    uint16_t& L = len[rows - 1];
    if (L < OB_BYTES * 8 && bit) b[rows - 1][L >> 3] |= 0x80 >> (L & 7);
    if (L < 0xFFFF) L++;                    // длиннее буфера — строка просто не совпадёт по длине
  }
  void newRow() {
    if (rows == 0) rows = 1;
    if (rows < OB_ROWS) { rows++; return; }
    len[rows - 1] = 0; memset(b[rows - 1], 0, OB_BYTES);   // переполнение: затираем последнюю
  }
  void sync() { if (rows == 0) rows = 1; if (len[rows - 1]) newRow(); }
  bool any() const { return rows > 1 || (rows == 1 && len[0] > 0); }
  bool same(int i, int j) const {
    if (len[i] != len[j]) return false;
    int n = len[i] > OB_BYTES * 8 ? OB_BYTES * 8 : len[i];
    for (int k = 0; k < n / 8; k++) if (b[i][k] != b[j][k]) return false;
    int r = n & 7;
    if (r && ((b[i][n / 8] ^ b[j][n / 8]) & (0xFF << (8 - r)))) return false;
    return true;
  }
  // строка длиной >= minBits, повторённая >= minRep раз (как bitbuffer_find_repeated_row)
  int repeated(int minRep, int minBits) const {
    for (int i = 0; i < rows; i++) {
      if (len[i] < minBits || len[i] > OB_BYTES * 8) continue;
      int c = 0;
      for (int j = 0; j < rows; j++) if (same(i, j)) c++;
      if (c >= minRep) return i;
    }
    return -1;
  }
  void invert() { for (int i = 0; i < rows; i++) for (int k = 0; k < OB_BYTES; k++) b[i][k] = ~b[i][k]; }
  // извлечь nbits начиная с бита off строки r
  void extract(int r, int off, uint8_t* out, int nbits) const {
    memset(out, 0, (nbits + 7) / 8);
    for (int i = 0; i < nbits; i++) {
      int s = off + i;
      if (s >= OB_BYTES * 8) break;
      if (b[r][s >> 3] & (0x80 >> (s & 7))) out[i >> 3] |= 0x80 >> (i & 7);
    }
  }
};

static uint8_t crc8(const uint8_t* m, int n, uint8_t poly, uint8_t init) {
  uint8_t r = init;
  for (int i = 0; i < n; i++) {
    r ^= m[i];
    for (int k = 0; k < 8; k++) r = (r & 0x80) ? (uint8_t)((r << 1) ^ poly) : (uint8_t)(r << 1);
  }
  return r;
}
static uint8_t lfsrDigest8Reflect(const uint8_t* m, int bytes, uint8_t gen, uint8_t key) {
  uint8_t sum = 0;
  for (int k = bytes - 1; k >= 0; --k)
    for (int i = 0; i < 8; ++i) {
      if ((m[k] >> i) & 1) sum ^= key;
      key = (key & 0x80) ? (uint8_t)((key << 1) ^ gen) : (uint8_t)(key << 1);
    }
  return sum;
}
static int parity8(uint8_t x) { x ^= x >> 4; x &= 0xF; return (0x6996 >> x) & 1; }
static uint16_t fnv16(const uint8_t* d, int nbits) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < (nbits + 7) / 8; i++) { h ^= d[i]; h *= 16777619u; }
  h ^= (uint32_t)nbits;
  return (uint16_t)(h ^ (h >> 16));
}

// ---------- нарезка ----------
struct Out { OokMsg* m; int n, max; };
typedef bool (*DecFn)(OokBits& B, OokMsg& m);
struct Proto {
  bool pwm;
  uint16_t sh, lo, sync, gapLim, reset, tol;
  DecFn fn;
};

static void emit(const OokMsg& m, Out& o) {
  for (int i = 0; i < o.n; i++)            // один датчик в посылке — одна запись
    if (o.m[i].proto == m.proto && o.m[i].id == m.id && o.m[i].ch == m.ch) return;
  if (o.n < o.max) o.m[o.n++] = m;
}

static OokBits s_B;                        // 600 байт — не на стеке
static void runChunk(const Proto& P, Out& o) {
  OokMsg m;
  if (P.fn(s_B, m)) emit(m, o);
  s_B.clear();
}

// PPM: бит по длине паузы (короткая — 0, длинная — 1)
static void slicePPM(const OokTrain& T, const Proto& P, Out& o) {
  s_B.clear();
  int zu = (P.sh + P.lo) / 2 + 1, ou = P.gapLim ? P.gapLim : P.reset;
  for (int n = 0; n < T.n; n++) {
    int g = T.g[n];
    if (g > 0 && g < zu) s_B.add(0);
    else if (g > zu - 1 && g < ou) s_B.add(1);
    else if (g < P.reset) s_B.newRow();
    if ((n == T.n - 1 || g >= P.reset) && s_B.any()) runChunk(P, o);
  }
}

// PWM: бит по длине импульса (короткий — 1, длинный — 0; как в rtl_433)
static void slicePWM(const OokTrain& T, const Proto& P, Out& o) {
  s_B.clear();
  int one_l, one_u, zero_l, zero_u, sync_l = 0, sync_u = 0;
  if (P.tol) {
    one_l = P.sh - P.tol; one_u = P.sh + P.tol; zero_l = P.lo - P.tol; zero_u = P.lo + P.tol;
    if (P.sync) { sync_l = P.sync - P.tol; sync_u = P.sync + P.tol; }
  } else if (!P.sync) {
    one_l = 0; one_u = (P.sh + P.lo) / 2 + 1; zero_l = one_u - 1; zero_u = 0x7FFFFFFF;
  } else if (P.sync < P.sh) {
    sync_l = 0; sync_u = (P.sync + P.sh) / 2 + 1; one_l = sync_u - 1; one_u = (P.sh + P.lo) / 2 + 1; zero_l = one_u - 1; zero_u = 0x7FFFFFFF;
  } else if (P.sync < P.lo) {
    one_l = 0; one_u = (P.sh + P.sync) / 2 + 1; sync_l = one_u - 1; sync_u = (P.sync + P.lo) / 2 + 1; zero_l = sync_u - 1; zero_u = 0x7FFFFFFF;
  } else {
    one_l = 0; one_u = (P.sh + P.lo) / 2 + 1; zero_l = one_u - 1; zero_u = (P.lo + P.sync) / 2 + 1; sync_l = zero_u - 1; sync_u = 0x7FFFFFFF;
  }
  for (int n = 0; n < T.n; n++) {
    int p = T.p[n], g = T.g[n];
    if (p > one_l && p < one_u) s_B.add(1);
    else if (p > zero_l && p < zero_u) s_B.add(0);
    else if (p > sync_l && p < sync_u) s_B.sync();
    else if (p <= one_l) {}                 // короткая помеха
    else s_B.newRow();
    if ((n == T.n - 1 || g > P.reset) && s_B.rows > 0) runChunk(P, o);
    else if (P.gapLim && g > P.gapLim && s_B.rows > 0 && s_B.len[s_B.rows - 1] > 0) s_B.newRow();
  }
}

static void keepRow(OokMsg& m, const uint8_t* b, int bits) {
  int n = bits > 96 ? 96 : bits;
  memcpy(m.row, b, (n + 7) / 8); m.rowBits = (uint8_t)n;
}

// ============================================================
//  Декодеры
// ============================================================

// Nexus / FreeTec / Solight / TFA 30.3209 (темп.+влажн.) и Rubicson / TFA 30.3197 (темп., CRC)
//   36 бит PPM: [id 8][bat 1][test 1][ch 2][temp 12 со знаком, ×10][1111][hum 8 | crc 8]
static bool decNexus(OokBits& B, OokMsg& m) {
  int r = B.repeated(3, 36);
  if (r < 0 || B.len[r] > 38) return false;
  const uint8_t* b = B.b[r];
  if ((b[3] & 0xF0) != 0xF0) return false;
  if ((b[0] == 0 && b[2] == 0 && b[3] == 0) || (b[0] == 0xFF && b[2] == 0xFF && b[3] == 0xFF)) return false;
  uint8_t c[5] = { b[0], b[1], b[2], (uint8_t)(b[3] & 0xF0), (uint8_t)(((b[3] & 0x0F) << 4) | (b[4] >> 4)) };
  bool rubicson = crc8(c, 5, 0x31, 0x6C) == 0;
  if (!rubicson && B.len[r] > 37) return false;
  if ((b[1] & 0x30) == 0x30) return false;
  int16_t raw = (int16_t)((b[1] << 12) | (b[2] << 4));
  m.t = (raw >> 4) * 0.1f;
  m.id = b[0];
  m.ch = ((b[1] & 0x30) >> 4) + 1;
  m.bat = (b[1] & 0x80) ? 1 : 0;
  if (rubicson) { m.proto = "Rubicson"; m.kind = OOK_T; }
  else {
    int hum = ((b[3] & 0x0F) << 4) | (b[4] >> 4);
    if (hum > 100) return false;
    m.proto = "Nexus";
    if (hum) { m.h = hum; m.kind = OOK_TH; } else m.kind = OOK_T;
  }
  if (m.t < -50 || m.t > 80) return false;
  keepRow(m, b, 36);
  return true;
}

// Prologue / ThermoPro TX2 / FreeTec NC-7104: 36 бит PPM 2000/4000
//   [type 4 = 9|5][id 8][bat 1][btn 1][ch 2][temp 12 со знаком][hum 8 (0xCC — нет)]
static bool decPrologue(OokBits& B, OokMsg& m) {
  if (B.len[0] <= 8 && B.len[0] != 0) return false;
  int r = B.repeated(4, 36);
  if (r < 0 || B.len[r] > 37) return false;
  const uint8_t* b = B.b[r];
  if ((b[0] & 0xF0) != 0x90 && (b[0] & 0xF0) != 0x50) return false;
  m.proto = "Prologue";
  m.id = ((b[0] & 0x0F) << 4) | (b[1] >> 4);
  m.bat = (b[1] & 0x08) ? 1 : 0;
  m.ch = (b[1] & 0x03) + 1;
  int16_t raw = (int16_t)((b[2] << 8) | (b[3] & 0xF0));
  m.t = (raw >> 4) * 0.1f;
  int hum = ((b[3] & 0x0F) << 4) | (b[4] >> 4);
  if (hum != 0xCC && hum <= 100) { m.h = hum; m.kind = OOK_TH; } else m.kind = OOK_T;
  if (m.t < -50 || m.t > 80) return false;
  keepRow(m, b, 36);
  return true;
}

// Acurite 609TXC: 40 бит PPM 1000/2000, [id 8][status 4][temp 12 со знаком][hum 8][sum 8]
static bool decAcurite609(OokBits& B, OokMsg& m) {
  for (int r = 0; r < B.rows; r++) {
    if (B.len[r] != 40) continue;
    const uint8_t* b = B.b[r];
    int sum = b[0] + b[1] + b[2] + b[3];
    if (sum == 0 || (sum & 0xFF) != b[4]) continue;
    if (b[3] > 100) continue;
    int16_t raw = (int16_t)(((b[1] & 0x0F) << 12) | (b[2] << 4));
    m.proto = "Acurite-609"; m.kind = OOK_TH;
    m.t = (raw >> 4) * 0.1f; m.h = b[3]; m.id = b[0]; m.ch = 0;
    m.bat = (b[1] & 0x80) ? 0 : 1;
    keepRow(m, b, 40);
    return true;
  }
  return false;
}

// LaCrosse TX141TH-Bv2 / TX141-Bv2 / TX141-Bv3 / TFA 30.3221: PWM 208/417, синхро 833
//   [id 8][bat 1][test 1][ch 2][temp 12, +500, ×10][hum 8][digest 8]
static bool decTx141(OokBits& B, OokMsg& m) {
  B.invert();                               // длинный импульс — 1
  int r = B.repeated(B.rows > 5 ? 5 : 3, 32);
  if (r < 0 && B.rows <= 4)
    for (int i = 0; i < B.rows; i++)
      if ((B.len[i] == 40 || B.len[i] == 41) && lfsrDigest8Reflect(B.b[i], 4, 0x31, 0xF4) == B.b[i][4]) { r = i; break; }
  if (r < 0) return false;
  int L = B.len[r];
  if (L > 41 || L < 32) return false;
  if (L == 41 && B.rows > 12) return false;
  const uint8_t* b = B.b[r];
  bool th = L >= 40;
  if (th && lfsrDigest8Reflect(b, 4, 0x31, 0xF4) != b[4]) return false;
  m.id = b[0];
  int batLow = th ? (b[1] >> 7) : !(b[1] >> 7);
  m.bat = batLow ? 0 : 1;
  m.ch = ((b[1] & 0x30) >> 4) + 1;
  int raw = ((b[1] & 0x0F) << 8) | b[2];
  m.t = (raw - 500) * 0.1f;
  if (L == 32) m.ch = 0;
  if (th) { m.h = b[3]; m.kind = OOK_TH; m.proto = "LaCrosse-TX141TH"; }
  else { m.kind = OOK_T; m.proto = "LaCrosse-TX141"; }
  if (m.id == 0 || (th && (m.h == 0 || m.h > 100)) || m.t < -40 || m.t > 80) return false;
  keepRow(m, b, L > 40 ? 40 : L);
  return true;
}

// Bresser 3CH / Renkforce DM-7511: PWM 250/500, синхро 750
//   инвертированные байты: [id 8][bat 1][test 1][ch 2][temp 12 °F +90 ×10][hum 8][sum 8]
static bool decBresser3ch(OokBits& B, OokMsg& m) {
  int r = B.repeated(3, 40);
  if (r < 0 || B.len[r] > 42) return false;
  uint8_t b[5];
  for (int i = 0; i < 5; i++) b[i] = ~B.b[r][i];
  if (((b[0] + b[1] + b[2] + b[3] - b[4]) & 0xFF) != 0) return false;
  int ch = (b[1] & 0x30) >> 4;
  float tf = ((((b[1] & 0x0F) << 8) + b[2]) - 900) * 0.1f;
  if (ch == 0 || b[3] > 100 || tf < -20 || tf > 160) return false;
  m.proto = "Bresser-3CH"; m.kind = OOK_TH;
  m.id = b[0]; m.ch = ch; m.bat = (b[1] & 0x80) ? 0 : 1;
  m.t = (tf - 32) * 5.0f / 9.0f; m.h = b[3];
  keepRow(m, b, 40);
  return true;
}

// Acurite 592TXR / 592TX (башенка): PWM 220/408, синхро 620; 7 байт
//   [CC IIIIII][ID 8][pB 000100][p HHHHHHH][p ??TTTTT][p TTTTTTT][sum]
static bool decAcuriteTower(OokBits& B, OokMsg& m) {
  B.invert();
  for (int r = 0; r < B.rows; r++) {
    int bl = B.len[r] / 8;
    if (bl < 7 || bl > 10) continue;
    const uint8_t* b = B.b[r];
    if ((b[2] & 0x3F) != 0x04) continue;
    int sum = 0; for (int i = 0; i < 6; i++) sum += b[i];
    if ((sum & 0xFF) != b[6]) continue;
    bool par = true; for (int i = 2; i < 6; i++) if (parity8(b[i])) par = false;
    if (!par) continue;
    int c = (b[0] & 0xC0) >> 6;
    if (c == 1) continue;
    int hum = b[3] & 0x7F;
    if (hum > 100 && hum != 127) continue;
    int raw = ((b[4] & 0x7F) << 7) | (b[5] & 0x7F);
    float t = (raw - 1000) * 0.1f;
    if (t < -40 || t > 70) continue;
    m.proto = "Acurite-Tower"; m.kind = hum == 127 ? OOK_T : OOK_TH;
    m.id = ((b[0] & 0x3F) << 8) | b[1];
    m.ch = c == 3 ? 1 : c == 2 ? 2 : 3;    // A B C
    m.bat = (b[2] & 0x40) ? 1 : 0;
    m.t = t; m.h = hum == 127 ? -1 : hum;
    keepRow(m, b, 56);
    return true;
  }
  return false;
}

// Fine Offset WH2 / WH2A / WH5 / TFA 30.3157 / Agimex: PWM 500/1500, ±160 мкс
//   после преамбулы: [type 4 = 4][id 8][temp 12][hum 8][crc8 0x31]
static bool decFineOffsetWH2(OokBits& B, OokMsg& m) {
  uint8_t b[6] = {};
  int model;
  if (B.len[0] == 48 && B.b[0][0] == 0xFF) { B.extract(0, 8, b, 40); model = 2; }
  else if (B.len[0] == 55 && B.b[0][0] == 0xFE) { B.extract(0, 7, b, 48); model = b[3] == 0xFF ? 8 : 3; }
  else if (B.len[0] == 47 && B.b[0][0] == 0xFE) { B.extract(0, 7, b, 40); model = 5; }
  else return false;
  if (b[4] != crc8(b, 4, 0x31, 0)) return false;
  if (model == 8 && ((b[0] + b[1] + b[2] + b[3] + b[4]) & 0xFF) != b[5]) return false;
  if ((b[0] >> 4) != 4) return false;
  int temp = ((b[1] & 0x0F) << 8) | b[2];
  m.bat = -1;
  if (model == 8) { m.bat = (temp & 0x800) ? 0 : 1; temp = (temp & 0x7FF) - 400; }
  else if (model == 5) temp -= 400;
  else if (temp & 0x800) temp = -(temp & 0x7FF);
  m.t = temp * 0.1f;
  if (m.t < -40 || m.t > 70) return false;
  m.proto = model == 5 ? "FineOffset-WH5" : model == 2 ? "FineOffset-WH2" : model == 3 ? "FineOffset-WH2A" : "TFA-30.3225";
  m.id = ((b[0] & 0x0F) << 4) | (b[1] >> 4);
  if (model != 8 && b[3] <= 100) { m.h = b[3]; m.kind = OOK_TH; } else m.kind = OOK_T;
  keepRow(m, b, 40);
  return true;
}

static const Proto PROTOS[] = {
  { false, 1000, 2000,   0, 3000,  5000,   0, decNexus },
  { false, 2000, 4000,   0, 7000, 10000,   0, decPrologue },
  { false, 1000, 2000,   0, 3000, 10000,   0, decAcurite609 },
  { true,   208,  417, 833,  625,  1700,   0, decTx141 },
  { true,   250,  500, 750,  625,  1250,   0, decBresser3ch },
  { true,   220,  408, 620,  500,  4000,   0, decAcuriteTower },
  { true,   500, 1500,   0,    0,  1200, 160, decFineOffsetWH2 },
};

// ============================================================
//  Кластеры длительностей (для анализатора и пультов)
// ============================================================
struct Cl { uint32_t sum; uint16_t c, n; };
static int cluster(const uint16_t* v, int n, Cl* cl, int maxC) {
  int k = 0;
  for (int i = 0; i < n; i++) {
    int x = v[i], best = -1, bd = 0x7FFFFFFF;
    for (int j = 0; j < k; j++) {
      int d = abs(x - cl[j].c);
      if (d * 10 <= cl[j].c * 3 + 300 && d < bd) { bd = d; best = j; }   // ±30% (+30 мкс)
    }
    if (best < 0 && k < maxC) { best = k++; cl[best].sum = 0; cl[best].n = 0; }
    if (best < 0) continue;
    cl[best].sum += x; cl[best].n++; cl[best].c = (uint16_t)(cl[best].sum / cl[best].n);
  }
  for (int a = 0; a < k; a++) for (int b = a + 1; b < k; b++) if (cl[b].c < cl[a].c) { Cl t = cl[a]; cl[a] = cl[b]; cl[b] = t; }
  return k;
}
// значимые (>= share от общего числа) кластеры в порядке возрастания
static int significant(const Cl* cl, int k, int total, int minShare, uint16_t* out, int maxOut) {
  int m = 0;
  for (int i = 0; i < k && m < maxOut; i++) if (cl[i].n * 100 >= total * minShare && cl[i].n >= 2) out[m++] = cl[i].c;
  return m;
}

// Пульты с постоянным кодом (звонки, шлагбаумы — EV1527/PT2262): PWM, длинный ≈ 3× короткий,
// строки по 24 бита, разделённые паузой ~31× короткий импульс. Код не сохраняем — только отпечаток.
// Плавающий код (KeeLoq и др.): 64–68 бит PWM с постоянным периодом — «rolling».
static bool decRemote(const OokTrain& T, OokMsg& m) {
  if (T.n < 26) return false;
  static Cl cl[8];
  int n = T.n > 400 ? 400 : T.n;
  int k = cluster(T.p, n, cl, 8);
  uint16_t P[3]; int np = significant(cl, k, n, 12, P, 3);
  if (np != 2) return false;
  float ratio = (float)P[1] / P[0];
  if (ratio < 1.6f || ratio > 4.5f) return false;
  int mid = (P[0] + P[1]) / 2, period = P[0] + P[1];
  // строки: пауза больше 2.5 периода — конец строки
  static OokBits& B = s_B;
  B.clear();
  for (int i = 0; i < T.n; i++) {
    if (T.p[i] > P[1] * 2) { B.sync(); continue; }           // длинная преамбула/заголовок
    B.add(T.p[i] > mid ? 1 : 0);
    if (T.g[i] > period * 5 / 2) B.newRow();
  }
  // постоянный код: строка 24 (или 25 со стоп-битом) повторена хотя бы дважды
  for (int L = 24; L <= 25; L++) {
    for (int i = 0; i < B.rows; i++) {
      if (B.len[i] != L) continue;
      int c = 0; for (int j = 0; j < B.rows; j++) if (B.same(i, j)) c++;
      if (c < 2) continue;
      if (ratio < 2.2f) return false;                         // у EV1527/PT2262 длинный ≈ 3× короткий
      const uint8_t* b = B.b[i];
      m.proto = "Remote"; m.kind = OOK_REMOTE;
      m.id = fnv16(b, 20);                                    // адрес пульта (20 бит) → отпечаток
      m.btn = b[2] & 0x0F;                                    // номер кнопки (4 бита) — не код
      m.btn = (m.btn & 8) ? 4 : (m.btn & 4) ? 3 : (m.btn & 2) ? 2 : (m.btn & 1) ? 1 : 0;
      return true;
    }
  }
  // плавающий код (KeeLoq и похожие): преамбула из 8+ ровных импульсов (импульс ≈ пауза),
  // заголовок — пауза 3..12 мс, затем 65..67 бит PWM. Каждое нажатие — новый код.
  for (int i = 0; i + 8 < T.n; i++) {
    int run = 0;
    while (i + run < T.n && run < 40) {
      int p = T.p[i + run], g = T.g[i + run];
      if (p < 250 || p > 700 || g * 10 < p * 6 || g * 10 > p * 15) break;
      run++;
    }
    if (run < 8 || i + run >= T.n) { i += run; continue; }
    int h = i + run;                                          // последний импульс преамбулы и пауза-заголовок
    if (T.g[h] < 3000 || T.g[h] > 12000) { i = h; continue; }
    int bits = 0;
    for (int j = h + 1; j < T.n && bits < 80; j++) {
      int s = T.p[j] + T.g[j];
      if (s > period * 2) { if (T.g[j] > period * 2) { bits++; break; } }
      bits++;
    }
    if (bits < 65 || bits > 67) { i = h; continue; }
    uint8_t fixed[5] = {};
    for (int j = 0; j < 32; j++) if (T.p[h + 1 + 32 + j] > mid) fixed[j >> 3] |= 0x80 >> (j & 7);   // вторая половина — номер пульта
    m.proto = "Rolling"; m.kind = OOK_REMOTE;
    m.id = fnv16(fixed, 28);
    m.btn = 0;
    return true;
  }
  return false;
}

int ookDecode(const OokTrain& T, OokMsg* out, int maxOut) {
  Out o = { out, 0, maxOut };
  if (T.n < 8) return 0;
  for (const Proto& P : PROTOS) {
    if (P.pwm) slicePWM(T, P, o); else slicePPM(T, P, o);
  }
  // Rubicson и Nexus — один формат; если сошлась CRC Rubicson, Nexus эту посылку уже не трогал
  if (o.n == 0) { OokMsg m; if (decRemote(T, m)) emit(m, o); }
  return o.n;
}

// ============================================================
//  Анализатор: форма посылки
// ============================================================
void ookAnalyze(const OokTrain& T, OokInfo& I, const OokMsg* dec, int nDec) {
  I = OokInfo();
  I.pulses = T.n;
  if (T.n < 12) { I.cls = "noise"; return; }
  static Cl cl[10];
  int n = T.n > 600 ? 600 : T.n;
  uint16_t P[4], G[4];
  int k = cluster(T.p, n, cl, 10);
  int np = significant(cl, k, n, 8, P, 4);
  k = cluster(T.g, n - 1, cl, 10);                            // последняя пауза — конец посылки
  int ng = significant(cl, k, n - 1, 8, G, 4);
  // синхро-пауза: самая длинная редкая пауза (не значимая)
  uint16_t syncG = 0;
  for (int i = 0; i < k; i++) if (cl[i].n >= 1 && cl[i].c > (ng ? G[ng - 1] : 0) * 2) { syncG = cl[i].c; }
  if (np) { I.shortW = P[0]; I.longW = P[np - 1]; }
  if (ng) { I.shortG = G[0]; I.longG = ng > 1 ? G[1] : 0; }
  I.syncG = syncG;

  // модуляция
  if (np >= 2 && I.longW > I.shortW * 13 / 10) {
    // период (импульс + пауза) постоянен — PWM; иначе — Манчестер
    int same = 0, cnt = 0, per = I.shortW + (ng ? G[ng - 1] : I.shortW);
    for (int i = 0; i < n - 1; i++) {
      int s = T.p[i] + T.g[i];
      if (T.g[i] > per * 2) continue;
      cnt++; if (abs(s - per) * 10 < per * 3) same++;
    }
    I.mod = (cnt && same * 10 >= cnt * 7) ? "PWM" : "Manchester";
  } else if (ng >= 2 && np >= 1) I.mod = "PPM";
  else I.mod = "?";

  // биты: самая длинная строка
  int best = 0, cur = 0, rows = 0;
  int brk = (int)(I.longW + (I.longG ? I.longG : I.shortG)) * 5 / 2;
  if (brk < 1500) brk = 1500;
  for (int i = 0; i < n; i++) {
    cur++;
    if (T.g[i] > brk || i == n - 1) { rows++; if (cur > best) best = cur; cur = 0; }
  }
  I.rows = rows;
  I.bits = strcmp(I.mod, "Manchester") == 0 ? best / 2 + 1 : best > 0 ? best - (strcmp(I.mod, "PPM") == 0 ? 1 : 0) : 0;

  // отпечаток посылки — по форме, а не по коду
  uint8_t sig[8] = { (uint8_t)(I.shortW >> 4), (uint8_t)(I.longW >> 4), (uint8_t)(I.shortG >> 4), (uint8_t)(I.longG >> 4),
                     (uint8_t)I.bits, (uint8_t)rows, 0, 0 };
  I.hash = fnv16(sig, 48);

  if (nDec > 0) {
    I.proto = dec[0].proto;
    if (dec[0].kind == OOK_REMOTE) {
      I.cls = strcmp(dec[0].proto, "Rolling") == 0 ? "rolling" : "remote";
      I.hash = (uint16_t)dec[0].id;
    } else {
      I.cls = "sensor";
      I.bits = dec[0].rowBits;
      int nb = (dec[0].rowBits + 7) / 8; if (nb > 12) nb = 12;
      for (int i = 0; i < nb; i++) snprintf(I.hex + i * 2, 3, "%02x", dec[0].row[i]);
    }
  } else if (np == 0 || I.bits < 8) I.cls = "noise";
  else I.cls = "unknown";
}
