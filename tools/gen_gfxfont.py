#!/usr/bin/env python3
"""Превращает TTF-шрифт в растровый шрифт формата Adafruit GFX (его понимает TFT_eSPI).

Запуск:  python3 tools/gen_gfxfont.py fonts/ShareTechMono-Regular.ttf 15 STM15 > src/core/fonts/STM15.h
Символы — печатный ASCII (0x20..0x7E). Нужен Pillow.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

def main():
    path, size, name = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    bitmap = bytearray()
    glyphs = []
    for code in range(0x20, 0x7F):
        ch = chr(code)
        adv = int(round(font.getlength(ch)))
        l, t, r, b = font.getbbox(ch, anchor="ls")
        w, h = max(0, r - l), max(0, b - t)
        offset = len(bitmap)
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-l, -t), ch, font=font, fill=255, anchor="ls")
            bits, nbits, acc = [], 0, 0
            for y in range(h):
                for x in range(w):
                    acc = (acc << 1) | (1 if img.getpixel((x, y)) >= 110 else 0)
                    nbits += 1
                    if nbits == 8:
                        bits.append(acc); acc = 0; nbits = 0
            if nbits:
                bits.append(acc << (8 - nbits))
            bitmap.extend(bits)
        glyphs.append((offset, w, h, adv, l, t, ch))
    out = [f"// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/gen_gfxfont.py из {path.split('/')[-1]}, {size} px.",
           f"// Шрифт: {path.split('/')[-1]} — SIL Open Font License 1.1 (см. fonts/).",
           "#pragma once", "#include <TFT_eSPI.h>", "",
           f"const uint8_t {name}Bitmaps[] PROGMEM = {{"]
    for i in range(0, len(bitmap), 16):
        out.append("  " + ", ".join(f"0x{v:02X}" for v in bitmap[i:i + 16]) + ",")
    out.append("};")
    out.append(f"const GFXglyph {name}Glyphs[] PROGMEM = {{")
    for off, w, h, adv, l, t, ch in glyphs:
        c = ch.replace("\\", "backslash")
        out.append(f"  {{ {off:5d}, {w:3d}, {h:3d}, {adv:3d}, {l:4d}, {t:4d} }},   // '{c}'")
    out.append("};")
    out.append(f"const GFXfont {name} PROGMEM = {{ (uint8_t*){name}Bitmaps, (GFXglyph*){name}Glyphs, 0x20, 0x7E, {ascent + descent} }};")
    print("\n".join(out))

if __name__ == "__main__":
    main()
