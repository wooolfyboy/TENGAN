// ============================================================
//  sensors.cpp — таблица датчиков 433 МГц и анализатор (без железа:
//  этот файл собирает и симулятор). Приём — sens_rx.cpp.
// ============================================================
#include "subghz/sensors.h"
#include "core/timeutil.h"

SensRec  g_sens[SENS_MAX];
int      g_sensN = 0;
SensLast g_sensLast;
uint32_t g_sensGen = 0, g_sensPackets = 0, g_sensNoise = 0;

static const uint32_t OUTSIDE_MS = 10 * 60 * 1000;   // «за окном» — слышали в последние 10 минут

void sensClear() { g_sensN = 0; g_sensLast = SensLast(); g_sensGen++; }

static void upsert(const OokMsg& m, int rssi, uint32_t now) {
  int k = -1;
  for (int i = 0; i < g_sensN; i++)
    if (g_sens[i].proto == m.proto && g_sens[i].id == m.id && g_sens[i].ch == m.ch) { k = i; break; }
  if (k < 0) {
    if (g_sensN < SENS_MAX) k = g_sensN++;
    else {                                          // место кончилось — забываем самый давний
      k = 0;
      for (int i = 1; i < g_sensN; i++) if (msSince(now, g_sens[i].lastMs) > msSince(now, g_sens[k].lastMs)) k = i;
    }
    g_sens[k] = SensRec();
    g_sens[k].proto = m.proto; g_sens[k].id = m.id; g_sens[k].ch = m.ch;
    g_sens[k].firstMs = now;
  }
  SensRec& s = g_sens[k];
  s.kind = m.kind; s.t = m.t; s.h = m.h; s.bat = m.bat; s.btn = m.btn;
  s.rssi = (int8_t)constrain(rssi, -127, 0);
  s.lastMs = now;
  if (s.n < 65535) s.n++;
}

void sensFeed(const OokTrain& T, int rssi, uint32_t now) {
  static OokMsg m[6];
  int n = ookDecode(T, m, 6);
  for (int i = 0; i < n; i++) upsert(m[i], rssi, now);
  OokInfo info;
  ookAnalyze(T, info, m, n);
  if (strcmp(info.cls, "noise") == 0) { g_sensNoise++; if (!n) return; }
  g_sensPackets++;
  SensLast& L = g_sensLast;
  L.valid = true; L.info = info; L.rssi = (int8_t)constrain(rssi, -127, 0); L.ms = now;
  // рисунок посылки: у датчиков — целиком (до 48 пар), у остального — только начало
  int lim = strcmp(info.cls, "sensor") == 0 ? SENS_PT : 12;
  int np = T.n < lim ? T.n : lim;
  for (int i = 0; i < np; i++) { L.pt[i * 2] = T.p[i]; L.pt[i * 2 + 1] = T.g[i]; }
  if (np && L.pt[np * 2 - 1] > 4000) L.pt[np * 2 - 1] = 4000;   // хвост — не бесконечная тишина
  L.ptN = np;
  uint32_t d = 0; for (int i = 0; i < T.n; i++) d += T.p[i] + (i < T.n - 1 ? T.g[i] : 0);
  L.durUs = d;
  g_sensGen++;
}

bool sensOutside(float& t, int& h, const char*& src, uint32_t now) {
  int best = -1;
  for (int i = 0; i < g_sensN; i++) {
    const SensRec& s = g_sens[i];
    if (s.kind == OOK_REMOTE || msSince(now, s.lastMs) > OUTSIDE_MS) continue;
    if (best < 0 || s.rssi > g_sens[best].rssi || (s.rssi == g_sens[best].rssi && s.lastMs > g_sens[best].lastMs)) best = i;
  }
  if (best < 0) return false;
  t = g_sens[best].t; h = g_sens[best].h; src = g_sens[best].proto;
  return true;
}

