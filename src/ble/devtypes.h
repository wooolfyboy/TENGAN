// ============================================================
//  devtypes.h — разбор BLE-рекламы и определение ТИПА устройства
//
//  Только пассивный разбор того, что устройство само рассылает:
//  имя, «внешний вид» (Appearance), UUID сервисов, данные
//  производителя. Ничего не подключаем и никуда не пишем.
// ============================================================
#pragma once
#include <Arduino.h>
#include <string.h>
#include <ctype.h>

// ---------- типы устройств ----------
enum DevType : uint8_t {
  T_UNKNOWN = 0,
  T_PHONE,     // телефон / планшет
  T_COMPUTER,  // ноутбук / ПК
  T_AUDIO,     // наушники / колонки
  T_WATCH,     // часы / фитнес-браслеты
  T_TV,        // ТВ / приставки
  T_TRACKER,   // метки (AirTag, SmartTag, Tile...)
  T_BEACON,    // маячки (iBeacon / Eddystone)
  T_INPUT,     // клавиатуры, мыши, геймпады, пульты
  T_SENSOR,    // датчики и умный дом
  T_NUM
};

inline const char* typeName(uint8_t t) {
  switch (t) {
    case T_PHONE:    return "phone";
    case T_COMPUTER: return "computer";
    case T_AUDIO:    return "audio";
    case T_WATCH:    return "watch";
    case T_TV:       return "TV";
    case T_TRACKER:  return "tracker";
    case T_BEACON:   return "beacon";
    case T_INPUT:    return "input";
    case T_SENSOR:   return "sensor";
    default:         return "unknown";
  }
}

// буква-значок типа (для одноцветной темы)
inline char typeGlyph(uint8_t t) {
  switch (t) {
    case T_PHONE:    return 'P';
    case T_COMPUTER: return 'C';
    case T_AUDIO:    return 'A';
    case T_WATCH:    return 'W';
    case T_TV:       return 'V';
    case T_TRACKER:  return 'T';
    case T_BEACON:   return 'B';
    case T_INPUT:    return 'K';
    case T_SENSOR:   return 'S';
    default:         return '?';
  }
}


// ---------- разобранная реклама ----------
struct AdvInfo {
  bool     hasMfr = false;
  uint16_t cid = 0;           // Company ID производителя
  uint8_t  mfr[8] = {0};      // первые байты данных производителя (после CID)
  uint8_t  mfrLen = 0;        // полная длина данных производителя (после CID)
  uint16_t uuids[8];          // 16-битные UUID сервисов (+ из service data)
  uint8_t  nUuid = 0;
  bool     hasApp = false;
  uint16_t appearance = 0;
  char     name[24] = {0};
  uint8_t  nameLen = 0;
};

inline void addUuid(AdvInfo& a, uint16_t u) {
  for (uint8_t i = 0; i < a.nUuid; i++) if (a.uuids[i] == u) return;
  if (a.nUuid < 8) a.uuids[a.nUuid++] = u;
}

// Разбор сырого пакета: последовательность [len][type][data...]
inline void parseAdv(const uint8_t* p, size_t n, AdvInfo& a) {
  size_t i = 0;
  while (i + 1 < n) {
    uint8_t L = p[i];
    if (L == 0 || i + 1 + L > n) break;
    uint8_t t = p[i + 1];
    const uint8_t* d = p + i + 2;
    uint8_t dl = L - 1;
    switch (t) {
      case 0x02: case 0x03:                          // 16-bit UUID list
        for (uint8_t k = 0; k + 1 < dl; k += 2) addUuid(a, d[k] | (d[k + 1] << 8));
        break;
      case 0x16:                                     // service data (16-bit UUID)
        if (dl >= 2) addUuid(a, d[0] | (d[1] << 8));
        break;
      case 0x08: case 0x09:                          // имя (короткое / полное)
        if (t == 0x09 || a.nameLen == 0) {
          uint8_t m = dl < sizeof(a.name) - 1 ? dl : sizeof(a.name) - 1;
          memcpy(a.name, d, m); a.name[m] = 0; a.nameLen = m;
        }
        break;
      case 0x19:                                     // appearance
        if (dl >= 2) { a.appearance = d[0] | (d[1] << 8); a.hasApp = true; }
        break;
      case 0xFF:                                     // manufacturer data
        if (dl >= 2 && !a.hasMfr) {
          a.hasMfr = true;
          a.cid = d[0] | (d[1] << 8);
          a.mfrLen = dl - 2;
          uint8_t m = a.mfrLen < sizeof(a.mfr) ? a.mfrLen : sizeof(a.mfr);
          memcpy(a.mfr, d + 2, m);
        }
        break;
    }
    i += 1 + L;
  }
}


