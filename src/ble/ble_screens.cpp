// ============================================================
//  ble_screens.cpp — экраны раздела Bluetooth
//
//  RADAR · PROXIMITY · SCOPE · TIMELINE · TYPES · DEVICES (+подробности) · WEATHER
// ============================================================
#include <algorithm>
#include "ble/ble_model.h"
#include "ble/vendors.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/settings.h"

static int   g_sweep = 0;                // угол луча на SCOPE
static float g_pieDisp[T_NUM] = {0};     // плавная анимация диаграммы TYPES

// RSSI -> ~метры (грубая оценка по модели затухания)
float estDistance(int rssi) {
  const float MEASURED = -59.0f; // ожидаемый RSSI на 1 м
  const float N = 2.5f;          // показатель затухания (помещение)
  float d = powf(10.0f, (MEASURED - (float)rssi) / (10.0f * N));
  if (d < 0.1f) d = 0.1f;
  if (d > 99.0f) d = 99.0f;
  return d;
}

// ---------- RADAR цвета ----------
static uint16_t colorForCount(int n) {
  if (n <= 2)  return C(70, 200, 130);
  if (n <= 6)  return C(235, 205, 70);
  if (n <= 12) return C(240, 150, 50);
  return C(235, 80, 80);
}
static const char* labelForCount(int n) {
  if (n <= 2)  return "calm";
  if (n <= 6)  return "busy";
  if (n <= 12) return "crowded";
  return "packed";
}

uint16_t typeColor(uint8_t t) {
  uint8_t r, g, b; typeRGB(t, r, g, b);
  return C(r, g, b);
}

static const char* akindName(uint8_t k) {
  switch (k) {
    case A_PUBLIC:   return "public";
    case A_STATIC:   return "random static";
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

static uint16_t tempColor(float t) {
  if (t < 0)  return C(130, 200, 255);
  if (t < 16) return C(80, 150, 240);
  if (t < 25) return C(70, 210, 130);
  if (t < 29) return C(240, 185, 60);
  return C(235, 80, 80);
}

// ---------- RADAR ----------
void drawRadar(int count) {
  int W = spr.width(), H = spr.height();
  uint16_t theme = colorForCount(count);
  spr.fillSprite(C_BG());
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C(120, 120, 135), C_BG());
  txt("BLE RADAR", W / 2, 6, 4);
  int cx = W / 2, cy = H / 2 + 8, R = 54, slots = 24;
  for (int i = 0; i < slots; i++) {
    float a = ((float)i / slots) * 2.0f * PI - (PI / 2.0f);
    int x = cx + (int)(cos(a) * R), y = cy + (int)(sin(a) * R);
    uint16_t c = (i < count) ? theme : C(40, 40, 48);
    spr.fillCircle(x, y, 3, c);
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_HOT(), C_BG());
  txtNum(count, cx, cy, 7);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(theme, C_BG());
  txt(labelForCount(count), W / 2, H - 6, 4);
  // сколько было «сырых» адресов до склейки
  if (g_lastRaw != count) {
    char buf[24]; sprintf(buf, "%d MAC", g_lastRaw);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C(90, 90, 100), C_BG());
    txt(buf, W - 4, 4, 2);
  }
  if (g_range > 0) {                 // включён фильтр дальности
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C(90, 90, 100), C_BG());
    txt(RANGES[g_range].name, 4, 4, 2);
  }
  present();
}

// ---------- PROXIMITY ----------
void drawProximity(bool have, int rssi, const Ent& best, int count) {
  uint8_t cat = best.cat, type = best.type;
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C(120, 120, 135), C_BG());
  txt("PROXIMITY", W / 2, 4, 4);

  if (!have) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C(90, 90, 100), C_BG());
    txt("no devices", W / 2, H / 2, 4);
    present();
    return;
  }

  int pct = (rssi + 95) * 100 / 50;
  pct = constrain(pct, 0, 100);
  uint16_t col; const char* word;
  if (pct > 66)      word = "CLOSE";
  else if (pct > 33) word = "NEAR";
  else               word = "FAR";
  col = heat(rssi);                              // чем ближе, тем «горячее»

  int bw = W - 60, bh = 26, bx = 30, by = H / 2 - 14;
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(col, C_BG());
  txt(word, W / 2, by - 6, 4);
  spr.drawRoundRect(bx, by, bw, bh, 4, C(70, 70, 80));
  int fillw = (bw - 4) * pct / 100;
  if (fillw > 0) spr.fillRoundRect(bx + 2, by + 2, fillw, bh - 4, 3, col);

  char buf[48];
  float d = estDistance(rssi);
  if (d < 10.0f) sprintf(buf, "~%.1f m  %d dBm", d, rssi);
  else           sprintf(buf, "~%.0f m  %d dBm", d, rssi);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C_HOT(), C_BG());
  txt(buf, W / 2, by + bh + 8, 4);

  // строка «тип  вендор  (N near)» — тип и вендор каждый своим цветом
  char vbuf[48];
  sprintf(vbuf, "  %s   (%d near)", vendorLabel(cat, best.vkind, best.vcode), count);
  const char* tn = typeName(type);
  int tw = txtW(tn, 2), vw = txtW(vbuf, 2);
  int x0 = W / 2 - (tw + vw) / 2, ty = by + bh + 34;
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(typeColor(type), C_BG());
  txt(tn, x0, ty, 2);
  spr.setTextColor(vendorColor(cat), C_BG());
  txt(vbuf, x0 + tw, ty, 2);
  present();
}

