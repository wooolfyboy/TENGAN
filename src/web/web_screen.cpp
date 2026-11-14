// ============================================================
//  web_screen.cpp — экран раздела Web: QR-код для подключения,
//  имя сети, пароль и адрес страницы
// ============================================================
#include "web/web.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"

void drawWebScreen(const WebView& v, const uint8_t* qr, int n) {
  const int W = spr.width();
  char buf[40];
  spr.fillSprite(C_BG());
  if (!v.ok) {
    uiHeader("Web", "error", true);
    uiEmpty("hotspot did not start", "side: back, then try again");
    uiFooter("", "side: back");
    present();
    return;
  }
  snprintf(buf, sizeof(buf), v.clients ? "%d connected" : "waiting", v.clients);
  uiHeader("Web", buf, false);
  // QR: тёмные модули на светлом поле (так его читают камеры)
  int scale = n > 0 ? min(4, 116 / (n + 4)) : 3;
  int side = (n + 4) * scale, qx = UI_M + 2, qy = UI_TOP + 4;
  spr.fillRect(qx, qy, side, side, TFT_WHITE);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (qr[y * n + x]) spr.fillRect(qx + (x + 2) * scale, qy + (y + 2) * scale, scale, scale, TFT_BLACK);

  int tx = qx + side + 12, y = UI_TOP + 6;
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_ACC());
  txt("1  SCAN QR OR JOIN WI-FI", tx, y, 1);
  uiReadout(tx, y + 12, "network", v.ssid, C_HOT());
  uiReadout(tx, y + 40, "password", v.pass, C_HOT());
  spr.setTextColor(C_ACC());
  txt("2  OPEN IN BROWSER", tx, y + 72, 1);
  spr.setTextColor(C_HOT());
  txt(v.url, tx, y + 84, 2);
  spr.setTextColor(UI_DIM());
  if (v.tab && v.tab[0]) { char tb[16]; size_t k = 0; for (; v.tab[k] && k < 15; k++) tb[k] = (char)toupper((unsigned char)v.tab[k]); tb[k] = 0;
                          snprintf(buf, sizeof(buf), "VIEWING: %s", tb); }
  else snprintf(buf, sizeof(buf), "PHONE OR COMPUTER  REQ %lu", (unsigned long)v.reqs);
  txt(buf, tx, y + 106, 1);
  uiFooter("bluetooth + wi-fi live", "side: back");
  present();
  (void)W;
}

// загрузка прошивки по Wi-Fi: рисуем и сразу выводим (сервер в это время занят)
void drawWebOta(const WebView& v, int pct, const char* err) {
  const int W = spr.width();
  char buf[40];
  spr.fillSprite(C_BG());
  bool bad = pct == -2;
  uiHeader("Firmware update", bad ? "failed" : pct >= 100 ? "done" : "receiving", bad);
  int bx = UI_M + 12, bw = W - 2 * bx, by = 76;
  spr.setTextDatum(TC_DATUM);
  if (bad) {
    spr.setTextColor(C_ALERT());
    txt("UPDATE FAILED", W / 2, 50, 4);
    spr.setTextColor(UI_LABEL());
    txt(err && err[0] ? err : "unknown error", W / 2, 84, 2);
    spr.setTextColor(UI_DIM());
    txt("OLD FIRMWARE KEPT - NOTHING CHANGED", W / 2, 110, 1);
  } else {
    snprintf(buf, sizeof(buf), "%d%%", max(0, pct));
    spr.setTextColor(C_HOT());
    txt(pct >= 100 ? "RESTARTING" : buf, W / 2, 38, 4);
    spr.drawRect(bx, by, bw, 14, UI_LINE());
    spr.fillRect(bx + 2, by + 2, (bw - 4) * constrain(pct, 0, 100) / 100, 10, C_ACC());
    spr.setTextColor(UI_DIM());
    txt("DO NOT TURN OFF", W / 2, by + 26, 1);
    txt("IF THE NEW FIRMWARE FAILS, THE OLD ONE COMES BACK", W / 2, by + 40, 1);
  }
  uiFooter("", "");
  present();
  (void)v;
}
