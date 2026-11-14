// ============================================================
//  menu.cpp — главное меню и навигация
//
//  Три уровня: главное меню (карточки разделов) -> подменю раздела
//  (список экранов с живыми цифрами) -> экран. Разделы описаны
//  таблицами Section, меню с ними работает одинаково.
// ============================================================
#include <ctype.h>
#include "core/menu.h"
#include "core/theme.h"
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
  drawSubmenu(title, C(s->r, s->g, s->b), items, n, s_subSel);
}

// главное меню — список, как на старых приборах: курсор «>», выбранная строка залита
static void drawMainMenu() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C(150, 150, 165));
  txt("RADIO RADAR", 6, 3, 2);
  drawBatteryIcon(W - 6, 4);
  spr.drawFastHLine(6, 22, W - 12, C(70, 70, 82));

  const int y0 = 28, rh = 31;
  for (int i = 0; i < N_SECTIONS; i++) {
    const Section* s = SECTIONS[i];
    bool sel = (i == s_menuSel);
    int y = y0 + i * rh, cy = y + (rh - 3) / 2;
    uint16_t acc = C(s->r, s->g, s->b);
    if (sel) spr.fillRect(6, y, W - 12, rh - 3, acc);
    char name[20] = "> ";
    size_t k = 2;
    for (size_t t = 0; s->title[t] && k < sizeof(name) - 1; t++) name[k++] = (char)toupper((unsigned char)s->title[t]);
    name[k] = 0;
    if (!sel) name[0] = ' ';
    char sub[24];
    s->subtitle(sub, sizeof(sub));
    uint16_t tc = sel ? C_BG() : C(175, 175, 190);
    spr.setTextColor(tc);
    spr.setTextDatum(ML_DATUM);
    txt(name, 10, cy, 4);
    spr.setTextColor(sel ? C_BG() : C(120, 120, 135));
    spr.setTextDatum(MR_DATUM);
    txt(sub, W - 12, cy, 2);
  }
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(C(80, 80, 92));
  txt("wheel: choose   click: open   hold side: power off", W / 2, H - 2, 1);
  present();
}

void navDraw(uint32_t now) {
  const Section* s = cur();
  if (!s)         drawMainMenu();
  else if (s_sub) drawSectionSub(s);
  else            s->draw(now);
}
