// ============================================================
//  ble_screens.cpp — экраны раздела Bluetooth
//
//  OVERVIEW · SCOPE · NEAREST · DEVICES (+подробности) · WEATHER
//  Все экраны собраны из компонентов core/ui.h по общей сетке.
// ============================================================
#include <algorithm>
#include "core/lang.h"
#include "core/timeutil.h"
#include "ble/ble_model.h"
#include "ble/vendors.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/settings.h"
#include "audio/audio.h"

static int g_sweep = 0;                  // угол луча на SCOPE

// подпись пользователя важнее имени из эфира; кириллица — шрифтом с русскими буквами
static bool isUtf8(const char* s) { for (; *s; s++) if ((uint8_t)*s >= 0x80) return true; return false; }
static int  fontFor(const char* s, int latin) { return isUtf8(s) ? (latin >= 4 ? 5 : 3) : latin; }

float estDistance(int rssi) {
  const float MEASURED = -59.0f;         // ожидаемый RSSI на 1 м
  const float N = 2.5f;                  // показатель затухания (помещение)
  float d = powf(10.0f, (MEASURED - (float)rssi) / (10.0f * N));
  return constrain(d, 0.1f, 99.0f);
}


static const char* akindName(uint8_t k) {
  switch (k) {
    case A_PUBLIC:   return "public";
    case A_STATIC:   return "static";
    case A_ROTATING: return "rotating";
    default:         return "random";
  }
}

static const char* uuidName(uint16_t u) {
  switch (u) {
    case 0x180F: return "Battery";   case 0x180A: return "DevInfo";
    case 0x1800: return "GAP";       case 0x1801: return "GATT";
    case 0xFE2C: return "FastPair";  case 0xFD6F: return "ExpNotif";
    case 0xFEAA: return "Eddystone"; case 0x181A: return "EnvSens";
    case 0xFCD2: return "BTHome";    case 0xFE95: return "Xiaomi";
    case 0x1812: return "HID";       case 0x180D: return "HeartRate";
    case 0xFEED: case 0xFEEC: return "Tile";
    case 0xFD5A: return "SmartTag";  case 0xFE9F: return "Google";
    case 0xFD3D: return "SwitchBot"; case 0xFDCD: return "Qingping";
    case 0x1809: return "Thermo";    case 0x184E: return "LEAudio";
    case 0xFE33: return "Chipolo";   case 0xFE07: return "Sonos";
    case 0xFE03: return "Amazon";    case 0xFD5F: return "Meta";
    default:     return "";
  }
}

static void upperCopy(const char* s, char* out, size_t n) { utf8Upper(s, out, n); }

// типы по убыванию количества
static int sortedTypes(const int* cnt, int* order) {
  int n = 0;
  for (int t = 0; t < T_NUM; t++) if (cnt[t] > 0) order[n++] = t;
  std::sort(order, order + n, [&](int a, int b) { return cnt[a] > cnt[b]; });
  return n;
}