// ---------- SCOPE ----------
void drawScope(const std::vector<Blip>& bl) {
  int W = spr.width(), H = spr.height();
  int cx = W / 2 + 26, cy = H / 2;     // радар сдвинут вправо, слева место под легенду
  int maxR = (H / 2) - 8;
  spr.fillSprite(C_BG());
  uint16_t ring = C(0, 70, 32);
  for (int k = 1; k <= 3; k++) spr.drawCircle(cx, cy, maxR * k / 3, ring);
  spr.drawLine(cx - maxR, cy, cx + maxR, cy, ring);
  spr.drawLine(cx, cy - maxR, cx, cy + maxR, ring);

  for (int t = 0; t < 40; t++) {
    float a = (g_sweep - t) * PI / 180.0f;
    int g = 140 - t * 3; if (g < 0) g = 0;
    spr.drawLine(cx, cy, cx + (int)(cos(a) * maxR), cy + (int)(sin(a) * maxR),
                 C(0, g, g / 2));
  }

  // цели — буквы-значки типа, цвет — «жар» (сила сигнала); метки — инверсией (тревога).
  // Сначала слабые, сильные рисуются поверх.
  std::vector<int> idx(bl.size());
  for (size_t i = 0; i < bl.size(); i++) idx[i] = (int)i;
  std::sort(idx.begin(), idx.end(), [&](int a, int b) { return bl[a].rssi < bl[b].rssi; });
  for (int i : idx) {
    float ang = (bl[i].h % 360) * PI / 180.0f;
    int pct = (bl[i].rssi + 95) * 100 / 50;
    pct = constrain(pct, 0, 100);
    int r = maxR - (maxR - 8) * pct / 100;
    int x = cx + (int)(cos(ang) * r), y = cy + (int)(sin(ang) * r);
    drawGlyph(x, y, typeGlyph(bl[i].type), heat(bl[i].rssi), bl[i].type == T_TRACKER);
  }

  // легенда слева: сколько каких типов (по убыванию)
  int tc[T_NUM] = {0};
  for (size_t i = 0; i < bl.size(); i++) tc[bl[i].type < T_NUM ? (int)bl[i].type : 0]++;
  for (int row = 0; row < 8; row++) {
    int bi = -1;
    for (int t = 0; t < T_NUM; t++) if (tc[t] > 0 && (bi < 0 || tc[t] > tc[bi])) bi = t;
    if (bi < 0) break;
    int y = 22 + row * 18;
    drawGlyph(9, y, typeGlyph(bi), bi == T_TRACKER ? C_HOT() : C(170, 170, 185), bi == T_TRACKER);
    char nb[8]; sprintf(nb, "%d", tc[bi]);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(C(150, 150, 165));
    txt(nb, 18, y, 2);
    tc[bi] = 0;
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C(0, 170, 80), C_BG());
  txt("SCOPE", cx, 2, 2);
  char buf[16]; sprintf(buf, "%d", (int)bl.size());
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C(90, 255, 150), C_BG());
  txt(buf, W - 4, 2, 4);

  present();
  g_sweep = (g_sweep + 6) % 360;
}

