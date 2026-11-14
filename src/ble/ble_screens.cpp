// ============================================================
//  ble_screens.cpp — экраны раздела Bluetooth
//
//  RADAR · PROXIMITY · SCOPE · TIMELINE · TYPES · DEVICES (+подробности) · WEATHER
// ============================================================
#include "ble/ble_model.h"
#include "ble/vendors.h"
#include "core/display.h"
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
  if (n <= 2)  return tft.color565(70, 200, 130);
  if (n <= 6)  return tft.color565(235, 205, 70);
  if (n <= 12) return tft.color565(240, 150, 50);
  return tft.color565(235, 80, 80);
}
static const char* labelForCount(int n) {
  if (n <= 2)  return "calm";
  if (n <= 6)  return "busy";
  if (n <= 12) return "crowded";
  return "packed";
}

uint16_t typeColor(uint8_t t) {
  uint8_t r, g, b; typeRGB(t, r, g, b);
  return tft.color565(r, g, b);
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
  if (t < 0)  return tft.color565(130, 200, 255);
  if (t < 16) return tft.color565(80, 150, 240);
  if (t < 25) return tft.color565(70, 210, 130);
  if (t < 29) return tft.color565(240, 185, 60);
  return tft.color565(235, 80, 80);
}

// ---------- RADAR ----------
void drawRadar(int count) {
  int W = spr.width(), H = spr.height();
  uint16_t theme = colorForCount(count);
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("BLE RADAR", W / 2, 6, 4);
  int cx = W / 2, cy = H / 2 + 8, R = 54, slots = 24;
  for (int i = 0; i < slots; i++) {
    float a = ((float)i / slots) * 2.0f * PI - (PI / 2.0f);
    int x = cx + (int)(cos(a) * R), y = cy + (int)(sin(a) * R);
    uint16_t c = (i < count) ? theme : tft.color565(40, 40, 48);
    spr.fillCircle(x, y, 3, c);
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawNumber(count, cx, cy, 7);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(theme, TFT_BLACK);
  spr.drawString(labelForCount(count), W / 2, H - 6, 4);
  // сколько было «сырых» адресов до склейки
  if (g_lastRaw != count) {
    char buf[24]; sprintf(buf, "%d MAC", g_lastRaw);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString(buf, W - 4, 4, 2);
  }
  if (g_range > 0) {                 // включён фильтр дальности
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString(RANGES[g_range].name, 4, 4, 2);
  }
  spr.pushSprite(0, 0);
}

// ---------- PROXIMITY ----------
void drawProximity(bool have, int rssi, const Ent& best, int count) {
  uint8_t cat = best.cat, type = best.type;
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("PROXIMITY", W / 2, 4, 4);

  if (!have) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString("no devices", W / 2, H / 2, 4);
    spr.pushSprite(0, 0);
    return;
  }

  int pct = (rssi + 95) * 100 / 50;
  pct = constrain(pct, 0, 100);
  uint16_t col; const char* word;
  if (pct > 66)      { col = tft.color565(235, 80, 80);  word = "CLOSE"; }
  else if (pct > 33) { col = tft.color565(240, 185, 60); word = "NEAR";  }
  else               { col = tft.color565(70, 200, 130); word = "FAR";   }

  int bw = W - 60, bh = 26, bx = 30, by = H / 2 - 14;
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(col, TFT_BLACK);
  spr.drawString(word, W / 2, by - 6, 4);
  spr.drawRoundRect(bx, by, bw, bh, 4, tft.color565(70, 70, 80));
  int fillw = (bw - 4) * pct / 100;
  if (fillw > 0) spr.fillRoundRect(bx + 2, by + 2, fillw, bh - 4, 3, col);

  char buf[48];
  float d = estDistance(rssi);
  if (d < 10.0f) sprintf(buf, "~%.1f m  %d dBm", d, rssi);
  else           sprintf(buf, "~%.0f m  %d dBm", d, rssi);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, W / 2, by + bh + 8, 4);

  // строка «тип  вендор  (N near)» — тип и вендор каждый своим цветом
  char vbuf[48];
  sprintf(vbuf, "  %s   (%d near)", vendorLabel(cat, best.vkind, best.vcode), count);
  const char* tn = typeName(type);
  int tw = spr.textWidth(tn, 2), vw = spr.textWidth(vbuf, 2);
  int x0 = W / 2 - (tw + vw) / 2, ty = by + bh + 34;
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(typeColor(type), TFT_BLACK);
  spr.drawString(tn, x0, ty, 2);
  spr.setTextColor(vendorColor(cat), TFT_BLACK);
  spr.drawString(vbuf, x0 + tw, ty, 2);
  spr.pushSprite(0, 0);
}

