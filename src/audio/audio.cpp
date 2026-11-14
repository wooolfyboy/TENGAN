// ============================================================
//  audio.cpp — звук
//
//  МИКРОФОН (PDM, I2S0, 16 кГц): отдельная задача читает блоки по 512
//  отсчётов (32 мс), считает громкость и спектр (БПФ) и отдаёт их экранам.
//  Работает, только пока открыт раздел Sound. Ничего не записывается.
//
//  ДИНАМИК (I2S1): короткие звуки интерфейса — щелчки колеса и «счётчик
//  Гейгера» в поиске. Громкость — Settings -> Sounds (off / quiet / loud).
//
//  Пины и настройки — из примера test_mic_speaker репозитория LilyGO.
// ============================================================
#include <driver/i2s_pdm.h>
#include <driver/i2s_std.h>
#include "audio/audio.h"
#include "audio/audio_model.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"
#include "core/menu.h"
#include "core/settings.h"
#include "core/timeutil.h"
#include "pc/pc.h"

#define MIC_CLK   39
#define MIC_DATA  42
#define SPK_BCLK  46
#define SPK_LRCLK 40
#define SPK_DIN   7
static const int SR = 16000;
static const int N  = 512;           // размер блока и БПФ

// ---------- данные для экранов ----------
AudioView g_au;
bool  g_micOk = false;
int   g_aMode = 0;
float g_auHist[AU_HIST];
int   g_auHistN = 0;
float g_auMax = -120, g_auAvg = -90;
MicDiag g_micDiag = {};
bool   g_auFromPc = false;
int    g_auSrcNow = 0;
int8_t g_auXY[AU_XY][2];
bool   g_auXYStereo = false;
static uint32_t s_pcAudT = 0;

const char* levelWord(float db) {
  if (db < -55) return "QUIET";
  if (db < -40) return "NORMAL";
  if (db < -25) return "LOUD";
  return "VERY LOUD";
}

// ---------- обмен задача микрофона -> основной цикл ----------
struct MicBlock { float level; float bandDb[AU_BANDS]; int16_t wave[AU_WAVE]; float domHz; int rawPeak; uint32_t seq; };
static MicBlock s_blk;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_micRun = false;
static TaskHandle_t s_micTask = nullptr;
static i2s_chan_handle_t s_rx = nullptr, s_tx = nullptr;   // микрофон (I2S0, PDM) и динамик (I2S1)
static uint8_t s_bandLo[AU_BANDS], s_bandHi[AU_BANDS];   // границы полос в бинах БПФ

// БПФ «на месте», радикс-2
static void fft(float* re, float* im, int n) {
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
  }
  for (int len = 2; len <= n; len <<= 1) {
    float ang = -2 * PI / len, wr = cosf(ang), wi = sinf(ang);
    for (int i = 0; i < n; i += len) {
      float cr = 1, ci = 0;
      for (int k = 0; k < len / 2; k++) {
        int a = i + k, b = a + len / 2;
        float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
        re[b] = re[a] - tr; im[b] = im[a] - ti;
        re[a] += tr;        im[a] += ti;
        float nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
      }
    }
  }
}

static void micTask(void*) {
  static int16_t buf[N];
  static float re[N], im[N], win[N];
  for (int i = 0; i < N; i++) win[i] = 0.5f - 0.5f * cosf(2 * PI * i / (N - 1));   // окно Ханна
  while (s_micRun) {
    size_t got = 0;
    i2s_channel_read(s_rx, buf, sizeof(buf), &got, 200);
    if (got < sizeof(buf)) { g_micDiag.timeouts++; continue; }
    float mean = 0;
    int rawPeak = 0;
    for (int i = 0; i < N; i++) { mean += buf[i]; rawPeak = max(rawPeak, abs((int)buf[i])); }
    mean /= N;
    double acc = 0;
    for (int i = 0; i < N; i++) {
      float v = buf[i] - mean;
      acc += v * v;
      re[i] = v * win[i]; im[i] = 0;
    }
    float level = 20.0f * log10f(sqrtf(acc / N) / 32768.0f + 1e-7f);
    fft(re, im, N);
    MicBlock b;
    b.level = level;
    b.rawPeak = rawPeak;
    int domBin = 2; float domMag = 0;
    for (int k = 0; k < AU_BANDS; k++) {
      float m = 0;
      for (int bin = s_bandLo[k]; bin <= s_bandHi[k]; bin++) {
        float mag = sqrtf(re[bin] * re[bin] + im[bin] * im[bin]);
        if (mag > m) m = mag;
        if (bin > 2 && mag > domMag) { domMag = mag; domBin = bin; }
      }
      b.bandDb[k] = 20.0f * log10f(m / (N * 8192.0f) + 1e-7f);
    }
    b.domHz = domBin * (float)SR / N;
    for (int i = 0; i < AU_WAVE; i++) b.wave[i] = buf[i * N / AU_WAVE];
    portENTER_CRITICAL(&s_mux);
    uint32_t seq = s_blk.seq + 1;
    s_blk = b; s_blk.seq = seq;
    portEXIT_CRITICAL(&s_mux);
  }
  s_micTask = nullptr;
  vTaskDelete(nullptr);
}

