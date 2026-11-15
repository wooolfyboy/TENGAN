#!/usr/bin/env python3
"""Шрифты палки для планшета: урезать до нужных символов и встроить в CSS.

  Rajdhani SemiBold  — заголовки и крупные числа (как на экране палки)
  Share Tech Mono    — текст HUD
  JetBrains Mono     — кириллица (названия треков, сети с русскими именами)
  IPAGothic          — только два знака печати 天眼 (IPA Font License; системный шрифт
                       fonts-ipafont-gothic, в репозиторий не кладём — нужен лишь при пересборке)

Результат: tools/webapp/src/fonts.css (@font-face с WOFF2 в base64 — без интернета).
Нужен pip install fonttools brotli. Запускать только при смене набора символов:
готовый fonts.css лежит в репозитории, CI его не пересобирает.
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


def face(family, weight, data):
    return ("@font-face{font-family:'%s';font-weight:%s;font-style:normal;font-display:block;"
            "src:url(data:font/woff2;base64,%s) format('woff2')}" % (family, weight, data))


def main():
    parts = [
        face("Rajdhani", 600, woff2(os.path.join(FONTS, "Rajdhani-SemiBold.ttf"), ASCII + EXTRA)),
        face("Share Tech Mono", 400, woff2(os.path.join(FONTS, "ShareTechMono-Regular.ttf"), ASCII + EXTRA)),
        face("JetBrains Mono", 500, woff2(os.path.join(FONTS, "JetBrainsMono-Medium.ttf"), ASCII + EXTRA + CYR)),
    ]
    if os.path.exists(IPA):
        parts.append(face("Tengan Seal", 400, woff2(IPA, "天眼")))
    else:
        print("нет %s — печать 天眼 останется системным шрифтом" % IPA)
    css = "/* АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/webapp/make_fonts.py — шрифты OFL; печать 天眼 — IPAGothic (IPA Font License), см. fonts/ */\n" + "\n".join(parts) + "\n"
    open(OUT, "w", encoding="utf-8").write(css)
    print("fonts.css: %d КБ" % (len(css) // 1024))


if __name__ == "__main__":
    main()
