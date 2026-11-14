"""
Собрать «прошивальщик в одном файле»: HTML-страница, внутри которой уже лежат
прошивка (radio_radar_full_0x0.bin) и библиотека esptool-js. Пишется всё, кроме
области настроек (NVS, 0x9000..0xE000) — настройки палки сохраняются.
Открыл в Chrome/Edge → кнопка «Прошить палку» → готово. Ни адресов, ни файлов.

    python tools/flasher/make_flasher.py <full_0x0.bin> <out.html> <версия>
"""
import base64, datetime, os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
fw_path, out_path, version = sys.argv[1], sys.argv[2], sys.argv[3]

bundle = open(os.path.join(here, "esptool-js-0.5.4.bundle.js"), encoding="utf-8").read()
# библиотека — ES-модуль с «export{...}» в конце: заменяем его на window.__esptool,
# чтобы код страницы в том же <script type="module"> мог её использовать
m = re.search(r"export\{([^}]*)\};?\s*$", bundle)
if not m:
    sys.exit("esptool-js bundle: export list not found")
pairs = [p.strip().split(" as ") for p in m.group(1).split(",")]
glue = "window.__esptool={" + ",".join("%s:%s" % (b, a) for a, b in pairs) + "};"
bundle = bundle[:m.start()] + glue
lic = open(os.path.join(here, "esptool-js.LICENSE"), encoding="utf-8").read()
bundle = "/* esptool-js 0.5.4 — https://github.com/espressif/esptool-js\n" + lic.replace("*/", "* /") + "\n*/\n" + bundle

fw = open(fw_path, "rb").read()
if fw[0] != 0xE9 or fw[0x8000:0x8002] != b"\xaa\x50":
    sys.exit("not a full 0x0 image (bootloader + partition table expected)")

html = open(os.path.join(here, "flasher_template.html"), encoding="utf-8").read()
html = (html.replace("__VERSION__", version)
            .replace("__BUILT__", datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d"))
            .replace("__ESPTOOL__", bundle)
            .replace("__FIRMWARE_B64__", base64.b64encode(fw).decode()))
open(out_path, "w", encoding="utf-8").write(html)
print("%s: %.1f MB (firmware %.2f MB)" % (out_path, len(html) / 1048576, len(fw) / 1048576))
