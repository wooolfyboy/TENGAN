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
#include <Update.h>
#include <esp_ota_ops.h>
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

// ---------- обновление прошивки по Wi-Fi (OTA) ----------
// Принимаем и обычный firmware.bin, и «общий» файл для прошивки на 0x0
// (в нём программа лежит со смещения 0x10000 — начало пропускаем).
// Новая прошивка пишется во второй слот; пока она не проработает 15 с
// (main.cpp), загрузчик готов откатить палку на старую.
static int      s_otaPct = -1;                  // -1 — загрузки нет
static String   s_otaErr;
static uint8_t* s_otaHead = nullptr;            // первые 0x8002 байт: решаем, «общий» ли файл
static size_t   s_otaHeadN = 0, s_otaPos = 0, s_otaSkip = 0;
static bool     s_otaStarted = false;
static const size_t OTA_PROBE = 0x8002, OTA_APP_OFS = 0x10000;

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

// какая вкладка открыта на телефоне: страница присылает её в каждом запросе (?tab=)
static void noteTab() {
  static const char* const T[][2] = { { "ov", "overview" }, { "ble", "bluetooth" }, { "wifi", "wi-fi" },
                                      { "sub", "sub-ghz" }, { "snd", "sound" }, { "sys", "system" },
                                      { "upd", "update" } };
  String t = s_srv->arg("tab");
  for (auto& p : T) if (t == p[0]) { s_tab = p[1]; return; }
}
static bool tabIs(const char* a) { return strcmp(s_tab, a) == 0; }

static void sendJson(const String& s) {
  noteTab();
  s_reqs++;
  // без этого заголовок и данные уходят двумя кусками, и телефон ждёт ~0.2-0.5 с
  // перед вторым (алгоритм Нейгла + отложенное подтверждение TCP)
  s_srv->client().setNoDelay(true);
  s_srv->sendHeader("Cache-Control", "no-store");
  s_srv->send(200, "application/json", s);
}

static void otaDrawProgress() {
  WebView v = { s_ssid, s_pass, "192.168.4.1", 1, s_reqs, true, "update" };
  drawWebOta(v, s_otaPct, s_otaErr.c_str());
}

static bool otaWrite(uint8_t* d, size_t n) {
  // пропустить загрузчик и таблицу разделов «общего» файла
  if (s_otaSkip) {
    size_t k = min(n, s_otaSkip);
    d += k; n -= k; s_otaSkip -= k;
    if (!n) return true;
  }
  if (!s_otaStarted) {
    if (d[0] != 0xE9) { s_otaErr = "not an ESP32 firmware file"; return false; }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { s_otaErr = Update.errorString(); return false; }
    s_otaStarted = true;
  }
  if (Update.write(d, n) != n) { s_otaErr = Update.errorString(); return false; }
  return true;
}

static void otaUpload() {
  HTTPUpload& up = s_srv->upload();
  if (up.status == UPLOAD_FILE_START) {
    s_otaErr = ""; s_otaPct = 0; s_otaPos = 0; s_otaHeadN = 0; s_otaSkip = 0; s_otaStarted = false;
    bleRadioOff();                                 // весь эфир — загрузке
    if (!s_otaHead) s_otaHead = (uint8_t*)malloc(OTA_PROBE);
    if (!s_otaHead) s_otaErr = "no memory";
    otaDrawProgress();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_otaErr.length()) return;
    uint8_t* d = up.buf; size_t n = up.currentSize;
    s_otaPos += n;
    if (s_otaHeadN < OTA_PROBE && s_otaHead) {     // копим начало файла
      size_t k = min(n, OTA_PROBE - s_otaHeadN);
      memcpy(s_otaHead + s_otaHeadN, d, k);
      s_otaHeadN += k; d += k; n -= k;
      if (s_otaHeadN < OTA_PROBE) return;
      bool merged = s_otaHead[0x8000] == 0xAA && s_otaHead[0x8001] == 0x50;   // таблица разделов на 0x8000
      if (merged) s_otaSkip = OTA_APP_OFS - OTA_PROBE;                        // остаток до 0x10000 пропустить
      else if (!otaWrite(s_otaHead, OTA_PROBE)) return;
    }
    if (n && !otaWrite(d, n)) return;
    static uint32_t lastDraw = 0;
    if (millis() - lastDraw > 400) {               // экран палки: прогресс (размер неизвестен — по ~2 МБ)
      lastDraw = millis();
      s_otaPct = min(99, (int)(s_otaPos * 100 / 2100000));
      otaDrawProgress();
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (!s_otaErr.length() && s_otaHeadN < OTA_PROBE && s_otaHead)   // файл меньше 32 КБ
      s_otaErr = "file is too small";
    if (!s_otaErr.length() && !Update.end(true)) s_otaErr = Update.errorString();
    if (s_otaErr.length() && s_otaStarted) Update.abort();
    s_otaPct = s_otaErr.length() ? -2 : 100;
    otaDrawProgress();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (s_otaStarted) Update.abort();
    s_otaErr = "upload interrupted"; s_otaPct = -2;
    otaDrawProgress();
  }
}

