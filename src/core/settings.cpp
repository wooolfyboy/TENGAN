// ============================================================
//  settings.cpp — настройки и их экран
// ============================================================
#include <Preferences.h>
#include "core/timeutil.h"
#include "core/settings.h"
#include "core/version.h"
#include "core/jsonutil.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"
#include "core/battery.h"
#include "core/ui.h"
#include "audio/audio.h"
#include "subghz/subghz_model.h"
#include "gallery/astro.h"

int g_range = 0;
int g_thresh = 7;
int g_ledBright = 50;
int g_scrBright = 100;
int g_dimIdx = 0;                        // по умолчанию экран не гаснет
int g_sndIdx = 1;                        // тихие звуки интерфейса
int g_clickEn = 1;
int g_theme = THEME_DENKO;                // оформление: DENKO 電光 (свет, HUD) / SUMI 墨 (тушь)
int g_micEn = 1;
int g_auSrc = 0;
int g_city = 0;
int g_tzOff = 10800;
int g_auLeds = 0;
int g_bleTicks = 1;
int g_wifiTicks = 1;
int g_wRange = 0;

static int      g_setPage = 0;           // страница: общие / раздел
static int      g_setItem = 0;           // выбранный пункт на странице
static bool     g_setDirty = false;
static uint32_t g_setChangedMs = 0;
static Preferences g_prefs;

// все пункты настроек; page — на какой странице показывать.
// Одна таблица на всё: экран палки (колесо) и планшет (/api/settings, /api/set) читают и меняют
// настройки через одни и те же getVal / setVal — логика не дублируется.
enum { S_THEME, S_SCREEN, S_DIM, S_LEDBR, S_VOLUME, S_CLICKS, S_CITY, S_BATTERY, S_VERSION,
       S_RANGE, S_LEDEV, S_BLETICK,
       S_WRANGE, S_WTICK,
       S_BAND,
       S_MIC, S_AULED, S_AUSRC, S_NUM };
enum { P_GENERAL, P_BLE, P_WIFI, P_SUBGHZ, P_SOUND, P_NUM };
enum { K_ENUM, K_RANGE, K_BOOL, K_INFO };                 // вид пункта (для планшета)
struct SetDef { const char* name; const char* hint; uint8_t page; const char* key; uint8_t kind; int16_t mn, mx, step; const char* unit; };
static const SetDef DEFS[S_NUM] = {
  { "Theme",          "look: denko light / sumi ink", P_GENERAL, "theme",      K_ENUM },
  { "Screen",         "screen backlight",            P_GENERAL, "screen",     K_RANGE, 20, 100, 10, "%" },
  { "Auto dim",       "dim screen when idle",        P_GENERAL, "dim",        K_ENUM },
  { "LED brightness", "led strip brightness",        P_GENERAL, "ledbr",      K_RANGE, 10, 100, 10, "%" },
  { "Volume",         "all sounds: off/quiet/loud",  P_GENERAL, "volume",     K_ENUM },
  { "Wheel clicks",   "click on every wheel step",   P_GENERAL, "clicks",     K_BOOL },
  { "City",           "for sky map and satellites",  P_GENERAL, "city",       K_ENUM },
  { "Battery",        "",                            P_GENERAL, "battery",    K_INFO },
  { "Version",        "TENGAN firmware",             P_GENERAL, "version",    K_INFO },
  { "Range",          "ignore weaker devices",       P_BLE,     "ble_range",  K_ENUM },
  { "LED events",     "leds flash on count jump",    P_BLE,     "ble_ledev",  K_RANGE, 0, 30, 1, "" },
  { "Find ticks",     "ticks on nearest screen",     P_BLE,     "ble_ticks",  K_BOOL },
  { "Range",          "hide weaker networks",        P_WIFI,    "wifi_range", K_ENUM },
  { "Find ticks",     "ticks on find screen",        P_WIFI,    "wifi_ticks", K_BOOL },
  { "Start band",     "band when section opens",     P_SUBGHZ,  "sub_band",   K_ENUM },
  { "Microphone",     "mic for sound section",       P_SOUND,   "mic",        K_BOOL },
  { "LED meter",      "leds show loudness",          P_SOUND,   "au_leds",    K_BOOL },
  { "Source",         "auto / mic / laptop audio",   P_SOUND,   "au_src",     K_ENUM },
};
static const char* const PAGE_NAMES[P_NUM] = { "General", "Bluetooth", "Wi-Fi", "Sub-GHz", "Sound" };
static const char* const PAGE_IDS[P_NUM]   = { "general", "ble", "wifi", "sub", "snd" };
static const char* const PAGE_DESCS[P_NUM] = { "theme, screen, leds, city", "range, leds, ticks",
                                               "range, ticks", "radio band", "source, mic, leds" };