// ---------- TIMELINE: число устройств во времени ----------
void drawTimeline() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());

  int plotL = 6, plotR = W - 6, plotT = 22, plotB = H - 22;
  int plotW = plotR - plotL, plotH = plotB - plotT;

  int peak = 1, nowv = 0;
  for (int i = 0; i < g_histLen; i++) if (g_hist[i].count > peak) peak = g_hist[i].count;
  if (g_histLen > 0) nowv = g_hist[g_histLen - 1].count;
  int ymax = peak + peak / 6 + 1;                 // запас сверху

  spr.drawRect(plotL, plotT, plotW, plotH, C(40, 40, 48));
  float step = (HIST_N > 1) ? (float)plotW / (HIST_N - 1) : plotW;

  // площадь + линия
  int prevx = 0, prevy = 0;
  for (int i = 0; i < g_histLen; i++) {
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    int y = plotB - (int)((long)g_hist[i].count * plotH / ymax);
    spr.drawFastVLine(x, y, plotB - y, C(15, 55, 35));
    if (i > 0) spr.drawLine(prevx, prevy, x, y, C(70, 220, 130));
    prevx = x; prevy = y;
  }

  // базовая линия (сглаженная) — тускло
  for (int i = 1; i < g_histLen; i++) {
    int x0 = plotR - (int)((g_histLen - 1 - (i - 1)) * step);
    int x1 = plotR - (int)((g_histLen - 1 - i) * step);
    int y0 = plotB - (int)(g_hist[i - 1].base * plotH / ymax);
    int y1 = plotB - (int)(g_hist[i].base * plotH / ymax);
    spr.drawLine(x0, y0, x1, y1, C(95, 95, 45));
  }

  // метки событий: ▲ пришли (зел), ▼ ушли (оранж)
  for (int i = 0; i < g_histLen; i++) {
    if (g_hist[i].ev == 0) continue;
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    if (g_hist[i].ev > 0)
      spr.fillTriangle(x, plotT + 2, x - 4, plotT + 10, x + 4, plotT + 10, C(70, 220, 130));
    else
      spr.fillTriangle(x - 4, plotT + 2, x + 4, plotT + 2, x, plotT + 10, C(240, 150, 50));
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C(120, 120, 135), C_BG());
  txt("TIMELINE", W / 2, 4, 2);

  char buf[48];
  sprintf(buf, "now %d   peak %d", nowv, peak);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(C(70, 220, 130), C_BG());
  txt(buf, W / 2, H - 4, 2);

  present();
}

// ---------- TYPES: кольцевая диаграмма по типам устройств ----------
static void drawTypesBars(const int* cnt, int total) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[24];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C(120, 120, 135));
  txt("TYPES", 4, 2, 2);
  sprintf(buf, "%d TOTAL", total);
  spr.setTextDatum(TR_DATUM);
  txt(buf, W - 4, 2, 2);

  int order[T_NUM], n = 0;
  for (int t = 0; t < T_NUM; t++) if (cnt[t] > 0) order[n++] = t;
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && cnt[order[j - 1]] < cnt[order[j]]; j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }
  if (n == 0) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C(90, 90, 100));
    txt("no devices", W / 2, H / 2, 4);
    present();
    return;
  }
  int maxc = cnt[order[0]];
  const int rows = n < 7 ? n : 7, rh = 20, y0 = 26;
  const int bx = 98, bwMax = W - bx - 36;
  for (int i = 0; i < rows; i++) {
    int t = order[i], cy = y0 + i * rh + rh / 2;
    bool alert = (t == T_TRACKER);
    uint16_t c = alert ? C_HOT() : (t == T_UNKNOWN ? C(110, 110, 125) : C(175, 175, 190));
    drawGlyph(10, cy, typeGlyph(t), c, alert);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(c);
    txt(typeName(t), 22, cy, 2);
    // сегментная полоска, как на старых индикаторах
    int len = bwMax * cnt[t] / maxc;
    for (int x = 0; x + 4 <= len; x += 6) spr.fillRect(bx + x, cy - 6, 4, 12, c);
    if (len < 4) spr.fillRect(bx, cy - 6, 2, 12, c);
    sprintf(buf, "%d", cnt[t]);
    spr.setTextDatum(MR_DATUM);
    txt(buf, W - 4, cy, 2);
  }
  present();
}

