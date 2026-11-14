// ============================================================
//  vendors.cpp — производители Bluetooth-устройств
// ============================================================
#include "ble/vendors.h"
#include "ble/ble_model.h"
#include "ble/btdb.h"            // база Bluetooth — подключается только здесь
#include "core/display.h"
#include "core/theme.h"

const char* btUuidOwner(uint16_t uuid) { return uuidOwner(uuid); }

// производители Bluetooth-чипов: их код говорит о «начинке», а не о бренде устройства
static bool isChipMaker(uint16_t cid) {
  switch (cid) {
    case 0x0002: case 0x0009: case 0x000A: case 0x000D: case 0x000F: case 0x001D:
    case 0x0046: case 0x0059: case 0x005D: case 0x0094: case 0x00D7: case 0x0131:
    case 0x0211: case 0x02B0: case 0x02E5: case 0x02FF: case 0x03E0: case 0x05D6:
    case 0x0642: case 0x07E3: case 0x0917: case 0x00D2:
      return true;
  }
  return false;
}

// UUID, которыми пользуются устройства любых брендов (о производителе ничего не говорят)
static bool isSharedUuid(uint16_t u) {
  return u == 0xFD6F /* уведомления о контактах */ || u == 0xFEAA /* Eddystone */ ||
         u == 0xFE2C /* Fast Pair в чужих наушниках */;
}

static uint8_t brandFromName(const char* name) {
  if (!name) return V_OTHER;
  char w[16]; int i = 0;
  while (name[i] && name[i] != ' ' && name[i] != '.' && i < 15) { w[i] = (char)tolower((unsigned char)name[i]); i++; }
  w[i] = 0;
  for (int b = 2; b < N_BRANDS; b++) if (!strcmp(w, BRANDS[b].key)) return (uint8_t)b;
  return V_OTHER;
}

// Выбираем лучшего «кандидата в производители» из Company ID и UUID сервисов
Vend resolveVendor(const AdvInfo& ai) {
  Vend best = { V_PRIV, 0, 0, 0 };
  auto consider = [&](uint8_t kind, uint16_t code, const char* name, bool chip) {
    uint8_t cat = brandFromName(name);
    uint8_t score = (cat >= 2) ? 3 : (chip || !name) ? 1 : 2;
    if (score > best.score) best = { cat, kind, score, code };
  };
  if (ai.hasMfr) consider(1, ai.cid, companyName(ai.cid), isChipMaker(ai.cid));
  for (uint8_t i = 0; i < ai.nUuid; i++) {
    uint16_t u = ai.uuids[i];
    if (isSharedUuid(u)) continue;
    const char* owner = uuidOwner(u);
    if (owner) consider(2, u, owner, false);
  }
  return best;
}

// имя производителя для экрана
const char* vendorLabel(uint8_t cat, uint8_t kind, uint16_t code) {
  if (cat >= 2 && cat < N_BRANDS) return BRANDS[cat].label;
  if (kind == 1) { const char* n = companyName(code); if (n) return n; }
  if (kind == 2) { const char* n = uuidOwner(code);   if (n) return n; }
  if (kind == 1) {
    static char hex[8]; sprintf(hex, "0x%04X", code); return hex;
  }
  return "-";
}
