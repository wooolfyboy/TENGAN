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
#include "audio/audio.h"

// порядок карточек в главном меню
static const Section* const SECTIONS[] = { &bleSection, &wifiSection, &subghzSection, &soundSection, &settingsSection };
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

// подпись раздела -> крупное значение + мелкая подпись («23 devices» -> «23» / «DEVICES»)
static void splitSubtitle(const char* sub, char* big, char* small, size_t n) {
  const char* colon = strchr(sub, ':');
  if (isdigit((unsigned char)sub[0])) {
    const char* sp = strchr(sub, ' ');
    size_t l = sp ? (size_t)(sp - sub) : strlen(sub);
    snprintf(big, n, "%.*s", (int)l, sub);
    snprintf(small, n, "%s", sp ? sp + 1 : "");
  } else if (colon) {
    snprintf(small, n, "%.*s", (int)(colon - sub), sub);
    const char* v = colon + 1; while (*v == ' ') v++;
    snprintf(big, n, "%s", v);
  } else {
    snprintf(big, n, "--");
    snprintf(small, n, "%s", sub);
  }
  for (char* p = big; *p; p++) *p = (char)toupper((unsigned char)*p);
  for (char* p = small; *p; p++) *p = (char)toupper((unsigned char)*p);
}

// главное меню HUD: четыре скошенные панели, сверху статус, снизу телеметрия
static void drawMainMenuHud() {
  int W = spr.width(), H = spr.height();
  uint16_t bg = C_BG(), acc = C_ACC(), steel = C(150, 150, 165), dim = C(110, 110, 125);
  spr.fillSprite(bg);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(dim);
  txt("// RADIO RADAR - SYS ONLINE", 8, 5, 1);
  // заряд: «PWR 87%» и пять сегментов
  char buf[32];
  if (g_bat.ok) {
    snprintf(buf, sizeof(buf), "PWR %d%%", g_bat.soc);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(g_bat.soc <= 20 ? C_ALERT() : acc);
    txt(buf, W - 44, 5, 1);
    for (int i = 0; i < 5; i++) {
      bool on = g_bat.soc > i * 20;
      spr.fillRect(W - 40 + i * 7, 5, 5, 8, on ? (g_bat.soc <= 20 ? C_ALERT() : acc) : C(40, 40, 50));
    }
  }
  for (int x = 8; x < W - 8; x++) {                     // линия, угасающая вправо
    float k = 1.0f - (float)(x - 8) / (W - 16);
    uint16_t c = acc;
    uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
    spr.drawPixel(x, 18, tft.color565(r * k, g * k, b * k));
  }

  const int gap = 5, cw = (W - 12 - gap * (N_SECTIONS - 1)) / N_SECTIONS, ch = 128, y0 = 24;
  int x0 = (W - (cw * N_SECTIONS + gap * (N_SECTIONS - 1))) / 2;
  for (int i = 0; i < N_SECTIONS; i++) {
    const Section* s = SECTIONS[i];
    bool sel = (i == s_menuSel);
    int x = x0 + i * (cw + gap), cx = x + cw / 2;
    uint16_t pbg = sel ? tft.color565(14, 42, 52) : bg;
    if (sel) hudPanel(x, y0, cw, ch, 12, pbg, true, bg);
    hudPanel(x, y0, cw, ch, 12, sel ? acc : C(45, 60, 72), false, bg);
    s->icon(cx, y0 + 30, sel ? acc : C(110, 110, 125), pbg);
    char title[16]; size_t k = 0;
    for (; s->title[k] && k < sizeof(title) - 1; k++) title[k] = (char)toupper((unsigned char)s->title[k]);
    title[k] = 0;
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(sel ? C_HOT() : steel);
    txt(title, cx, y0 + 62, txtW(title, 2) > cw - 4 ? 1 : 2);   // длинное название — мельче
    char sub[24], big[16], small[16];
    s->subtitle(sub, sizeof(sub));
    splitSubtitle(sub, big, small, sizeof(big));
    bool alertish = (strcmp(small, "NO CHIP") == 0);
    spr.setTextColor(alertish ? C_ALERT() : (sel ? acc : steel));
    txt(big, cx, y0 + 82, 4);
    spr.setTextColor(dim);
    char sm[16]; snprintf(sm, sizeof(sm), "%.*s", cw / 6, small);   // сколько влезает
    txt(sm, cx, y0 + 110, 1);
  }

  // телеметрия: время работы
  uint32_t t = millis() / 1000;
  snprintf(buf, sizeof(buf), "T+%02lu:%02lu:%02lu", (unsigned long)(t / 3600), (unsigned long)(t / 60 % 60), (unsigned long)(t % 60));
  spr.setTextDatum(BL_DATUM);
  spr.setTextColor(dim);
  txt("CLICK: OPEN   HOLD SIDE: OFF", 8, H - 4, 1);
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(acc);
  txt(buf, W - 8, H - 4, 1);
  present();
}

static void drawMainMenu() { drawMainMenuHud(); }

void navDraw(uint32_t now) {
  const Section* s = cur();
  if (!s)         drawMainMenu();
  else if (s_sub) drawSectionSub(s);
  else            s->draw(now);
}
