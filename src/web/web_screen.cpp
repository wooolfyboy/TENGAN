// ============================================================
//  web_screen.cpp — экран раздела Web: QR-код для подключения,
//  имя сети, пароль и адрес страницы
// ============================================================
#include "web/web.h"
#include "core/lang.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"

void drawWebScreen(const WebView& v, const uint8_t* qr, int n) {
  const int W = spr.width();
  char buf[40];
  spr.fillSprite(C_BG());
  if (!v.ok) {
    uiHeader(L("Web", "Веб"), L("error", "ошибка"), true);
    uiEmpty(L("access point did not start", "точка доступа не запустилась"), L("go back and try again", "выйдите и попробуйте снова"));
    uiFooter("", L("side: back", "кнопка: назад"));
    present();
    return;
  }
  if (v.clients) snprintf(buf, sizeof(buf), L("%d connected", "подключено: %d"), v.clients); else snprintf(buf, sizeof(buf), "%s", L("waiting", "ожидание"));
  uiHeader(L("Web", "Веб"), buf, false);
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
  txt(L("1  SCAN THE QR OR JOIN WI-FI", "1  QR-КОД ИЛИ ВРУЧНУЮ"), tx, y, 1);
  uiReadout(tx, y + 12, L("network", "сеть"), v.ssid, C_HOT());
  uiReadout(tx, y + 40, L("password", "пароль"), v.pass, C_HOT());
  spr.setTextColor(C_ACC());
  txt(L("2  OPEN IN A BROWSER", "2  ОТКРОЙТЕ В БРАУЗЕРЕ"), tx, y + 72, 1);
  spr.setTextColor(C_HOT());
  txt(v.url, tx, y + 84, 2);
  spr.setTextColor(UI_DIM());
  if (v.tab && v.tab[0]) { char tb[32];
                          const char* tl = v.tab;
                          if (g_lang) {
                            static const char* const TM[][2] = { { "overview", "обзор" }, { "bluetooth", "bluetooth" }, { "wi-fi", "wi-fi" }, { "sub-ghz", "sub-ghz" }, { "sound", "звук" }, { "system", "система" }, { "update", "обновление" } };
                            for (auto& m : TM) if (!strcmp(v.tab, m[0])) tl = m[1];
                          }
                          utf8Upper(tl, tb, sizeof tb);
                          snprintf(buf, sizeof(buf), L("VIEWING: %s", "ОТКРЫТО: %s"), tb); }
  else snprintf(buf, sizeof(buf), "%s", L("PHONE, TABLET OR COMPUTER", "ТЕЛЕФОН, ПЛАНШЕТ ИЛИ ПК"));
  txt(buf, tx, y + 106, 1);
  uiFooter(L("Bluetooth and Wi-Fi live", "Bluetooth и Wi-Fi в эфире"), L("side: back", "кнопка: назад"));
  present();
  (void)W;
}

// загрузка прошивки по Wi-Fi: рисуем и сразу выводим (сервер в это время занят)
void drawWebOta(const WebView& v, int pct, const char* err) {
  const int W = spr.width();
  char buf[40];
  spr.fillSprite(C_BG());
  bool bad = pct == -2;
  uiHeader(L("Firmware update", "Обновление"), bad ? L("failed", "ошибка") : pct >= 100 ? L("done", "готово") : L("receiving", "приём"), bad);
  int bx = UI_M + 12, bw = W - 2 * bx, by = 76;
  spr.setTextDatum(TC_DATUM);
  if (bad) {
    spr.setTextColor(C_ALERT());
    txt(L("UPDATE FAILED", "НЕ УДАЛОСЬ"), W / 2, 50, 4);
    spr.setTextColor(UI_LABEL());
    txt(err && err[0] ? err : L("unknown error", "неизвестная ошибка"), W / 2, 84, 2);
    spr.setTextColor(UI_DIM());
    txt(L("THE PREVIOUS FIRMWARE IS KEPT", "ПРЕЖНЯЯ ПРОШИВКА СОХРАНЕНА"), W / 2, 110, 1);
  } else {
    snprintf(buf, sizeof(buf), "%d%%", max(0, pct));
    spr.setTextColor(C_HOT());
    txt(pct >= 100 ? L("RESTARTING", "ПЕРЕЗАПУСК") : buf, W / 2, 38, 4);
    spr.drawRect(bx, by, bw, 14, UI_LINE());
    spr.fillRect(bx + 2, by + 2, (bw - 4) * constrain(pct, 0, 100) / 100, 10, C_ACC());
    spr.setTextColor(UI_DIM());
    txt(L("DO NOT TURN OFF", "НЕ ВЫКЛЮЧАЙТЕ"), W / 2, by + 26, 1);
    txt(L("IF THE NEW FIRMWARE FAILS, THE OLD ONE COMES BACK", "ЕСЛИ НОВАЯ НЕ ЗАПУСТИТСЯ, ВЕРНЁТСЯ ПРЕЖНЯЯ"), W / 2, by + 40, 1);
  }
  uiFooter("", "");
  present();
  (void)v;
}