static uint32_t s_micStartMs = 0;
static int      s_maxRaw = 0;            // максимум с момента запуска — для автопроверки канала

static bool micStart(int slot) {
  if (s_micRun) return true;
  g_micDiag.slot = slot; g_micDiag.blocks = 0; g_micDiag.timeouts = 0; g_micDiag.rawPeak = 0;
  s_maxRaw = 0; s_micStartMs = millis();
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);   // PDM — только I2S0
  if (i2s_new_channel(&cc, nullptr, &s_rx) != ESP_OK) { s_rx = nullptr; g_micDiag.stage = 1; return false; }
  i2s_pdm_rx_config_t pc = {};
  pc.clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SR);
  pc.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  pc.slot_cfg.slot_mask = slot ? I2S_PDM_SLOT_RIGHT : I2S_PDM_SLOT_LEFT;
  pc.gpio_cfg.clk = (gpio_num_t)MIC_CLK;
  pc.gpio_cfg.din = (gpio_num_t)MIC_DATA;
  if (i2s_channel_init_pdm_rx_mode(s_rx, &pc) != ESP_OK) { i2s_del_channel(s_rx); s_rx = nullptr; g_micDiag.stage = 2; return false; }
  if (i2s_channel_enable(s_rx) != ESP_OK) { i2s_del_channel(s_rx); s_rx = nullptr; g_micDiag.stage = 3; return false; }
  g_micDiag.stage = 4;
  // границы полос: 60 Гц .. 8 кГц логарифмически, бин = 31.25 Гц
  for (int k = 0; k < AU_BANDS; k++) {
    float f0 = 60.0f * powf(8000.0f / 60.0f, (float)k / AU_BANDS);
    float f1 = 60.0f * powf(8000.0f / 60.0f, (float)(k + 1) / AU_BANDS);
    int b0 = max(1, (int)(f0 * N / SR)), b1 = max(b0, (int)(f1 * N / SR));
    s_bandLo[k] = (uint8_t)min(b0, N / 2 - 1); s_bandHi[k] = (uint8_t)min(b1, N / 2 - 1);
  }
  s_micRun = true;
  xTaskCreatePinnedToCore(micTask, "mic", 6144, nullptr, 2, &s_micTask, 1);
  return true;
}

static void micStop() {
  if (!s_micRun) return;
  s_micRun = false;
  for (int i = 0; i < 40 && s_micTask; i++) delay(10);   // дождаться выхода задачи
  if (s_rx) { i2s_channel_disable(s_rx); i2s_del_channel(s_rx); s_rx = nullptr; }
}

// ---------- обработка для экранов (в основном цикле) ----------
static uint32_t s_lastSeq = 0, s_histMs = 0;
static float s_agcTop = -40;          // автоусиление: верх шкалы полос, dB