// ---------- SCOPE ----------
void drawScope(const std::vector<Blip>& bl) {
  int W = spr.width(), H = spr.height();
  int cx = W / 2 + 26, cy = H / 2;     // радар сдвинут вправо, слева место под легенду
  int maxR = (H / 2) - 8;
  spr.fillSprite(TFT_BLACK);
  uint16_t ring = tft.color565(0, 70, 32);
  for (int k = 1; k <= 3; k++) spr.drawCircle(cx, cy, maxR * k / 3, ring);
  spr.drawLine(cx - maxR, cy, cx + maxR, cy, ring);
  spr.drawLine(cx, cy - maxR, cx, cy + maxR, ring);

  for (int t = 0; t < 40; t++) {
    float a = (g_sweep - t) * PI / 180.0f;
    int g = 140 - t * 3; if (g < 0) g = 0;
    spr.drawLine(cx, cy, cx + (int)(cos(a) * maxR), cy + (int)(sin(a) * maxR),
                 tft.color565(0, g, g / 2));
  }

  // блипы — только цветные точки, без букв
  for (size_t i = 0; i < bl.size(); i++) {
    float ang = (bl[i].h % 360) * PI / 180.0f;
    int pct = (bl[i].rssi + 95) * 100 / 50;
    pct = constrain(pct, 0, 100);
    int r = maxR - (maxR - 8) * pct / 100;
    int x = cx + (int)(cos(ang) * r), y = cy + (int)(sin(ang) * r);
    spr.fillCircle(x, y, 3, vendorColor(bl[i].cat));
  }

  // легенда слева: 6 самых частых производителей на экране сейчас
  int bc[N_BRANDS] = {0};
  for (size_t i = 0; i < bl.size(); i++) if (bl[i].cat < N_BRANDS) bc[bl[i].cat]++;
  spr.setTextDatum(ML_DATUM);
  for (int row = 0; row < 6; row++) {
    int bi = -1;
    for (int b = 0; b < N_BRANDS; b++) if (bc[b] > 0 && (bi < 0 || bc[b] > bc[bi])) bi = b;
    if (bi < 0) break;
    int y = 14 + row * 24;
    char lb[8]; snprintf(lb, sizeof(lb), "%s", BRANDS[bi].label);
    spr.fillCircle(7, y, 3, vendorColor(bi));
    spr.setTextColor(vendorColor(bi), TFT_BLACK);
    spr.drawString(lb, 14, y, 1);
    char nb[6]; sprintf(nb, "%d", bc[bi]);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString(nb, 14, y + 9, 1);
    bc[bi] = 0;
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(0, 170, 80), TFT_BLACK);
  spr.drawString("SCOPE", cx, 2, 2);
  char buf[16]; sprintf(buf, "%d", (int)bl.size());
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(tft.color565(90, 255, 150), TFT_BLACK);
  spr.drawString(buf, W - 4, 2, 4);

  spr.pushSprite(0, 0);
  g_sweep = (g_sweep + 6) % 360;
}

