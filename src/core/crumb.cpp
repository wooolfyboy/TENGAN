#include "core/crumb.h"
#include <esp_system.h>
#include <esp_attr.h>

static RTC_NOINIT_ATTR char s_crumb[48];
static RTC_NOINIT_ATTR uint32_t s_magic;
static char s_lastStep[48] = "";
static const char* s_reason = "?";
static bool s_crash = false;

void crumb(const char* s) {
  s_magic = 0x7E6A0001;
  strncpy(s_crumb, s, sizeof(s_crumb) - 1);
  s_crumb[sizeof(s_crumb) - 1] = 0;
}

void crumbBoot() {
  if (s_magic == 0x7E6A0001) { memcpy(s_lastStep, s_crumb, sizeof(s_lastStep)); s_lastStep[sizeof(s_lastStep) - 1] = 0; }
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   s_reason = "power on"; break;
    case ESP_RST_SW:        s_reason = "restart"; break;
    case ESP_RST_PANIC:     s_reason = "panic"; s_crash = true; break;
    case ESP_RST_INT_WDT:   s_reason = "int watchdog"; s_crash = true; break;
    case ESP_RST_TASK_WDT:  s_reason = "task watchdog"; s_crash = true; break;
    case ESP_RST_WDT:       s_reason = "watchdog"; s_crash = true; break;
    case ESP_RST_BROWNOUT:  s_reason = "brownout"; s_crash = true; break;
    case ESP_RST_DEEPSLEEP: s_reason = "wake"; break;
    default:                s_reason = "other"; break;
  }
  crumb("boot");
}

const char* crumbLastReason() { return s_reason; }
const char* crumbLastStep() { return s_lastStep; }
bool crumbWasCrash() { return s_crash; }
