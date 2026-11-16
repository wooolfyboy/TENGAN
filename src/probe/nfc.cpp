// ============================================================
//  nfc.cpp — PN532 по I2C (минимальный драйвер), раздел NFC, JSON.
//
//  Опрос без ожидания: шлём «кто тут?» (InListPassiveTarget) и
//  возвращаемся в loop(); ответ забираем, когда PN532 готов.
//  Карта нашлась — одним коротким заходом (~0.1 с) узнаём остальное:
//  платёжная ли (только SELECT PPSE, ответ сразу затираем), NDEF метки.
// ============================================================
#include <Wire.h>
#include "probe/nfc.h"
#include "core/jsonutil.h"
#include "core/crumb.h"
#include "core/timeutil.h"

#define PN_ADDR  0x24
#define PN_RST   45                   // BOARD_PN532_RF_REST: LOW — PN532 спит
#define PN_BUF   64

static uint8_t  s_rx[PN_BUF + 1];
static bool     s_trunc = false;      // ответ длиннее буфера (хвост не читали)
static int      s_poll = 0;           // 0 — ждём паузу, 1 — ждём ответ на опрос
static uint32_t s_pollMs = 0;
static int      s_cycle = 0;          // каждый 3-й опрос — FeliCa
static int      s_fails = 0;

static bool pnWrite(const uint8_t* cmd, int len) {
  uint8_t f[PN_BUF];
  if (len + 8 > (int)sizeof(f)) return false;
  int n = 0; uint8_t L = len + 1, sum = 0xD4;
  f[n++] = 0x00; f[n++] = 0x00; f[n++] = 0xFF; f[n++] = L; f[n++] = (uint8_t)(~L + 1); f[n++] = 0xD4;
  for (int i = 0; i < len; i++) { f[n++] = cmd[i]; sum += cmd[i]; }
  f[n++] = (uint8_t)(~sum + 1); f[n++] = 0x00;
  Wire.beginTransmission(PN_ADDR);
  Wire.write(f, n);
  return Wire.endTransmission() == 0;
}

static bool pnReady() {
  if (Wire.requestFrom((uint8_t)PN_ADDR, (uint8_t)1) != 1) return false;
  return Wire.read() & 1;
}

static bool pnWait(uint32_t tmo) {
  uint32_t t0 = millis();
  while (millis() - t0 < tmo) { if (pnReady()) return true; delay(1); }
  return false;
}

static bool pnAck(uint32_t tmo) {
  if (!pnWait(tmo)) return false;
  uint8_t b[7];
  if (Wire.requestFrom((uint8_t)PN_ADDR, (uint8_t)7) != 7) return false;
  for (int i = 0; i < 7; i++) b[i] = Wire.read();
  static const uint8_t ACK[6] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00 };
  return (b[0] & 1) && !memcmp(b + 1, ACK, 6);
}

// забрать ответ (PN532 уже готов): данные после D5 <cmd+1>; -1 — ошибка
static int pnRead(uint8_t cmd, const uint8_t*& data) {
  int got = Wire.requestFrom((uint8_t)PN_ADDR, (uint8_t)(PN_BUF + 1));
  if (got < 9) return -1;
  for (int i = 0; i < got; i++) s_rx[i] = Wire.read();
  if (!(s_rx[0] & 1)) return -1;
  int i = 1;
  while (i < 4 && s_rx[i] == 0x00 && s_rx[i + 1] == 0x00) i++;          // преамбула
  if (s_rx[i] != 0x00 || s_rx[i + 1] != 0xFF) return -1;
  uint8_t L = s_rx[i + 2];
  if ((uint8_t)(L + s_rx[i + 3]) != 0 || L < 2) return -1;
  if (s_rx[i + 4] != 0xD5 || s_rx[i + 5] != (uint8_t)(cmd + 1)) return -1;
  int len = L - 2, avail = got - (i + 6) - 2;                          // минус DCS и постамбула
  s_trunc = len > avail;
  if (s_trunc) len = max(0, got - (i + 6));
  data = s_rx + i + 6;
  return len;
}

static int pnCmd(const uint8_t* cmd, int len, const uint8_t*& data, uint32_t tmo = 120) {
  if (!pnWrite(cmd, len) || !pnAck(40)) return -1;
  if (!pnWait(tmo)) { static const uint8_t ACK[6] = { 0, 0, 0xFF, 0, 0xFF, 0 }; Wire.beginTransmission(PN_ADDR); Wire.write(ACK, 6); Wire.endTransmission(); return -1; }
  return pnRead(cmd[0], data);
}

