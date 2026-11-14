// ============================================================
//  pc.cpp — раздел PC: приём строк с ноутбука по USB и сам раздел
// ============================================================
#include "core/timeutil.h"
#include "pc/pc.h"
#include "core/display.h"

PcStat g_pc;
int    g_pcPage = 0;
int      g_pcLastCmd = 0;
uint32_t g_pcLastCmdMs = 0;

static char   s_line[512];
static size_t s_len = 0;

static void parseList(const char* v, uint8_t* out, int& n) {
  n = 0;
  while (*v && n < PC_CORES) {
    out[n++] = (uint8_t)constrain(atoi(v), 0, 100);
    const char* c = strchr(v, ',');
    if (!c) break;
    v = c + 1;
  }
}

static void parseVol(char* s) {                      // "PV vol=35 mute=0" — громкость ноута
  char* save = nullptr;
  for (char* tok = strtok_r(s + 3, " ", &save); tok; tok = strtok_r(nullptr, " ", &save)) {
    if (!strncmp(tok, "vol=", 4))  g_pc.vol = constrain(atoi(tok + 4), 0, 100);
    if (!strncmp(tok, "mute=", 5)) g_pc.mute = atoi(tok + 5) != 0;
  }
  g_pc.volT = millis();
}

static void parseLine(char* s) {
  if (!strncmp(s, "PV ", 3)) { parseVol(s); return; }
  if (strncmp(s, "PC ", 3) != 0) return;            // не наша строка — пропускаем
  PcStat& p = g_pc;
  char* save = nullptr;
  for (char* tok = strtok_r(s + 3, " ", &save); tok; tok = strtok_r(nullptr, " ", &save)) {
    char* eq = strchr(tok, '=');
    if (!eq) continue;
    *eq = 0;
    const char* k = tok; const char* v = eq + 1;
    float f = atof(v);
    if      (!strcmp(k, "cpu"))  p.cpu = f;
    else if (!strcmp(k, "freq")) p.freq = f;
    else if (!strcmp(k, "c"))    parseList(v, p.core, p.nCores);
    else if (!strcmp(k, "ram"))  p.ram = f;
    else if (!strcmp(k, "ramu")) p.ramU = f;
    else if (!strcmp(k, "ramt")) p.ramT = f;
    else if (!strcmp(k, "dsk"))  p.disk = f;
    else if (!strcmp(k, "dr"))   p.dR = f;
    else if (!strcmp(k, "dw"))   p.dW = f;
    else if (!strcmp(k, "nd"))   p.nD = f;
    else if (!strcmp(k, "nu"))   p.nU = f;
    else if (!strcmp(k, "bat"))  p.bat = (int)f;
    else if (!strcmp(k, "plug")) p.plug = f > 0;
    else if (!strcmp(k, "up"))   p.up = (uint32_t)f;
    else if (!strcmp(k, "top"))  { strncpy(p.top, v, sizeof(p.top) - 1); p.top[sizeof(p.top) - 1] = 0; }
    else if (!strcmp(k, "host")) { strncpy(p.host, v, sizeof(p.host) - 1); p.host[sizeof(p.host) - 1] = 0; }
  }
  p.have = true;
  p.t = millis();
  if (p.histN < PC_HIST) { p.cpuH[p.histN] = p.cpu; p.ramH[p.histN] = p.ram; p.histN++; }
  else {
    memmove(p.cpuH, p.cpuH + 1, sizeof(float) * (PC_HIST - 1)); p.cpuH[PC_HIST - 1] = p.cpu;
    memmove(p.ramH, p.ramH + 1, sizeof(float) * (PC_HIST - 1)); p.ramH[PC_HIST - 1] = p.ram;
  }
}

void pcPoll(uint32_t) {
  int budget = 600;                                  // не застревать, если данных много
  while (Serial.available() && budget--) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (s_len) { s_line[s_len] = 0; parseLine(s_line); s_len = 0; }
    } else if (s_len < sizeof(s_line) - 1) s_line[s_len++] = c;
    else s_len = 0;                                  // слишком длинная строка — мусор
  }
}

// «связи нет» — только после 10 с тишины (Windows иногда притормаживает отправку)
bool pcLive(uint32_t now) { return g_pc.have && msSince(now, g_pc.t) < 10000; }

// ---------- пульт: команды ноутбуку ----------
// Программа на ноуте читает строки «KEY ...» и нажимает мультимедийные клавиши.
static void pcSend(const char* cmd, int code) {
  g_pcLastCmd = code; g_pcLastCmdMs = millis();
  if (!pcLive(millis())) return;                     // программа не запущена — некому слать
  if (Serial.availableForWrite() < 16) return;       // не ждать, если ноут не читает
  Serial.print(cmd);
  Serial.print('\n');
}

// ---------- раздел ----------
static void pcSubtitle(char* out, size_t n) {
  if (pcLive(millis())) snprintf(out, n, "%d%% cpu", (int)g_pc.cpu);
  else                  snprintf(out, n, "usb laptop");
}
static void pcEnter() { g_pcPage = 0; }
static void pcLeave() {}
static void pcClick() {
  if (g_pcPage == 2) pcSend("KEY PLAY", 2);          // на странице музыки — пауза/играть
  else g_pcPage = (g_pcPage + 1) % PC_PAGES;
}
static void pcRotate(int dir) {
  if (g_pcPage == 2) { pcSend(dir > 0 ? "KEY VOLUP" : "KEY VOLDN", dir > 0 ? 1 : -1); return; }   // колесо = громкость
  static uint32_t last = 0;
  if (millis() - last < 150) return;
  last = millis();
  g_pcPage = (g_pcPage + dir + PC_PAGES) % PC_PAGES;
}
static bool pcBack() {                               // со страницы музыки боковая — на обзор, а не из раздела
  if (g_pcPage == 2) { g_pcPage = 0; return true; }
  return false;
}
static void pcDraw(uint32_t now) {
  if (!pcLive(now)) drawPcWaiting(now);
  else if (g_pcPage == 0) drawPc(now);
  else if (g_pcPage == 1) drawPcCores(now);
  else drawPcMedia(now);
}

const Section pcSection = {
  "PC", 120, 220, 200, iconLaptop, pcSubtitle,
  pcEnter, pcLeave,
  0, nullptr, nullptr, nullptr, nullptr,
  pcClick, pcRotate, pcBack, nullptr, pcDraw,
};
