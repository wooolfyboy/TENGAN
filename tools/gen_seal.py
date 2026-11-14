#!/usr/bin/env python3
"""Печать 天眼 (TENGAN) для заставки: вертикальная «ханко» — залитый прямоугольник,
иероглифы «вырезаны» (фон). Результат — 1-битная картинка src/core/seal.h.
Шрифт: IPAGothic (IPA Font License v1.0) — используется только для растра двух знаков.

    python3 tools/gen_seal.py /usr/share/fonts/opentype/ipafont-gothic/ipag.ttf > src/core/seal.h
"""
import sys
from PIL import Image, ImageDraw, ImageFont

font_path = sys.argv[1]
W, H, PAD, SIZE = 32, 60, 3, 25
img = Image.new("L", (W, H), 255)                       # 255 — заливка печати
d = ImageDraw.Draw(img)
font = ImageFont.truetype(font_path, SIZE)
for i, ch in enumerate("天眼"):
    cy = PAD + 2 + i * (H - 2 * PAD) // 2 + (H - 2 * PAD) // 4
    d.text((W // 2, cy), ch, font=font, fill=0, anchor="mm")
# рамка-«выщербина» внутри края, как у настоящей печати
d.rectangle([1, 1, W - 2, H - 2], outline=0)
bits = bytearray()
for y in range(H):
    acc = n = 0
    for x in range(W):
        acc = (acc << 1) | (1 if img.getpixel((x, y)) > 127 else 0)
        n += 1
        if n == 8:
            bits.append(acc); acc = n = 0
    if n:
        bits.append(acc << (8 - n))
print("// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/gen_seal.py: печать 天眼 (TENGAN) для заставки.")
print("// Иероглифы отрисованы шрифтом IPAGothic (IPA Font License v1.0).")
print("#pragma once\n#include <stdint.h>\n")
print("static const int SEAL_W = %d, SEAL_H = %d;   // 1 — заливка печати, 0 — вырезанный знак" % (W, H))
print("static const uint8_t SEAL_BITS[] = {")
for i in range(0, len(bits), 16):
    print("  " + ", ".join("0x%02X" % b for b in bits[i:i + 16]) + ",")
print("};")
