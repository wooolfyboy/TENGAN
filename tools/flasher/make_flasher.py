"""
Собрать «прошивальщик в одном файле»: HTML-страница, внутри которой уже лежат
прошивка (tengan_full_0x0.bin) и библиотека esptool-js. Пишется всё, кроме
области настроек (NVS, 0x9000..0xE000) — настройки устройства сохраняются.
Открыл в Chrome/Edge → кнопка «Установить» → готово. Ни адресов, ни файлов.

    python tools/flasher/make_flasher.py <full_0x0.bin> <out.html> <версия>

Страница на двух языках (EN / RU): по языку браузера, переключатель вверху.
"""
import base64, datetime, html as htmlmod, json, os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
fw_path, out_path, version = sys.argv[1], sys.argv[2], sys.argv[3]

# тексты страницы. HTML-ключи (h_*) можно размечать <b>, остальные — простой текст для JS
TEXT = {
    "en": {
        "title": "TENGAN %(v)s — firmware installer",
        "sub": "firmware installer · version %(v)s · built %(b)s",
        "btn": "INSTALL FIRMWARE",
        "idle": "Connect TENGAN with a USB cable and press the button.",
        "h_s1": "Close <b>TENGAN Desktop</b> and any other app that uses the device's USB port.",
        "h_s2": "Press <b>Install firmware</b>, pick <b>USB JTAG/serial debug unit</b> in the list, then <b>Connect</b>.",
        "h_s3": "Wait about a minute until the progress bar is full.",
        "h_s4": "When you see <b>Done</b>, unplug the cable and plug it back in.",
        "h_note": "No addresses or files to choose: the firmware is built into this page. Your settings are kept.",
        "details": "technical log",
        "noserial": "This browser can't access USB devices. Open this file in Chrome or Edge.",
        "nopick": "No device selected. Press the button again and choose TENGAN in the list.",
        "connect": "Connecting to TENGAN…",
        "wrongchip": "this is not a TENGAN board",
        "write": "Installing firmware…",
        "done": "Done. Unplug the cable and plug it back in — TENGAN will start with the new firmware.",
        "busy": "The USB port is busy. Close TENGAN Desktop and other serial tools, then try again.",
        "fail1": "Installation failed: ",
        "fail2": ". Reconnect the cable and try again. If it keeps failing, open the technical log below.",
    },
    "ru": {
        "title": "TENGAN %(v)s — установка прошивки",
        "sub": "установка прошивки · версия %(v)s · сборка %(b)s",
        "btn": "УСТАНОВИТЬ ПРОШИВКУ",
        "idle": "Подключите TENGAN USB-кабелем и нажмите кнопку.",
        "h_s1": "Закройте <b>TENGAN Desktop</b> и другие программы, которые используют USB-порт устройства.",
        "h_s2": "Нажмите <b>«Установить прошивку»</b>, выберите в списке <b>USB JTAG/serial debug unit</b> и нажмите <b>«Подключиться»</b>.",
        "h_s3": "Подождите около минуты, пока шкала не заполнится.",
        "h_s4": "Когда появится <b>«Готово»</b>, отключите кабель и подключите снова.",
        "h_note": "Адрес и файл выбирать не нужно: прошивка уже встроена в эту страницу. Настройки сохраняются.",
        "details": "технический журнал",
        "noserial": "Этот браузер не работает с USB-устройствами. Откройте файл в Chrome или Edge.",
        "nopick": "Устройство не выбрано. Нажмите кнопку ещё раз и выберите TENGAN в списке.",
        "connect": "Подключение к TENGAN…",
        "wrongchip": "это не плата TENGAN",
        "write": "Установка прошивки…",
        "done": "Готово. Отключите кабель и подключите снова — TENGAN запустится с новой прошивкой.",
        "busy": "USB-порт занят. Закройте TENGAN Desktop и другие программы для порта, затем повторите.",
        "fail1": "Не удалось установить: ",
        "fail2": ". Переподключите кабель и попробуйте снова. Если ошибка повторяется, откройте технический журнал ниже.",
    },
}

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

built = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")
page = open(os.path.join(here, "flasher_template.html"), encoding="utf-8").read()
js = {}
for lg, t in TEXT.items():                          # оба языка — в странице, выбор при открытии
    d = {k: (v % {"v": version, "b": built} if isinstance(v, str) and k in ("title", "sub") else v) for k, v in t.items()}
    js[lg] = d
page = page.replace("__VER__", htmlmod.escape(version)).replace("__JS_TEXT__", json.dumps(js, ensure_ascii=False))
if "{{" in page:
    sys.exit("flasher template: unfilled placeholder")
html = (page.replace("__ESPTOOL__", bundle)
            .replace("__FIRMWARE_B64__", base64.b64encode(fw).decode()))
open(out_path, "w", encoding="utf-8").write(html)
print("%s [en+ru]: %.1f MB (firmware %.2f MB)" % (out_path, len(html) / 1048576, len(fw) / 1048576))