static void auUpdate(uint32_t now) {
  MicBlock b;
  portENTER_CRITICAL(&s_mux);
  b = s_blk;
  portEXIT_CRITICAL(&s_mux);
  // автопроверка канала: 1.5 с одной «немой» тишины на левом — пробуем правый
  if (g_micDiag.slot == 0 && now - s_micStartMs > 1500 && s_maxRaw < 8) {
    micStop();
    micStart(1);
    return;
  }
  if (b.seq == s_lastSeq) return;
  s_lastSeq = b.seq;
  g_micDiag.blocks++;
  g_micDiag.rawPeak = b.rawPeak;
  s_maxRaw = max(s_maxRaw, b.rawPeak);

  g_au.level = b.level;
  g_au.peak = max(b.level, g_au.peak - 0.4f);
  g_au.domHz = b.domHz;
  memcpy(g_au.wave, b.wave, sizeof(g_au.wave));
  float mx = -120, e = 0;
  for (int k = 0; k < AU_BANDS; k++) mx = max(mx, b.bandDb[k]);
  s_agcTop = max(mx, s_agcTop - 0.15f);                 // верх шкалы медленно оседает
  s_agcTop = max(s_agcTop, -70.0f);                     // в тишине не «раздуваем» шум
  for (int k = 0; k < AU_BANDS; k++) {
    float v = constrain((b.bandDb[k] - (s_agcTop - 45)) / 45.0f, 0.0f, 1.0f);
    g_au.bands[k] = v > g_au.bands[k] ? v : g_au.bands[k] * 0.82f + v * 0.18f;   // быстро вверх, плавно вниз
    g_au.caps[k] = max(g_au.bands[k], g_au.caps[k] - 0.012f);
    e += g_au.bands[k];
  }
  g_au.energy = e / AU_BANDS;
  g_auMax = max(g_auMax, b.level);
  g_auAvg = g_auAvg * 0.97f + b.level * 0.03f;
  if (now - s_histMs >= 500) {
    s_histMs = now;
    if (g_auHistN < AU_HIST) g_auHist[g_auHistN++] = b.level;
    else { memmove(g_auHist, g_auHist + 1, sizeof(float) * (AU_HIST - 1)); g_auHist[AU_HIST - 1] = b.level; }
  }
  if (g_auLeds && g_ledEvent == 0) {                     // лента — индикатор громкости (настройка)
    float x = constrain((b.level + 70) / 55.0f, 0.0f, 1.0f);
    int lv = (int)(x * WS2812_N + 0.5f);
    strip.clear();
    for (int i = 0; i < lv; i++) strip.setPixelColor(i, strip.Color(10 + i * 14, 50 + i * 20, 70 + i * 22));
    strip.show();
  }
}

// ============================================================
//  ДИНАМИК — короткие звуки (своя задача, очередь звуков)
// ============================================================
struct Tone { uint16_t hz, ms; uint8_t kind; };       // kind: 0 — тон, 1 — щелчок
static QueueHandle_t s_toneQ = nullptr;

static uint16_t volAmp() {
  switch (g_sndIdx) { case 1: return 2500; case 2: return 9000; default: return 0; }
}

static void spkTask(void*) {
  static int16_t buf[256];
  Tone t;
  for (;;) {
    if (xQueueReceive(s_toneQ, &t, portMAX_DELAY) != pdTRUE) continue;
    uint16_t amp = volAmp();
    if (!amp) continue;
    int total = SR * t.ms / 1000, done = 0;
    float ph = 0, dph = 2 * PI * t.hz / SR;
    while (done < total) {
      int n = min(256, total - done);
      for (int i = 0; i < n; i++) {
        float env = 1.0f - (float)(done + i) / total;    // затухание
        if (t.kind == 1) env *= env * env;                // щелчок — очень короткий спад
        buf[i] = (int16_t)(sinf(ph) * amp * env);
        ph += dph;
      }
      size_t w;
      i2s_channel_write(s_tx, buf, n * 2, &w, 50);
      done += n;
    }
  }
}

void audioBegin() {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  cc.auto_clear = true;                                  // без звука — тишина, а не «залипший» тон
  if (i2s_new_channel(&cc, &s_tx, nullptr) != ESP_OK) { s_tx = nullptr; return; }
  i2s_std_config_t sc = {};
  sc.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SR);
  sc.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  sc.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  sc.gpio_cfg.bclk = (gpio_num_t)SPK_BCLK;
  sc.gpio_cfg.ws   = (gpio_num_t)SPK_LRCLK;
  sc.gpio_cfg.dout = (gpio_num_t)SPK_DIN;
  sc.gpio_cfg.din  = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(s_tx, &sc) != ESP_OK || i2s_channel_enable(s_tx) != ESP_OK) {
    i2s_del_channel(s_tx); s_tx = nullptr; return;
  }
  s_toneQ = xQueueCreate(8, sizeof(Tone));
  xTaskCreatePinnedToCore(spkTask, "spk", 3072, nullptr, 1, nullptr, 1);
}

static void push(uint16_t hz, uint16_t ms, uint8_t kind) {
  if (!s_toneQ || g_sndIdx == 0) return;
  Tone t = { hz, ms, kind };
  xQueueSend(s_toneQ, &t, 0);                            // очередь полна — звук пропускаем
}
void audioClick()                     { if (g_clickEn) push(2600, 6, 1); }
void audioBeep(uint16_t hz, uint16_t ms) { push(hz, ms, 0); }
void audioGeiger(int pct) {
  static uint32_t last = 0;
  uint32_t now = millis();
  uint32_t interval = 900 - (uint32_t)constrain(pct, 0, 100) * 8;   // 900 мс … 100 мс
  if (now - last >= interval) { last = now; push(1800 + pct * 12, 4, 1); }
}

