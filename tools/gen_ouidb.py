#!/usr/bin/env python3
"""Генерирует src/ouidb.h — производители сетевых устройств по первым 3 байтам MAC (OUI).

Источник (скачать заранее):
  nmap-mac-prefixes — https://raw.githubusercontent.com/nmap/nmap/master/nmap-mac-prefixes
  (данные IEEE, собранные проектом Nmap)

Запуск:  python3 tools/gen_ouidb.py nmap-mac-prefixes > src/ouidb.h
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_btdb import short, cstr

def main():
    ent = {}
    for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        pfx, _, name = line.partition(" ")
        if len(pfx) != 6:
            continue                      # только полные 24-битные OUI
        ent[int(pfx, 16)] = short(name.strip())
    keys = sorted(ent)
    names = sorted(set(ent.values()))
    idx = {n: i for i, n in enumerate(names)}
    out = ["// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/gen_ouidb.py — не править руками.",
           "// Производители по OUI (первые 3 байта MAC). Данные IEEE через Nmap.",
           "#pragma once", "#include <stdint.h>", ""]
    out.append(f"static const char* const OUI_NAMES[{len(names)}] = {{")
    for i in range(0, len(names), 8):
        out.append("  " + ", ".join(cstr(n) for n in names[i:i + 8]) + ",")
    out.append("};")
    out.append(f"static const uint32_t OUI_KEYS[{len(keys)}] = {{")
    for i in range(0, len(keys), 12):
        out.append("  " + ", ".join(f"0x{k:06X}" for k in keys[i:i + 12]) + ",")
    out.append("};")
    out.append(f"static const uint16_t OUI_IDX[{len(keys)}] = {{")
    for i in range(0, len(keys), 16):
        out.append("  " + ", ".join(str(idx[ent[k]]) for k in keys[i:i + 16]) + ",")
    out.append("};")
    out.append("""
// имя производителя по первым трём байтам MAC (или nullptr)
static inline const char* ouiVendor(const uint8_t* mac) {
  uint32_t k = ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | mac[2];
  int lo = 0, hi = (int)(sizeof(OUI_KEYS) / sizeof(OUI_KEYS[0])) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (OUI_KEYS[mid] == k) return OUI_NAMES[OUI_IDX[mid]];
    if (OUI_KEYS[mid] < k) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}""")
    print("\n".join(out))
    print(f"// {len(keys)} OUI, {len(names)} names", file=sys.stderr)

if __name__ == "__main__":
    main()
