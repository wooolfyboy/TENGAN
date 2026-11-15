#!/usr/bin/env python3
"""Данные для Gallery › Sky и Globe -> src/gallery/astrodata.h

Источники (tools/astro_src/):
  stars.mag5.json, constellations*.json, starnames.proper.json — d3-celestial © Olaf Frohn, BSD-3
    (звёзды — каталог Hipparcos, до 5-й величины; линии и названия созвездий)
  land-110m.json — world-atlas © Mike Bostock, ISC (контуры суши Natural Earth 1:110m, общественное достояние)

    python3 tools/gen_astro.py > src/gallery/astrodata.h
"""
import json, math, os
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "astro_src")
J = lambda f: json.load(open(os.path.join(SRC, f), encoding="utf-8"))

def ra_deg(x):  # d3-celestial хранит прямое восхождение как долготу -180..180
    return x % 360.0

out = ["// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/gen_astro.py — не править руками.",
       "// Звёзды/созвездия: d3-celestial (BSD-3, © Olaf Frohn; каталог Hipparcos).",
       "// Суша: world-atlas (ISC, © Mike Bostock) по Natural Earth 1:110m.",
       "#pragma once", "#include <stdint.h>", ""]

# ---------- звёзды (по яркости: сначала яркие) ----------
stars = J("stars.mag5.json")["features"]
stars.sort(key=lambda f: f["properties"]["mag"])
idx_of = {}
out.append("struct AStar { uint16_t ra; int16_t dec; uint8_t mag; };   // ra, dec — градусы*100; mag — (величина+2)*10")
out.append("static const AStar STARS[] = {")
row = []
for i, f in enumerate(stars):
    ra, dec = f["geometry"]["coordinates"]
    m = f["properties"]["mag"]
    idx_of[str(f["id"])] = i
    row.append("{%d,%d,%d}" % (round(ra_deg(ra) * 100) % 36000, round(dec * 100), max(0, min(255, round((m + 2) * 10)))))
    if len(row) == 8:
        out.append("  " + ",".join(row) + ","); row = []
if row: out.append("  " + ",".join(row) + ",")
out.append("};")
out.append("static const int N_STARS = %d;" % len(stars))

# ---------- имена ярких звёзд ----------
names = J("starnames.proper.json")
named = []
for sid, v in names.items():
    i = idx_of.get(sid)
    if i is not None and stars[i]["properties"]["mag"] <= 1.6 and v["name"]:
        named.append((i, v["name"]))
named.sort()
out.append("struct AStarName { uint16_t star; const char* name; };")
out.append("static const AStarName STAR_NAMES[] = {")
for i, n in named:
    out.append('  {%d, "%s"},' % (i, n.replace('"', '')))
out.append("};")
out.append("static const int N_STAR_NAMES = %d;" % len(named))

# ---------- линии созвездий: точки + начала ломаных ----------
lines = J("constellations.lines.json")["features"]
pts, starts = [], []
for f in lines:
    for seg in f["geometry"]["coordinates"]:
        starts.append(len(pts))
        for ra, dec in seg:
            pts.append((round(ra_deg(ra) * 100) % 36000, round(dec * 100)))
starts.append(len(pts))
out.append("// ломаная k — точки CL_PTS[CL_START[k] .. CL_START[k+1])")
out.append("static const uint16_t CL_PTS[][2] = {")
for k in range(0, len(pts), 10):
    out.append("  " + ",".join("{%d,%d}" % (a, b & 0xFFFF) for a, b in pts[k:k + 10]) + ",")
out.append("};")
out.append("static const uint16_t CL_START[] = {" + ",".join(map(str, starts)) + "};")
out.append("static const int N_CL = %d;" % (len(starts) - 1))

# ---------- названия созвездий (крупные: rank 1-2), по-русски ----------
cons = [f for f in J("constellations.json")["features"] if f["properties"].get("rank") in ("1", "2")]
out.append("struct AConName { uint16_t ra; int16_t dec; const char* ab; const char* ru; };   // ab — латинское сокращение (Ori)")
out.append("static const AConName CON_NAMES[] = {")
for f in cons:
    ra, dec = f["geometry"]["coordinates"]
    out.append('  {%d, %d, "%s", "%s"},' % (round(ra_deg(ra) * 100) % 36000, round(dec * 100), f["properties"]["desig"], f["properties"]["ru"].replace('"', '')))
out.append("};")
out.append("static const int N_CON_NAMES = %d;" % len(cons))

# ---------- суша: topojson -> ломаные, упрощение (Рамер-Дуглас-Пекер) ----------
topo = J("land-110m.json")
sx, sy = topo["transform"]["scale"]; tx, ty = topo["transform"]["translate"]
arcs = []
for arc in topo["arcs"]:
    x = y = 0; a = []
    for dx, dy in arc:
        x += dx; y += dy
        a.append((x * sx + tx, y * sy + ty))
    arcs.append(a)

def rdp(p, eps):
    if len(p) < 3: return p
    (x1, y1), (x2, y2) = p[0], p[-1]
    dx, dy = x2 - x1, y2 - y1
    n = math.hypot(dx, dy) or 1e-9
    best, bi = 0, 0
    for i in range(1, len(p) - 1):
        d = abs(dy * p[i][0] - dx * p[i][1] + x2 * y1 - y2 * x1) / n
        if d > best: best, bi = d, i
    if best > eps:
        return rdp(p[:bi + 1], eps)[:-1] + rdp(p[bi:], eps)
    return [p[0], p[-1]]

land, lstarts = [], []
for a in arcs:
    s = rdp(a, 0.45)
    if len(s) < 2: continue
    lstarts.append(len(land))
    land += [(round(lo * 100), round(la * 100)) for lo, la in s]
lstarts.append(len(land))
out.append("// контуры суши: ломаная k — LAND_PTS[LAND_START[k] .. LAND_START[k+1]); долгота, широта — градусы*100")
out.append("static const int16_t LAND_PTS[][2] = {")
for k in range(0, len(land), 10):
    out.append("  " + ",".join("{%d,%d}" % p for p in land[k:k + 10]) + ",")
out.append("};")
out.append("static const uint16_t LAND_START[] = {" + ",".join(map(str, lstarts)) + "};")
out.append("static const int N_LAND = %d;" % (len(lstarts) - 1))
print("\n".join(out))
import sys
print("stars %d, named %d, con lines %d pts, con names %d, land %d pts / %d lines" %
      (len(stars), len(named), len(pts), len(cons), len(land), len(lstarts) - 1), file=sys.stderr)
