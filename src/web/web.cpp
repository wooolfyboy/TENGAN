// ============================================================
//  web.cpp — раздел Web
//
//  Палка поднимает свою точку Wi-Fi (имя и пароль — из MAC-адреса,
//  не меняются), веб-сервер на 192.168.4.1 и «перехват» DNS: телефон
//  при подключении сам предлагает открыть страницу. Страница (web_page.h)
//  каждые пару секунд спрашивает у палки данные:
//    /api/ble  — устройства Bluetooth (сканирование идёт всё время)
//    /api/wifi — сети вокруг (скан раз в 20 с, короткий — чтобы точка не «падала»)
//    /api/info — батарея и время работы
//  Работает, только пока открыт раздел.
// ============================================================
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <qrcode.h>
#include "web/web.h"
#include "web/web_page.h"
#include "ble/ble.h"
#include "wifi/wifi.h"
#include "wifi/wifi_model.h"
#include "subghz/subghz.h"
#include "audio/audio.h"
#include "core/battery.h"
#include "core/display.h"
#include "core/leds.h"

static WebServer* s_srv = nullptr;
static DNSServer  s_dns;
static bool       s_on = false, s_ok = false;
static char       s_ssid[24], s_pass[12];
static uint32_t   s_reqs = 0, s_scanMs = 0;
static bool       s_scanned = false;
static String     s_wifiJson = "{\"scanned\":false,\"nets\":[]}";
static uint8_t    s_qr[45 * 45];
static int        s_qrN = 0;
static int        s_netCount = 0;
static uint32_t   s_subMs = 0, s_sndMs = 0;     // когда вкладки Sub-GHz / Sound последний раз спрашивали
static bool       s_subOn = false, s_sndOn = false;
static const char* s_tab = "";                  // что сейчас смотрят на телефоне (для экрана палки)

static void qrStore(esp_qrcode_handle_t q) {
  int n = esp_qrcode_get_size(q);
  if (n > 45) { s_qrN = 0; return; }
  s_qrN = n;
  for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) s_qr[y * n + x] = esp_qrcode_get_module(q, x, y) ? 1 : 0;
}

static void makeNames() {
  uint8_t m[6]; WiFi.macAddress(m);
  snprintf(s_ssid, sizeof(s_ssid), "RadioRadar-%02X%02X", m[4], m[5]);
  uint32_t h = 2166136261u;                       // пароль: 8 цифр из MAC (одинаковый каждый раз)
  for (int i = 0; i < 6; i++) h = (h ^ m[i]) * 16777619u;
  snprintf(s_pass, sizeof(s_pass), "%08lu", (unsigned long)(h % 100000000UL));
  char wifiQr[80];
  snprintf(wifiQr, sizeof(wifiQr), "WIFI:T:WPA;S:%s;P:%s;;", s_ssid, s_pass);
  esp_qrcode_config_t cfg = { qrStore, 6, ESP_QRCODE_ECC_LOW };
  esp_qrcode_generate(&cfg, wifiQr);
}

static const char* encStr(wifi_auth_mode_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN: return "open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: case WIFI_AUTH_WPA_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA3_PSK: case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    default: return "?";
  }
}
static void jsonAppend(String& o, const String& s) {
  o += '"';
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  o += '"';
}

static void scanCollect() {
  int n = WiFi.scanComplete();
  if (n < 0) return;                              // ещё идёт (или не запускали)
  String o; o.reserve(64 + n * 90);
  o += "{\"scanned\":true,\"nets\":[";
  for (int i = 0; i < n; i++) {
    if (i) o += ',';
    uint8_t* b = WiFi.BSSID(i);
    char mac[18]; snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
    const char* v = ouiName(b);                   // производитель по первым байтам адреса
    o += "{\"s\":"; jsonAppend(o, WiFi.SSID(i));
    o += ",\"b\":\""; o += mac; o += '"';
    o += ",\"v\":"; jsonAppend(o, v ? v : "");
    o += ",\"r\":"; o += WiFi.RSSI(i);
    o += ",\"c\":"; o += WiFi.channel(i);
    o += ",\"e\":\""; o += encStr(WiFi.encryptionType(i)); o += "\"}";
  }
  o += "]}";
  s_wifiJson = o;
  s_netCount = n;
  s_scanned = true;
  WiFi.scanDelete();
}

static void sendJson(const String& s) {
  s_reqs++;
  s_srv->sendHeader("Cache-Control", "no-store");
  s_srv->send(200, "application/json", s);
}