void drawTypes(const int* cnt, int total) {
  if (g_theme == THEME_PHOSPHOR) { drawTypesBars(cnt, total); return; }
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());

  // плавно подтягиваем отображаемые доли к реальным
  float dispSum = 0;
  for (int t = 0; t < T_NUM; t++) {
    g_pieDisp[t] += 0.2f * ((float)cnt[t] - g_pieDisp[t]);
    if (g_pieDisp[t] < 0.02f) g_pieDisp[t] = 0;
    dispSum += g_pieDisp[t];
  }

  int cx = 82, cy = H / 2 + 6, R = 68, IR = 42;
  if (dispSum < 0.05f) {
    spr.drawArc(cx, cy, R, IR, 0, 360, C(35, 35, 42), C_BG(), false);
  } else {
    // порядок секторов фиксированный (по типам), начинаем сверху, по часовой
    float acc = 0;
    for (int t = 0; t < T_NUM; t++) {
      if (g_pieDisp[t] <= 0) continue;
      int a0 = (int)(acc / dispSum * 360.0f + 0.5f);
      acc += g_pieDisp[t];
      int a1 = (int)(acc / dispSum * 360.0f + 0.5f);
      if (a1 <= a0) continue;
      if (a1 - a0 >= 360) {
        spr.drawArc(cx, cy, R, IR, 0, 360, typeColor(t), C_BG(), false);
      } else {
        // у drawArc 0° внизу -> сдвиг на 180°, чтобы начинать сверху
        spr.drawArc(cx, cy, R, IR, (a0 + 180) % 360, (a1 + 180) % 360,
                    typeColor(t), C_BG(), false);
        // тонкий разделитель между секторами
        float ar = (a1 - 90) * PI / 180.0f;
        spr.drawLine(cx + (int)(cos(ar) * IR), cy + (int)(sin(ar) * IR),
                     cx + (int)(cos(ar) * R),  cy + (int)(sin(ar) * R), C_BG());
      }
    }
  }

  // в центре — общее число
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_HOT(), C_BG());
  txtNum(total, cx, cy - 4, 4);
  spr.setTextColor(C(110, 110, 125), C_BG());
  txt("devices", cx, cy + 16, 1);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C(120, 120, 135), C_BG());
  txt("TYPES", 4, 2, 2);

  // легенда справа: только непустые, по убыванию
  int order[T_NUM], n = 0;
  for (int t = 0; t < T_NUM; t++) if (cnt[t] > 0) order[n++] = t;
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && cnt[order[j - 1]] < cnt[order[j]]; j--) {
      int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
    }
  const int maxRows = 8, rowH = 20;
  int rows = n < maxRows ? n : maxRows;
  int ly = (H - rows * rowH) / 2 + 2;
  int lx = 168;
  char buf[24];
  for (int i = 0; i < rows; i++) {
    int t = order[i], y = ly + i * rowH;
    uint16_t c = typeColor(t);
    spr.fillRoundRect(lx, y + 3, 10, 10, 2, c);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(c, C_BG());
    txt(typeName(t), lx + 16, y, 2);
    sprintf(buf, "%d", cnt[t]);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_HOT(), C_BG());
    txt(buf, W - 40, y, 2);
    sprintf(buf, "%d%%", total ? (cnt[t] * 100 + total / 2) / total : 0);
    spr.setTextColor(C(110, 110, 125), C_BG());
    txt(buf, W - 4, y, 2);
  }
  if (n == 0) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C(90, 90, 100), C_BG());
    txt("no devices", lx + (W - lx) / 2, H / 2, 2);
  }

  present();
}

void drawList(const std::map<uint32_t, Ent>& ents) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[48];

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), C_BG());
  sprintf(buf, "DEVICES  %d", (int)g_order.size());
  txt(buf, 4, 1, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt("wheel: pick  click: info", W - 4, 5, 1);

  if (g_order.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_DIM(), C_BG());
    txt("no devices", W / 2, H / 2, 4);
    present();
    return;
  }

  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = g_top + i;
    if (idx >= (int)g_order.size()) break;
    uint32_t id = g_order[idx];
    auto it = ents.find(id);
    int y = LIST_Y0 + i * LIST_ROWH;
    bool sel = (id == g_sel);
    if (sel) spr.fillRoundRect(0, y, W - 4, LIST_ROWH - 1, 3, C(28, 38, 62));
    uint16_t bg = sel ? C(28, 38, 62) : C_BG();
    if (it == ents.end()) {
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C_DIM(), bg);
      txt("(gone)", 12, y + 1, 2);
      continue;
    }
    const Ent& e = it->second;
    uint16_t tc = typeColor(e.type);
    bool alert = (e.type == T_TRACKER);
    drawGlyph(8, y + LIST_ROWH / 2 - 1, typeGlyph(e.type), alert ? C_HOT() : C(170, 170, 185), alert);

    fitText(e.name[0] ? e.name : typeName(e.type), buf, sizeof(buf), 128, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(e.name[0] ? C_HOT() : tc, bg);
    txt(buf, 16, y + 1, 2);

    spr.setTextColor(vendorColor(e.cat), bg);
    char vb[16];
    fitText(vendorLabel(e.cat, e.vkind, e.vcode), vb, sizeof(vb), 84, 2);
    txt(vb, 150, y + 1, 2);

    sprintf(buf, "%d", e.rssi);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(heat(e.rssi), bg);
    txt(buf, 268, y + 1, 2);
    drawBars(274, y + 2, e.rssi, heat(e.rssi));
  }

  // полоса прокрутки
  int n = (int)g_order.size();
  if (n > LIST_ROWS) {
    int trackH = LIST_ROWS * LIST_ROWH;
    int barH = trackH * LIST_ROWS / n; if (barH < 8) barH = 8;
    int barY = LIST_Y0 + (trackH - barH) * g_top / (n - LIST_ROWS);
    spr.fillRect(W - 2, LIST_Y0, 2, trackH, C(30, 30, 38));
    spr.fillRect(W - 2, barY, 2, barH, C_GRAY());
  }
  present();
}

