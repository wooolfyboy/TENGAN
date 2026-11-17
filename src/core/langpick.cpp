// ============================================================
//  langpick.cpp — first-boot language picker (English / Russian).
//  Shown while no language is saved (g_langAsk). Wheel = choose, click = confirm.
//  The highlighted option is applied live, so the labels change as you turn the wheel.
// ============================================================
#include "core/langpick.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/settings.h"
#include "core/lang.h"

static int s_sel = -1;                    // selected: 0 English, 1 Russian

static void ensure() { if (s_sel < 0) s_sel = g_lang ? 1 : 0; }

void langPickRotate(int dir) { ensure(); if (dir) s_sel = s_sel ? 0 : 1; g_lang = s_sel; }
void langPickClick() { ensure(); settingsSetLang(s_sel); s_sel = -1; }

void langPickDraw(uint32_t now) {
  ensure();
  g_lang = s_sel;                          // preview: the whole screen uses the highlighted language
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  uiHeader(L("LANGUAGE", "ЯЗЫК"), L("first start", "первое включение"));

  const char* names[2] = { "English", "Русский" };
  const char* subs[2]  = { "interface language", "язык интерфейса" };
  const int bw = 132, bh = 74, gap = 16, y = 46;
  int x0 = (W - (2 * bw + gap)) / 2;
  for (int i = 0; i < 2; i++) {
    int x = x0 + i * (bw + gap);
    bool on = i == s_sel;
    if (on) {
      spr.fillRect(x, y, bw, bh, C_SELBG());
      if (!themeKurenai()) uiFrame(x - 3, y - 3, bw + 6, bh + 6, C_ACC());
    } else uiFrame(x, y, bw, bh, UI_LINE());
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(on ? C_SELFG() : UI_LABEL());
    txt(names[i], x + bw / 2, y + bh / 2 - 8, 4);
    spr.setTextColor(on ? C_SELACC() : UI_DIM());
    txt(subs[i], x + bw / 2, y + bh / 2 + 18, 1);
    if (on && ((now / 400) & 1)) {        // blinking selection marker
      spr.fillTriangle(x + bw / 2 - 5, y + bh + 6, x + bw / 2 + 5, y + bh + 6, x + bw / 2, y + bh + 1, C_ACC());
    }
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(UI_DIM());
  txt(L("you can change it later: Settings > Language", "потом можно сменить: Настройки > Язык"), W / 2, H - 34, 1);
  uiFooter(L("wheel: choose", "колесо: выбор"), L("click: OK", "нажать: ОК"));
  present();
}
