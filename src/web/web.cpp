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
#include "core/jsonutil.h"
#include "web/web.h"
#include "core/board.h"
#include "web/web_page.h"
#include "web/tablet_page.h"    // приложение для планшета (gzip, собирается tools/webapp/build.py)
#include <lwip/sockets.h>
#include "ble/ble.h"
#include "wifi/wifi.h"
#include "wifi/wifi_model.h"
#include "subghz/subghz.h"
#include "audio/audio.h"
#include "pc/pc.h"
#include "gallery/astro.h"
#include "gallery/sat.h"
#include "core/settings.h"
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
static uint32_t   s_wifiGen = 0;              // растёт при каждом новом результате скана (поток планшета)
static uint8_t    s_qr[45 * 45];
static int        s_qrN = 0;
static int        s_netCount = 0;
static uint32_t   s_subMs = 0, s_sndMs = 0;     // когда вкладки Sub-GHz / Sound последний раз спрашивали
static bool       s_subOn = false, s_sndOn = false;
static const char* s_tab = "";
static int8_t      s_bleWanted = -1;            // что webTick последний раз решил про Bluetooth (-1 — пересчитать)

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
  snprintf(s_ssid, sizeof(s_ssid), "TENGAN-%02X%02X", m[4], m[5]);
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

// Скан сетей «по одному каналу»: раньше палка уходила с канала точки на все 13 каналов
// разом (~1.2 с) — планшет/телефон терял связь и мог отвалиться от Wi-Fi. Теперь раз в
// SCAN_STEP_MS слушаем один канал ~90 мс и сразу возвращаемся; полный круг — ~20 с,
// результаты копятся в таблице и устаревают через NET_FORGET_MS.
#define NET_MAX 64
static const uint32_t SCAN_STEP_MS = 1500, NET_FORGET_MS = 60000;
struct NetRec { uint8_t b[6]; char ssid[33]; int8_t r; uint8_t ch, enc; uint32_t t; };
static NetRec   s_nets[NET_MAX];
static int      s_nNets = 0;
static uint8_t  s_scanCh = 1;

static void netsJson() {
  uint32_t now = millis();
  int w = 0;                                       // выбросить давно не слышанные
  for (int i = 0; i < s_nNets; i++) if (now - s_nets[i].t < NET_FORGET_MS) s_nets[w++] = s_nets[i];
  s_nNets = w;
  String o; o.reserve(64 + s_nNets * 90);
  o += "{\"scanned\":true,\"nets\":[";
  for (int i = 0; i < s_nNets; i++) {
    const NetRec& n = s_nets[i];
    if (i) o += ',';
    char mac[18]; snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", n.b[0], n.b[1], n.b[2], n.b[3], n.b[4], n.b[5]);
    const char* v = ouiName(n.b);                  // производитель по первым байтам адреса
    o += "{\"s\":"; jsonStr(o, n.ssid);
    o += ",\"b\":\""; o += mac; o += '"';
    o += ",\"v\":"; jsonStr(o, v ? v : "");
    o += ",\"r\":"; o += n.r;
    o += ",\"c\":"; o += n.ch;
    o += ",\"e\":\""; o += encStr((wifi_auth_mode_t)n.enc); o += "\"}";
  }
  o += "]}";
  s_wifiJson = o;
  s_netCount = s_nNets;
  s_wifiGen++;
  s_scanned = true;
}

static void scanCollect() {
  int n = WiFi.scanComplete();
  if (n < 0) return;                              // ещё идёт (или не запускали)
  uint32_t now = millis();
  for (int i = 0; i < n; i++) {
    uint8_t* b = WiFi.BSSID(i);
    int k = 0;
    while (k < s_nNets && memcmp(s_nets[k].b, b, 6)) k++;
    if (k == s_nNets) { if (s_nNets >= NET_MAX) continue; s_nNets++; memcpy(s_nets[k].b, b, 6); }
    NetRec& r = s_nets[k];
    strlcpy(r.ssid, WiFi.SSID(i).c_str(), sizeof(r.ssid));
    r.r = WiFi.RSSI(i); r.ch = WiFi.channel(i); r.enc = (uint8_t)WiFi.encryptionType(i); r.t = now;
  }
  WiFi.scanDelete();
  netsJson();
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
  s_srv->sendHeader("Access-Control-Allow-Origin", "*");   // приложение планшета живёт на своём адресе (localhost)
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
  s_bleWanted = -1;                                // радио вернёт webTick — по тому, что смотрят
}

