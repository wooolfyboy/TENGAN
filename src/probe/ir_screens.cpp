// ============================================================
//  ir_screens.cpp — экран IR: протокол крупно, вероятная марка,
//  адрес/команда, последние нажатия и сама пачка импульсов.
// ============================================================
#include "probe/ir.h"
#include "core/lang.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/settings.h"
#include "core/timeutil.h"


// пульт с ИК-волнами (значок меню)
void iconRemote(int x, int y, uint16_t c, uint16_t bg) {
  spr.drawSmoothRoundRect(x - 9, y - 10, 5, 3, 18, 33, c, bg);
  spr.fillSmoothCircle(x, y - 3, 3, c, bg);
  for (int r = 0; r < 3; r++) for (int k = -1; k <= 1; k++) spr.fillRect(x - 1 + k * 5, y + 6 + r * 5, 3, 3, c);
  for (int i = 0; i < 2; i++) {
    int r = 7 + i * 6;
    spr.drawArc(x, y - 13, r, r - 2, 135, 225, c, bg, true);   // волны вверх
  }
}

static void hex(char* b, size_t n, uint32_t v, int digits) { snprintf(b, n, "0x%0*lX", digits, (unsigned long)v); }

// пачка импульсов: вспышки — «высокий» уровень; развёртка пробегает слева направо после приёма
static void drawPulses(int x, int y, int w, int h, uint32_t now) {
  const IrModel& m = g_ir;
  uint16_t line = UI_LINE(), acc = C_ACC(), hot = C_HOT();
  uiFrame(x - 2, y - 2, w + 4, h + 4, line);
  if (m.rawN < 2) return;
  uint32_t total = 0;
  for (int i = 0; i < m.rawN; i++) total += m.raw[i];
  if (!total) return;
  float sc = (float)w / total;
  float sweep = constrain(msSince(now, m.last.t) / 450.0f, 0.0f, 1.0f);   // 0..1 — развёртка
  int sx = x + (int)(sweep * w);
  int yH = y + 3, yL = y + h - 3;
  float fx = x;
  int prevY = yL;
  for (int i = 0; i < m.rawN; i++) {
    bool mark = (i % 2) == 0;
    int x0 = (int)fx; fx += m.raw[i] * sc; int x1 = max(x0 + 1, (int)fx);
    if (x1 > x + w) x1 = x + w;
    int yy = mark ? yH : yL;
    uint16_t c = x0 < sx ? (mark ? hot : acc) : scale565(acc, 35);
    spr.drawFastVLine(x0, min(prevY, yy), abs(prevY - yy) + 1, c);
    spr.drawFastHLine(x0, yy, x1 - x0 + 1, c);
    prevY = yy;
  }
  if (sweep < 1) spr.drawFastVLine(sx, y, h, hot);   // курсор развёртки
}

// ожидание: пульт слева шлёт волны в «глаз» приёмника
static void drawIdle(uint32_t now) {
  int W = spr.width();
  int cy = 76, rx = W / 2 + 70;
  uint16_t acc = C_ACC();
  iconRemote(W / 2 - 90, cy + 6, UI_LABEL(), C_BG());
  for (int i = 0; i < 4; i++) {                              // волны бегут к приёмнику
    float ph = fmodf(now / 900.0f + i / 4.0f, 1.0f);
    int x = W / 2 - 70 + (int)(ph * 120);
    int r = 10 + (int)(ph * 6);
    uint16_t c = scale565(acc, (int)(100 * (1 - ph * 0.8f)));
    spr.drawArc(x, cy, r, r - 2, 225, 315, c, C_BG(), true);
  }
  // приёмник: купол; вспыхивает, когда волна доходит
  float pulse = fmodf(now / 900.0f, 0.25f) * 4;
  spr.fillSmoothCircle(rx, cy, 13, scale565(acc, 25 + (int)(pulse * 25)), C_BG());
  spr.drawSmoothCircle(rx, cy, 13, acc, C_BG());
  spr.fillSmoothCircle(rx, cy, 5, C_HOT(), C_BG());
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(UI_LABEL());
  txt(L("POINT ANY REMOTE AT TENGAN", "НАПРАВЬТЕ ПУЛЬТ НА TENGAN"), W / 2, 118, 1);
  spr.setTextColor(UI_DIM());
  txt(L("PRESS A BUTTON TO SEE WHOSE REMOTE IT IS", "НАЖМИТЕ КНОПКУ - УЗНАЕМ, ЧЕЙ ЭТО ПУЛЬТ"), W / 2, 132, 1);
}

