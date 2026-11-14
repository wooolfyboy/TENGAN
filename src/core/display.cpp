// ============================================================
//  display.cpp — экран и общие «кирпичики» интерфейса
// ============================================================
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"

TFT_eSPI    tft;
TFT_eSprite spr = TFT_eSprite(&tft);

void displayBegin() {
  // подсветка включается только после очистки экрана: иначе на долю секунды
  // видно «мусор» из памяти дисплея (часто красный)
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, LOW);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(C_BG());
  delay(20);
  digitalWrite(TFT_BL, HIGH);
  spr.setColorDepth(16);
  if (!spr.createSprite(tft.width(), tft.height())) {   // не хватило памяти — 8 бит на точку
    spr.setColorDepth(8);
    spr.createSprite(tft.width(), tft.height());
  }
}


bool g_fastFrame = false;

void macStr(const uint8_t* m, char* out) {   // m — младший байт первым
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[5], m[4], m[3], m[2], m[1], m[0]);
}

// укоротить строку до ширины px (font), добавив ".."
void fitText(const char* src, char* dst, size_t cap, int px, int font) {
  strncpy(dst, src, cap - 1); dst[cap - 1] = 0;
  size_t n = strlen(dst);
  if (strlen(src) > n) {                                      // обрезали копированием — не оставить «пол-буквы»
    size_t k = n;
    while (k && ((uint8_t)dst[k - 1] & 0xC0) == 0x80) k--;    // k-1 — начало последнего символа
    if (k) {
      uint8_t lead = (uint8_t)dst[k - 1];
      size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
      if (n - (k - 1) < need) n = k - 1;
    }
    dst[n] = 0;
  }
  if (txtW(dst, font) <= px) return;
  while (n > 1) {
    n--;                                                      // убрать последний символ целиком (UTF-8)
    while (n > 0 && ((uint8_t)dst[n] & 0xC0) == 0x80) n--;
    dst[n] = 0;
    char tmp[96]; snprintf(tmp, sizeof(tmp), "%s..", dst);
    if (txtW(tmp, font) <= px) { strcpy(dst, tmp); return; }
  }
}

// 4 столбика уровня сигнала
void drawBars(int x, int y, int rssi, uint16_t col) {
  int lvl = rssi > -60 ? 4 : rssi > -70 ? 3 : rssi > -80 ? 2 : rssi > -90 ? 1 : 0;
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 3;
    spr.fillRect(x + i * 6, y + 12 - h, 4, h, i < lvl ? col : C(45, 45, 55));
  }
}

void drawSpark(int gx, int gy, int gw, int gh, const int8_t* buf, int n, int cap, uint16_t col) {
  int px = -1, py = -1;
  for (int i = 0; i < n; i++) {
    int v = buf[i];
    int xx = gx + 1 + (gw - 3) * (i + (cap - n)) / (cap - 1);
    if (v <= -127) { px = -1; continue; }
    int c = constrain(v, -100, -30);
    int yy = gy + gh - 2 - (c + 100) * (gh - 4) / 70;
    if (px >= 0) spr.drawLine(px, py, xx, yy, col);
    else         spr.drawPixel(xx, yy, col);
    px = xx; py = yy;
  }
}

void drawSubmenu(const char* title, uint16_t acc, SubItem* it, int n, int sel) {
  int W = spr.width();
  spr.fillSprite(C_BG());
  uiHeader(title, "click: open");
  int rh = n > 5 ? 18 : 25, y0 = UI_TOP + 1;
  for (int i = 0; i < n; i++) {
    int y = y0 + i * rh, cy = y + (rh - 2) / 2;
    bool isSel = (i == sel);
    if (isSel) uiRowHighlight(y, rh - 2);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(isSel ? C_HOT() : UI_LABEL());
    txt(it[i].name, UI_M + 8, cy + 1, n > 5 ? 2 : 4);
    char d[32]; size_t k = 0;
    for (; it[i].desc[k] && k < sizeof(d) - 1; k++) d[k] = (char)toupper((unsigned char)it[i].desc[k]);
    d[k] = 0;
    spr.setTextColor(isSel ? UI_LABEL() : UI_DIM());
    txt(d, 118, cy + 1, 1);
    char v[24]; fitText(it[i].value, v, sizeof(v), 84, 2);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(isSel ? acc : C_HOT());
    txt(v, W - UI_M - 8, cy + 1, 2);
  }
  uiFooter("wheel: choose", "side: back");
  present();
}

void hudPanel(int x, int y, int w, int h, int cut, uint16_t col, bool filled, uint16_t bg) {
  if (filled) {
    spr.fillRect(x, y, w, h, col);
    spr.fillTriangle(x + w - cut, y, x + w, y, x + w, y + cut, bg);   // срез справа сверху
    spr.fillTriangle(x, y + h - cut, x, y + h, x + cut, y + h, bg);   // срез слева снизу
    return;
  }
  spr.drawFastHLine(x, y, w - cut, col);
  spr.drawLine(x + w - cut, y, x + w - 1, y + cut, col);
  spr.drawFastVLine(x + w - 1, y + cut, h - cut, col);
  spr.drawFastHLine(x + cut, y + h - 1, w - cut, col);
  spr.drawLine(x, y + h - cut, x + cut, y + h - 1, col);
  spr.drawFastVLine(x, y, h - cut, col);
}

