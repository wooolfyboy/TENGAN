// ============================================================
//  settings.cpp — настройки и их экран
// ============================================================
#include <Preferences.h>
#include "core/settings.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"
#include "core/battery.h"

int g_range = 0;
int g_thresh = 7;
int g_ledBright = 50;
int g_scrBright = 100;

static int      g_setItem = 0;           // выбранный пункт
static bool     g_setDirty = false;
static uint32_t g_setChangedMs = 0;
static Preferences g_prefs;
static const int N_SET_ITEMS = 6;        // последний пункт — батарея (только показывает)
static const int SET_ROWS = 5;           // строк на экране (список прокручивается)

// ---------- настройки ----------
void applyBrightness() {
  strip.setBrightness(g_ledBright * 255 / 100);
  analogWrite(TFT_BL, g_scrBright * 255 / 100);
}

void loadSettings() {
  g_prefs.begin("radar", true);
  g_range     = g_prefs.getInt("range", 0);
  g_thresh    = g_prefs.getInt("thresh", 7);
  g_ledBright = g_prefs.getInt("ledbr", 50);
  g_scrBright = g_prefs.getInt("scrbr", 100);
  g_theme     = g_prefs.getInt("theme", THEME_PHOSPHOR);
  g_prefs.end();
  if (g_range < 0 || g_range >= N_RANGES) g_range = 0;
  if (g_thresh < 0 || g_thresh > 30) g_thresh = 7;
  if (g_ledBright < 10 || g_ledBright > 100) g_ledBright = 50;
  if (g_scrBright < 20 || g_scrBright > 100) g_scrBright = 100;
  if (g_theme < 0 || g_theme >= N_THEMES) g_theme = THEME_PHOSPHOR;
}

void saveSettings() {
  g_prefs.begin("radar", false);
  g_prefs.putInt("range", g_range);
  g_prefs.putInt("thresh", g_thresh);
  g_prefs.putInt("ledbr", g_ledBright);
  g_prefs.putInt("scrbr", g_scrBright);
  g_prefs.putInt("theme", g_theme);
  g_prefs.end();
  g_setDirty = false;
}

void settingsSaveNow() { if (g_setDirty) saveSettings(); }
void settingsAutosave(uint32_t now) { if (g_setDirty && now - g_setChangedMs > 3000) saveSettings(); }
void settingsBegin() { loadSettings(); themeApply(); applyBrightness(); }

void settingsAdjust(int dir) {
  switch (g_setItem) {
    case 0: g_range = constrain(g_range + dir, 0, N_RANGES - 1); break;
    case 1: g_thresh = constrain(g_thresh + dir, 0, 30); break;
    case 2: g_ledBright = constrain(g_ledBright + dir * 10, 10, 100); break;
    case 3: g_scrBright = constrain(g_scrBright + dir * 10, 20, 100); break;
    case 4: g_theme = (g_theme + dir + N_THEMES) % N_THEMES; themeApply(); break;
    default: return;                       // батарея — только показывает
  }
  applyBrightness();
  if (g_setItem == 2) { ledsEvent(+1); }   // показать яркость ленты
  g_setDirty = true; g_setChangedMs = millis();
}

void drawSettings() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char val[32];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C(120, 120, 135), C_BG());
  txt("SETTINGS", 4, 1, 2);

  const char* names[N_SET_ITEMS] = { "Range", "LED events", "LED bright", "Screen", "Theme", "Battery" };
  const char* hints[N_SET_ITEMS] = {
    "ignore devices weaker than this",
    "flash when count jumps by this",
    "brightness of the LED strip",
    "screen backlight",
    "phosphor: green CRT look   classic: original colors",
    "",
  };
  char batLine[64]; batSummary(batLine, sizeof(batLine));
  hints[5] = batLine;
  int first = constrain(g_setItem - (SET_ROWS - 1), 0, N_SET_ITEMS - SET_ROWS);
  for (int i = first; i < first + SET_ROWS; i++) {
    int y = 20 + (i - first) * 25;
    bool sel = (i == g_setItem);
    uint16_t bg = sel ? C(28, 38, 62) : C_BG();
    if (sel) spr.fillRoundRect(0, y - 2, W, 26, 4, bg);
    switch (i) {
      case 0:
        if (g_range == 0) sprintf(val, "all");
        else sprintf(val, "%s  %d dBm", RANGES[g_range].name, RANGES[g_range].dbm);
        break;
      case 1: if (g_thresh == 0) sprintf(val, "off"); else sprintf(val, "+-%d", g_thresh); break;
      case 2: sprintf(val, "%d%%", g_ledBright); break;
      case 3: sprintf(val, "%d%%", g_scrBright); break;
      case 4: sprintf(val, "%s", themeName(g_theme)); break;
      case 5:
        if (g_bat.ok) sprintf(val, "%d%%  %.2fV", g_bat.soc, g_bat.mv / 1000.0f);
        else          sprintf(val, "n/a");
        break;
    }
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(sel ? C_HOT() : C(150, 150, 165), bg);
    txt(names[i], 10, y, 4);
    // значение крупно, а если не влезает рядом с названием — помельче
    int room = W - 20 - txtW(names[i], 4) - 12;
    bool big = txtW(val, 4) <= room;
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(sel ? C(90, 220, 140) : C(110, 110, 125), bg);
    txt(val, W - 10, big ? y : y + 5, big ? 4 : 2);
  }
  spr.setTextDatum(BL_DATUM);
  spr.setTextColor(C(110, 110, 125), C_BG());
  txt(hints[g_setItem], 4, H - 12, 1);
  spr.setTextColor(C(80, 80, 92), C_BG());
  txt("wheel: change   click: next item   side: back", 4, H - 1, 1);
  present();
}

// ---------- раздел «Настройки» (без подменю) ----------
static void setSubtitle(char* out, size_t n) { snprintf(out, n, "range: %s", RANGES[g_range].name); }
static void setEnter() { g_setItem = 0; }
static void setLeave() { settingsSaveNow(); }
static void setClick() { g_setItem = (g_setItem + 1) % N_SET_ITEMS; }
static void setDraw(uint32_t) { drawSettings(); }

const Section settingsSection = {
  "Settings", 200, 160, 90, iconGear, setSubtitle,
  setEnter, setLeave,
  0, nullptr, nullptr, nullptr, nullptr,
  setClick, settingsAdjust, nullptr, nullptr, setDraw,
};
