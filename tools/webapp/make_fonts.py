#!/usr/bin/env python3
"""Шрифты интерфейса для планшета: урезать до нужных символов и встроить в CSS.

  JetBrains Mono     — весь текст (обе темы)
  Exo 2              — заголовки и крупные числа, тема DENKO
  Oswald             — заголовки и крупные числа, тема KURENAI
  У всех трёх латиница и кириллица из одной семьи — при смене языка ничего не прыгает.
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
# знаки, которые рисует страница: печать 天眼, штаб HQ (NERV/DOMAIN), погода (IPAGothic)
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
    # adjust — size-adjust: шрифт рисуется в этой доле от заданного размера (и в CSS, и на холстах),
    # чтобы новые шрифты занимали столько же места, сколько прежние, и раскладка не поехала
    sa = ";size-adjust:%s" % adjust if adjust else ""
    return ("@font-face{font-family:'%s';font-weight:%s;font-style:normal;font-display:block%s;"
            "src:url(data:font/woff2;base64,%s) format('woff2')}" % (family, weight, sa, data))


def main():
    # Одна семья на латиницу и кириллицу — при смене языка ничего не прыгает:
    #   текст (обе темы) — JetBrains Mono; заголовки и цифры: DENKO — Exo 2, KURENAI — Oswald
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
        parts.append(face("Tengan Seal", 400, woff2(IPA, SEAL_TEXT)))
    else:
        print("нет %s — печать 天眼 останется системным шрифтом" % IPA)
    css = "/* АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/webapp/make_fonts.py — шрифты OFL; печать 天眼 — IPAGothic (IPA Font License), см. fonts/ */\n" + "\n".join(parts) + "\n"
    open(OUT, "w", encoding="utf-8").write(css)
    print("fonts.css: %d КБ" % (len(css) // 1024))


if __name__ == "__main__":
    main()
