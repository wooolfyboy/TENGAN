#!/usr/bin/env python3
"""Converts a TTF font to an Adafruit GFX bitmap font (TFT_eSPI understands it).

Usage:  python3 tools/gen_gfxfont.py fonts/JetBrainsMono-Medium.ttf 12 JBM12 --cyr > src/core/fonts/JBM12.h
Glyphs: printable ASCII (0x20..0x7E). --latin1 adds · « » ° ×; --cyr adds those plus Cyrillic (up to U+045F);
codes in between stay empty (a GFX font needs a contiguous table).
TFT_eSPI parses UTF-8 itself, so Russian text is drawn as is. Needs Pillow.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

def main():
    path, size, name = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    cyr = "--cyr" in sys.argv[4:]
    lat = "--latin1" in sys.argv[4:] or cyr     # from Latin-1 only what the UI needs: · « » ° ×
    last = 0x45F if cyr else 0xFF if lat else 0x7E
    EXTRA = {0xB7, 0xAB, 0xBB, 0xB0, 0xD7}
    thr = next((int(a.split('=')[1]) for a in sys.argv[4:] if a.startswith('--thr=')), 110)   # "pixel is set" threshold: lower = bolder
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    bitmap = bytearray()
    glyphs = []
    for code in range(0x20, last + 1):
        ch = chr(code)
        if 0x7F <= code < 0x400 and not (lat and code in EXTRA):   # gap between ASCII and Cyrillic stays empty
            glyphs.append((len(bitmap), 0, 0, 0, 0, 0, "?"))
            continue
        adv = int(round(font.getlength(ch)))
        if ch == ' ' and '--widespace' in sys.argv[4:]: adv = max(adv, int(round(size * 0.38)))   # wider space so words don't merge on the small screen
        l, t, r, b = font.getbbox(ch, anchor="ls")
        w, h = max(0, r - l), max(0, b - t)
        offset = len(bitmap)
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-l, -t), ch, font=font, fill=255, anchor="ls")
            bits, nbits, acc = [], 0, 0
            for y in range(h):
                for x in range(w):
                    acc = (acc << 1) | (1 if img.getpixel((x, y)) >= thr else 0)
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
        c = ch.replace("\\", "backslash") if ch != "?" or not w else "-"
        out.append(f"  {{ {off:5d}, {w:3d}, {h:3d}, {adv:3d}, {l:4d}, {t:4d} }},   // '{c}'")
    out.append("};")
    out.append(f"const GFXfont {name} PROGMEM = {{ (uint8_t*){name}Bitmaps, (GFXglyph*){name}Glyphs, 0x20, 0x{last:X}, {ascent + descent} }};")
    print("\n".join(out))

if __name__ == "__main__":
    main()
