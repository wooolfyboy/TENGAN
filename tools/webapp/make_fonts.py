#!/usr/bin/env python3
"""Subsets the tablet UI fonts to the needed glyphs and embeds them in src/fonts.css (WOFF2 base64, offline).

  JetBrains Mono  - all text (both themes)
  Exo 2 / Oswald  - headings and big numbers (DENKO / KURENAI)
  IPAGothic       - only the 天眼 seal glyphs (system font fonts-ipafont-gothic, not in the repo)
Latin and Cyrillic come from one family, so nothing shifts on language change.
Needs `pip install fonttools brotli`. Run only when the glyph set changes; CI does not rebuild fonts.css.
"""
import base64, io, os
from fontTools import subset
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "..", "fonts")
OUT = os.path.join(HERE, "src", "fonts.css")
IPA = "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf"

ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
EXTRA = "°·—–…‹›«»×←→↑↓▲▼◆◇●○■□♪⚡±≈≤≥µ№™✓✕▸◂"
# glyphs the page draws: 天眼 seal, HQ (NERV/DOMAIN), weather (IPAGothic)
SEAL_TEXT = "中令低信光動受司告器地報外天定室嵐平度意感態探接敵断新既晴曇月析棒機気波注活滝状発眼知穏系紅索統網線自解記設警赤距近速針録離雨雷電静音風齢"
CYR = "".join(chr(c) for c in range(0x410, 0x450)) + "ЁёІіЇїЄєҐґ"


def woff2(path, text):
    f = TTFont(path)
    opt = subset.Options()
    opt.flavor = "woff2"
    opt.layout_features = ["kern", "liga", "tnum"]
    opt.name_IDs = []
    opt.notdef_outline = True
    s = subset.Subsetter(opt)
    s.populate(text=text)
    s.subset(f)
    buf = io.BytesIO()
    f.flavor = "woff2"
    f.save(buf)
    return base64.b64encode(buf.getvalue()).decode()


def face(family, weight, data, adjust=None):
    # adjust = size-adjust: keeps the new fonts the same size as the old ones so the layout does not shift
    sa = ";size-adjust:%s" % adjust if adjust else ""
    return ("@font-face{font-family:'%s';font-weight:%s;font-style:normal;font-display:block%s;"
            "src:url(data:font/woff2;base64,%s) format('woff2')}" % (family, weight, sa, data))


def main():
    # text: JetBrains Mono; headings and digits: DENKO - Exo 2, KURENAI - Oswald
    T = ASCII + EXTRA + CYR
    F = lambda n: os.path.join(FONTS, n)
    parts = [
        face("JetBrains Mono", "100 550", woff2(F("JetBrainsMono-Medium.ttf"), T), "90%"),
        face("JetBrains Mono", "551 900", woff2(F("JetBrainsMono-SemiBold.ttf"), T), "90%"),
        face("Exo 2", "100 650", woff2(F("Exo2-SemiBold.ttf"), T), "87%"),
        face("Exo 2", "651 900", woff2(F("Exo2-Bold.ttf"), T), "87%"),
        face("Oswald", "100 900", woff2(F("Oswald-SemiBold.ttf"), T), "92%"),
    ]
    if os.path.exists(IPA):
        parts.append(face("Tengen Seal", 400, woff2(IPA, SEAL_TEXT)))
    else:
        print("нет %s — печать 天眼 останется системным шрифтом" % IPA)
    css = "/* АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/webapp/make_fonts.py — шрифты OFL; печать 天眼 — IPAGothic (IPA Font License), см. fonts/ */\n" + "\n".join(parts) + "\n"
    open(OUT, "w", encoding="utf-8").write(css)
    print("fonts.css: %d КБ" % (len(css) // 1024))


if __name__ == "__main__":
    main()