void drawDetail(const Dev& d, const Ent& e, bool inMap, uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[96], tmp[48];
  uint16_t tc = typeColor(e.type);

  // заголовок: имя (или тип)
  fitText(d.name[0] ? d.name : typeName(e.type), tmp, sizeof(tmp), W - 70, 4);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(d.name[0] ? C_HOT() : tc, C_BG());
  txt(tmp, 4, 2, 4);
  int pos = -1;
  for (size_t i = 0; i < g_order.size(); i++) if (g_order[i] == g_sel) pos = (int)i;
  if (pos >= 0) {
    sprintf(buf, "%d/%d", pos + 1, (int)g_order.size());
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), C_BG());
    txt(buf, W - 4, 6, 2);
  }

  // тип + производитель
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tc, C_BG());
  txt(typeName(e.type), 4, 30, 2);
  int x = 4 + txtW(typeName(e.type), 2) + 10;
  if (e.vkind) sprintf(buf, "%s  %s%04X", vendorLabel(e.cat, e.vkind, e.vcode),
                      e.vkind == 1 ? "0x" : "svc ", e.vcode);
  else          sprintf(buf, "maker unknown");
  spr.setTextColor(vendorColor(e.cat), C_BG());
  txt(buf, x, 30, 2);

  // левая колонка
  uint32_t ago = (now - e.t) / 1000;
  int y = 50;
  const int LH = 15;
  spr.setTextColor(C_GRAY(), C_BG());
  if (!inMap)          { spr.setTextColor(C(235, 80, 80), C_BG()); sprintf(buf, "out of range"); }
  else if (ago >= 3)   sprintf(buf, "signal %d (%lus ago)", e.rssi, (unsigned long)ago);
  else                 sprintf(buf, "signal %d dBm", e.rssi);
  txt(buf, 4, y, 2); y += LH;
  spr.setTextColor(C_GRAY(), C_BG());
  sprintf(buf, "range  %d .. %d", d.rmin, d.rmax);
  txt(buf, 4, y, 2); y += LH;
  float dist = estDistance(e.rssi);
  if (dist < 10) sprintf(buf, "dist   ~%.1f m", dist); else sprintf(buf, "dist   ~%.0f m", dist);
  txt(buf, 4, y, 2); y += LH;
  uint32_t secs = (now - (e.first ? e.first : now)) / 1000;
  sprintf(buf, "seen   %lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  txt(buf, 4, y, 2); y += LH;
  sprintf(buf, "pkts   %lu", (unsigned long)e.pkts);
  txt(buf, 4, y, 2);

  // правая колонка: график сигнала за ~30 с
  int gx = 176, gy = 50, gw = W - gx - 4, gh = 70;
  spr.drawRect(gx, gy, gw, gh, C(40, 40, 50));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt("signal, 30s", gx + 3, gy + 3, 1);
  drawSpark(gx, gy, gw, gh, g_spark, g_sparkN, SPARK_N, heat(e.rssi));

  // низ: технические подробности мелким шрифтом
  y = 126;
  macStr(d.mac, tmp);
  if (e.nMacs > 1) sprintf(buf, "MAC %s  %s  (%d MACs)", tmp, akindName(d.akind), e.nMacs);
  else             sprintf(buf, "MAC %s  %s", tmp, akindName(d.akind));
  spr.setTextColor(C_GRAY(), C_BG());
  txt(buf, 4, y, 1); y += 10;

  if (d.nU) {
    int n = sprintf(buf, "SVC");
    for (uint8_t i = 0; i < d.nU && n < 52; i++) {
      const char* nm = uuidName(d.uuids[i]);
      if (!nm[0]) { const char* o = btUuidOwner(d.uuids[i]); if (o) nm = o; }
      n += snprintf(buf + n, sizeof(buf) - n, nm[0] ? " %04X %s" : " %04X", d.uuids[i], nm);
    }
    txt(buf, 4, y, 1); y += 10;
  }
  if (d.hasCid) {
    int n = sprintf(buf, "MFR %02X %02X |", d.cid & 0xFF, d.cid >> 8);
    int shown = d.mfrLen < 6 ? d.mfrLen : 6;
    for (int i = 0; i < shown; i++) n += sprintf(buf + n, " %02X", d.mfr[i]);
    sprintf(buf + n, "%s  (len %d)", d.mfrLen > 6 ? " .." : "", d.mfrLen);
    txt(buf, 4, y, 1); y += 10;
  }
  if (d.appearance) {
    sprintf(buf, "APP 0x%04X", d.appearance);
    txt(buf, 4, y, 1);
  }
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt("click: back", W - 4, H - 1, 1);
  present();
}