// ---------- включение ----------
bool nfcStart() {
  NfcModel& m = g_nfc;
  if (m.state != NFC_OFF) return m.state == NFC_SCAN;
  crumb("nfc start");
  pinMode(PN_RST, OUTPUT);
  digitalWrite(PN_RST, LOW); delay(3);
  digitalWrite(PN_RST, HIGH); delay(40);
  const uint8_t* d; int n = -1;
  static const uint8_t FW[] = { 0x02 };
  for (int k = 0; k < 3 && n < 4; k++) { n = pnCmd(FW, 1, d, 100); if (n < 4) delay(15); }   // первая посылка будит PN532
  if (n < 4) {
    m.state = NFC_NOREADER; m.gen++;
    digitalWrite(PN_RST, LOW);
    return false;
  }
  m.fw = (uint32_t)d[0] << 16 | d[1] << 8 | d[2];
  static const uint8_t SAM[] = { 0x14, 0x01, 0x14, 0x00 };             // обычный режим
  pnCmd(SAM, sizeof(SAM), d);
  static const uint8_t RETRY[] = { 0x32, 0x05, 0xFF, 0x01, 0x02 };     // искать карту коротко (2 попытки) — не висеть
  pnCmd(RETRY, sizeof(RETRY), d);
  m.state = NFC_SCAN; m.present = false;
  s_poll = 0; s_pollMs = 0; s_fails = 0;
  return true;
}

void nfcStop() {
  NfcModel& m = g_nfc;
  if (m.state == NFC_OFF) return;
  crumb("nfc stop");
  if (m.state == NFC_SCAN) {
    const uint8_t* d;
    static const uint8_t RF_OFF[] = { 0x32, 0x01, 0x00 };
    pnCmd(RF_OFF, sizeof(RF_OFF), d);
  }
  digitalWrite(PN_RST, LOW);                                             // спать (почти не ест батарею)
  m.state = NFC_OFF; m.present = false;
}

// ---------- карта нашлась: дочитать то, что она открыто рассказывает ----------
static void inspectA(NfcCard& c) {
  const uint8_t* d; int n;
  if (c.sak & 0x20) {                                                    // ISO-DEP: платёжная? (только «да/нет»)
    static const uint8_t PPSE[] = { 0x40, 0x01, 0x00, 0xA4, 0x04, 0x00, 0x0E,
      '2', 'P', 'A', 'Y', '.', 'S', 'Y', 'S', '.', 'D', 'D', 'F', '0', '1', 0x00 };
    n = pnCmd(PPSE, sizeof(PPSE), d, 200);
    if (n >= 3 && d[0] == 0x00) {
      bool ok = d[1] == 0x6F || (!s_trunc && d[n - 2] == 0x90 && d[n - 1] == 0x00);
      c.bank = ok;
    }
    memset(s_rx, 0, sizeof(s_rx));                                       // ответ карты не храним
  }
  if (c.sak == 0x00 && (c.atqa & 0xFF) == 0x44) {                        // Ultralight / NTAG: открытая память
    static uint8_t mem[160];
    int got = 0;
    uint8_t rd[4] = { 0x40, 0x01, 0x30, 3 };
    n = pnCmd(rd, 4, d);
    if (n >= 17 && d[0] == 0x00 && d[1] == 0xE1) {                       // CC: метка размечена под NDEF
      for (int pg = 4; got + 16 <= (int)sizeof(mem); pg += 4) {
        rd[3] = pg;
        n = pnCmd(rd, 4, d);
        if (n < 17 || d[0] != 0x00) break;
        memcpy(mem + got, d + 1, 16); got += 16;
        bool end = false;
        for (int i = 1; i < 17; i++) if (d[i] == 0xFE) end = true;
        if (end && got >= 32) break;
      }
      if (got) nfcParseNdef(mem, got, c);
    }
    static const uint8_t VER[] = { 0x42, 0x60 };                         // GET_VERSION: NTAG213/215/216?
    n = pnCmd(VER, sizeof(VER), d);
    nfcClassify(c, (n >= 9 && d[0] == 0x00 && d[1] == 0x00 && d[2] == 0x04) ? d[7] : -1);
    return;
  }
  nfcClassify(c, -1);
}

static bool sameUid(const NfcCard& a, const NfcCard& b) {
  return a.uidLen == b.uidLen && !memcmp(a.uid, b.uid, a.uidLen) && a.felica == b.felica;
}