// ============================================================
//  OVERVIEW — сколько вокруг, как менялось, кто это
// ============================================================
void drawOverview(int count, int raw, const int* typeCnt) {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d mac  range %s", raw, RANGES[g_range].name);
  uiHeader("BLE overview", buf);

  // слева: главное число и тренд
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_HOT());
  txtNum(count, UI_M + 8, UI_TOP + 2, 6);
  spr.setTextColor(UI_DIM());
  txt("CONTACTS", UI_M + 8, UI_TOP + 50, 1);
  int delta = 0;                                   // изменение за ~2 минуты
  if (g_histLen > 120) delta = g_hist[g_histLen - 1].count - g_hist[g_histLen - 121].count;
  snprintf(buf, sizeof(buf), "%+d", delta);
  uiReadout(UI_M + 8, UI_TOP + 64, "2 min", buf, delta > 0 ? C_ACC() : UI_LABEL(), 2);

  // справа: график числа устройств
  int gx = 112, gy = UI_TOP + 2, gw = W - UI_M - 4 - gx, gh = 82;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  int peak = 1;
  for (int i = 0; i < g_histLen; i++) peak = max(peak, g_hist[i].count);
  int ymax = peak + peak / 5 + 1;
  float step = (float)(gw - 4) / (HIST_N - 1);
  int px = -1, py = -1;
  for (int i = 0; i < g_histLen; i++) {
    int x = gx + gw - 3 - (int)((g_histLen - 1 - i) * step);
    int y = gy + gh - 3 - (int)((long)g_hist[i].count * (gh - 8) / ymax);
    spr.drawFastVLine(x, y, gy + gh - 3 - y, UI_PANEL());
    if (px >= 0) spr.drawLine(px, py, x, y, C_ACC());
    if (g_hist[i].ev > 0) spr.fillTriangle(x, gy + 18, x - 3, gy + 13, x + 3, gy + 13, C_HOT());   // «пришла группа»
    px = x; py = y;
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt("COUNT  2.5 MIN", gx + 6, gy + 4, 1);
  snprintf(buf, sizeof(buf), "PEAK %d", peak);
  spr.setTextDatum(TR_DATUM);
  txt(buf, gx + gw - 6, gy + 4, 1);

  // внизу: из чего состоит толпа — полоса долей и значки с числами
  int order[T_NUM];
  int n = sortedTypes(typeCnt, order), total = 0;
  for (int i = 0; i < n; i++) total += typeCnt[order[i]];
  int by = UI_TOP + 92, bx = UI_M + 8, bw = W - 2 * UI_M - 16;
  if (total > 0) {
    int x = bx;
    for (int i = 0; i < n; i++) {
      int t = order[i];
      int w = (i == n - 1) ? bx + bw - x : bw * typeCnt[t] / total;
      if (w < 2) w = 2;
      uint16_t c = t == T_TRACKER ? C_ALERT() : heatV(0.9f - 0.12f * i);
      spr.fillRect(x, by, w - 1, 5, c);
      x += w;
    }
    for (int i = 0; i < n && i < 6; i++) {
      int t = order[i], cx = bx + 4 + i * 50, cy = by + 20;
      bool alert = (t == T_TRACKER);
      drawGlyph(cx, cy, typeGlyph(t), alert ? C_ALERT() : UI_LABEL(), alert);
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(alert ? C_ALERT() : C_HOT());
      txtNum(typeCnt[t], cx + 9, cy, 2);
    }
  } else {
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(UI_DIM());
    txt("NO DEVICES IN RANGE", bx, by + 14, 1);
  }
  uiFooter("wheel: screens", "side: back");
  present();
}

// ============================================================
//  SCOPE — радар: расстояние по силе сигнала, угол условный
// ============================================================
void drawScope(const std::vector<Blip>& bl) {
  int W = spr.width();
  char buf[24];
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d contacts", (int)bl.size());
  uiHeader("Scope", buf);

  int maxR = 62, cx = W - UI_M - 6 - maxR, cy = UI_TOP + 2 + maxR;
  uint16_t ring = UI_LINE();
  for (int k = 1; k <= 3; k++) spr.drawCircle(cx, cy, maxR * k / 3, ring);
  spr.drawFastHLine(cx - maxR, cy, 2 * maxR, ring);
  spr.drawFastVLine(cx, cy - maxR, 2 * maxR, ring);
  // след луча: линии всё тусклее
  for (int t = 0; t < 36; t++) {
    float a = (g_sweep - t) * PI / 180.0f;
    float k = 1.0f - t / 36.0f;
    spr.drawLine(cx, cy, cx + (int)(cosf(a) * maxR), cy + (int)(sinf(a) * maxR), heatV(0.15f + 0.55f * k * k));
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt("1M", cx + 3, cy - maxR / 3 + 2, 1);
  txt("5M", cx + 3, cy - 2 * maxR / 3 + 2, 1);

  // цели: слабые снизу, сильные поверх; метки — тревога
  static std::vector<int> idx;                         // static: без выделения памяти каждый кадр
  idx.resize(bl.size());
  for (size_t i = 0; i < bl.size(); i++) idx[i] = (int)i;
  std::sort(idx.begin(), idx.end(), [&](int a, int b) { return bl[a].rssi < bl[b].rssi; });
  for (int i : idx) {
    float ang = (bl[i].h % 360) * PI / 180.0f;
    int pct = constrain((bl[i].rssi + 95) * 100 / 50, 0, 100);
    int r = maxR - (maxR - 6) * pct / 100;
    drawMarker(cx + (int)(cosf(ang) * r), cy + (int)(sinf(ang) * r), heat(bl[i].rssi), bl[i].type == T_TRACKER);
  }

  // легенда: сколько каких типов
  int cnt[T_NUM] = {0};
  for (size_t i = 0; i < bl.size(); i++) cnt[bl[i].type < T_NUM ? (int)bl[i].type : 0]++;
  int order[T_NUM];
  int n = sortedTypes(cnt, order);
  for (int i = 0; i < n && i < 7; i++) {
    int t = order[i], y = UI_TOP + 8 + i * 17;
    bool alert = (t == T_TRACKER);
    drawGlyph(UI_M + 12, y, typeGlyph(t), alert ? C_ALERT() : UI_LABEL(), alert);
    char nm[16]; upperCopy(typeName(t), nm, sizeof(nm));
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(alert ? C_ALERT() : UI_LABEL());
    txt(nm, UI_M + 22, y + 1, 1);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(alert ? C_ALERT() : C_HOT());
    txtNum(cnt[t], UI_M + 100, y, 2);
  }
  uiFooter("wheel: screens", "side: back");
  present();
  g_sweep = (g_sweep + 6) % 360;
}

// ============================================================
//  NEAREST — ближайшее устройство на дуге-шкале
// ============================================================
void drawNearest(bool have, int rssi, const Ent& best, int count) {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d in range", count);
  uiHeader("Nearest", buf);
  if (!have) { uiEmpty("no devices", "nothing heard in range"); present(); return; }

  int pct = constrain((rssi + 95) * 100 / 50, 0, 100);
  const char* word = pct > 66 ? "CLOSE" : pct > 33 ? "NEAR" : "FAR";
  bool tracker = (best.type == T_TRACKER);
  uint16_t col = tracker ? C_ALERT() : heat(rssi);
  int cx = W / 2, cy = 120;
  drawArcGauge(cx, cy, 72, 9, pct, col, UI_LINE(), C_BG());

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_HOT());
  txtNum(rssi, cx, cy - 22, 6);
  float d = estDistance(rssi);
  if (d < 10.0f) snprintf(buf, sizeof(buf), "DBM  %s  ~%.1f M", word, d);
  else           snprintf(buf, sizeof(buf), "DBM  %s  ~%.0f M", word, d);
  spr.setTextColor(UI_LABEL());
  txt(buf, cx, cy + 4, 1);

  // кто это
  char tn[16]; upperCopy(typeName(best.type), tn, sizeof(tn));
  char who[40]; snprintf(who, sizeof(who), "%s  %s", tn, vendorLabel(best.cat, best.vkind, best.vcode));
  const char* nm = best.label[0] ? best.label : best.name[0] ? best.name : who;
  int nf = fontFor(nm, 2);
  char fit[48]; fitText(nm, fit, sizeof(fit), 230, nf);
  spr.setTextColor(tracker ? C_ALERT() : C_HOT());
  txt(fit, cx, cy + 22, nf);
  if (best.label[0] || best.name[0]) {
    char w2[40]; upperCopy(who, w2, sizeof(w2));
    spr.setTextColor(UI_DIM());
    txt(w2, cx, cy + 36, 1);
  }
  if (tracker) {
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_ALERT());
    txt("! TRACKER", UI_M + 8, UI_TOP + 4, 2);
  }
  present();
  if (g_bleTicks) audioGeiger(pct);                // щелчки чаще — ближе
}