static void otaDone() {
  s_reqs++;
  bool ok = s_otaPct == 100 && !s_otaErr.length();
  String o = "{\"ok\":"; o += ok ? "true" : "false";
  o += ",\"err\":\""; o += s_otaErr; o += "\"}";
  s_srv->client().setNoDelay(true);
  s_srv->send(200, "application/json", o);
  if (ok) {                                        // ответить телефону и перезагрузиться в новую прошивку
    delay(600);
    ESP.restart();
  }
  s_otaPct = -1;
  bleRadioOn();
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
  s_srv->on("/api/ble", [] { String o; bleJson(o); sendJson(o); });
  s_srv->on("/api/bledev", [] { String o; bleDevJson((uint32_t)strtoul(s_srv->arg("id").c_str(), nullptr, 10), o); sendJson(o); });
  s_srv->on("/api/wifi", [] { sendJson(s_wifiJson); });
  s_srv->on("/api/sub", [] {
    s_subMs = millis();
    s_subOn = subWebUse(s_srv->arg("m").toInt(), s_srv->hasArg("b") ? s_srv->arg("b").toInt() : 1);
    String o; subJson(o); sendJson(o);
  });
  s_srv->on("/api/snd", [] {
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
  s_srv->on("/update", HTTP_POST, otaDone, otaUpload);
  s_srv->on("/api/ver", [] {
    const esp_partition_t* run = esp_ota_get_running_partition();
    String o = "{\"build\":\"" __DATE__ " " __TIME__ "\",\"slot\":\""; o += run ? run->label : "?";
    o += "\",\"max\":"; o += (uint32_t)ESP.getFreeSketchSpace(); o += '}';
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
  s_tab = "";
}

static void webTick(uint32_t now) {
  if (!s_on || !s_ok) return;
  s_dns.processNextRequest();
  s_srv->handleClient();
  scanCollect();
  // работает только то, что нужно открытой вкладке, остальное молчит:
  //  Bluetooth — на «Обзоре» и «Bluetooth»; скан сетей — на «Обзоре» и «Wi-Fi»;
  //  CC1101 и микрофон — только на своих вкладках (ниже)
  static bool bleWanted = true;
  bool wantBle = s_tab[0] == 0 || tabIs("overview") || tabIs("bluetooth");
  if (wantBle != bleWanted) { bleWanted = wantBle; if (wantBle) bleRadioOn(); else bleRadioOff(); }
  bool wantScan = s_tab[0] == 0 || tabIs("overview") || tabIs("wi-fi");
  // CC1101 и микрофон работают, только пока их вкладку смотрят на телефоне
  // время берём заново: запрос мог прийти уже после того, как посчитали now,
  // и тогда now - s_sndMs «перекручивается» в огромное число (микрофон гас сразу)
  uint32_t t = millis();
  if (s_subOn) { if ((int32_t)(t - s_subMs) > 4000) { subWebStop(); s_subOn = false; } else subWebTick(t); }
  if (s_sndOn) { if ((int32_t)(t - s_sndMs) > 4000) { audioWebStop(); s_sndOn = false; } else audioWebTick(t); }
  if (wantScan && now - s_scanMs > 20000 && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {   // скан «глушит» точку на ~1 с
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