// ============================================================
//  «Вторая дверь» для планшета: поток SSE на порту 81.
//  Планшет открывает http://192.168.4.1:81/stream?want=ble,wifi,snd и не
//  закрывает — палка сама толкает туда события:
//    event: st    раз в 2 с    (батарея, аптайм, клиенты)
//    event: ble   раз в 1 с    (устройства, компактно)
//    event: wifi  после скана  (сети)
//    event: snd   ~25 раз в с  (полосы, уровень, волна) — только если просили
//    event: sub   ~7 раз в с   (спектр CC1101; режим и полоса — &sm= &sb=)
//    event: pc    раз в 1 с    (ноутбук: нагрузка, трек, город процессов)
//    event: sat   при подключении (орбиты спутников и город — небо считает планшет)
//  Отправка без ожидания: если планшет не успевает принимать, кадр звука
//  пропускается, а палка не «зависает» на записи.
// ============================================================
#define SSE_MAX 2
struct SseCli {
  NetworkClient c;
  bool on = false, ble = false, wifi = false, snd = false, sub = false, pc = false, satSent = false;
  int8_t sm = 0, sb = 1;                         // режим и полоса Sub-GHz
  String pend;                                   // не отправленный хвост (докидываем без ожидания)
  uint32_t tSt = 0, tBle = 0, tSnd = 0, tSub = 0, tPc = 0, wifiSeen = 0;
};
static NetworkServer* s_sseSrv = nullptr;
static SseCli s_sse[SSE_MAX];

static bool sseFlush(SseCli& k) {                // true — всё ушло
  while (k.pend.length()) {
    int n = send(k.c.fd(), k.pend.c_str(), k.pend.length(), MSG_DONTWAIT);
    if (n > 0) { k.pend.remove(0, n); continue; }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return false;   // буфер полон — позже
    k.on = false; k.c.stop(); return false;      // соединение умерло
  }
  return true;
}
static void sseEvent(SseCli& k, const char* ev, const String& data, bool droppable) {
  if (!k.on) return;
  if (k.pend.length() && droppable) { sseFlush(k); return; }   // не успевает — кадр звука пропускаем
  if (k.pend.length() > 24000) { k.on = false; k.c.stop(); return; }
  k.pend += "event: "; k.pend += ev; k.pend += "\ndata: "; k.pend += data; k.pend += "\n\n";
  sseFlush(k);
}

static void sseAccept() {
  NetworkClient nc = s_sseSrv->accept();
  if (!nc) return;
  // запрос приходит сразу за подключением: читаем первую строку и заголовки (до 300 мс)
  String req; uint32_t t0 = millis(); bool done = false;
  nc.setTimeout(300);
  while (millis() - t0 < 300 && nc.connected()) {
    if (!nc.available()) { delay(1); continue; }
    String line = nc.readStringUntil('\n');
    if (!req.length()) req = line;
    if (line == "\r" || line.length() == 0) { done = true; break; }
  }
  if (!done || !req.startsWith("GET /stream")) {
    nc.print("HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 0\r\n\r\n");
    nc.stop(); return;
  }
  int slot = 0;
  for (int i = 0; i < SSE_MAX; i++) if (!s_sse[i].on) { slot = i; break; }   // оба заняты — вытесняем первого
  SseCli& k = s_sse[slot];
  if (k.on) k.c.stop();
  k = SseCli();
  k.c = nc; k.c.setNoDelay(true); k.on = true;
  int q = req.indexOf("want="); String want = q >= 0 ? req.substring(q + 5, req.indexOf(' ', q)) : String("ble,wifi");
  k.ble = want.indexOf("ble") >= 0; k.wifi = want.indexOf("wifi") >= 0; k.snd = want.indexOf("snd") >= 0;
  k.sub = want.indexOf("sub") >= 0; k.pc = want.indexOf("pc") >= 0;
  auto qInt = [&](const char* key, int def) { int p = req.indexOf(key); return p >= 0 ? req.substring(p + strlen(key)).toInt() : def; };
  k.sm = (int8_t)qInt("sm=", 0); k.sb = (int8_t)qInt("sb=", 1);
  k.pend = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
           "Connection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\nretry: 1500\n\n";
  sseFlush(k);
  k.tSt = k.tBle = 0; k.wifiSeen = 0;
  s_reqs++;
}