// ============================================================
//  DEVICES — список (ближние сверху) и подробности
// ============================================================
void drawList(const std::map<uint32_t, Ent>& ents) {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d  nearest first", (int)g_order.size());
  uiHeader("Devices", buf);
  if (g_order.empty()) { uiEmpty("no devices", "nothing heard in range"); uiFooter("", "side: back"); present(); return; }

  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = g_top + i;
    if (idx >= (int)g_order.size()) break;
    uint32_t id = g_order[idx];
    auto it = ents.find(id);
    int y = LIST_Y0 + i * LIST_ROWH, cy = y + LIST_ROWH / 2 - 1;
    bool sel = (id == g_sel);
    if (sel) uiRowHighlight(y, LIST_ROWH - 1);
    if (it == ents.end()) {
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(UI_DIM());
      txt("(gone)", UI_M + 18, cy, 2);
      continue;
    }
    const Ent& e = it->second;
    bool alert = (e.type == T_TRACKER);
    drawGlyph(UI_M + 8, cy, typeGlyph(e.type), alert ? C_ALERT() : UI_LABEL(), alert);
    const char* nm = e.label[0] ? e.label : e.name[0] ? e.name : typeName(e.type);
    int nf = fontFor(nm, 2);
    fitText(nm, buf, sizeof(buf), 124, nf);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(alert ? C_ALERT() : (e.label[0] || e.name[0] ? C_HOT() : UI_LABEL()));
    txt(buf, UI_M + 18, cy, nf);
    char vb[16];
    fitText(vendorLabel(e.cat, e.vkind, e.vcode), vb, sizeof(vb), 78, 2);
    spr.setTextColor(UI_DIM());
    txt(vb, 156, cy, 2);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(heat(e.rssi));
    txtNum(e.rssi, 266, cy, 2);
    drawBars(274, cy - 7, e.rssi, heat(e.rssi));
  }
  int n = (int)g_order.size();                     // полоса прокрутки
  if (n > LIST_ROWS) {
    int trackH = LIST_ROWS * LIST_ROWH;
    int barH = max(8, trackH * LIST_ROWS / n);
    int barY = LIST_Y0 + (trackH - barH) * g_top / (n - LIST_ROWS);
    spr.fillRect(W - UI_M + 2, LIST_Y0, 2, trackH, UI_LINE());
    spr.fillRect(W - UI_M + 2, barY, 2, barH, C_ACC());
  }
  uiFooter("wheel: pick  click: details", "side: back");
  present();
}