// ---------- TIMELINE: число устройств во времени ----------
void drawTimeline() {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);

  int plotL = 6, plotR = W - 6, plotT = 22, plotB = H - 22;
  int plotW = plotR - plotL, plotH = plotB - plotT;

  int peak = 1, nowv = 0;
  for (int i = 0; i < g_histLen; i++) if (g_hist[i].count > peak) peak = g_hist[i].count;
  if (g_histLen > 0) nowv = g_hist[g_histLen - 1].count;
  int ymax = peak + peak / 6 + 1;                 // запас сверху

  spr.drawRect(plotL, plotT, plotW, plotH, tft.color565(40, 40, 48));
  float step = (HIST_N > 1) ? (float)plotW / (HIST_N - 1) : plotW;

  // площадь + линия
  int prevx = 0, prevy = 0;
  for (int i = 0; i < g_histLen; i++) {
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    int y = plotB - (int)((long)g_hist[i].count * plotH / ymax);
    spr.drawFastVLine(x, y, plotB - y, tft.color565(15, 55, 35));
    if (i > 0) spr.drawLine(prevx, prevy, x, y, tft.color565(70, 220, 130));
    prevx = x; prevy = y;
  }

  // базовая линия (сглаженная) — тускло
  for (int i = 1; i < g_histLen; i++) {
    int x0 = plotR - (int)((g_histLen - 1 - (i - 1)) * step);
    int x1 = plotR - (int)((g_histLen - 1 - i) * step);
    int y0 = plotB - (int)(g_hist[i - 1].base * plotH / ymax);
    int y1 = plotB - (int)(g_hist[i].base * plotH / ymax);
    spr.drawLine(x0, y0, x1, y1, tft.color565(95, 95, 45));
  }

  // метки событий: ▲ пришли (зел), ▼ ушли (оранж)
  for (int i = 0; i < g_histLen; i++) {
    if (g_hist[i].ev == 0) continue;
    int x = plotR - (int)((g_histLen - 1 - i) * step);
    if (g_hist[i].ev > 0)
      spr.fillTriangle(x, plotT + 2, x - 4, plotT + 10, x + 4, plotT + 10, tft.color565(70, 220, 130));
    else
      spr.fillTriangle(x - 4, plotT + 2, x + 4, plotT + 2, x, plotT + 10, tft.color565(240, 150, 50));
  }

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("TIMELINE", W / 2, 4, 2);

  char buf[48];
  sprintf(buf, "now %d   peak %d", nowv, peak);
  spr.setTextDatum(BC_DATUM);
  spr.setTextColor(tft.color565(70, 220, 130), TFT_BLACK);
  spr.drawString(buf, W / 2, H - 4, 2);

  spr.pushSprite(0, 0);
}

// ---------- TYPES: кольцевая диаграмма по типам устройств ----------
void drawTypes(const int* cnt, int total) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);

  // плавно подтягиваем отображаемые доли к реальным
  float dispSum = 0;
  for (int t = 0; t < T_NUM; t++) {
    g_pieDisp[t] += 0.2f * ((float)cnt[t] - g_pieDisp[t]);
    if (g_pieDisp[t] < 0.02f) g_pieDisp[t] = 0;
    dispSum += g_pieDisp[t];
  }

  int cx = 82, cy = H / 2 + 6, R = 68, IR = 42;
  if (dispSum < 0.05f) {
    spr.drawArc(cx, cy, R, IR, 0, 360, tft.color565(35, 35, 42), TFT_BLACK, false);
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
        spr.drawArc(cx, cy, R, IR, 0, 360, typeColor(t), TFT_BLACK, false);
      } else {
        // у drawArc 0° внизу -> сдвиг на 180°, чтобы начинать сверху
        spr.drawArc(cx, cy, R, IR, (a0 + 180) % 360, (a1 + 180) % 360,
                    typeColor(t), TFT_BLACK, false);
        // тонкий разделитель между секторами
        float ar = (a1 - 90) * PI / 180.0f;
        spr.drawLine(cx + (int)(cos(ar) * IR), cy + (int)(sin(ar) * IR),
                     cx + (int)(cos(ar) * R),  cy + (int)(sin(ar) * R), TFT_BLACK);
      }
    }
  }

  // в центре — общее число
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawNumber(total, cx, cy - 4, 4);
  spr.setTextColor(tft.color565(110, 110, 125), TFT_BLACK);
  spr.drawString("devices", cx, cy + 16, 1);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tft.color565(120, 120, 135), TFT_BLACK);
  spr.drawString("TYPES", 4, 2, 2);

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
    spr.setTextColor(c, TFT_BLACK);
    spr.drawString(typeName(t), lx + 16, y, 2);
    sprintf(buf, "%d", cnt[t]);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString(buf, W - 40, y, 2);
    sprintf(buf, "%d%%", total ? (cnt[t] * 100 + total / 2) / total : 0);
    spr.setTextColor(tft.color565(110, 110, 125), TFT_BLACK);
    spr.drawString(buf, W - 4, y, 2);
  }
  if (n == 0) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(tft.color565(90, 90, 100), TFT_BLACK);
    spr.drawString("no devices", lx + (W - lx) / 2, H / 2, 2);
  }

  spr.pushSprite(0, 0);
}