static const int SET_ROWS = 7;           // строк на экране (список прокручивается)

// пункты текущей страницы
static int pageItems(int page, int* out) {
  int n = 0;
  for (int i = 0; i < S_NUM; i++) if (DEFS[i].page == page) out[n++] = i;
  return n;
}

// ---------- настройки ----------
bool g_screenOff = false;
void applyBrightness() {
  strip.setBrightness(g_ledBright * 255 / 100);
  analogWrite(TFT_BL, g_screenOff ? 0 : g_scrBright * 255 / 100);
}

void loadSettings() {
  g_prefs.begin("radar", true);
  g_range     = g_prefs.getInt("range", 0);
  g_thresh    = g_prefs.getInt("thresh", 7);
  g_ledBright = g_prefs.getInt("ledbr", 50);
  g_scrBright = g_prefs.getInt("scrbr", 100);
  g_dimIdx    = g_prefs.getInt("dim", 0);
  g_sndIdx    = g_prefs.getInt("snd", 1);
  g_micEn     = g_prefs.getInt("mic", 1) ? 1 : 0;
  g_clickEn   = g_prefs.getInt("click", 1) ? 1 : 0;
  g_theme     = g_prefs.getInt("theme", THEME_DENKO) == THEME_SUMI ? THEME_SUMI : THEME_DENKO;
  g_auLeds    = g_prefs.getInt("auled", 0) ? 1 : 0;
  g_auSrc     = constrain(g_prefs.getInt("ausrc", 0), 0, 2);
  g_gpsKnown  = g_prefs.getBool("gpsok", false);
  g_gpsCity.lat = g_prefs.getFloat("gpslat", 0); g_gpsCity.lon = g_prefs.getFloat("gpslon", 0);
  g_city      = constrain(g_prefs.getInt("city", 0), 0, cityCount() - 1);
  g_tzOff     = g_prefs.getInt("tz", 10800);
  g_bleTicks  = g_prefs.getInt("bltick", 1) ? 1 : 0;
  g_wifiTicks = g_prefs.getInt("wftick", 1) ? 1 : 0;
  g_wRange    = g_prefs.getInt("wrange", 0);
  g_band      = g_prefs.getInt("band", 1);
  g_prefs.end();
  if (g_range < 0 || g_range >= N_RANGES) g_range = 0;
  if (g_thresh < 0 || g_thresh > 30) g_thresh = 7;
  if (g_ledBright < 10 || g_ledBright > 100) g_ledBright = 50;
  if (g_scrBright < 20 || g_scrBright > 100) g_scrBright = 100;
  if (g_dimIdx < 0 || g_dimIdx >= N_DIMS) g_dimIdx = 0;
  if (g_sndIdx < 0 || g_sndIdx > 2) g_sndIdx = 1;
  if (g_wRange < 0 || g_wRange >= N_RANGES) g_wRange = 0;
  if (g_band < 0 || g_band >= N_BANDS) g_band = 1;
}

void saveSettings() {
  g_prefs.begin("radar", false);
  g_prefs.putInt("range", g_range);
  g_prefs.putInt("thresh", g_thresh);
  g_prefs.putInt("ledbr", g_ledBright);
  g_prefs.putInt("scrbr", g_scrBright);
  g_prefs.putInt("dim", g_dimIdx);
  g_prefs.putInt("snd", g_sndIdx);
  g_prefs.putInt("mic", g_micEn);
  g_prefs.putInt("click", g_clickEn);
  g_prefs.putInt("theme", g_theme);
  g_prefs.putInt("auled", g_auLeds);
  g_prefs.putInt("ausrc", g_auSrc);
  g_prefs.putInt("city", g_city);
  g_prefs.putBool("gpsok", g_gpsKnown); g_prefs.putFloat("gpslat", g_gpsCity.lat); g_prefs.putFloat("gpslon", g_gpsCity.lon);
  g_prefs.putInt("tz", g_tzOff);
  g_prefs.putInt("bltick", g_bleTicks);
  g_prefs.putInt("wftick", g_wifiTicks);
  g_prefs.putInt("wrange", g_wRange);
  g_prefs.putInt("band", g_band);
  g_prefs.end();
  g_setDirty = false;
}

void settingsSaveNow() { if (g_setDirty) saveSettings(); }
void settingsAutosave(uint32_t now) { if (g_setDirty && msSince(now, g_setChangedMs) > 3000) saveSettings(); }
void settingsBegin() { loadSettings(); applyBrightness(); }