void drawArcGauge(int cx, int cy, int r, int thick, int pct, uint16_t col, uint16_t track, uint16_t bg) {
  pct = constrain(pct, 0, 100);
  // у drawArc 0° — внизу, по часовой; верхний полукруг — от 90° до 270°
  spr.drawArc(cx, cy, r, r - thick, 90, 270, track, bg, false);
  int a1 = 90 + 180 * pct / 100;
  if (a1 > 90) spr.drawArc(cx, cy, r, r - thick, 90, a1, col, bg, false);
  for (int k = 0; k <= 12; k++) {                         // деления снаружи
    float a = PI + k * PI / 12;
    int r0 = r + 3, r1 = r + (k % 3 == 0 ? 9 : 6);
    spr.drawLine(cx + cosf(a) * r0, cy + sinf(a) * r0, cx + cosf(a) * r1, cy + sinf(a) * r1, k * 100 / 12 <= pct ? col : track);
  }
}

// --- иконки главного меню ---
// --- иконки ---
void iconBluetooth(int x, int y, uint16_t c, uint16_t bg) {
  float w = 3.5f;
  spr.drawWideLine(x, y - 22, x, y + 22, w, c, bg);
  spr.drawWideLine(x, y - 22, x + 12, y - 11, w, c, bg);
  spr.drawWideLine(x + 12, y - 11, x - 11, y + 10, w, c, bg);
  spr.drawWideLine(x, y + 22, x + 12, y + 11, w, c, bg);
  spr.drawWideLine(x + 12, y + 11, x - 11, y - 10, w, c, bg);
}
void iconWifi(int x, int y, uint16_t c, uint16_t bg) {
  int cy = y + 16;
  for (int i = 0; i < 3; i++) {
    int r = 12 + i * 11;
    spr.drawArc(x, cy, r, r - 5, 135, 225, c, bg, true);   // дуги вверх
  }
  spr.fillSmoothCircle(x, cy - 1, 4, c, bg);
}
void iconGear(int x, int y, uint16_t c, uint16_t bg) {
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    spr.drawWideLine(x + cos(a) * 14, y + sin(a) * 14, x + cos(a) * 22, y + sin(a) * 22, 7, c, bg);
  }
  spr.fillSmoothCircle(x, y, 17, c, bg);
  spr.fillSmoothCircle(x, y, 7, bg, c);
}
// антенна с волнами (Sub-GHz)
void iconAntenna(int x, int y, uint16_t c, uint16_t bg) {
  spr.drawWideLine(x, y - 8, x, y + 22, 3.5f, c, bg);
  spr.drawWideLine(x, y + 2, x - 9, y + 22, 3, c, bg);
  spr.drawWideLine(x, y + 2, x + 9, y + 22, 3, c, bg);
  spr.fillSmoothCircle(x, y - 10, 4, c, bg);
  for (int i = 0; i < 2; i++) {
    int r = 11 + i * 8;
    spr.drawArc(x, y - 10, r, r - 3, 225, 315, c, bg, true);    // волны слева
    spr.drawArc(x, y - 10, r, r - 3, 45, 135, c, bg, true);     // волны справа
  }
}

// микрофон (Sound)
void iconMic(int x, int y, uint16_t c, uint16_t bg) {
  spr.fillSmoothRoundRect(x - 7, y - 20, 14, 26, 7, c, bg);
  spr.drawArc(x, y - 2, 13, 10, 270, 90, c, bg, true);   // «подкова» под капсюлем
  spr.drawWideLine(x, y + 12, x, y + 20, 3, c, bg);
  spr.drawWideLine(x - 8, y + 21, x + 8, y + 21, 3, c, bg);
}

// куб в кубе (Gallery)
void iconCube(int x, int y, uint16_t c, uint16_t bg) {
  const int o = 14, i = 7, d = 5;
  spr.drawRect(x - o - d / 2, y - o + d / 2, 2 * o, 2 * o, c);
  spr.drawRect(x - o + d / 2 + 1, y - o - d / 2, 2 * o, 2 * o, c);
  spr.drawRect(x - i, y - i, 2 * i, 2 * i, c);
  spr.drawLine(x - o - d / 2, y - o + d / 2, x - i, y - i, c);
  spr.drawLine(x + o - d / 2 - 1, y + o + d / 2 - 1, x + i - 1, y + i - 1, c);
  spr.drawLine(x + o + d / 2, y - o - d / 2, x + i - 1, y - i, c);
  spr.drawLine(x - o + d / 2 + 1, y + o - d / 2 - 1, x - i, y + i - 1, c);
  (void)bg;
}

// телефон с волнами (Web)
void iconPhone(int x, int y, uint16_t c, uint16_t bg) {
  spr.drawSmoothRoundRect(x - 9, y - 17, 3, 2, 18, 34, c, bg);
  spr.drawFastHLine(x - 3, y + 12, 6, c);
  spr.drawArc(x + 8, y - 13, 7, 5, 180, 270, c, bg, true);
  spr.drawArc(x + 8, y - 13, 12, 10, 180, 270, c, bg, true);
}

// ноутбук (PC)
void iconLaptop(int x, int y, uint16_t c, uint16_t bg) {
  spr.drawRoundRect(x - 17, y - 16, 34, 23, 2, c);
  spr.fillRect(x - 13, y - 12, 26, 15, c);
  spr.fillRect(x - 11, y - 10, 22, 11, bg);
  spr.drawLine(x - 9, y - 1, x - 4, y - 6, c); spr.drawLine(x - 4, y - 6, x + 1, y - 3, c); spr.drawLine(x + 1, y - 3, x + 8, y - 9, c);
  spr.fillRoundRect(x - 22, y + 9, 44, 5, 2, c);
}