void drawDetail(const Dev& d, const Ent& e, bool inMap, uint32_t now) {
  int W = spr.width();
  char buf[96], tmp[48];
  spr.fillSprite(C_BG());
  int pos = -1;
  for (size_t i = 0; i < g_order.size(); i++) if (g_order[i] == g_sel) pos = (int)i;
  if (pos >= 0) snprintf(buf, sizeof(buf), "%d / %d  click: list", pos + 1, (int)g_order.size());
  else          snprintf(buf, sizeof(buf), "click: list");
  uiHeader("Device", buf);

  // имя и «кто это»
  bool alert = (e.type == T_TRACKER);
  const char* nm = e.label[0] ? e.label : d.name[0] ? d.name : typeName(e.type);
  int nf = fontFor(nm, 4);
  fitText(nm, tmp, sizeof(tmp), W - 2 * UI_M - 24, nf);
  drawGlyph(UI_M + 8, UI_TOP + 11, typeGlyph(e.type), alert ? C_ALERT() : UI_LABEL(), alert);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(alert ? C_ALERT() : C_HOT());
  txt(tmp, UI_M + 20, UI_TOP, nf);
  char tn[16]; upperCopy(typeName(e.type), tn, sizeof(tn));
  if (e.vkind) snprintf(buf, sizeof(buf), "%s  %s  %s%04X", tn, vendorLabel(e.cat, e.vkind, e.vcode), e.vkind == 1 ? "0x" : "svc ", e.vcode);
  else         snprintf(buf, sizeof(buf), "%s  maker unknown", tn);
  char bu[96]; upperCopy(buf, bu, sizeof(bu));
  spr.setTextColor(UI_LABEL());
  txt(bu, UI_M + 20, UI_TOP + 26, 1);

  // показатели: 3 колонки × 2 ряда
  uint32_t ago = msSince(now, e.t) / 1000;
  int x0 = UI_M + 8, y0 = UI_TOP + 40, cw = 64;
  if (!inMap) { uiReadout(x0, y0, "signal", "lost", C_ALERT()); }
  else { snprintf(tmp, sizeof(tmp), "%d", e.rssi); uiReadout(x0, y0, ago >= 3 ? "signal (old)" : "signal", tmp, heat(e.rssi)); }
  float dist = estDistance(e.rssi);
  if (dist < 10) snprintf(tmp, sizeof(tmp), "~%.1fm", dist); else snprintf(tmp, sizeof(tmp), "~%.0fm", dist);
  uiReadout(x0 + cw, y0, "distance", tmp, C_HOT());
  uint32_t secs = (now - (e.first ? e.first : now)) / 1000;
  snprintf(tmp, sizeof(tmp), "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  uiReadout(x0 + 2 * cw, y0, "seen", tmp, C_HOT());
  snprintf(tmp, sizeof(tmp), "%d/%d", d.rmin, d.rmax);
  uiReadout(x0, y0 + 28, "range", tmp, C_HOT());
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)e.pkts);
  uiReadout(x0 + cw, y0 + 28, "packets", tmp, C_HOT());
  snprintf(tmp, sizeof(tmp), "%d", e.nMacs);
  uiReadout(x0 + 2 * cw, y0 + 28, "macs", tmp, C_HOT());

  // график сигнала
  int gx = 206, gy = y0, gw = W - UI_M - 4 - gx, gh = 52;
  uiFrame(gx, gy, gw, gh, UI_LINE());
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_DIM());
  txt("SIGNAL 30S", gx + 5, gy + 3, 1);
  drawSpark(gx, gy, gw, gh, g_spark, g_sparkN, SPARK_N, heat(e.rssi));

  // технические подробности
  int y = UI_TOP + 98;
  macStr(d.mac, tmp);
  snprintf(buf, sizeof(buf), "MAC %s  %s", tmp, akindName(d.akind));
  upperCopy(buf, bu, sizeof(bu));
  spr.setTextColor(UI_DIM());
  txt(bu, x0, y, 1); y += 10;
  if (d.nU) {
    int n = snprintf(buf, sizeof(buf), "SVC");
    for (uint8_t i = 0; i < d.nU && n < 46; i++) {
      const char* nm = uuidName(d.uuids[i]);
      if (!nm[0]) { const char* o = btUuidOwner(d.uuids[i]); if (o) nm = o; }
      n += snprintf(buf + n, sizeof(buf) - n, nm[0] ? " %04X %s" : " %04X", d.uuids[i], nm);
    }
    upperCopy(buf, bu, sizeof(bu));
    txt(bu, x0, y, 1); y += 10;
  }
  if (d.hasCid) {
    int n = snprintf(buf, sizeof(buf), "MFR %02X %02X |", d.cid & 0xFF, d.cid >> 8);
    int shown = d.mfrLen < 6 ? d.mfrLen : 6;
    for (int i = 0; i < shown; i++) n += snprintf(buf + n, sizeof(buf) - n, " %02X", d.mfr[i]);
    snprintf(buf + n, sizeof(buf) - n, "%s  LEN %d", d.mfrLen > 6 ? " .." : "", d.mfrLen);
    txt(buf, x0, y, 1);
  }
  present();
}

