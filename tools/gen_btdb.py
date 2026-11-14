#!/usr/bin/env python3
"""Генерирует src/ble/btdb.h — таблицы производителей Bluetooth.

Источники (скачать заранее):
  company_ids.json  — https://raw.githubusercontent.com/NordicSemiconductor/bluetooth-numbers-database/master/v1/company_ids.json
  member_uuids.yaml — https://bitbucket.org/bluetooth-SIG/public/raw/main/assigned_numbers/uuids/member_uuids.yaml

Запуск:  python3 tools/gen_btdb.py company_ids.json member_uuids.yaml > src/ble/btdb.h
"""
import json, re, sys

DROP = {"inc", "ltd", "llc", "corp", "corporation", "co", "company", "gmbh", "ag", "ab",
        "sa", "asa", "bv", "b.v", "limited", "oy", "plc", "srl", "s.r.l", "kg", "pte",
        "technology", "technologies", "international", "industries", "information",
        "electronic", "electronics", "systems", "group", "mobile", "communications",
        "telecommunications", "communication", "intelligent", "device", "devices", "services", "innovations",
        "semiconductor", "lab126", "holdings", "kk", "pty", "nv", "n.v", "a/s", "as", "spa",
        "s.p.a", "sas", "sarl", "development", "science", "and", "&"}
CITY = {"shenzhen", "guangdong", "guangzhou", "zhuhai", "beijing", "shanghai", "hangzhou",
        "anhui", "jiangsu", "suzhou", "ningbo", "xiamen", "dongguan", "chengdu", "wuhan",
        "nanjing", "qingdao", "tianjin", "chongqing", "hong", "kong", "hk", "the"}

def short(name: str) -> str:
    n = re.sub(r"\(.*?\)", " ", name.replace('"', "").replace("'", ""))
    n = n.split(",")[0]
    words = [w for w in re.split(r"\s+", n.strip()) if w]
    while words and words[0].lower().strip(".") in CITY and len(words) > 1:
        words.pop(0)
    while len(words) > 1 and words[-1].lower().strip(".,") in DROP:
        words.pop()
    s = " ".join(words).strip(" .,")
    if not s:
        s = name.strip()[:18]
    return s[:18].rstrip()

def cstr(s: str) -> str:
    return '"' + s.encode("ascii", "replace").decode().replace("\\", "\\\\").replace('"', '\\"') + '"'

def main():
    cids = json.load(open(sys.argv[1], encoding="utf-8"))
    mem = open(sys.argv[2], encoding="utf-8").read()
    uu = re.findall(r"uuid:\s*0x([0-9A-Fa-f]+)\s*\n\s*name:\s*([^\n]+)", mem)

    comp = sorted({e["code"]: short(e["name"]) for e in cids if e["code"] <= 0xFFFF}.items())
    memb = sorted({int(u, 16): short(n) for u, n in uu}.items())

    out = []
    out.append("// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/gen_btdb.py — не править руками.")
    out.append("// Производители Bluetooth (Company ID) и владельцы 16-битных UUID (member UUIDs).")
    out.append("#pragma once\n#include <stdint.h>\n")
    out.append("struct BtName { uint16_t id; const char* name; };\n")
    out.append(f"static const BtName BT_COMPANIES[{len(comp)}] = {{")
    out += [f"  {{0x{c:04X}, {cstr(n)}}}," for c, n in comp]
    out.append("};\n")
    out.append(f"static const BtName BT_MEMBER_UUIDS[{len(memb)}] = {{")
    out += [f"  {{0x{u:04X}, {cstr(n)}}}," for u, n in memb]
    out.append("};\n")
    out.append("""static inline const char* btLookup(const BtName* t, int n, uint16_t id) {
  int lo = 0, hi = n - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (t[mid].id == id) return t[mid].name;
    if (t[mid].id < id) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}
static inline const char* companyName(uint16_t cid) {
  return btLookup(BT_COMPANIES, sizeof(BT_COMPANIES) / sizeof(BT_COMPANIES[0]), cid);
}
static inline const char* uuidOwner(uint16_t uuid) {
  return btLookup(BT_MEMBER_UUIDS, sizeof(BT_MEMBER_UUIDS) / sizeof(BT_MEMBER_UUIDS[0]), uuid);
}""")
    print("\n".join(out))

if __name__ == "__main__":
    main()
