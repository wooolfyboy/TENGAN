#!/usr/bin/env python3
"""Картинки для README (docs/): гифки и галерея — из кадров симулятора.

    SIM_ANIM=1 bash tools/sim/build.sh && python3 tools/make_docs.py
"""
import glob, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "tools/sim/out")
DOCS = os.path.join(ROOT, "docs")
BG = (2, 9, 11)
os.makedirs(DOCS, exist_ok=True)


def gif(prefix, name, ms, scale=1.0, hold_last=0):
    files = sorted(glob.glob(os.path.join(OUT, prefix + "_*.png")))
    frames = []
    for f in files:
        im = Image.open(f).convert("RGB")
        if scale != 1.0:
            im = im.resize((int(im.width * scale), int(im.height * scale)), Image.LANCZOS)
        frames.append(im)
    if hold_last:
        frames += [frames[-1]] * hold_last
    pal = [fr.quantize(colors=96, method=Image.MEDIANCUT, dither=Image.NONE) for fr in frames]
    path = os.path.join(DOCS, name)
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=ms, loop=0, optimize=True, disposal=1)
    print("%-22s %3d frames  %4d KB" % (name, len(frames), os.path.getsize(path) // 1024))


def font(size):
    for p in ("/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-SemiBold.ttf",
              os.path.join(ROOT, "fonts/JetBrainsMono-SemiBold.ttf")):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def gallery(items, name, cols=3, scale=0.75):
    tiles = [(Image.open(os.path.join(OUT, f + ".png")).convert("RGB"), label) for f, label in items]
    tw, th = int(640 * scale), int(340 * scale)
    pad, lab = 18, 30
    rows = (len(tiles) + cols - 1) // cols
    W = cols * tw + (cols + 1) * pad
    H = rows * (th + lab) + (rows + 1) * pad
    sheet = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(sheet)
    f = font(17)
    for i, (im, label) in enumerate(tiles):
        x = pad + (i % cols) * (tw + pad)
        y = pad + (i // cols) * (th + lab + pad)
        sheet.paste(im.resize((tw, th), Image.LANCZOS), (x, y))
        d.rectangle([x - 1, y - 1, x + tw, y + th], outline=(18, 48, 58))
        d.text((x + 2, y + th + 6), label, font=f, fill=(127, 151, 163))
    sheet.save(os.path.join(DOCS, name), optimize=True)
    print("%-22s %dx%d  %4d KB" % (name, W, H, os.path.getsize(os.path.join(DOCS, name)) // 1024))


gif("anim_boot", "boot.gif", 40, scale=1.0, hold_last=12)
gif("anim_city", "city.gif", 50, scale=0.75)
gif("anim_tess", "tesseract.gif", 40, scale=0.75)
gif("anim_off", "shutdown.gif", 40, scale=0.75, hold_last=6)
gallery([("01_menu", "главное меню"), ("13_ble_city", "Bluetooth · City (3D)"), ("12_ble_scope", "Bluetooth · Scope"),
         ("16_ble_detail", "Bluetooth · подробности"), ("22_wifi_channels", "Wi-Fi · каналы"), ("25_wifi_find", "Wi-Fi · Find"),
         ("30_subghz_spectrum", "Sub-GHz · спектр + водопад"), ("40b_sound_terrain", "Sound · Terrain (3D)"), ("45b_sound_xy_pc", "Sound · XY по звуку ноута"),
         ("71_pc_overview", "PC · монитор ноутбука"), ("73_pc_media", "PC · что играет"), ("60_web", "Web · подключение телефона"),
         ("50_gallery_tesseract", "Gallery · тессеракт"), ("53_gallery_pendulum", "Gallery · двойной маятник"), ("08_power_hold", "выключение")],
        "screens.png")
