// ============================================================
//  wifi_model.cpp — данные раздела Wi-Fi и помощники без железа
//  (их используют и логика, и экраны, и симулятор экрана на ПК)
// ============================================================
#include <esp_wifi.h>
#include "wifi/wifi_model.h"
#include "wifi/ouidb.h"          // база производителей — подключается только здесь
#include "core/display.h"
#include "core/theme.h"

int  g_wMode = 0;
bool g_wEverScanned = false;
std::map<uint64_t, Ap> g_aps;
bool     g_scanning = false;
std::vector<uint64_t> g_wOrder;
uint64_t g_wSel = 0;
int      g_wTop = 0;
bool     g_wDetail = false;
int8_t   g_wSpark[W_SPARK_N];
int      g_wSparkN = 0;
int8_t   g_fSpark[F_SPARK_N];
int      g_fSparkN = 0;
uint32_t g_fLastHit = 0;
bool     g_prom = false;
int      g_hopCh = 1;
float    g_ppsMgmt[CH_MAX + 1] = {0}, g_ppsData[CH_MAX + 1] = {0};
float    g_noise[CH_MAX + 1] = {0};       // шум по каналам, дБм (0 = нет данных)
float    g_actHist[ACT_HIST_N];
int      g_actHistN = 0;
std::map<uint64_t, Sta> g_stas;
std::map<uint64_t, uint32_t> g_probes;

const char* ouiName(const uint8_t* mac) { return ouiVendor(mac); }

uint64_t macKey(const uint8_t* m) {
  uint64_t k = 0;
  for (int i = 0; i < 6; i++) k = (k << 8) | m[i];
  return k;
}

// ---------- подписи и цвета ----------
const char* encName(uint8_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Ent";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    case WIFI_AUTH_OWE:             return "OWE";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
    default:                        return "other";
  }
}


const char* genName(uint8_t g) {
  switch (g) { case 6: return "Wi-Fi 6"; case 4: return "Wi-Fi 4"; case 3: return "11g"; default: return "11b"; }
}


uint16_t dimColor(uint16_t c, int pct) {
  uint8_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return C(r * pct / 100, g * pct / 100, b * pct / 100);
}

const char* ssidOr(const Ap& a) { return a.ssid[0] ? a.ssid : "(hidden)"; }

// цель для FIND: выбранная в списке сеть, иначе самая сильная
uint64_t findTarget() {
  if (g_wSel && g_aps.count(g_wSel)) return g_wSel;
  uint64_t best = 0; int br = -127;
  for (auto& kv : g_aps) if (kv.second.rssiS > br) { br = kv.second.rssiS; best = kv.first; }
  g_wSel = best;
  return best;
}

int clientsOf(uint64_t bssid) {
  int n = 0;
  for (auto& kv : g_stas) if (kv.second.bssid == bssid) n++;
  return n;
}

float apCenter(const Ap& a) { return a.ch + a.sec * 2.0f; }

float apHalf(const Ap& a)   { return a.sec ? 4.0f : 2.0f; }

// насколько сеть мешает каналу k (0..1)
float interference(const Ap& a, int k) {
  float d = fabsf(apCenter(a) - k);
  float reach = apHalf(a) + 2.0f + 1.0f;      // наша ширина 20 МГц (±2) + запас
  return d >= reach ? 0.0f : 1.0f - d / reach;
}

int bestChannel() {
  if (g_aps.empty()) return 0;
  const int cand[3] = { 1, 6, 11 };
  float best = 1e9; int bestCh = 1;
  for (int k = 0; k < 3; k++) {
    float score = 0;
    for (auto& kv : g_aps) score += interference(kv.second, cand[k]) * (kv.second.rssiS + 100);
    if (score < best) { best = score; bestCh = cand[k]; }
  }
  return bestCh;
}
