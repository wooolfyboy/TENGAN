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
#include <driver/i2s.h>
#include "audio/audio.h"
#include "audio/audio_model.h"
#include "core/board.h"
#include "core/display.h"
#include "core/theme.h"
#include "core/leds.h"
#include "core/menu.h"
#include "core/settings.h"

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
    i2s_read(I2S_NUM_0, buf, sizeof(buf), &got, pdMS_TO_TICKS(100));
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
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  cfg.sample_rate = SR;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = slot ? I2S_CHANNEL_FMT_ONLY_RIGHT : I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;     // как в примере записи LilyGO
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = I2S_PIN_NO_CHANGE;
  pins.ws_io_num = MIC_CLK;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_DATA;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) { g_micDiag.stage = 1; return false; }
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) { i2s_driver_uninstall(I2S_NUM_0); g_micDiag.stage = 2; return false; }
  // как в примере записи LilyGO: частоту PDM задаём явно
  if (i2s_set_clk(I2S_NUM_0, SR, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO) != ESP_OK) {
    i2s_driver_uninstall(I2S_NUM_0); g_micDiag.stage = 3; return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
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
  for (int i = 0; i < 30 && s_micTask; i++) delay(10);   // дождаться выхода задачи
  i2s_driver_uninstall(I2S_NUM_0);
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
  if (g_ledEvent == 0) {                                 // лента — индикатор громкости
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
      i2s_write(I2S_NUM_1, buf, n * 2, &w, pdMS_TO_TICKS(50));
      done += n;
    }
  }
}

void audioBegin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SR;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.tx_desc_auto_clear = true;                         // без звука — тишина, а не «залипший» тон
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = SPK_BCLK;
  pins.ws_io_num = SPK_LRCLK;
  pins.data_out_num = SPK_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr) != ESP_OK) return;
  if (i2s_set_pin(I2S_NUM_1, &pins) != ESP_OK) { i2s_driver_uninstall(I2S_NUM_1); return; }
  i2s_zero_dma_buffer(I2S_NUM_1);
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
static void sndSubtitle(char* out, size_t n) {
  if (s_micRun && g_micOk) snprintf(out, n, "%d dBfs", (int)g_au.level);
  else                     snprintf(out, n, g_micEn ? "listen" : "mic off");
}
static void sndEnter() {
  g_micDiag.disabled = !g_micEn;
  if (!g_micEn) { g_micOk = false; g_micDiag.stage = 0; return; }
  g_micOk = micStart(0);
  g_auMax = -120; g_auHistN = 0;
}
static void sndLeave() { micStop(); ledsOff(); }
static void sndInfo(int i, SubItem& it) {
  static const char* const names[A_NUM] = { "Bars", "Reactor", "Level", "Scope" };
  static const char* const descs[A_NUM] = { "sound spectrum", "pulse animation", "noise meter", "waveform" };
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
static void sndTick(uint32_t now) { if (g_micOk) auUpdate(now); }
static void sndDraw(uint32_t now) {
  // нет драйвера, выключен или данные не идут больше секунды — экран диагностики
  if (!g_micOk || (g_micDiag.blocks == 0 && now - s_micStartMs > 1000)) { drawMicMissing(); return; }
  switch (g_aMode) {
    case A_BARS:    drawAuBars(now); break;
    case A_REACTOR: drawReactor(now); break;
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