int sensSorted(uint8_t* idx, int max, uint32_t now) {
  int n = 0;
  for (int i = 0; i < g_sensN && n < max; i++) idx[n++] = i;
  auto rank = [&](const SensRec& s) {             // термодатчики выше пультов, внутри — по силе сигнала
    return (s.kind == OOK_REMOTE ? 0 : 1000) + s.rssi + 128 - (msSince(now, s.lastMs) > OUTSIDE_MS ? 500 : 0);
  };
  for (int a = 0; a < n; a++)
    for (int b = a + 1; b < n; b++)
      if (rank(g_sens[idx[b]]) > rank(g_sens[idx[a]])) { uint8_t t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  return n;
}

void sensId(const SensRec& s, char* out, size_t n) {
  char p[20]; size_t i = 0;
  for (; i < sizeof(p) - 1 && s.proto[i]; i++) p[i] = tolower((unsigned char)s.proto[i]);
  p[i] = 0;
  if (s.kind == OOK_REMOTE) snprintf(out, n, "%s-%04lx", p, (unsigned long)(s.id & 0xFFFF));
  else snprintf(out, n, "%s-%d-0x%lx", p, s.ch, (unsigned long)s.id);
}

static void jsonOptF(String& o, const char* k, bool has, float v) {
  o += ",\""; o += k; o += "\":"; if (has) o += String(v, 1); else o += "null";
}

void sensJson(String& o, uint32_t now) {
  o.reserve(300 + g_sensN * 130 + SENS_PT * 12);
  o = "{\"on\":"; o += sensRxOn() ? 1 : 0;
  o += ",\"pk\":"; o += g_sensPackets;
  o += ",\"noise\":"; o += g_sensNoise;
  o += ",\"sensors\":[";
  uint8_t idx[SENS_MAX];
  int n = sensSorted(idx, SENS_MAX, now);
  char id[40];
  for (int k = 0; k < n; k++) {
    const SensRec& s = g_sens[idx[k]];
    if (k) o += ',';
    sensId(s, id, sizeof id);
    o += "{\"id\":\""; o += id; o += "\",\"p\":\""; o += s.proto; o += "\",\"ch\":"; o += s.ch;
    o += ",\"kind\":\""; o += s.kind == OOK_REMOTE ? "remote" : s.kind == OOK_TH ? "th" : "t"; o += '"';
    jsonOptF(o, "t", s.kind != OOK_REMOTE, s.t);
    o += ",\"h\":"; if (s.h >= 0) o += s.h; else o += "null";
    o += ",\"bat\":"; if (s.bat >= 0) o += s.bat; else o += "null";
    if (s.kind == OOK_REMOTE) { o += ",\"btn\":"; o += s.btn; }
    o += ",\"rssi\":"; o += s.rssi;
    o += ",\"ago\":"; o += msSince(now, s.lastMs) / 1000;
    o += ",\"n\":"; o += s.n;
    o += '}';
  }
  o += ']';
  const SensLast& L = g_sensLast;
  if (L.valid) {
    const OokInfo& I = L.info;
    char b[220];
    snprintf(b, sizeof b, ",\"last\":{\"p\":\"%s\",\"cls\":\"%s\",\"pulses\":%d,\"bits\":%d,\"rows\":%d,\"mod\":\"%s\",\"hex\":\"%s\","
             "\"short\":%u,\"long\":%u,\"gs\":%u,\"gl\":%u,\"gap\":%u,\"hash\":\"%04x\",\"rssi\":%d,\"ago\":%lu,\"us\":%lu,\"pt\":[",
             I.proto, I.cls, I.pulses, I.bits, I.rows, I.mod, I.hex, I.shortW, I.longW, I.shortG, I.longG,
             I.syncG ? I.syncG : I.longG, I.hash, L.rssi, (unsigned long)(msSince(now, L.ms) / 1000), (unsigned long)L.durUs);
    o += b;
    for (int i = 0; i < L.ptN * 2; i++) { if (i) o += ','; o += L.pt[i]; }
    o += "]}";
  } else o += ",\"last\":null";
  o += '}';
}
