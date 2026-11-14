#!/usr/bin/env bash
# Симулятор экрана: собирает настоящий код рисования на компьютере и сохраняет
# все экраны в картинки (tools/sim/out/*.png, увеличение x2) + общий лист contact.png.
# Нужны: g++, python3 + Pillow, установленные библиотеки PlatformIO (pio run хотя бы раз).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SIM="$ROOT/tools/sim"
WORK="${SIM_WORK:-$SIM/.build}"
OUT="$SIM/out"
LIB="$ROOT/.pio/libdeps/t-embed-cc1101/TFT_eSPI"
mkdir -p "$WORK" "$OUT"

# копия TFT_eSPI с правкой для 64-битного компьютера (адрес шрифта хранится в uint32_t)
if [ ! -d "$WORK/tftlib" ]; then
  cp -r "$LIB" "$WORK/tftlib"
  sed -i 's/uint32_t flash_address/uintptr_t flash_address/g' "$WORK/tftlib/Extensions/Sprite.cpp" "$WORK/tftlib/TFT_eSPI.cpp"
fi

DEFS="-DUSER_SETUP_LOADED=1 -DST7789_DRIVER=1 -DTFT_WIDTH=170 -DTFT_HEIGHT=320 -DTFT_MISO=10 -DTFT_MOSI=9
 -DTFT_SCLK=11 -DTFT_CS=41 -DTFT_DC=16 -DTFT_RST=-1 -DTFT_BL=21 -DLOAD_GLCD=1 -DLOAD_FONT2=1 -DLOAD_FONT4=1
 -DLOAD_FONT6=1 -DLOAD_FONT7=1 -DLOAD_FONT8=1 -DLOAD_GFXFF=1 -DSPI_FREQUENCY=40000000 -DDISABLE_ALL_LIBRARY_WARNINGS"
SRC=(
  "$SIM/sim.cpp" "$WORK/tftlib/TFT_eSPI.cpp"
  "$ROOT/src/core/display.cpp" "$ROOT/src/core/theme.cpp" "$ROOT/src/core/menu.cpp" "$ROOT/src/core/settings.cpp"
  "$ROOT/src/core/battery.cpp" "$ROOT/src/core/ui.cpp" "$ROOT/src/core/leds.cpp" "$ROOT/src/core/boot.cpp" "$ROOT/src/core/power_screens.cpp"
  "$ROOT/src/ble/ble_screens.cpp" "$ROOT/src/ble/ble_city.cpp" "$ROOT/src/ble/vendors.cpp"
  "$ROOT/src/wifi/wifi_model.cpp" "$ROOT/src/wifi/wifi_screens.cpp"
  "$ROOT/src/subghz/subghz_screens.cpp" "$ROOT/src/audio/audio_screens.cpp" "$ROOT/src/gallery/gallery.cpp" "$ROOT/src/web/web_screen.cpp" "$ROOT/src/pc/pc.cpp" "$ROOT/src/pc/pc_screens.cpp"
)
g++ -O1 -std=gnu++17 -w -I"$SIM/stubs" -I"$WORK/tftlib" -I"$ROOT/src" $DEFS "${SRC[@]}" -o "$WORK/sim"
rm -f "$OUT"/*.ppm "$OUT"/*.png
"$WORK/sim" "$OUT"
python3 - "$OUT" <<'PY'
import sys, glob, os
from PIL import Image
out = sys.argv[1]
files = sorted(glob.glob(os.path.join(out, "*.ppm")))
imgs = []
for f in files:
    im = Image.open(f).convert("RGB").resize((640, 340), Image.NEAREST)
    im.save(f[:-4] + ".png"); os.remove(f)
    if not os.path.basename(f).startswith("anim_"):   # кадры гифок — не в общий лист
        imgs.append(im)
cols = 3
rows = (len(imgs) + cols - 1) // cols
sheet = Image.new("RGB", (cols * 660, rows * 360), (40, 40, 40))
for i, im in enumerate(imgs):
    sheet.paste(im, ((i % cols) * 660 + 10, (i // cols) * 360 + 10))
sheet.save(os.path.join(out, "contact.png"))
print(f"{len(imgs)} screens -> {out}/contact.png")
PY
