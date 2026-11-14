// ============================================================
//  audio_model.h — данные раздела Sound, общие для логики (audio.cpp)
//  и экранов (audio_screens.cpp)
// ============================================================
#pragma once
#include <Arduino.h>

enum { A_BARS = 0, A_REACTOR, A_LEVEL, A_SCOPE, A_NUM };

#define AU_BANDS 32          // полосы спектра (логарифмически, 60 Гц .. 8 кГц)
#define AU_WAVE  256         // точек формы волны
#define AU_HIST  120         // история громкости: точка раз в 0.5 с => 1 мин

struct AudioView {
  float level;               // громкость, dBFS (0 — максимум микрофона)
  float peak;                // пик громкости с медленным спадом, dBFS
  float bands[AU_BANDS];     // полосы 0..1 (с автоусилением), сглаженные
  float caps[AU_BANDS];      // «шапки» пиков полос 0..1
  int16_t wave[AU_WAVE];     // форма волны (последний блок)
  float domHz;               // самая громкая частота
  float energy;              // общая «энергия» 0..1 (для анимаций)
};

extern AudioView g_au;
extern bool  g_micOk;                // микрофон отвечает
extern int   g_aMode;
extern float g_auHist[AU_HIST];      // громкость, dBFS
extern int   g_auHistN;
extern float g_auMax;                // максимум за время на экране
extern float g_auAvg;                // среднее (скользящее)

const char* levelWord(float db);     // QUIET / NORMAL / LOUD / VERY LOUD

// экраны (audio_screens.cpp)
void drawAuBars(uint32_t now);
void drawReactor(uint32_t now);
void drawLevel(uint32_t now);
void drawWave(uint32_t now);
void drawMicMissing();
