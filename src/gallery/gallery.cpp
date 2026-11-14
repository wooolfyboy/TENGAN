// ============================================================
//  gallery.cpp — раздел Gallery
//
//  TESSERACT — четырёхмерный куб: вращается в плоскостях XW, YW, ZW
//              и XY, проецируется 4D → 3D → экран; внутренний куб
//              «выворачивается» через внешний. След — три прошлых кадра.
//  TORUS     — бублик из ~900 точек; яркость — освещение и глубина.
//
//  Колесо — скорость, нажатие — назад в подменю.
// ============================================================
#include <algorithm>
#include "gallery/gallery.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/ui.h"
#include "core/menu.h"

enum { G_TESS = 0, G_TORUS, G_NUM };
static int   g_gal = 0;
static float s_speed = 1.0f;                // множитель скорости (колесо)
static float s_phase = 0;                   // «время» анимации с учётом скорости
static uint32_t s_lastMs = 0;

static uint16_t shade(uint16_t c, int pct) {
  uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return tft.color565(r * pct / 100, g * pct / 100, b * pct / 100);
}

static void advance(uint32_t now) {
  uint32_t dt = s_lastMs ? min<uint32_t>(now - s_lastMs, 100) : 33;
  s_lastMs = now;
  s_phase += dt * 0.001f * s_speed;
}

static void speedLabel(char* buf, size_t n) { snprintf(buf, n, "speed x%.1f", s_speed); }

// ---------- TESSERACT ----------
#define TS_TRAIL 3
static int16_t s_trail[TS_TRAIL][16][2];    // экранные точки прошлых кадров
static int     s_trailN = 0, s_trailHead = 0;

void drawTesseract(uint32_t now) {
  const int W = spr.width(), H = spr.height();
  g_fastFrame = true;
  advance(now);
  spr.fillSprite(C_BG());
  const float t = s_phase;
  const float a1 = t * 0.9f, a2 = t * 0.55f, a3 = t * 0.35f, a4 = t * 0.25f;
  const int cx = W / 2, cy = (UI_TOP + UI_BOTTOM) / 2 + 2;

  float px[16], py[16], pw[16];
  for (int i = 0; i < 16; i++) {
    float x = (i & 1) ? 1 : -1, y = (i & 2) ? 1 : -1, z = (i & 4) ? 1 : -1, w = (i & 8) ? 1 : -1;
    float c, s, tx;
    c = cosf(a1); s = sinf(a1); tx = x * c - w * s; w = x * s + w * c; x = tx;   // XW
    c = cosf(a2); s = sinf(a2); tx = y * c - w * s; w = y * s + w * c; y = tx;   // YW
    c = cosf(a3); s = sinf(a3); tx = z * c - w * s; w = z * s + w * c; z = tx;   // ZW
    c = cosf(a4); s = sinf(a4); tx = x * c - y * s; y = x * s + y * c; x = tx;   // XY
    float k4 = 2.6f / (3.2f - w);                         // 4D → 3D (перспектива по w)
    x *= k4; y *= k4; z *= k4;
    c = cosf(0.5f); s = sinf(0.5f);                       // лёгкий наклон камеры
    tx = y * c - z * s; z = y * s + z * c; y = tx;
    float k3 = 175.0f / (5.2f - z);                       // 3D → экран
    px[i] = cx + x * k3; py[i] = cy + y * k3; pw[i] = w;
  }
  // след: прошлые кадры всё тусклее
  for (int f = 0; f < s_trailN; f++) {
    int slot = (s_trailHead - 1 - f + TS_TRAIL * 2) % TS_TRAIL;
    uint16_t col = shade(C_ACC(), 30 - f * 9);
    for (int i = 0; i < 16; i++)
      for (int b = 0; b < 4; b++) {
        int j = i ^ (1 << b);
        if (j > i) spr.drawLine(s_trail[slot][i][0], s_trail[slot][i][1], s_trail[slot][j][0], s_trail[slot][j][1], col);
      }
  }
  // рёбра: соединяют вершины, отличающиеся одной координатой. Цвет — по «глубине» в 4D
  for (int i = 0; i < 16; i++)
    for (int b = 0; b < 4; b++) {
      int j = i ^ (1 << b);
      if (j < i) continue;
      float w = (pw[i] + pw[j]) * 0.5f;                   // -1.4 .. 1.4
      uint16_t col = heatV(constrain(0.35f + w * 0.35f, 0.1f, 1.0f));
      spr.drawLine(px[i], py[i], px[j], py[j], col);
      spr.drawLine(px[i] + 1, py[i], px[j] + 1, py[j], shade(col, 45));   // «толщина»
    }
  for (int i = 0; i < 16; i++) {
    int r = pw[i] > 0.3f ? 3 : 2;
    spr.fillCircle(px[i], py[i], r, pw[i] > 0 ? C_HOT() : C_ACC());
  }
  for (int i = 0; i < 16; i++) { s_trail[s_trailHead][i][0] = px[i]; s_trail[s_trailHead][i][1] = py[i]; }
  s_trailHead = (s_trailHead + 1) % TS_TRAIL;
  if (s_trailN < TS_TRAIL) s_trailN++;

  char buf[24]; speedLabel(buf, sizeof(buf));
  uiHeader("Tesseract", buf);
  uiFooter("wheel: speed   click: back", "side: back");
  present();
  (void)H;
}