static int curItem() {
  int ids[S_NUM]; int n = pageItems(g_setPage, ids);
  return n ? ids[constrain(g_setItem, 0, n - 1)] : -1;
}

// ---------- значения пунктов: одна логика для колеса и для планшета ----------
static const char* const VOL_NAMES[3] = { "off", "quiet", "loud" };
static const char* const SRC_NAMES[3] = { "auto", "mic", "laptop" };

static int optCount(int id) {                             // сколько вариантов у пункта-списка
  switch (id) {
    case S_THEME: return 2;
    case S_DIM: return N_DIMS;
    case S_VOLUME: return 3;
    case S_CITY: return cityCount();
    case S_RANGE: case S_WRANGE: return N_RANGES;
    case S_BAND: return N_BANDS;
    case S_AUSRC: return 3;
  }
  return 0;
}
static void optName(int id, int i, char* out, size_t n) {   // имя варианта (как на планшете)
  switch (id) {
    case S_THEME:  snprintf(out, n, "%s", i == THEME_SUMI ? "sumi" : "denko"); break;
    case S_DIM:    if (DIM_SECS[i] == 0) snprintf(out, n, "off"); else if (DIM_SECS[i] < 60) snprintf(out, n, "%d s", DIM_SECS[i]); else snprintf(out, n, "%d min", DIM_SECS[i] / 60); break;
    case S_VOLUME: snprintf(out, n, "%s", VOL_NAMES[i]); break;
    case S_CITY:   snprintf(out, n, "%s", i == N_CITIES ? g_gpsCity.name : CITIES[i].name); break;
    case S_RANGE: case S_WRANGE: snprintf(out, n, "%s", RANGES[i].name); break;
    case S_BAND:   snprintf(out, n, "%s", BANDS[i].name); break;
    case S_AUSRC:  snprintf(out, n, "%s", SRC_NAMES[i]); break;
    default: out[0] = 0;
  }
}
static int getVal(int id) {                               // индекс варианта / число / 0-1
  switch (id) {
    case S_THEME: return g_theme;          case S_SCREEN: return g_scrBright;   case S_DIM: return g_dimIdx;
    case S_LEDBR: return g_ledBright;      case S_VOLUME: return g_sndIdx;      case S_CLICKS: return g_clickEn;
    case S_CITY: return g_city;            case S_RANGE: return g_range;        case S_LEDEV: return g_thresh;
    case S_BLETICK: return g_bleTicks;     case S_WRANGE: return g_wRange;      case S_WTICK: return g_wifiTicks;
    case S_BAND: return g_band;            case S_MIC: return g_micEn;          case S_AULED: return g_auLeds;
    case S_AUSRC: return g_auSrc;
  }
  return 0;
}
// поставить значение: проверка границ и те же побочные действия, что при повороте колеса
static bool setVal(int id, int v) {
  const SetDef& d = DEFS[id];
  if (d.kind == K_INFO) return false;
  if (d.kind == K_ENUM)  v = constrain(v, 0, optCount(id) - 1);
  if (d.kind == K_BOOL)  v = v ? 1 : 0;
  if (d.kind == K_RANGE) v = constrain(v, d.mn, d.mx);
  switch (id) {
    case S_THEME:   g_theme = v ? THEME_SUMI : THEME_DENKO; break;
    case S_SCREEN:  g_scrBright = v; break;
    case S_DIM:     g_dimIdx = v; break;
    case S_LEDBR:   g_ledBright = v; break;
    case S_VOLUME:  g_sndIdx = v; audioBeep(1400, 60); break;
    case S_CLICKS:  g_clickEn = v; break;
    case S_CITY:    g_city = v; break;
    case S_RANGE:   g_range = v; break;
    case S_LEDEV:   g_thresh = v; break;
    case S_BLETICK: g_bleTicks = v; break;
    case S_WRANGE:  g_wRange = v; break;
    case S_WTICK:   g_wifiTicks = v; break;
    case S_BAND:    g_band = v; break;
    case S_MIC:     g_micEn = v; break;
    case S_AULED:   g_auLeds = v; if (!g_auLeds) ledsOff(); break;
    case S_AUSRC:   g_auSrc = v; break;
    default: return false;
  }
  applyBrightness();
  if (id == S_LEDBR) ledsEvent(+1);        // показать яркость ленты
  settingsTouch();
  return true;
}