void drawIr(uint32_t now) {
  const IrModel& m = g_ir;
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  if (m.on && !m.ok) {
    uiHeader(L("IR", "ИК"), L("error", "ошибка"), true);
    uiEmpty(L("receiver did not start", "приёмник не запустился"), L("go back and try again", "выйдите и попробуйте снова"));
    uiFooter("", L("side: back", "кнопка: назад"));
    present();
    return;
  }
  if (m.presses) snprintf(buf, sizeof(buf), "%lu %s", (unsigned long)m.presses, PL((int)m.presses, "press", "presses", "нажатие", "нажатия", "нажатий")); else snprintf(buf, sizeof(buf), "%s", L("listening", "слушаю"));
  uiHeader(L("IR · whose remote?", "ИК · чей пульт?"), buf, false);
  if (!m.have) {
    drawIdle(now);
    uiFooter(L("receive only", "только приём"), L("side: back", "кнопка: назад"));
    present();
    return;
  }
  const IrCode& c = m.last;
  bool fresh = msSince(now, c.t) < 400;
  // протокол крупно; при повторе — плашка HOLD
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(fresh ? C_HOT() : UI_LABEL());
  txt(c.proto, UI_M + 4, 26, 4);
  int pw = txtW(c.proto, 4);
  if (c.rep && msSince(now, c.t) < 300) {
    int bx = UI_M + 4 + pw + 8;
    hudPanel(bx, 30, 38, 15, 5, C_ACC(), true, C_BG());
    spr.setTextColor(C_BG());
    spr.setTextDatum(MC_DATUM);
    txt(L("HOLD", "ДЕРЖ"), bx + 19, 38, 1);
    spr.setTextDatum(TL_DATUM);
  }
  // марка: «вероятно»
  bool known = c.brand && strcmp(c.brand, IR_UNKNOWN) && strcmp(c.brand, IR_UNKNOWN_REMOTE) && strcmp(c.brand, IR_NEC_GENERIC);
  spr.setTextColor(C_ACC());
  char br[40]; fitText(c.brand ? c.brand : "", br, sizeof(br), 186, 2);
  txt(br, UI_M + 4, 54, 2);
  spr.setTextColor(UI_DIM());
  txt(known ? L("LIKELY · GUESSED BY ADDRESS", "ВЕРОЯТНО · ПО АДРЕСУ") : L("BRAND NOT IN TABLE", "МАРКИ НЕТ В ТАБЛИЦЕ"), UI_M + 4, 72, 1);
  // адрес / команда / биты
  bool raw = !strcmp(c.proto, "RAW");
  char a[16], k[16], b[8];
  if (raw) { snprintf(a, sizeof a, "--"); hex(k, sizeof k, c.cmd, 8); }
  else { hex(a, sizeof a, c.addr, c.addr > 0xFF ? 4 : 2); hex(k, sizeof k, c.cmd, c.cmd > 0xFF ? 4 : 2); }
  snprintf(b, sizeof b, "%u", c.bits);
  uiReadout(UI_M + 4, 84, L("addr", "адрес"), a, C_HOT());
  uiReadout(UI_M + 66, 84, raw ? L("print", "отпечаток") : L("cmd", "команда"), k, C_HOT());
  uiReadout(UI_M + 152, 84, raw ? L("pulses", "импульсы") : L("bits", "биты"), b, UI_LABEL());

  // история нажатий справа
  int hx = 212, hy = 26;
  spr.drawFastVLine(hx - 6, 26, 80, UI_LINE());
  spr.setTextColor(UI_DIM());
  txt(L("LAST", "ПОСЛЕДНИЕ"), hx, hy, 1);
  for (int i = 0; i < m.histN && i < 7; i++) {
    const IrCode& h = m.hist[i];
    int y = hy + 12 + i * 11;
    uint32_t age = msSince(now, h.t) / 1000;
    if (h.proto[0] == 'R' && h.proto[1] == 'A') snprintf(buf, sizeof buf, "RAW %04lX", (unsigned long)(h.cmd & 0xFFFF));
    else snprintf(buf, sizeof buf, "%.7s %lX:%lX", h.proto, (unsigned long)h.addr, (unsigned long)h.cmd);
    char f[20]; fitText(buf, f, sizeof f, 74, 1);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(i == 0 ? C_HOT() : scale565(UI_LABEL(), 100 - i * 9));
    txt(f, hx, y, 1);
    snprintf(buf, sizeof buf, age < 60 ? L("%lus", "%luс") : L("%lum", "%luм"), (unsigned long)(age < 60 ? age : age / 60));
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(UI_DIM());
    txt(buf, W - UI_M - 2, y, 1);
  }
  spr.setTextDatum(TL_DATUM);
  drawPulses(UI_M + 4, 116, W - 2 * UI_M - 8, 30, now);
  uiFooter(L("click: clear", "нажать: очистить"), L("side: back", "кнопка: назад"));
  present();
}