// что сейчас нужно планшетам (для выключения ненужного радио)
static int sseClients(bool& ble, bool& wifi, bool& snd, int& subM, int& subB) {
  int n = 0; ble = wifi = snd = false; subM = -1; subB = 1;
  for (auto& k : s_sse) if (k.on) { n++; ble |= k.ble; wifi |= k.wifi; snd |= k.snd; if (k.sub) { subM = k.sm; subB = k.sb; } }
  return n;
}

static void sseTick(uint32_t now) {
  if (!s_sseSrv) return;
  sseAccept();
  for (auto& k : s_sse) {
    if (!k.on) continue;
    if (!k.c.connected()) { k.on = false; k.c.stop(); continue; }
    if (k.pend.length()) sseFlush(k);
    if (!k.tSt || now - k.tSt >= 2000) {
      k.tSt = now;
      String o = "{\"name\":\"" DEVICE_NAME "\",\"up\":"; o += millis() / 1000;
      o += ",\"bat\":"; o += g_bat.ok ? g_bat.soc : -1; o += ",\"mv\":"; o += g_bat.mv;
      o += ",\"usb\":"; o += g_bat.bus ? 1 : 0; o += ",\"chg\":"; o += g_bat.chg;
      o += ",\"cli\":"; o += WiFi.softAPgetStationNum(); o += ",\"heap\":"; o += ESP.getFreeHeap() / 1024;
      o += ",\"theme\":\""; o += g_theme == THEME_SUMI ? "sumi" : "denko"; o += '"';
      o += ",\"radio\":\""; o += bleRadioActive() ? "b" : ""; o += s_scanMs && now - s_scanMs < 25000 ? "w" : "";
      o += s_subOn ? "s" : ""; o += s_sndOn ? "m" : ""; o += pcLive(now) ? "p" : ""; o += '"';
      o += '}';
      sseEvent(k, "st", o, false);
      if (!k.satSent) {                          // один раз: город и орбиты — небо и глобус считает планшет
        k.satSent = true;
        const City& c = curCity();
        String a = "{\"city\":"; jsonStr(a, c.name);
        a += ",\"lat\":"; a += String(c.lat, 3); a += ",\"lon\":"; a += String(c.lon, 3);
        a += ",\"sats\":"; satJson(a); a += '}';
        sseEvent(k, "sat", a, false);
      }
    }
    if (k.ble && (!k.tBle || now - k.tBle >= 1000)) { k.tBle = now; String o; bleStreamJson(o); sseEvent(k, "ble", o, false); }
    if (k.wifi && k.wifiSeen != s_wifiGen) { k.wifiSeen = s_wifiGen; sseEvent(k, "wifi", s_wifiJson, false); }
    if (k.snd && now - k.tSnd >= 40) { k.tSnd = now; String o; audioStreamJson(o); sseEvent(k, "snd", o, true); }
    if (k.sub && s_subOn && now - k.tSub >= 140) { k.tSub = now; String o; subJson(o); sseEvent(k, "sub", o, true); }
    if (k.pc && now - k.tPc >= 1000) { k.tPc = now; String o; pcStreamJson(o); sseEvent(k, "pc", o, false); }
  }
}