void settingsAdjust(int dir) {
  int id = curItem();
  if (id < 0) return;
  const SetDef& d = DEFS[id];
  switch (d.kind) {
    case K_ENUM:
      if (id == S_CITY) setVal(id, (getVal(id) + dir + optCount(id)) % optCount(id));   // города — по кругу
      else setVal(id, getVal(id) + dir);
      break;
    case K_BOOL:  setVal(id, dir > 0); break;
    case K_RANGE: setVal(id, getVal(id) + dir * d.step); break;
    default: break;                        // батарея и версия — только показывают
  }
}

// ---------- для планшета: все пункты в JSON и смена по ключу ----------
static void itemJson(int id, String& o) {
  const SetDef& d = DEFS[id];
  char b[48];
  o += "{\"k\":\""; o += d.key; o += "\",\"name\":"; jsonStr(o, d.name); o += ",\"hint\":"; jsonStr(o, d.hint);
  switch (d.kind) {
    case K_ENUM:
      o += ",\"type\":\"enum\",\"v\":"; optName(id, getVal(id), b, sizeof(b)); jsonStr(o, b);
      o += ",\"opts\":[";
      for (int i = 0; i < optCount(id); i++) { if (i) o += ','; optName(id, i, b, sizeof(b)); jsonStr(o, b); }
      o += ']';
      break;
    case K_RANGE:
      o += ",\"type\":\"range\",\"v\":"; o += getVal(id);
      o += ",\"min\":"; o += d.mn; o += ",\"max\":"; o += d.mx; o += ",\"step\":"; o += d.step; o += ",\"unit\":"; jsonStr(o, d.unit);
      break;
    case K_BOOL:
      o += ",\"type\":\"bool\",\"v\":"; o += getVal(id) ? 1 : 0;
      break;
    default:
      if (id == S_VERSION) snprintf(b, sizeof(b), "v%s", FW_VERSION);
      else if (g_bat.ok) snprintf(b, sizeof(b), "%d%%  %.2f V", g_bat.soc, g_bat.mv / 1000.0f);
      else snprintf(b, sizeof(b), "n/a");
      o += ",\"type\":\"info\",\"v\":"; jsonStr(o, b);
  }
  o += '}';
}

void settingsJson(String& o) {
  o.reserve(2600);
  o = "{\"pages\":[";
  for (int p = 0; p < P_NUM; p++) {
    if (p) o += ',';
    o += "{\"id\":\""; o += PAGE_IDS[p]; o += "\",\"name\":"; jsonStr(o, PAGE_NAMES[p]); o += ",\"items\":[";
    bool first = true;
    for (int i = 0; i < S_NUM; i++) if (DEFS[i].page == p) { if (!first) o += ','; first = false; itemJson(i, o); }
    o += "]}";
  }
  o += "]}";
}

bool settingsSetKey(const char* key, const char* val, String& o) {
  int id = -1;
  for (int i = 0; i < S_NUM; i++) if (!strcmp(DEFS[i].key, key)) id = i;
  const char* err = nullptr;
  if (id < 0) err = "unknown key";
  else if (DEFS[id].kind == K_INFO) err = "read-only";
  else {
    int v = 0;
    if (DEFS[id].kind == K_ENUM) {                        // имя варианта (без учёта регистра) или его номер
      v = -1;
      char b[48];
      for (int i = 0; i < optCount(id); i++) { optName(id, i, b, sizeof(b)); if (!strcasecmp(b, val)) v = i; }
      if (v < 0 && isdigit((unsigned char)val[0])) v = atoi(val);
      if (v < 0 || v >= optCount(id)) err = "bad option";
    } else if (DEFS[id].kind == K_BOOL) {
      v = (!strcasecmp(val, "1") || !strcasecmp(val, "true") || !strcasecmp(val, "on")) ? 1 : 0;
    } else {
      if (!val[0] || !(isdigit((unsigned char)val[0]) || val[0] == '-')) err = "bad number";
      else v = atoi(val);
    }
    if (!err && !setVal(id, v)) err = "not applied";
  }
  if (err) { o = "{\"ok\":false,\"err\":"; jsonStr(o, err); o += '}'; return false; }
  o = "{\"ok\":true,\"item\":"; itemJson(id, o); o += '}';
  return true;
}

void settingsTouch() { g_setDirty = true; g_setChangedMs = millis(); }

static const char* onOff(int v) { return v ? "on" : "off"; }
static void rangeText(char* val, int r) {
  if (r == 0) sprintf(val, "all");
  else sprintf(val, "%s %d dBm", RANGES[r].name, RANGES[r].dbm);
}