// ---------- TORUS ----------
#define TO_U 56
#define TO_V 18
struct TPt { int16_t x, y; float z; float lum; };
static TPt s_pts[TO_U * TO_V];

void drawTorus(uint32_t now) {
  const int W = spr.width();
  g_fastFrame = true;
  advance(now);
  spr.fillSprite(C_BG());
  const float A = s_phase * 0.8f, B = s_phase * 0.45f;
  const float cA = cosf(A), sA = sinf(A), cB = cosf(B), sB = sinf(B);
  const float R1 = 1.0f, R2 = 2.1f, K2 = 6.5f, K1 = 112.0f;
  const int cx = W / 2, cy = (UI_TOP + UI_BOTTOM) / 2 + 2;
  // свет сверху-спереди
  const float lx = 0, ly = 0.7071f, lz = -0.7071f;
  int n = 0;
  for (int i = 0; i < TO_V; i++) {
    float th = i * 2 * PI / TO_V, ct = cosf(th), st = sinf(th);
    for (int j = 0; j < TO_U; j++) {
      float ph = j * 2 * PI / TO_U + i * 0.09f, cp = cosf(ph), sp = sinf(ph);
      float ox = R2 + R1 * ct, oy = R1 * st;                 // круг сечения
      // поворот вокруг Y (ph), затем X (A), затем Z (B)
      float x = ox * (cB * cp + sA * sB * sp) - oy * cA * sB;
      float y = ox * (sB * cp - sA * cB * sp) + oy * cA * cB;
      float z = K2 + cA * ox * sp + oy * sA;
      // нормаль
      float nx = ct * (cB * cp + sA * sB * sp) - st * cA * sB;
      float ny = ct * (sB * cp - sA * cB * sp) + st * cA * cB;
      float nz = cA * ct * sp + st * sA;
      float L = nx * lx + ny * ly + nz * lz;                 // -1..1
      TPt& p = s_pts[n++];
      p.x = cx + (int)(K1 * x / z); p.y = cy - (int)(K1 * y / z * 0.95f);
      p.z = z; p.lum = L;
    }
  }
  static int16_t order[TO_U * TO_V];
  for (int i = 0; i < n; i++) order[i] = i;
  std::sort(order, order + n, [](int16_t a, int16_t b) { return s_pts[a].z > s_pts[b].z; });   // дальние раньше
  for (int k = 0; k < n; k++) {
    const TPt& p = s_pts[order[k]];
    float depth = constrain((9.5f - p.z) / 7.0f, 0.15f, 1.0f);    // ближе — ярче
    float lum = p.lum > 0 ? p.lum : 0;
    float v = constrain(0.08f + 0.92f * lum * depth + 0.12f * depth, 0.0f, 1.0f);
    uint16_t col = heatV(v);
    if (v > 0.55f) spr.fillRect(p.x, p.y, 2, 2, col);
    else           spr.drawPixel(p.x, p.y, col);
  }
  char buf[24]; speedLabel(buf, sizeof(buf));
  uiHeader("Torus", buf);
  uiFooter("wheel: speed   click: back", "side: back");
  present();
}

// ---------- раздел ----------
static void galSubtitle(char* out, size_t n) { snprintf(out, n, "%d demos", G_NUM); }
static void galEnter() { s_lastMs = 0; }
static void galLeave() {}
static void galInfo(int i, SubItem& it) {
  static const char* const names[G_NUM] = { "Tesseract", "Torus" };
  static const char* const descs[G_NUM] = { "4D hypercube", "donut of points" };
  it.name = names[i]; it.desc = descs[i]; it.value[0] = 0;
  if (i == G_TESS) strcpy(it.value, "4D");
  else             strcpy(it.value, "3D");
}
static int  galNow() { return g_gal; }
static void galOpen(int m) { g_gal = m; s_lastMs = 0; s_trailN = 0; }
static void galClose() {}
static void galClick() { navToSub(); }
static void galRotate(int dir) { s_speed = constrain(s_speed + dir * 0.25f, 0.0f, 4.0f); }
static void galDraw(uint32_t now) { if (g_gal == G_TESS) drawTesseract(now); else drawTorus(now); }

const Section gallerySection = {
  "Gallery", 150, 120, 255, iconCube, galSubtitle,
  galEnter, galLeave,
  G_NUM, galInfo, galNow, galOpen, galClose,
  galClick, galRotate, nullptr, nullptr, galDraw,
};