static void webStart() {
  if (s_on) return;
  wifiRadioOff();                                  // раздел Wi-Fi мог оставить радио в своём режиме
  bleLowDuty(true);
  s_bleWanted = -1;                                // Bluetooth включит webTick, только если его смотрят (реже — делит эфир с точкой)
  WiFi.mode(WIFI_AP_STA);
  makeNames();                                     // MAC известен после включения Wi-Fi
  s_ok = WiFi.softAP(s_ssid, s_pass, 6, 0, 4);
  if (!s_ok) { s_on = true; return; }
  IPAddress ip = WiFi.softAPIP();
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", ip);                        // любые адреса -> на палку (страница откроется сама)
  s_srv = new WebServer(80);
  // вход: «/» — запуск, заставка и выбор LITE / FULL (то же приложение, что /full);
  // «/lite» — лёгкая телефонная страница; «/full» — сразу полная сцена, без выбора.
  // Старые адреса /phone и /tablet оставлены и ведут туда же.
  static auto sendApp = [] {                       // приложение для планшета: один файл, сжатый gzip
    s_reqs++;
    s_srv->sendHeader("Content-Encoding", "gzip");
    s_srv->sendHeader("Cache-Control", "no-cache");
    s_srv->send_P(200, "text/html", (const char*)TABLET_GZ, TABLET_GZ_LEN);
  };
  s_srv->on("/", [] { sendApp(); });
  s_srv->on("/full", [] { sendApp(); });
  s_srv->on("/tablet", [] { sendApp(); });
  static auto sendLite = [] { s_reqs++; s_srv->send_P(200, "text/html", WEB_PAGE); };
  s_srv->on("/lite", [] { sendLite(); });
  s_srv->on("/phone", [] { sendLite(); });
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
  // приложение планшета присылает свой GPS: небо и глобус палки считаются «отсюда»
  s_srv->on("/api/loc", [] {
    float lat = s_srv->arg("lat").toFloat(), lon = s_srv->arg("lon").toFloat();
    bool ok = s_srv->hasArg("lat") && s_srv->hasArg("lon") && fabsf(lat) <= 90 && fabsf(lon) <= 180 && (lat != 0 || lon != 0);
    if (ok) setGpsCity(lat, lon);
    sendJson(ok ? "{\"ok\":true}" : "{\"ok\":false}");
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
    o += ",\"theme\":\""; o += g_theme == THEME_SUMI ? "sumi" : "denko"; o += '"';
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
  s_sseSrv = new NetworkServer(81);                  // «вторая дверь» — поток для планшета
  s_sseSrv->begin();
  s_sseSrv->setNoDelay(true);
  s_scanMs = millis();                             // первый канал — через пару секунд
  s_on = true;
}

static void webStop() {
  if (!s_on) return;
  if (s_srv) { s_srv->stop(); delete s_srv; s_srv = nullptr; }
  for (auto& k : s_sse) { if (k.on) k.c.stop(); k = SseCli(); }
  if (s_sseSrv) { s_sseSrv->end(); delete s_sseSrv; s_sseSrv = nullptr; }
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
  sseTick(millis());
  bool sBle, sWifi, sSnd; int subM, subB;
  int nSse = sseClients(sBle, sWifi, sSnd, subM, subB);
  if (nSse && sSnd) { s_sndMs = millis(); if (!s_sndOn) s_sndOn = audioWebUse(); }   // планшет смотрит звук
  if (nSse && subM >= 0) { s_subMs = millis(); s_subOn = subWebUse(subM, subB); }    // планшет смотрит Sub-GHz
  // работает только то, что нужно открытой вкладке, остальное молчит:
  //  Bluetooth — на «Обзоре» и «Bluetooth»; скан сетей — на «Обзоре» и «Wi-Fi»;
  //  CC1101 и микрофон — только на своих вкладках (ниже)
  // пока никто ничего не смотрит — радио молчит (железное правило: что работает, то и читается)
  bool wantBle = tabIs("overview") || tabIs("bluetooth");
  if (nSse) wantBle = sBle;                          // планшет подключён — решают его подписки
  if ((int8_t)wantBle != s_bleWanted) { s_bleWanted = wantBle; if (wantBle) bleRadioOn(); else bleRadioOff(); }
  bool wantScan = tabIs("overview") || tabIs("wi-fi");
  if (nSse) wantScan = sWifi;
  // CC1101 и микрофон работают, только пока их вкладку смотрят на телефоне
  // время берём заново: запрос мог прийти уже после того, как посчитали now,
  // и тогда now - s_sndMs «перекручивается» в огромное число (микрофон гас сразу)
  uint32_t t = millis();
  if (s_subOn) { if ((int32_t)(t - s_subMs) > 4000) { subWebStop(); s_subOn = false; } else subWebTick(t); }
  if (s_sndOn) { if ((int32_t)(t - s_sndMs) > 4000) { audioWebStop(); s_sndOn = false; } else audioWebTick(t); }
  if (wantScan && now - s_scanMs > SCAN_STEP_MS && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {   // один канал за раз — точка не пропадает
    s_scanMs = now;
    WiFi.scanNetworks(true, true, false, 90, s_scanCh);   // асинхронно, 90 мс на один канал
    s_scanCh = s_scanCh >= 13 ? 1 : s_scanCh + 1;
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