// ---------- по «внешнему виду» (Appearance, категория = значение >> 6) ----------
inline uint8_t typeFromAppearance(uint16_t app) {
  switch (app >> 6) {
    case 1:  return T_PHONE;
    case 2:  return T_COMPUTER;
    case 3:  return T_WATCH;
    case 5:  return T_TV;          // display
    case 6:  return T_INPUT;       // remote control
    case 8:  case 9: return T_TRACKER;   // tag / keyring
    case 10: return T_AUDIO;       // media player
    case 15: return T_INPUT;       // HID
    case 33: case 34: case 37: return T_AUDIO;  // audio sink / source / wearable audio
    case 81: return T_WATCH;       // outdoor sports activity
    case 12: case 13: case 14: case 16: case 17: case 18:
    case 21: case 22: case 23: case 24: case 25: case 26:
    case 49: case 50: case 51:
      return T_SENSOR;             // термометры, пульс, датчики, свет, умный дом
    default: return T_UNKNOWN;
  }
}

// ---------- по имени (ключевые слова) ----------
inline bool nameHas(const char* low, const char* const* words) {
  for (int i = 0; words[i]; i++) if (strstr(low, words[i])) return true;
  return false;
}

inline uint8_t typeFromName(const char* name) {
  if (!name[0]) return T_UNKNOWN;
  char low[24];
  size_t i = 0;
  for (; i < sizeof(low) - 1 && name[i]; i++) low[i] = (char)tolower((unsigned char)name[i]);
  low[i] = 0;

  static const char* const trk[] = { "tile", "smarttag", "chipolo", "airtag", nullptr };
  static const char* const aud[] = { "airpods", "buds", "beats", "jbl", "bose", "headphone",
    "headset", "earbud", "speaker", "soundbar", "soundcore", "marshall", "wh-", "wf-",
    "tws", "pods", "audio", "freebuds", "sound", nullptr };
  static const char* const wch[] = { "watch", "band", "amazfit", "fitbit", "garmin",
    "forerunner", "fenix", "gtr", "gts", nullptr };
  static const char* const tv[]  = { "tv", "bravia", "chromecast", "roku", "tizen",
    "webos", "projector", nullptr };
  static const char* const inp[] = { "keyboard", "mouse", "mx master", "mx keys",
    "controller", "gamepad", "xbox", "dualsense", "dualshock", "joy-con", "remote", "pencil", nullptr };
  static const char* const sen[] = { "lywsd", "atc_", "thermo", "sensor", "hygro", "scale",
    "bthome", "flower", "plant", "bulb", "lamp", "light", "mibfs", "switchbot", nullptr };
  static const char* const pc[]  = { "macbook", "laptop", "desktop", "imac", "thinkpad",
    "surface", "ipad", "tablet", nullptr };
  static const char* const ph[]  = { "iphone", "galaxy", "pixel", "redmi", "xiaomi",
    "phone", "oneplus", "honor", "huawei", "poco", "realme", "oppo", "vivo", nullptr };

  // порядок важен: "galaxy buds" — это наушники, а не телефон
  if (nameHas(low, trk)) return T_TRACKER;
  if (nameHas(low, aud)) return T_AUDIO;
  if (nameHas(low, wch)) return T_WATCH;
  if (nameHas(low, tv))  return T_TV;
  if (nameHas(low, inp)) return T_INPUT;
  if (nameHas(low, sen)) return T_SENSOR;
  if (nameHas(low, pc))  return T_COMPUTER;
  if (nameHas(low, ph))  return T_PHONE;
  return T_UNKNOWN;
}

// ---------- по UUID сервисов ----------
inline bool isTrackerUuid(uint16_t u) {
  return u == 0xFEED || u == 0xFEEC ||   // Tile
         u == 0xFD5A ||                  // Samsung SmartTag
         u == 0xFE33;                    // Chipolo
}

