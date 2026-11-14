// ============================================================
//  menu.cpp — главное меню и навигация
//
//  Три уровня: главное меню (карточки разделов) -> подменю раздела
//  (список экранов с живыми цифрами) -> экран. Разделы описаны
//  таблицами Section, меню с ними работает одинаково.
// ============================================================
#include <ctype.h>
#include "core/menu.h"
#include "core/battery.h"
#include "core/settings.h"
#include "ble/ble.h"
#include "wifi/wifi.h"
#include "subghz/subghz.h"

// порядок карточек в главном меню
static const Section* const SECTIONS[] = { &bleSection, &wifiSection, &subghzSection, &settingsSection };
static const int N_SECTIONS = sizeof(SECTIONS) / sizeof(SECTIONS[0]);

static int  s_cur = -1;          // открытый раздел; -1 — главное меню
static int  s_menuSel = 0;       // выбранная карточка
static bool s_sub = false;       // показано подменю
static int  s_subSel = 0;        // курсор в подменю

static const Section* cur() { return s_cur >= 0 ? SECTIONS[s_cur] : nullptr; }
bool navActive(const Section* s) { return cur() == s; }
bool navInSub() { return s_sub; }

void navBegin() { s_cur = -1; s_menuSel = 0; s_sub = false; }

static void enterSection(int i) {
  s_cur = i;
  const Section* s = cur();
  s->enter();
  s_sub = s->nScreens > 0;
  if (s_sub) s_subSel = s->screenNow();
}

static void goMain() {
  const Section* s = cur();
  if (s) s->leave();
  s_menuSel = s_cur;
  s_cur = -1;
  s_sub = false;
}

void navClick() {
  const Section* s = cur();
  if (!s)    { enterSection(s_menuSel); return; }
  if (s_sub) { s->open(s_subSel); s_sub = false; return; }
  if (s->click) s->click();
}

void navRotate(int dir) {
  const Section* s = cur();
  if (!s)    { s_menuSel = (s_menuSel + dir + N_SECTIONS) % N_SECTIONS; return; }
  if (s_sub) { s_subSel = (s_subSel + dir + s->nScreens) % s->nScreens; return; }
  if (s->rotate) s->rotate(dir);
}

void navToSub() {
  const Section* s = cur();
  if (!s || s->nScreens == 0) return;
  s_subSel = s->screenNow();
  s->close();
  s_sub = true;
}

void navBack() {
  const Section* s = cur();
  if (!s) return;
  if (!s_sub && s->nScreens > 0) {
    if (s->back && s->back()) return;      // раздел закрыл что-то своё (например, подробности)
    navToSub();
    return;
  }
  goMain();
}

void navTick(uint32_t now) {
  const Section* s = cur();
  if (s && s->tick) s->tick(now);
}

// ---------- рисование ----------
static void drawSectionSub(const Section* s) {
  SubItem items[8];
  int n = min(s->nScreens, 8);
  for (int i = 0; i < n; i++) s->screenInfo(i, items[i]);
  char title[16];
  size_t k = 0;
  for (; s->title[k] && k < sizeof(title) - 1; k++) title[k] = (char)toupper((unsigned char)s->title[k]);
  title[k] = 0;
  drawSubmenu(title, tft.color565(s->r, s->g, s->b), items, n, s_subSel);
}

static void drawMainMenu() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  // верхняя строка: название + батарея
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tft.color565(90, 90, 104), TFT_BLACK);
  spr.drawString("RADIO RADAR", 4, 3, 1);
  drawBatteryIcon(W - 4, 1);

  int cw = 74, ch = 124, gap = 6, x0 = (W - (cw * N_SECTIONS + gap * (N_SECTIONS - 1))) / 2, y0 = 16;
  for (int i = 0; i < N_SECTIONS; i++) {
    const Section* s = SECTIONS[i];
    uint16_t acc = tft.color565(s->r, s->g, s->b);
    int x = x0 + i * (cw + gap);
    bool sel = (i == s_menuSel);
    uint16_t c = sel ? acc : tft.color565(85, 85, 98);
    uint16_t bg = sel ? tft.color565(22, 26, 38) : TFT_BLACK;
    if (sel) {
      spr.fillRoundRect(x, y0, cw, ch, 10, bg);
      spr.drawRoundRect(x, y0, cw, ch, 10, acc);
      spr.drawRoundRect(x + 1, y0 + 1, cw - 2, ch - 2, 9, acc);
    } else {
      spr.drawRoundRect(x, y0, cw, ch, 10, tft.color565(40, 40, 50));
    }
    int ix = x + cw / 2;
    s->icon(ix, y0 + 40, c, bg);
    char sub[24];
    s->subtitle(sub, sizeof(sub));
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(sel ? TFT_WHITE : tft.color565(150, 150, 165), bg);
    spr.drawString(s->title, ix, y0 + 78, 2);
    spr.setTextColor(sel ? acc : tft.color565(90, 90, 104), bg);
    spr.drawString(sub, ix, y0 + 100, 1);
  }
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(tft.color565(80, 80, 92), TFT_BLACK);
  spr.drawString("wheel: choose   click: open   hold side: power off", W / 2, H - 2, 1);
  spr.pushSprite(0, 0);
}

void navDraw(uint32_t now) {
  const Section* s = cur();
  if (!s)         drawMainMenu();
  else if (s_sub) drawSectionSub(s);
  else            s->draw(now);
}