static void gotTarget(const uint8_t* d, int n, bool felica, uint32_t now) {
  NfcModel& m = g_nfc;
  NfcCard c; memset(&c, 0, sizeof(c));
  if (felica) {
    if (n < 12 || d[3] != 0x01) return;
    c.felica = true; c.uidLen = 8; memcpy(c.uid, d + 4, 8);
  } else {
    if (n < 6) return;
    c.atqa = (uint16_t)d[2] << 8 | d[3]; c.sak = d[4];
    c.uidLen = min((int)d[5], 10);
    if (6 + c.uidLen > n) return;
    memcpy(c.uid, d + 6, c.uidLen);
    c.isoDep = c.sak & 0x20;
  }
  if (m.present && m.have && sameUid(c, m.last)) { m.lastSeen = now; return; }   // та же карта всё ещё лежит
  crumb("nfc card");
  if (felica) nfcClassify(c, -1); else inspectA(c);
  nfcCommit(c, millis());
}

void nfcTick(uint32_t now) {
  NfcModel& m = g_nfc;
  if (m.state != NFC_SCAN) return;
  if (m.present && msSince(now, m.lastSeen) > 900) m.present = false;
  if (s_poll == 0) {
    if (msSince(now, s_pollMs) < 120) return;
    crumb("nfc poll");
    bool fe = (++s_cycle % 3) == 0;
    static const uint8_t PA[] = { 0x4A, 0x01, 0x00 };
    static const uint8_t PF[] = { 0x4A, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0x01, 0x00 };
    bool ok = fe ? pnWrite(PF, sizeof(PF)) : pnWrite(PA, sizeof(PA));
    if (!ok || !pnAck(30)) {
      s_pollMs = now;
      if (++s_fails > 8) { m.state = NFC_NOREADER; m.gen++; digitalWrite(PN_RST, LOW); }   // пропал с шины
      return;
    }
    s_fails = 0;
    s_poll = fe ? 2 : 1; s_pollMs = now;
    return;
  }
  // ждём ответ, не блокируя loop()
  if (!pnReady()) {
    if (msSince(now, s_pollMs) > 400) {                                  // молчит — отменить и начать заново
      static const uint8_t ACK[6] = { 0, 0, 0xFF, 0, 0xFF, 0 };
      Wire.beginTransmission(PN_ADDR); Wire.write(ACK, 6); Wire.endTransmission();
      s_poll = 0; s_pollMs = now;
    }
    return;
  }
  bool fe = s_poll == 2;
  s_poll = 0; s_pollMs = now;
  const uint8_t* d;
  int n = pnRead(0x4A, d);
  if (n >= 1 && d[0] >= 1) gotTarget(d, n, fe, now);
}

// {"fam":"MIFARE Classic 1K","hint":"..","uid":"04A2••","atqa":"0044","sak":"08","ndef":[".."],"t":123456}
// без считывателя: {"err":"no reader","t":..}
void nfcJson(String& o) {
  const NfcModel& m = g_nfc;
  o.reserve(320);
  if (m.state == NFC_NOREADER) { o = "{\"err\":\"no reader\",\"t\":"; o += millis(); o += '}'; return; }
  if (!m.have) { o = "{\"fam\":\"\",\"t\":0}"; return; }
  const NfcCard& c = m.last;
  char u[24], h[8];
  nfcUidMasked(c, u, sizeof(u), true);
  o = "{\"fam\":"; jsonStr(o, c.fam);
  o += ",\"hint\":"; jsonStr(o, c.hint);
  o += ",\"uid\":"; jsonStr(o, u);
  snprintf(h, sizeof h, "%04X", c.atqa); o += ",\"atqa\":\""; o += c.felica ? "" : h; o += '"';
  snprintf(h, sizeof h, "%02X", c.sak);  o += ",\"sak\":\""; o += c.felica ? "" : h; o += '"';
  o += ",\"ndef\":[";
  for (int i = 0; i < c.nNdef; i++) { if (i) o += ','; jsonStr(o, c.ndef[i]); }
  o += "],\"present\":"; o += m.present ? 1 : 0;
  o += ",\"t\":"; o += c.t; o += '}';
}

// ---------- раздел ----------
static void nfcSubtitle(char* out, size_t n) {
  if (g_nfc.state == NFC_NOREADER) snprintf(out, n, "no chip");
  else if (g_nfc.taps) snprintf(out, n, "%lu cards", (unsigned long)g_nfc.taps);
  else snprintf(out, n, "what card?");
}
static void nfcEnter() { nfcStart(); }
static void nfcLeave() { nfcStop(); }
static void nfcClick() {                                                 // нет считывателя — попробовать снова
  if (g_nfc.state == NFC_NOREADER) { g_nfc.state = NFC_OFF; nfcStart(); }
}

const Section nfcSection = {
  "NFC", 120, 230, 160, iconCard, nfcSubtitle,
  nfcEnter, nfcLeave,
  0, nullptr, nullptr, nullptr, nullptr,
  nfcClick, nullptr, nullptr, nfcTick, drawNfc,
};