void drawList(const std::map<uint32_t, Ent>& ents) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  sprintf(buf, "DEVICES  %d", (int)g_order.size());
  spr.drawString(buf, 4, 1, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("wheel: pick  click: info", W - 4, 5, 1);

  if (g_order.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString("no devices", W / 2, H / 2, 4);
    spr.pushSprite(0, 0);
    return;
  }

  for (int i = 0; i < LIST_ROWS; i++) {
    int idx = g_top + i;
    if (idx >= (int)g_order.size()) break;
    uint32_t id = g_order[idx];
    auto it = ents.find(id);
    int y = LIST_Y0 + i * LIST_ROWH;
    bool sel = (id == g_sel);
    if (sel) spr.fillRoundRect(0, y, W - 4, LIST_ROWH - 1, 3, tft.color565(28, 38, 62));
    uint16_t bg = sel ? tft.color565(28, 38, 62) : TFT_BLACK;
    if (it == ents.end()) {
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C_DIM(), bg);
      spr.drawString("(gone)", 12, y + 1, 2);
      continue;
    }
    const Ent& e = it->second;
    uint16_t tc = typeColor(e.type);
    spr.fillRect(3, y + 3, 4, LIST_ROWH - 7, tc);

    fitText(e.name[0] ? e.name : typeName(e.type), buf, sizeof(buf), 132, 2);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(e.name[0] ? TFT_WHITE : tc, bg);
    spr.drawString(buf, 12, y + 1, 2);

    spr.setTextColor(vendorColor(e.cat), bg);
    char vb[16];
    fitText(vendorLabel(e.cat, e.vkind, e.vcode), vb, sizeof(vb), 84, 2);
    spr.drawString(vb, 150, y + 1, 2);

    sprintf(buf, "%d", e.rssi);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_GRAY(), bg);
    spr.drawString(buf, 268, y + 1, 2);
    drawBars(274, y + 2, e.rssi, tft.color565(90, 220, 140));
  }

  // полоса прокрутки
  int n = (int)g_order.size();
  if (n > LIST_ROWS) {
    int trackH = LIST_ROWS * LIST_ROWH;
    int barH = trackH * LIST_ROWS / n; if (barH < 8) barH = 8;
    int barY = LIST_Y0 + (trackH - barH) * g_top / (n - LIST_ROWS);
    spr.fillRect(W - 2, LIST_Y0, 2, trackH, tft.color565(30, 30, 38));
    spr.fillRect(W - 2, barY, 2, barH, C_GRAY());
  }
  spr.pushSprite(0, 0);
}

