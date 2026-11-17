// ============================================================
//  crumb.h — black box: the last step before a crash.
//  crumb("...") stores a short tag in RTC_NOINIT memory that survives a soft reset;
//  after a crash the reset reason and last step show in Settings > Version, /api/info, stream st.
// ============================================================
#pragma once
#include <Arduino.h>

void crumb(const char* s);                 // mark current step
void crumbBoot();                          // at start of setup(): read previous step and reset reason
const char* crumbLastReason();             // "power on", "panic", "watchdog", "brownout", ...
const char* crumbLastStep();               // last step before reset ("" on a clean start)
bool crumbWasCrash();                      // previous reset was a crash (panic / watchdog / brownout)
// main-loop watchdog: a hang > 15 s reboots the device and logs the step
void crumbWatchdogStart();
void crumbFeed();                          // "I'm alive": call each loop() pass and inside long operations
