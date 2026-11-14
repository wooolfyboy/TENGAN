// ============================================================
//  display.cpp — экран и общие «кирпичики» интерфейса
// ============================================================
#include "core/display.h"
#include "core/theme.h"

TFT_eSPI    tft;
TFT_eSprite spr = TFT_eSprite(&tft);

void displayBegin() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(C_BG());
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  spr.setColorDepth(16);
  if (!spr.createSprite(tft.width(), tft.height())) {   // не хватило памяти — 8 бит на точку
    spr.setColorDepth(8);
    spr.createSprite(tft.width(), tft.height());
  }
}

uint16_t C_GRAY() { return C(120, 120, 135); }
uint16_t C_DIM()  { return C(80, 80, 92); }

void macStr(const uint8_t* m, char* out) {   // m — младший байт первым
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[5], m[4], m[3], m[2], m[1], m[0]);
}

// укоротить строку до ширины px (font), добавив ".."
void fitText(const char* src, char* dst, size_t cap, int px, int font) {
  strncpy(dst, src, cap - 1); dst[cap - 1] = 0;
  if (txtW(dst, font) <= px) return;
  size_t n = strlen(dst);
  while (n > 1) {
    dst[--n] = 0;
    char tmp[48]; snprintf(tmp, sizeof(tmp), "%s..", dst);
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
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(acc, C_BG());
  txt(title, 4, 1, 2);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C(80, 80, 92), C_BG());
  txt("click: open   side: back", W - 4, 5, 1);
  int rh = n > 5 ? 21 : 28, y0 = 20;
  for (int i = 0; i < n; i++) {
    int y = y0 + i * rh;
    bool isSel = (i == sel);
    uint16_t bg = isSel ? C(22, 26, 38) : C_BG();
    if (isSel) {
      spr.fillRoundRect(0, y, W, rh - 2, 5, bg);
      spr.fillRoundRect(0, y + 3, 4, rh - 8, 2, acc);
    }
    int ty = y + (rh - 2) / 2;
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(isSel ? C_HOT() : C(170, 170, 185), bg);
    txt(it[i].name, 12, ty, 2);
    spr.setTextColor(isSel ? C(130, 130, 145) : C(75, 75, 88), bg);
    txt(it[i].desc, 104, ty + 1, 1);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(isSel ? acc : C(110, 110, 125), bg);
    txt(it[i].value, W - 8, ty, 2);
  }
  present();
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