// ============================================================
//  WEATHER — BLE-термометры вокруг
// ============================================================
void drawWeather(const std::vector<Sensor>& ss, uint32_t now) {
  int W = spr.width();
  char buf[48];
  spr.fillSprite(C_BG());
  snprintf(buf, sizeof(buf), "%d sensor%s", (int)ss.size(), ss.size() == 1 ? "" : "s");
  uiHeader("Weather", ss.empty() ? "" : buf);
  if (ss.empty()) {
    uiEmpty("no sensors yet", "xiaomi pvvx/atc, bthome, govee, qingping, switchbot, ruuvi");
    uiFooter("wheel: screens", "side: back");
    present();
    return;
  }
  const int PER = 3, RH = 42;
  int pages = ((int)ss.size() + PER - 1) / PER;
  int page = pages > 1 ? (int)((now / 4000) % pages) : 0;
  for (int i = 0; i < PER; i++) {
    int k = page * PER + i;
    if (k >= (int)ss.size()) break;
    const Sensor& S = ss[k];
    int y = UI_TOP + 2 + i * RH;
    if (i > 0) spr.drawFastHLine(UI_M + 8, y - 3, W - 2 * UI_M - 16, UI_LINE());
    // подпись: имя или модель + хвост MAC; справа батарея и «когда»
    if (S.name[0]) fitText(S.name, buf, sizeof(buf), 160, 1);
    else snprintf(buf, sizeof(buf), "%s %02X%02X", S.r.model, S.mac[1], S.mac[0]);
    char bu[48]; upperCopy(buf, bu, sizeof(bu));
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(UI_LABEL());
    txt(bu, UI_M + 8, y, 1);
    uint32_t ago = msSince(now, S.t) / 1000;
    int n = 0;
    if (S.r.hasB) n += snprintf(buf + n, sizeof(buf) - n, "BAT %d%%  ", S.r.batt);
    if (ago < 60) snprintf(buf + n, sizeof(buf) - n, "%luS AGO", (unsigned long)ago);
    else          snprintf(buf + n, sizeof(buf) - n, "%luM AGO", (unsigned long)(ago / 60));
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor((S.r.hasB && S.r.batt <= 15) ? C_ALERT() : UI_DIM());
    txt(buf, W - UI_M - 8, y, 1);
    int vy = y + 11;
    if (S.r.hasT) {
      snprintf(buf, sizeof(buf), "%.1f", S.r.temp);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C_HOT());
      txt(buf, UI_M + 8, vy, 4);
      int x = UI_M + 8 + txtW(buf, 4) + 3;
      spr.drawCircle(x + 2, vy + 4, 2, C_HOT());
      txt("C", x + 7, vy, 4);
    }
    if (S.r.hasH) {
      snprintf(buf, sizeof(buf), "%.0f%%", S.r.hum);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C_ACC());
      txt(buf, 120, vy, 4);
    }
    int ex = 200;
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(UI_LABEL());
    if (S.r.hasP)   { snprintf(buf, sizeof(buf), "%.0f hPa", S.r.press); txt(buf, ex, vy + 5, 2); }
    else if (S.r.hasCO2) { snprintf(buf, sizeof(buf), "%d ppm", S.r.co2); txt(buf, ex, vy + 5, 2); }
  }
  if (pages > 1) {                                   // листы датчиков — номером в шапке
    snprintf(buf, sizeof(buf), "%d sensors  %d/%d", (int)ss.size(), page + 1, pages);
    uiHeader("Weather", buf);
  }
  uiFooter("wheel: screens", "side: back");
  present();
}