void drawSettings() {
  int W = spr.width();
  spr.fillSprite(C_BG());
  int ids[S_NUM]; int n = pageItems(g_setPage, ids);
  g_setItem = constrain(g_setItem, 0, n - 1);
  int cur = ids[g_setItem];
  char batLine[64]; batSummary(batLine, sizeof(batLine));
  uiHeader(PAGE_NAMES[g_setPage], cur == S_BATTERY ? batLine : DEFS[cur].hint);
  const int rh = 18;
  int rows = min(n, SET_ROWS);
  int first = constrain(g_setItem - (rows - 1), 0, n - rows);
  for (int r = first; r < first + rows; r++) {
    int id = ids[r];
    int y = UI_TOP + 2 + (r - first) * rh, cy = y + rh / 2 - 1;
    bool sel = (r == g_setItem);
    if (sel) uiRowHighlight(y, rh - 2);
    char val[32];
    bool alert = false;
    switch (id) {
      case S_RANGE:   rangeText(val, g_range); break;
      case S_WRANGE:  rangeText(val, g_wRange); break;
      case S_LEDEV:   if (g_thresh == 0) sprintf(val, "off"); else sprintf(val, "+/- %d", g_thresh); break;
      case S_LEDBR:   sprintf(val, "%d%%", g_ledBright); break;
      case S_SCREEN:  sprintf(val, "%d%%", g_scrBright); break;
      case S_DIM:
        if (DIM_SECS[g_dimIdx] == 0) sprintf(val, "off");
        else if (DIM_SECS[g_dimIdx] < 60) sprintf(val, "%d s", DIM_SECS[g_dimIdx]);
        else sprintf(val, "%d min", DIM_SECS[g_dimIdx] / 60);
        break;
      case S_VOLUME:  sprintf(val, "%s", g_sndIdx == 0 ? "off" : g_sndIdx == 1 ? "quiet" : "loud"); break;
      case S_CLICKS:  strcpy(val, onOff(g_clickEn)); break;
      case S_THEME:   strcpy(val, g_theme == THEME_SUMI ? "sumi" : "denko"); break;
      case S_BLETICK: strcpy(val, onOff(g_bleTicks)); break;
      case S_WTICK:   strcpy(val, onOff(g_wifiTicks)); break;
      case S_BAND:    strcpy(val, BANDS[g_band].name); break;
      case S_MIC:     strcpy(val, onOff(g_micEn)); break;
      case S_AULED:   strcpy(val, onOff(g_auLeds)); break;
      case S_AUSRC:   strcpy(val, g_auSrc == 0 ? "auto" : g_auSrc == 1 ? "mic" : "laptop"); break;
      case S_CITY:    snprintf(val, sizeof(val), "%s", curCity().name); break;
      case S_VERSION: snprintf(val, sizeof(val), "v%s", FW_VERSION); break;
      default:
        if (g_bat.ok) { sprintf(val, "%d%%  %.2f V", g_bat.soc, g_bat.mv / 1000.0f); alert = g_bat.soc <= 20; }
        else          sprintf(val, "n/a");
        break;
    }
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(sel ? C_HOT() : UI_LABEL());
    txt(DEFS[id].name, UI_M + 8, cy, 2);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(alert ? C_ALERT() : (sel ? C_ACC() : C_HOT()));
    txt(val, W - UI_M - 8, cy, 2);
    if (sel && id != S_BATTERY && id != S_VERSION) {                 // стрелка «крути колесо»
      int vw = txtW(val, 2);
      spr.setTextColor(UI_DIM());
      txt("<", W - UI_M - 16 - vw, cy, 2);
    }
  }
  uiFooter(n > 1 ? "wheel: change   click: next" : "wheel: change", "side: back");
  present();
}

// ---------- раздел «Настройки»: подменю страниц ----------
static void setSubtitle(char* out, size_t n) { snprintf(out, n, "general + sections"); }
static void setEnter() { g_setPage = 0; g_setItem = 0; }
static void setLeave() { settingsSaveNow(); }
static void setInfo(int i, SubItem& it) {
  it.name = PAGE_NAMES[i]; it.desc = PAGE_DESCS[i];
  int ids[S_NUM]; sprintf(it.value, "%d", pageItems(i, ids));
}
static int  setNow() { return g_setPage; }
static void setOpen(int p) { g_setPage = p; g_setItem = 0; }
static void setClose() { settingsSaveNow(); }
static void setClick() {
  int ids[S_NUM]; int n = pageItems(g_setPage, ids);
  g_setItem = n ? (g_setItem + 1) % n : 0;
}
static void setDraw(uint32_t) { drawSettings(); }

const Section settingsSection = {
  "Settings", 200, 160, 90, iconGear, setSubtitle,
  setEnter, setLeave,
  P_NUM, setInfo, setNow, setOpen, setClose,
  setClick, settingsAdjust, nullptr, nullptr, setDraw,
};
