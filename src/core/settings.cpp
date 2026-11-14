// ============================================================
//  settings.cpp — настройки и их экран
// ============================================================
#include <Preferences.h>
#include "core/settings.h"
#include "core/display.h"
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
static const int N_SET_ITEMS = 5;        // 5-й пункт — батарея (только показывает)

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
  g_prefs.end();
  if (g_range < 0 || g_range >= N_RANGES) g_range = 0;
  if (g_thresh < 0 || g_thresh > 30) g_thresh = 7;
  if (g_ledBright < 10 || g_ledBright > 100) g_ledBright = 50;
  if (g_scrBright < 20 || g_scrBright > 100) g_scrBright = 100;
}

void saveSettings() {
  g_prefs.begin("radar", false);
  g_prefs.putInt("range", g_range);
  g_prefs.putInt("thresh", g_thresh);
  g_prefs.putInt("ledbr", g_ledBright);
  g_prefs.putInt("scrbr", g_scrBright);
  g_prefs.end();
  g_setDirty = false;
}

void settingsSaveNow() { if (g_setDirty) saveSettings(); }
void settingsAutosave(uint32_t now) { if (g_setDirty && now - g_setChangedMs > 3000) saveSettings(); }
void settingsBegin() { loadSettings(); applyBrightness(); }

void settingsAdjust(int dir) {
  switch (g_setItem) {
    case 0: g_range = constrain(g_range + dir, 0, N_RANGES - 1); break;
    case 1: g_thresh = constrain(g_thresh + dir, 0, 30); break;
    case 2: g_ledBright = constrain(g_ledBright + dir * 10, 10, 100); break;
    case 3: g_scrBright = constrain(g_scrBright + dir * 10, 20, 100); break;
  }
  applyBrightness();
  if (g_setItem == 2) { ledsEvent(+1); }   // показать яркость ленты
  g_setDirty = true; g_setChangedMs = millis();
}

void drawSettings() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char val[32];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("SETTINGS", 4, 1, 2);

  const char* names[N_SET_ITEMS] = { "Range", "LED events", "LED bright", "Screen", "Battery" };
  const char* hints[N_SET_ITEMS] = {
    "ignore devices weaker than this",
    "flash when count jumps by this",
    "brightness of the LED strip",
    "screen backlight",
    "",
  };
  char batLine[64]; batSummary(batLine, sizeof(batLine));
  hints[4] = batLine;
  for (int i = 0; i < N_SET_ITEMS; i++) {
    int y = 20 + i * 25;
    bool sel = (i == g_setItem);
    uint16_t bg = sel ? tft.color565(28, 38, 62) : TFT_BLACK;
    if (sel) spr.fillRoundRect(0, y - 2, W, 26, 4, bg);
    switch (i) {
      case 0:
        if (g_range == 0) sprintf(val, "all");
        else sprintf(val, "%s  %d dBm", RANGES[g_range].name, RANGES[g_range].dbm);
        break;
      case 1: if (g_thresh == 0) sprintf(val, "off"); else sprintf(val, "+-%d", g_thresh); break;
      case 2: sprintf(val, "%d%%", g_ledBright); break;
      case 3: sprintf(val, "%d%%", g_scrBright); break;
      case 4:
        if (g_bat.ok) sprintf(val, "%d%%  %.2fV", g_bat.soc, g_bat.mv / 1000.0f);
        else          sprintf(val, "n/a");
        break;
    }
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(sel ? TFT_WHITE : tft.color565(150, 150, 165), bg);
    spr.drawString(names[i], 10, y, 4);
    // значение крупно, а если не влезает рядом с названием — помельче
    int room = W - 20 - spr.textWidth(names[i], 4) - 12;
    bool big = spr.textWidth(val, 4) <= room;
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(sel ? tft.color565(90, 220, 140) : tft.color565(110, 110, 125), bg);
    spr.drawString(val, W - 10, big ? y : y + 5, big ? 4 : 2);
  }
  spr.setTextDatum(BL_DATUM);
  spr.setTextColor(tft.color565(110, 110, 125), TFT_BLACK);
  spr.drawString(hints[g_setItem], 4, H - 12, 1);
  spr.setTextColor(tft.color565(80, 80, 92), TFT_BLACK);
  spr.drawString("wheel: change   click: next item   side: back", 4, H - 1, 1);
  spr.pushSprite(0, 0);
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