// ============================================================
//  Планшет подключён — вместо пароля живая картинка по теме:
//   «небесный глаз» (天眼): оглядывается, кольца радужки вращаются, моргает.
//  Колесо или нажатие — снова QR и пароль (web.cpp).
// ============================================================
#include "core/settings.h"

static float ease(float a, float b, float k) { return a + (b - a) * k; }

static void drawEye(uint32_t now, int cx, int cy) {
  static float lx = 0, ly = 0, tx = 0, ty = 0;
  static uint32_t nextLook = 0, blinkAt = 0;
  if (!blinkAt) blinkAt = now + 2500;
  if ((int32_t)(now - nextLook) > 0) { tx = random(-26, 27); ty = random(-7, 8); nextLook = now + 900 + random(2600); }
  lx = ease(lx, tx, 0.14f); ly = ease(ly, ty, 0.14f);
  float open = 1;                                        // моргание: 260 мс, раз в 3–7 с
  if ((int32_t)(now - blinkAt) >= 0) {
    float k = (now - blinkAt) / 260.0f;
    if (k >= 1) { blinkAt = now + 3000 + random(4000); if (random(5) == 0) blinkAt = now + 350; }   // иногда — двойное
    else open = fabsf(cosf(k * PI));
  }
  const int Wh = 112, Hh = 38;
  auto lid = [&](int x) { float u = (float)(x - cx) / Wh; return Hh * open * (1 - u * u); };
  // пульс вокруг: расходящиеся кольца
  for (int i = 0; i < 3; i++) {
    float ph = fmodf(now / 2400.0f + i / 3.0f, 1.0f);
    int r = 46 + (int)(ph * 90);
    spr.drawEllipse(cx, cy, r * 2, r * 0.7f, scale565(C_ACC(), (int)(40 * (1 - ph))));
  }
  // радужка: кольца, вращающиеся риски, зрачок-щель с бликом
  int ix = cx + (int)lx, iy = cy + (int)ly;
  spr.fillCircle(ix, iy, 31, scale565(C_ACC(), 16));
  spr.drawCircle(ix, iy, 31, C_ACC());
  spr.drawCircle(ix, iy, 24, scale565(C_ACC(), 60));
  for (int k = 0; k < 24; k++) {
    float a = (k * 15 + now * 0.03f) * DEG_TO_RAD, b = (k * 15 - now * 0.05f) * DEG_TO_RAD;
    spr.drawLine(ix + cosf(a) * 25, iy + sinf(a) * 25, ix + cosf(a) * 30, iy + sinf(a) * 30, k % 3 ? scale565(C_ACC(), 70) : C_HOT());
    if (k % 2 == 0) spr.drawLine(ix + cosf(b) * 14, iy + sinf(b) * 14, ix + cosf(b) * 21, iy + sinf(b) * 21, heatV(0.5f + 0.5f * (k % 4 == 0)));
  }
  spr.fillEllipse(ix, iy, 5, 11, C_BG());                // зрачок — вертикальная щель
  spr.drawEllipse(ix, iy, 5, 11, C_HOT());
  spr.fillCircle(ix + 7, iy - 9, 3, C_HOT());            // блик
  // веки: закрашиваем всё вне миндалины и рисуем контур
  for (int x = cx - Wh - 2; x <= cx + Wh + 2; x++) {
    int h = (int)lid(x);
    if (h < 0) h = 0;
    spr.drawFastVLine(x, cy - 44, 44 - h, C_BG());
    spr.drawFastVLine(x, cy + h + 1, 44 - h, C_BG());
  }
  int px = cx - Wh, pu = cy, pd = cy;
  for (int x = cx - Wh + 2; x <= cx + Wh; x += 2) {
    int h = (int)lid(x), yu = cy - h, yd = cy + h;
    spr.drawLine(px, pu, x, yu, C_ACC()); spr.drawLine(px, pd, x, yd, C_ACC());
    px = x; pu = yu; pd = yd;
  }
  // уголки глаза — штрихи
  spr.drawLine(cx - Wh - 10, cy, cx - Wh - 2, cy, C_HOT());
  spr.drawLine(cx + Wh + 2, cy, cx + Wh + 10, cy, C_HOT());
}

#include "core/crumb.h"
void drawWebLink(const WebLink& l, uint32_t now) {
  crumb("draw link");
  const int W = spr.width(), H = spr.height(), cx = W / 2, cy = H / 2 - 6;
  spr.fillSprite(C_BG());
  drawEye(now, cx, cy);
  // подпись — мелко, чтобы не мешать картинке
  char buf[48];
  float kb = l.bps / 1024.0f;
  snprintf(buf, sizeof(buf), L("APP CONNECTED  ·  %.1f KB/S", "ПРИЛОЖЕНИЕ  ·  %.1f КБ/С"), kb);
  spr.setTextDatum(BL_DATUM);
  spr.setTextColor(UI_DIM());
  txt(buf, UI_M + 2, H - 4, 1);
  spr.setTextDatum(BR_DATUM);
  txt(L("WHEEL: PASSWORD", "КОЛЕСО: ПАРОЛЬ"), W - UI_M - 2, H - 4, 1);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(UI_LINE());
  txt(l.streams, UI_M + 2, 4, 1);
  present();
}
