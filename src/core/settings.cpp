// ============================================================
//  settings.cpp — настройки и их экран
// ============================================================
#include <Preferences.h>
#include "core/settings.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"
#include "core/battery.h"
#include "core/ui.h"
#include "audio/audio.h"

int g_range = 0;
int g_thresh = 7;
int g_ledBright = 50;
int g_scrBright = 100;
int g_dimIdx = 0;
int g_sndIdx = 1;                          // тихие звуки интерфейса                          // по умолчанию экран не гаснет

static int      g_setItem = 0;           // выбранный пункт
static bool     g_setDirty = false;
static uint32_t g_setChangedMs = 0;
static Preferences g_prefs;
static const int N_SET_ITEMS = 7;        // последний пункт — батарея (только показывает)

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
  g_dimIdx    = g_prefs.getInt("dim", 0);
  g_sndIdx    = g_prefs.getInt("snd", 1);
  g_prefs.end();
  if (g_range < 0 || g_range >= N_RANGES) g_range = 0;
  if (g_thresh < 0 || g_thresh > 30) g_thresh = 7;
  if (g_ledBright < 10 || g_ledBright > 100) g_ledBright = 50;
  if (g_scrBright < 20 || g_scrBright > 100) g_scrBright = 100;
  if (g_dimIdx < 0 || g_dimIdx >= N_DIMS) g_dimIdx = 0;
  if (g_sndIdx < 0 || g_sndIdx > 2) g_sndIdx = 1;
}

void saveSettings() {
  g_prefs.begin("radar", false);
  g_prefs.putInt("range", g_range);
  g_prefs.putInt("thresh", g_thresh);
  g_prefs.putInt("ledbr", g_ledBright);
  g_prefs.putInt("scrbr", g_scrBright);
  g_prefs.putInt("dim", g_dimIdx);
  g_prefs.putInt("snd", g_sndIdx);
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
    case 4: g_dimIdx = constrain(g_dimIdx + dir, 0, N_DIMS - 1); break;
    case 5: g_sndIdx = constrain(g_sndIdx + dir, 0, 2); audioBeep(1400, 60); break;
    default: return;                       // батарея — только показывает
  }
  applyBrightness();
  if (g_setItem == 2) { ledsEvent(+1); }   // показать яркость ленты
  g_setDirty = true; g_setChangedMs = millis();
}

void drawSettings() {
  int W = spr.width();
  spr.fillSprite(C_BG());

  static const char* const names[N_SET_ITEMS] = { "Range", "LED events", "LED brightness", "Screen", "Auto dim", "Sounds", "Battery" };
  static const char* const hints[N_SET_ITEMS] = {
    "bluetooth: ignore weaker",
    "leds flash on count jump",
    "led strip brightness",
    "screen backlight",
    "dim when idle",
    "clicks + find ticks",
    "",
  };
  char batLine[64]; batSummary(batLine, sizeof(batLine));
  uiHeader("Settings", g_setItem == 6 ? batLine : hints[g_setItem]);   // подсказка к выбранному пункту
  const int rh = 18;
  for (int i = 0; i < N_SET_ITEMS; i++) {
    int y = UI_TOP + 2 + i * rh, cy = y + rh / 2 - 1;
    bool sel = (i == g_setItem);
    if (sel) uiRowHighlight(y, rh - 2);
    char val[32];
    bool alert = false;
    switch (i) {
      case 0:
        if (g_range == 0) sprintf(val, "all");
        else sprintf(val, "%s %d dBm", RANGES[g_range].name, RANGES[g_range].dbm);
        break;
      case 1: if (g_thresh == 0) sprintf(val, "off"); else sprintf(val, "+/- %d", g_thresh); break;
      case 2: sprintf(val, "%d%%", g_ledBright); break;
      case 3: sprintf(val, "%d%%", g_scrBright); break;
      case 4:
        if (DIM_SECS[g_dimIdx] == 0) sprintf(val, "off");
        else if (DIM_SECS[g_dimIdx] < 60) sprintf(val, "%d s", DIM_SECS[g_dimIdx]);
        else sprintf(val, "%d min", DIM_SECS[g_dimIdx] / 60);
        break;
      case 5: sprintf(val, "%s", g_sndIdx == 0 ? "off" : g_sndIdx == 1 ? "quiet" : "loud"); break;
      default:
        if (g_bat.ok) { sprintf(val, "%d%%  %.2f V", g_bat.soc, g_bat.mv / 1000.0f); alert = g_bat.soc <= 20; }
        else          sprintf(val, "n/a");
        break;
    }
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(sel ? C_HOT() : UI_LABEL());
    txt(names[i], UI_M + 8, cy, 2);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(alert ? C_ALERT() : (sel ? C_ACC() : C_HOT()));
    txt(val, W - UI_M - 8, cy, 2);
    if (sel && i < 6) {                           // стрелка «крути колесо»
      int vw = txtW(val, 2);
      spr.setTextColor(UI_DIM());
      txt("<", W - UI_M - 16 - vw, cy, 2);
    }
  }
  uiFooter("wheel: change   click: next", "side: back");
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