void drawWeather(const std::vector<Sensor>& ss, uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(C_BG());
  char buf[48];

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), C_BG());
  txt("WEATHER", 4, 1, 2);

  if (ss.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_GRAY(), C_BG());
    txt("no sensors yet", W / 2, H / 2 - 16, 4);
    spr.setTextColor(C_DIM(), C_BG());
    txt("Xiaomi PVVX/ATC, BTHome, Govee,", W / 2, H / 2 + 14, 2);
    txt("Qingping, SwitchBot, RuuviTag", W / 2, H / 2 + 32, 2);
    present();
    return;
  }

  const int PER = 3, RH = 50;
  int pages = ((int)ss.size() + PER - 1) / PER;
  int page = pages > 1 ? (int)((now / 4000) % pages) : 0;

  sprintf(buf, "%d sensor%s", (int)ss.size(), ss.size() == 1 ? "" : "s");
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), C_BG());
  txt(buf, W - 4 - (pages > 1 ? pages * 8 + 4 : 0), 1, 2);
  for (int p = 0; p < pages && pages > 1; p++)
    spr.fillCircle(W - 4 - (pages - 1 - p) * 8 - 3, 9, 2, p == page ? C_HOT() : C_DIM());

  for (int i = 0; i < PER; i++) {
    int k = page * PER + i;
    if (k >= (int)ss.size()) break;
    const Sensor& S = ss[k];
    int y = 20 + i * RH;
    if (i > 0) spr.drawFastHLine(4, y - 3, W - 8, C(30, 30, 38));

    // подпись: имя или модель + хвост MAC
    if (S.name[0]) fitText(S.name, buf, sizeof(buf), 150, 2);
    else sprintf(buf, "%s %02X%02X", S.r.model, S.mac[1], S.mac[0]);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_GRAY(), C_BG());
    txt(buf, 6, y, 2);

    uint32_t ago = (now - S.t) / 1000;
    char right[32] = "";
    int n = 0;
    if (S.r.hasB) n += sprintf(right + n, "bat %d%%  ", S.r.batt);
    if (ago < 60) sprintf(right + n, "%lus", (unsigned long)ago);
    else          sprintf(right + n, "%lum", (unsigned long)(ago / 60));
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), C_BG());
    txt(right, W - 6, y, 2);

    int vy = y + 17;
    int x = 6;
    if (S.r.hasT) {
      sprintf(buf, "%.1f", S.r.temp);
      uint16_t c = tempColor(S.r.temp);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(c, C_BG());
      txt(buf, x, vy, 4);
      x += txtW(buf, 4) + 3;
      spr.drawCircle(x + 2, vy + 4, 2, c);          // значок градуса
      txt("C", x + 7, vy, 4);
    }
    if (S.r.hasH) {
      sprintf(buf, "%.0f%%", S.r.hum);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C(90, 170, 245), C_BG());
      txt(buf, 130, vy, 4);
    }
    int ex = 214, ey = vy + 5;
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_GRAY(), C_BG());
    if (S.r.hasP)   { sprintf(buf, "%.0f hPa", S.r.press); txt(buf, ex, ey, 2); ey += 15; }
    if (S.r.hasCO2) { sprintf(buf, "%d ppm", S.r.co2);    txt(buf, ex, ey, 2); }
  }
  present();
}

