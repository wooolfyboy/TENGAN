// ============================================================
//  audio_model.h — Sound section data shared by logic (audio.cpp)
//  and screens (audio_screens.cpp)
// ============================================================
#pragma once
#include <Arduino.h>

enum { A_BARS = 0, A_TERRAIN, A_REACTOR, A_LEVEL, A_SCOPE, A_XY, A_NUM };

#define AU_BANDS 32          // spectrum bands (logarithmic, 60 Hz .. 8 kHz)
#define AU_WAVE  256         // waveform points
#define AU_HIST  120         // loudness history: one point per 0.5 s => 1 min

struct AudioView {
  float level;               // loudness, dBFS (0 = mic maximum)
  float peak;                // loudness peak with slow decay, dBFS
  float bands[AU_BANDS];     // bands 0..1 (auto-gain), smoothed
  float caps[AU_BANDS];      // band peak caps 0..1
  int16_t wave[AU_WAVE];     // waveform (last block)
  float domHz;               // loudest frequency
  float energy;              // overall energy 0..1 (for animations)
};

extern AudioView g_au;
extern bool  g_micOk;                // mic responds
extern int   g_aMode;
extern float g_auHist[AU_HIST];      // loudness, dBFS
extern int   g_auHistN;
extern float g_auMax;                // max while on screen
extern float g_auAvg;                // average (moving)

// mic diagnostics (shown on screens when something is wrong)
struct MicDiag {
  int      stage;            // 0 off, 1 driver failed, 2 pins, 3 frequency, 4 working
  int      slot;             // 0 = left channel, 1 = right (we try both)
  uint32_t blocks;           // audio blocks received
  uint32_t timeouts;         // times data didn't arrive in time
  int      rawPeak;          // max |sample| in the last block (silence ≈ tens)
  bool     disabled;         // disabled in settings
};
extern MicDiag g_micDiag;

// laptop audio (PC section: a laptop program listens to what the speakers play)
extern bool   g_auFromPc;            // visuals now follow laptop audio, not the mic
extern int    g_auSrcNow;            // current source: 0 auto, 1 mic, 2 laptop (from settings)
void drawPcAudioWait(uint32_t now);  // "laptop" selected but no audio from it
#define AU_XY 64
extern int8_t g_auXY[AU_XY][2];      // XY scope points: left / right channel
extern bool   g_auXYStereo;

const char* levelWord(float db);     // QUIET / NORMAL / LOUD / VERY LOUD

// screens (audio_screens.cpp)
void drawAuBars(uint32_t now);
void drawReactor(uint32_t now);
void drawTerrain(uint32_t now);    // 3D "sound mountains"
void drawXY(uint32_t now);         // XY scope: left channel on X, right on Y
void drawLevel(uint32_t now);
void drawWave(uint32_t now);
void drawMicMissing();             // driver failed / disabled / no data