static void webStart() {
  if (s_on) return;
  wifiRadioOff();                                  // раздел Wi-Fi мог оставить радио в своём режиме
  bleLowDuty(true);
  bleRadioOn();                                    // Bluetooth слушает (реже — делит эфир с точкой)
  WiFi.mode(WIFI_AP_STA);
  makeNames();                                     // MAC известен после включения Wi-Fi
  s_ok = WiFi.softAP(s_ssid, s_pass, 6, 0, 4);
  if (!s_ok) { s_on = true; return; }
  IPAddress ip = WiFi.softAPIP();
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", ip);                        // любые адреса -> на палку (страница откроется сама)
  s_srv = new WebServer(80);
  s_srv->on("/", [] { s_reqs++; s_srv->send_P(200, "text/html", WEB_PAGE); });
  s_srv->on("/api/ble", [] { s_tab = "bluetooth"; String o; bleJson(o); sendJson(o); });
  s_srv->on("/api/bledev", [] { String o; bleDevJson((uint32_t)strtoul(s_srv->arg("id").c_str(), nullptr, 10), o); sendJson(o); });
  s_srv->on("/api/wifi", [] { s_tab = "wi-fi"; sendJson(s_wifiJson); });
  s_srv->on("/api/sub", [] {
    s_tab = "sub-ghz";
    s_subMs = millis();
    s_subOn = subWebUse(s_srv->arg("m").toInt(), s_srv->hasArg("b") ? s_srv->arg("b").toInt() : 1);
    String o; subJson(o); sendJson(o);
  });
  s_srv->on("/api/snd", [] {
    s_tab = "sound";
    s_sndMs = millis();
    s_sndOn = audioWebUse();
    String o; audioJson(o); sendJson(o);
  });
  s_srv->on("/api/info", [] {
    String o; o.reserve(400);
    o = "{\"up\":"; o += millis() / 1000;
    o += ",\"bat\":{\"ok\":"; o += g_bat.ok ? "true" : "false";
    o += ",\"soc\":"; o += g_bat.soc; o += ",\"mv\":"; o += g_bat.mv; o += ",\"ma\":"; o += g_bat.ma;
    o += ",\"t\":"; o += String(g_bat.tempC, 1); o += ",\"tte\":"; o += g_bat.tte; o += ",\"ttf\":"; o += g_bat.ttf;
    o += ",\"rem\":"; o += g_bat.remain; o += ",\"full\":"; o += g_bat.full;
    o += ",\"usb\":"; o += g_bat.bus ? "true" : "false"; o += ",\"chg\":"; o += g_bat.chg; o += '}';
    o += ",\"chip\":"; o += String(temperatureRead(), 1);
    o += ",\"heap\":"; o += ESP.getFreeHeap() / 1024; o += ",\"psram\":"; o += ESP.getFreePsram() / 1024;
    o += ",\"cli\":"; o += WiFi.softAPgetStationNum();
    o += ",\"nets\":"; o += s_netCount;
    o += ",\"build\":\"" __DATE__ " " __TIME__ "\"";
    o += ",\"ssid\":\""; o += s_ssid; o += "\"}";
    sendJson(o);
  });
  s_srv->onNotFound([] {                           // проверки «есть ли интернет» у телефонов -> на страницу
    s_srv->sendHeader("Location", "http://192.168.4.1/", true);
    s_srv->send(302, "text/plain", "");
  });
  s_srv->begin();
  s_scanMs = millis() - 15000;                     // первый скан — через пару секунд
  s_on = true;
}

static void webStop() {
  if (!s_on) return;
  if (s_srv) { s_srv->stop(); delete s_srv; s_srv = nullptr; }
  s_dns.stop();
  WiFi.scanDelete();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  bleRadioOff();
  bleLowDuty(false);
  if (s_subOn) { subWebStop(); s_subOn = false; }
  if (s_sndOn) { audioWebStop(); s_sndOn = false; }
  s_on = false;
}

static void webTick(uint32_t now) {
  if (!s_on || !s_ok) return;
  s_dns.processNextRequest();
  s_srv->handleClient();
  scanCollect();
  // CC1101 и микрофон работают, только пока их вкладку смотрят на телефоне
  if (s_subOn) { if (now - s_subMs > 4000) { subWebStop(); s_subOn = false; } else subWebTick(now); }
  if (s_sndOn) { if (now - s_sndMs > 4000) { audioWebStop(); s_sndOn = false; } else audioWebTick(now); }
  if (now - s_scanMs > 20000 && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    s_scanMs = now;
    WiFi.scanNetworks(true, true, false, 90);       // асинхронно, 90 мс на канал
  }
}

static void webDraw(uint32_t) {
  WebView v = { s_ssid, s_pass, "192.168.4.1", s_ok ? (int)WiFi.softAPgetStationNum() : 0, s_reqs, s_ok, s_tab };
  drawWebScreen(v, s_qr, s_qrN);
}

static void webSubtitle(char* out, size_t n) {
  if (s_on && s_ok) snprintf(out, n, "%d online", (int)WiFi.softAPgetStationNum());
  else              snprintf(out, n, "phone view");
}
static void webEnter() { webStart(); }
static void webLeave() { webStop(); ledsOff(); }
static void webClick() {}
static void webRotate(int) {}

const Section webSection = {
  "Web", 90, 200, 230, iconPhone, webSubtitle,
  webEnter, webLeave,
  0, nullptr, nullptr, nullptr, nullptr,
  webClick, webRotate, nullptr, webTick, webDraw,
};