void drawDetail(const Dev& d, const Ent& e, bool inMap, uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[96], tmp[48];
  uint16_t tc = typeColor(e.type);

  // заголовок: имя (или тип)
  fitText(d.name[0] ? d.name : typeName(e.type), tmp, sizeof(tmp), W - 70, 4);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(d.name[0] ? TFT_WHITE : tc, TFT_BLACK);
  spr.drawString(tmp, 4, 2, 4);
  int pos = -1;
  for (size_t i = 0; i < g_order.size(); i++) if (g_order[i] == g_sel) pos = (int)i;
  if (pos >= 0) {
    sprintf(buf, "%d/%d", pos + 1, (int)g_order.size());
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString(buf, W - 4, 6, 2);
  }

  // тип + производитель
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(tc, TFT_BLACK);
  spr.drawString(typeName(e.type), 4, 30, 2);
  int x = 4 + spr.textWidth(typeName(e.type), 2) + 10;
  if (e.vkind) sprintf(buf, "%s  %s%04X", vendorLabel(e.cat, e.vkind, e.vcode),
                      e.vkind == 1 ? "0x" : "svc ", e.vcode);
  else          sprintf(buf, "maker unknown");
  spr.setTextColor(vendorColor(e.cat), TFT_BLACK);
  spr.drawString(buf, x, 30, 2);

  // левая колонка
  uint32_t ago = (now - e.t) / 1000;
  int y = 50;
  const int LH = 15;
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  if (!inMap)          { spr.setTextColor(tft.color565(235, 80, 80), TFT_BLACK); sprintf(buf, "out of range"); }
  else if (ago >= 3)   sprintf(buf, "signal %d (%lus ago)", e.rssi, (unsigned long)ago);
  else                 sprintf(buf, "signal %d dBm", e.rssi);
  spr.drawString(buf, 4, y, 2); y += LH;
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  sprintf(buf, "range  %d .. %d", d.rmin, d.rmax);
  spr.drawString(buf, 4, y, 2); y += LH;
  float dist = estDistance(e.rssi);
  if (dist < 10) sprintf(buf, "dist   ~%.1f m", dist); else sprintf(buf, "dist   ~%.0f m", dist);
  spr.drawString(buf, 4, y, 2); y += LH;
  uint32_t secs = (now - (e.first ? e.first : now)) / 1000;
  sprintf(buf, "seen   %lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  spr.drawString(buf, 4, y, 2); y += LH;
  sprintf(buf, "pkts   %lu", (unsigned long)e.pkts);
  spr.drawString(buf, 4, y, 2);

  // правая колонка: график сигнала за ~30 с
  int gx = 176, gy = 50, gw = W - gx - 4, gh = 70;
  spr.drawRect(gx, gy, gw, gh, tft.color565(40, 40, 50));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("signal, 30s", gx + 3, gy + 3, 1);
  drawSpark(gx, gy, gw, gh, g_spark, g_sparkN, SPARK_N, tft.color565(90, 220, 140));

  // низ: технические подробности мелким шрифтом
  y = 126;
  macStr(d.mac, tmp);
  if (e.nMacs > 1) sprintf(buf, "MAC %s  %s  (%d MACs)", tmp, akindName(d.akind), e.nMacs);
  else             sprintf(buf, "MAC %s  %s", tmp, akindName(d.akind));
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString(buf, 4, y, 1); y += 10;

  if (d.nU) {
    int n = sprintf(buf, "SVC");
    for (uint8_t i = 0; i < d.nU && n < 52; i++) {
      const char* nm = uuidName(d.uuids[i]);
      if (!nm[0]) { const char* o = btUuidOwner(d.uuids[i]); if (o) nm = o; }
      n += snprintf(buf + n, sizeof(buf) - n, nm[0] ? " %04X %s" : " %04X", d.uuids[i], nm);
    }
    spr.drawString(buf, 4, y, 1); y += 10;
  }
  if (d.hasCid) {
    int n = sprintf(buf, "MFR %02X %02X |", d.cid & 0xFF, d.cid >> 8);
    int shown = d.mfrLen < 6 ? d.mfrLen : 6;
    for (int i = 0; i < shown; i++) n += sprintf(buf + n, " %02X", d.mfr[i]);
    sprintf(buf + n, "%s  (len %d)", d.mfrLen > 6 ? " .." : "", d.mfrLen);
    spr.drawString(buf, 4, y, 1); y += 10;
  }
  if (d.appearance) {
    sprintf(buf, "APP 0x%04X", d.appearance);
    spr.drawString(buf, 4, y, 1);
  }
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString("click: back", W - 4, H - 1, 1);
  spr.pushSprite(0, 0);
}

void drawWeather(const std::vector<Sensor>& ss, uint32_t now) {
  int W = spr.width(), H = spr.height();
  spr.fillSprite(TFT_BLACK);
  char buf[48];

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_GRAY(), TFT_BLACK);
  spr.drawString("WEATHER", 4, 1, 2);

  if (ss.empty()) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_GRAY(), TFT_BLACK);
    spr.drawString("no sensors yet", W / 2, H / 2 - 16, 4);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString("Xiaomi PVVX/ATC, BTHome, Govee,", W / 2, H / 2 + 14, 2);
    spr.drawString("Qingping, SwitchBot, RuuviTag", W / 2, H / 2 + 32, 2);
    spr.pushSprite(0, 0);
    return;
  }

  const int PER = 3, RH = 50;
  int pages = ((int)ss.size() + PER - 1) / PER;
  int page = pages > 1 ? (int)((now / 4000) % pages) : 0;

  sprintf(buf, "%d sensor%s", (int)ss.size(), ss.size() == 1 ? "" : "s");
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_DIM(), TFT_BLACK);
  spr.drawString(buf, W - 4 - (pages > 1 ? pages * 8 + 4 : 0), 1, 2);
  for (int p = 0; p < pages && pages > 1; p++)
    spr.fillCircle(W - 4 - (pages - 1 - p) * 8 - 3, 9, 2, p == page ? TFT_WHITE : C_DIM());

  for (int i = 0; i < PER; i++) {
    int k = page * PER + i;
    if (k >= (int)ss.size()) break;
    const Sensor& S = ss[k];
    int y = 20 + i * RH;
    if (i > 0) spr.drawFastHLine(4, y - 3, W - 8, tft.color565(30, 30, 38));

    // подпись: имя или модель + хвост MAC
    if (S.name[0]) fitText(S.name, buf, sizeof(buf), 150, 2);
    else sprintf(buf, "%s %02X%02X", S.r.model, S.mac[1], S.mac[0]);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_GRAY(), TFT_BLACK);
    spr.drawString(buf, 6, y, 2);

    uint32_t ago = (now - S.t) / 1000;
    char right[32] = "";
    int n = 0;
    if (S.r.hasB) n += sprintf(right + n, "bat %d%%  ", S.r.batt);
    if (ago < 60) sprintf(right + n, "%lus", (unsigned long)ago);
    else          sprintf(right + n, "%lum", (unsigned long)(ago / 60));
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(C_DIM(), TFT_BLACK);
    spr.drawString(right, W - 6, y, 2);

    int vy = y + 17;
    int x = 6;
    if (S.r.hasT) {
      sprintf(buf, "%.1f", S.r.temp);
      uint16_t c = tempColor(S.r.temp);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(c, TFT_BLACK);
      spr.drawString(buf, x, vy, 4);
      x += spr.textWidth(buf, 4) + 3;
      spr.drawCircle(x + 2, vy + 4, 2, c);          // значок градуса
      spr.drawString("C", x + 7, vy, 4);
    }
    if (S.r.hasH) {
      sprintf(buf, "%.0f%%", S.r.hum);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(tft.color565(90, 170, 245), TFT_BLACK);
      spr.drawString(buf, 130, vy, 4);
    }
    int ex = 214, ey = vy + 5;
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C_GRAY(), TFT_BLACK);
    if (S.r.hasP)   { sprintf(buf, "%.0f hPa", S.r.press); spr.drawString(buf, ex, ey, 2); ey += 15; }
    if (S.r.hasCO2) { sprintf(buf, "%d ppm", S.r.co2);    spr.drawString(buf, ex, ey, 2); }
  }
  spr.pushSprite(0, 0);
}

