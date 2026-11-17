#!/usr/bin/env python3
"""App icon and splash: the 天眼 seal (light blue, as in the DENKO theme) on a dark background.
Needs Pillow and the IPAGothic font (fonts-ipafont-gothic). Run when the artwork changes."""
import os
from PIL import Image, ImageDraw, ImageFont

RES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "android", "app", "src", "main", "res")
FONT = "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf"
BG, ACC = (5, 8, 13), (62, 230, 255)


def seal(size, scale=0.62):
    """Square size x size image: a filled seal with two glyphs cut out."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    h = int(size * scale); w = int(h * 0.56); x0 = (size - w) // 2; y0 = (size - h) // 2; c = int(h * 0.08)
    d.polygon([(x0 + c, y0), (x0 + w, y0), (x0 + w, y0 + h - c), (x0 + w - c, y0 + h), (x0, y0 + h), (x0, y0 + c)], fill=ACC)
    i = max(2, int(h * 0.04))
    d.rectangle([x0 + i * 2, y0 + i * 2, x0 + w - i * 2, y0 + h - i * 2], outline=BG, width=max(1, i // 2))
    f = ImageFont.truetype(FONT, int(h * 0.38))
    for ch, k in (("天", 0.29), ("眼", 0.71)):
        d.text((x0 + w / 2, y0 + h * k), ch, font=f, fill=BG, anchor="mm", stroke_width=max(1, int(h * 0.012)), stroke_fill=BG)
    return im


def main():
    sizes = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
    for dpi, s in sizes.items():
        d = os.path.join(RES, "mipmap-" + dpi)
        full = Image.new("RGBA", (s, s), BG + (255,)); full.alpha_composite(seal(s, 0.7))
        full.save(os.path.join(d, "ic_launcher.png"))
        rnd = Image.new("RGBA", (s, s), (0, 0, 0, 0)); m = Image.new("L", (s, s), 0); ImageDraw.Draw(m).ellipse([0, 0, s - 1, s - 1], fill=255)
        rnd.paste(full, (0, 0), m); rnd.save(os.path.join(d, "ic_launcher_round.png"))
        fg = int(s * 108 / 48)                         # adaptive icon: 108dp, visible area ~66dp
        seal(fg, 0.42).save(os.path.join(d, "ic_launcher_foreground.png"))
    open(os.path.join(RES, "values", "ic_launcher_background.xml"), "w").write(
        '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <color name="ic_launcher_background">#05080D</color>\n</resources>\n')
    # Android launch splash (before our progress bar)
    for root, _, files in os.walk(RES):
        for fn in files:
            if fn == "splash.png":
                p = os.path.join(root, fn); w, h = Image.open(p).size
                im = Image.new("RGBA", (w, h), BG + (255,)); s = min(w, h); sl = seal(s, 0.3)
                im.alpha_composite(sl, ((w - s) // 2, (h - s) // 2)); im.convert("RGB").save(p)
    print("иконки и заставка готовы")


if __name__ == "__main__":
    main()
