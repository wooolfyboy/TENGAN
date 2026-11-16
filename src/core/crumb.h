// ============================================================
//  crumb.h — «чёрный ящик»: последний шаг перед падением.
//  crumb("...") пишет короткую метку в память, которая переживает
//  программную перезагрузку (RTC_NOINIT). После падения палка знает
//  причину сброса и последний шаг: Settings › Version, /api/info, поток st.
// ============================================================
#pragma once
#include <Arduino.h>

void crumb(const char* s);                 // отметить текущий шаг
void crumbBoot();                          // в начале setup(): прочитать прошлый шаг и причину сброса
const char* crumbLastReason();             // "power on", "panic", "watchdog", "brownout", ...
const char* crumbLastStep();               // последний шаг перед сбросом ("" если чистый запуск)
bool crumbWasCrash();                      // прошлый сброс — падение (panic / watchdog / brownout)