// ============================================================
//  раздел Sound
// ============================================================
// ============================================================
//  звук с ноутбука: готовый спектр приходит по USB (ноут считает сам)
// ============================================================
bool audioPcLive(uint32_t now) { return s_pcAudT && msSince(now, s_pcAudT) < 600; }

void audioPcFrame(float level, int hz, const uint8_t* bands, const int8_t* wave, const int8_t* xy, bool stereo) {
  if (g_auSrcNow == 1) return;                       // выбран микрофон — звук ноута не берём
  uint32_t now = millis();
  s_pcAudT = now;
  g_auFromPc = true;
  g_au.level = level;
  g_au.peak = max(level, g_au.peak - 0.4f);
  g_au.domHz = hz;
  float e = 0;
  for (int k = 0; k < AU_BANDS; k++) {
    float v = bands[k] / 255.0f;
    g_au.bands[k] = v > g_au.bands[k] ? v : g_au.bands[k] * 0.78f + v * 0.22f;   // быстро вверх, плавно вниз
    g_au.caps[k] = max(g_au.bands[k], g_au.caps[k] - 0.012f);
    e += g_au.bands[k];
  }
  g_au.energy = e / AU_BANDS;
  for (int i = 0; i < AU_WAVE; i++) {                  // 64 точки -> 256 (линейно между соседями)
    float f = i * 63.0f / (AU_WAVE - 1);
    int a = (int)f; int b = min(a + 1, 63);
    g_au.wave[i] = (int16_t)((wave[a] + (wave[b] - wave[a]) * (f - a)) * 200);
  }
  memcpy(g_auXY, xy, sizeof(g_auXY));
  g_auXYStereo = stereo;
  g_auMax = max(g_auMax, level);
  g_auAvg = g_auAvg * 0.97f + level * 0.03f;
  if (now - s_histMs >= 500) {
    s_histMs = now;
    if (g_auHistN < AU_HIST) g_auHist[g_auHistN++] = level;
    else { memmove(g_auHist, g_auHist + 1, sizeof(float) * (AU_HIST - 1)); g_auHist[AU_HIST - 1] = level; }
  }
}

