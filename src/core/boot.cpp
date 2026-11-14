// ============================================================
//  boot.cpp — заставка при включении (~1.5 с)
// ============================================================
#include "core/boot.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/battery.h"

void bootScreen() {
  char bat[24];
  if (g_bat.ok) snprintf(bat, sizeof(bat), "%d%%", g_bat.soc); else snprintf(bat, sizeof(bat), "n/a");
  char theme[16];
  snprintf(theme, sizeof(theme), "%s", themeName(g_theme));
  for (char* p = theme; *p; p++) *p = (char)toupper((unsigned char)*p);
  const char* labels[] = { "BLUETOOTH RADIO", "WI-FI RADIO", "SUB-GHZ CC1101", "BATTERY", "DISPLAY" };
  const char* vals[]   = { "OK", "OK", "STANDBY", bat, theme };
  const int N = 5;
  for (int step = 0; step <= N + 1; step++) {
    int W = spr.width();
    spr.fillSprite(C_BG());
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(C(90, 230, 150));
    txt("RADIO RADAR", 10, 6, 4);
    spr.setTextColor(C(120, 120, 135));
    txt("receive-only radio analyzer", 12, 36, 1);
    spr.drawFastHLine(10, 48, W - 20, C(70, 70, 82));
    for (int i = 0; i < step && i < N; i++) {
      int y = 56 + i * 18;
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C(150, 150, 165));
      txt("> ", 12, y, 2);
      txt(labels[i], 28, y, 2);
      // точки-заполнитель между названием и результатом
      for (int x = 28 + txtW(labels[i], 2) + 6; x < W - 100; x += 6) spr.fillRect(x, y + 9, 2, 2, C(70, 70, 82));
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(C(90, 230, 150));
      txt(vals[i], W - 14, y, 2);
    }
    if (step > N) {
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C_HOT());
      txt("READY_", 12, 56 + N * 18 + 4, 2);
    }
    present();
    delay(step > N ? 450 : 170);
  }
}