inline uint8_t typeFromUuids(const AdvInfo& a) {
  for (uint8_t i = 0; i < a.nUuid; i++) {
    switch (a.uuids[i]) {
      case 0xFEAA: return T_BEACON;                    // Eddystone
      case 0xFE2C: return T_AUDIO;                     // Google Fast Pair (обычно наушники)
      case 0x184E: case 0x184F: case 0x1853: return T_AUDIO;  // LE Audio
      case 0xFE07: return T_AUDIO;                     // Sonos
      case 0x1812: return T_INPUT;                     // HID
      case 0x180D: return T_WATCH;                     // пульс (часы / нагрудный датчик)
      case 0xFD6F: return T_PHONE;                     // уведомления о контактах (телефоны)
      case 0x181A: case 0xFCD2: case 0xFE95:           // датчики среды, BTHome, Xiaomi
      case 0x1809: case 0x1810: case 0x1808: case 0x181D:
      case 0x1816: case 0x1814:
        return T_SENSOR;
    }
  }
  return T_UNKNOWN;
}

// ---------- по данным производителя ----------
inline uint8_t typeFromMfr(const AdvInfo& a) {
  if (!a.hasMfr) return T_UNKNOWN;
  if (a.cid == 0x004C && a.mfrLen >= 1) {          // Apple: первый байт = тип сообщения
    switch (a.mfr[0]) {
      case 0x02: return T_BEACON;                  // iBeacon
      case 0x07: return T_AUDIO;                   // AirPods / Beats (proximity pairing)
      case 0x09: return T_TV;                      // AirPlay target (Apple TV и т.п.)
      case 0x0B: return T_WATCH;                   // Apple Watch
      case 0x06: return T_SENSOR;                  // HomeKit
      case 0x12: {                                 // Find My
        if (a.mfrLen >= 3) {
          switch ((a.mfr[2] >> 4) & 0x03) {        // байт статуса: кто вещает
            case 1: case 2: return T_TRACKER;      // AirTag / сторонняя метка Find My
            case 3: return T_AUDIO;                // AirPods
            default: return T_PHONE;               // iPhone / iPad / Mac
          }
        }
        return T_TRACKER;
      }
      case 0x05: case 0x0A: case 0x0C: case 0x0D:
      case 0x0E: case 0x0F: case 0x10:
        return T_PHONE;                            // AirDrop / Handoff / Nearby — iPhone и др.
    }
    return T_UNKNOWN;
  }
  if (a.cid == 0x0006 && a.mfrLen >= 1 && a.mfr[0] == 0x01) return T_COMPUTER;  // Windows-ПК
  switch (a.cid) {
    case 0x0087: return T_WATCH;    // Garmin
    case 0x0157: return T_WATCH;    // Huami (Amazfit / Mi Band)
    case 0x009E: return T_AUDIO;    // Bose
    case 0x0057: return T_AUDIO;    // Harman (JBL)
  }
  return T_UNKNOWN;
}

// ---------- итоговая классификация ----------
inline uint8_t classify(const AdvInfo& a) {
  for (uint8_t i = 0; i < a.nUuid; i++) if (isTrackerUuid(a.uuids[i])) return T_TRACKER;
  uint8_t t;
  if (a.hasApp && (t = typeFromAppearance(a.appearance)) != T_UNKNOWN) return t;
  if ((t = typeFromName(a.name)) != T_UNKNOWN) return t;
  if ((t = typeFromUuids(a)) != T_UNKNOWN) return t;
  return typeFromMfr(a);
}

// ---------- «отпечаток» рекламы для склейки смены адресов ----------
// Берём только то, что не меняется у устройства от пакета к пакету:
// производитель, тип сообщения, длина данных, сервисы, имя.
// 0 = отпечаток пустой (склеивать не по чему).
inline uint32_t fnv(uint32_t h, uint32_t v) { h ^= v; return h * 16777619u; }

inline uint32_t advSignature(const AdvInfo& a) {
  if (!a.hasMfr && a.nUuid == 0 && a.nameLen == 0 && !a.hasApp) return 0;
  uint32_t h = 2166136261u;
  if (a.hasMfr) {
    h = fnv(h, a.cid);
    h = fnv(h, a.mfrLen);
    if (a.mfrLen >= 1) h = fnv(h, a.mfr[0]);
  }
  // UUID — в отсортированном порядке
  uint16_t u[8]; uint8_t n = a.nUuid;
  memcpy(u, a.uuids, n * sizeof(uint16_t));
  for (uint8_t i = 1; i < n; i++)
    for (uint8_t j = i; j > 0 && u[j - 1] > u[j]; j--) { uint16_t x = u[j]; u[j] = u[j - 1]; u[j - 1] = x; }
  for (uint8_t i = 0; i < n; i++) h = fnv(h, u[i]);
  if (a.hasApp) h = fnv(h, a.appearance);
  for (uint8_t i = 0; i < a.nameLen; i++) h = fnv(h, (uint8_t)a.name[i]);
  return h ? h : 1;
}