static void sndSubtitle(char* out, size_t n) {
  if (g_auFromPc) snprintf(out, n, "laptop");
  else if (s_micRun && g_micOk) snprintf(out, n, "%d dBfs", (int)g_au.level);
  else                     snprintf(out, n, g_micEn ? "listen" : "mic off");
}
static bool s_micTried = false;             // микрофон уже пробовали запустить (не долбить драйвер, если не встал)
static bool s_pcAsked = false;              // просили ноут слать звук
static void sndEnter() {
  g_auSrcNow = g_auSrc;                     // источник — из настроек (Settings > Sound > Source)
  g_micDiag.disabled = !g_micEn;
  g_auMax = -120; g_auHistN = 0;
  s_micTried = false;
  if (!g_micEn) { g_micOk = false; g_micDiag.stage = 0; return; }
  if (g_auSrcNow != 2) { g_micOk = micStart(0); s_micTried = true; }   // «только ноутбук» — микрофон не трогаем
}
static void sndLeave() {
  micStop(); ledsOff();
  if (s_pcAsked) { pcSendLine("AUD OFF"); s_pcAsked = false; }
  g_auFromPc = false;
}
static void sndInfo(int i, SubItem& it) {
  static const char* const names[A_NUM] = { "Bars", "Terrain", "Reactor", "Level", "Scope", "XY" };
  static const char* const descs[A_NUM] = { "sound spectrum", "3D sound mountains", "pulse animation", "noise meter", "waveform", "oscilloscope XY" };
  it.name = names[i]; it.desc = descs[i]; it.value[0] = 0;
  if (!g_micEn) { strcpy(it.value, "mic off"); return; }
  if (!g_micOk) { strcpy(it.value, "no mic"); return; }
  if (i == A_LEVEL) snprintf(it.value, sizeof(it.value), "%d dBfs", (int)g_au.level);
  if (i == A_BARS && g_au.energy > 0.15f) snprintf(it.value, sizeof(it.value), "%d Hz", (int)g_au.domHz);
}
static int  sndNow() { return g_aMode; }
static void sndOpen(int m) { g_aMode = m; if (m == A_LEVEL) { g_auMax = -120; } }
static void sndClose() {}
static void sndClick() {
  if (g_aMode == A_LEVEL) { g_auMax = -120; g_auHistN = 0; return; }   // сброс максимума и графика
  navToSub();
}
static void sndRotate(int dir) {
  static uint32_t last = 0;
  if (millis() - last < 120) return;
  last = millis();
  g_aMode = (g_aMode + dir + A_NUM) % A_NUM;
}
static void sndTick(uint32_t now) {
  // источник: 0 авто (ноут, если звук идёт, иначе микрофон), 1 только микрофон, 2 только ноут
  static uint32_t s_audReq = 0;
  bool wantPc = g_auSrcNow != 1;
  if (wantPc) {
    if (!s_audReq || msSince(now, s_audReq) > 1500) { s_audReq = now; pcSendLine("AUD ON"); s_pcAsked = true; }   // «ноут, шли звук»
  } else if (s_pcAsked) { pcSendLine("AUD OFF"); s_pcAsked = false; s_audReq = 0; }
  bool pc = wantPc && audioPcLive(now);
  g_auFromPc = pc;
  bool useMic = g_auSrcNow == 1 || (g_auSrcNow == 0 && !pc);
  if (!useMic) {                                     // звук ноута — микрофон не нужен
    if (s_micRun) micStop();
    s_micTried = false;                              // понадобится — запустим снова
    return;
  }
  if (!s_micRun && g_micEn && !s_micTried) { s_micTried = true; g_micOk = micStart(0); g_micDiag.blocks = 0; }
  if (g_micOk && s_micRun) auUpdate(now);
}
static void sndDraw(uint32_t now) {
  // нет драйвера, выключен или данные не идут больше секунды — экран диагностики
  bool pc = g_auSrcNow != 1 && audioPcLive(now);
  if (g_auSrcNow == 2 && !pc) { drawPcAudioWait(now); return; }        // выбран ноут, а звука нет
  if (!pc && (!g_micOk || (g_micDiag.blocks == 0 && msSince(now, s_micStartMs) > 1000))) { drawMicMissing(); return; }
  switch (g_aMode) {
    case A_BARS:    drawAuBars(now); break;
    case A_REACTOR: drawReactor(now); break;
    case A_TERRAIN: drawTerrain(now); break;
    case A_XY:      drawXY(now); break;
    case A_LEVEL:   drawLevel(now); break;
    default:        drawWave(now); break;
  }
}


const Section soundSection = {
  "Sound", 120, 200, 255, iconMic, sndSubtitle,
  sndEnter, sndLeave,
  A_NUM, sndInfo, sndNow, sndOpen, sndClose,
  sndClick, sndRotate, nullptr, sndTick, sndDraw,
};

// ============================================================
//  для веб-страницы: микрофон включается, пока вкладка Sound на
//  телефоне спрашивает данные (web.cpp выключит его через пару секунд тишины)
// ============================================================
bool audioWebUse() {
  if (!g_micEn) return false;
  if (!s_micRun) { g_micOk = micStart(0); g_auMax = -120; g_auHistN = 0; }
  return g_micOk;
}
void audioWebTick(uint32_t now) { if (s_micRun && g_micOk) auUpdate(now); }
void audioWebStop() { if (s_micRun) { micStop(); ledsOff(); } }

void audioJson(String& o) {
  o.reserve(600);
  o = "{\"en\":"; o += g_micEn ? "true" : "false";
  bool live = s_micRun && g_micOk && g_micDiag.blocks > 0;
  o += ",\"ok\":"; o += live ? "true" : "false";
  if (live) {
    o += ",\"lv\":"; o += String(g_au.level, 1);
    o += ",\"pk\":"; o += String(g_au.peak, 1);
    o += ",\"mx\":"; o += String(g_auMax, 1);
    o += ",\"avg\":"; o += String(g_auAvg, 1);
    o += ",\"hz\":"; o += (int)g_au.domHz;
    o += ",\"e\":"; o += String(g_au.energy, 2);
    o += ",\"w\":\""; o += levelWord(g_au.level); o += '"';
    o += ",\"b\":[";
    for (int k = 0; k < AU_BANDS; k++) { if (k) o += ','; o += (int)(g_au.bands[k] * 100); }
    o += ']';
  }
  o += '}';
}
