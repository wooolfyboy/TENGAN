# Third-party components, data and licenses

Project code is licensed under **MIT** (`LICENSE`, "Copyright (c) 2026 wooolfyboy"). This section lists what else is used: libraries, fonts, data sets, bundled scripts; where it lives in the repository; and what the licensing and responsible-use terms are. License texts of bundled third-party material are kept next to it (`fonts/`, `tools/astro_src/`, `tools/flasher/`).

## Firmware libraries

Declared in `platformio.ini` (`lib_deps`, fetched by PlatformIO, not stored in the repository):

| Library | Version | Used for |
|---|---|---|
| TFT_eSPI (`bodmer/TFT_eSPI`) | `^2.5.43` | Display driver (ST7789 170x320), sprites, text. Configured with build flags instead of `User_Setup.h`. Fonts are loaded as Adafruit GFX arrays. |
| NimBLE-Arduino (`h2zero/NimBLE-Arduino`) | `^2.1.0` | Bluetooth LE scanning (passive; the device does not transmit). |
| Adafruit NeoPixel (`adafruit/Adafruit NeoPixel`) | `^1.12.3` | The LEDs on the board. |

Platform: PlatformIO `espressif32` with the Arduino framework, which brings Arduino-ESP32 and ESP-IDF (Wi-Fi, lwIP, mbedTLS, OTA, the QR code API `qrcode.h` used in `src/web/web.cpp`, and others). The board profile `boards/T_Embed_PN532.json` comes from the LilyGO repository.

Code written for the project (no third-party code copied): the CC1101 driver, OOK decoders, IR decoder, PN532 NFC reading, orbit calculation (`src/gallery/sat.cpp`: TLE parsing, Kepler + J2 approximation), and so on. The OOK decoders follow the open protocol documentation of [rtl_433](https://github.com/merbanan/rtl_433) (bit slicing rules, formats, checksums; see `src/subghz/ook.h`), but the code is original. `tools/ook_test/` checks them against the `rtl_433_tests` capture set (`.cu8` files with expected `.json`) that you download separately.

## Fonts

Fonts are in `fonts/` with their license files. Both Latin and Cyrillic are drawn by the same family, so switching the language never shifts the layout.

| Font | File(s) | License per `fonts/` and README | Used for |
|---|---|---|---|
| Exo 2 (SemiBold, Bold) | `Exo2-*.ttf`, `OFL-Exo2.txt` | SIL OFL 1.1, (c) 2013 The Exo 2 Project Authors | Headings and big numbers (DENKO theme) |
| Oswald (SemiBold) | `Oswald-SemiBold.ttf`, `OFL-Oswald.txt` | SIL OFL 1.1, (c) 2016 The Oswald Project Authors | Condensed headings and numbers (KURENAI theme) |
| JetBrains Mono (Medium, SemiBold) | `JetBrainsMono-*.ttf`, `OFL-JetBrainsMono.txt` | SIL OFL 1.1, (c) 2020 The JetBrains Mono Project Authors (font version 2.304) | All other text |
| IPAGothic | not in the repository; `LICENSE-IPAFont.txt` | IPA Font License v1.0 (c) 2007-2011 Information-technology Promotion Agency, Japan | Only the two glyphs of the 天眼 seal |


How fonts reach the product:

- **Device screen.** `tools/gen_gfxfont.py` rasterizes a TTF with Pillow into an Adafruit GFX bitmap font (`src/core/fonts/`: `JBM10/11/12/14/19`, `EXO20/37`, `OSW20/36`). Options: `--latin1` (adds `· « » ° ×`), `--cyr` (plus Cyrillic up to U+045F), `--thr=N` (boldness threshold, default 110). Glyph range is contiguous, so codes between Latin and Cyrillic are empty. TFT_eSPI decodes UTF-8 by itself.
- **Seal.** `tools/gen_seal.py <path to ipag.ttf>` draws the glyphs of 天眼 as a 1-bit "hanko" (32x60) into `src/core/seal.h`. The font is used only to rasterize; it is not shipped.
- **Tablet page.** `tools/webapp/make_fonts.py` subsets the fonts to the glyphs actually used and embeds them as base64 WOFF2 in `tools/webapp/src/fonts.css`, so the page works fully offline. IPAGothic here is the system font `fonts-ipafont-gothic` (`/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf`).

## Data sets

All generated data is committed as C headers (or inlined into the page); the sources are not downloaded during a normal build. Generators are run by hand.

| Data | Source | Generator | Output |
|---|---|---|---|
| Bluetooth vendors by Company ID | Bluetooth SIG assigned numbers via Nordic's [bluetooth-numbers-database](https://github.com/NordicSemiconductor/bluetooth-numbers-database) (`company_ids.json`) | `tools/gen_btdb.py` | `src/ble/btdb.h` (about 127 KB) |
| Bluetooth vendors by 16-bit member service UUID | Bluetooth SIG `member_uuids.yaml` (assigned_numbers) | `tools/gen_btdb.py` | `src/ble/btdb.h` |
| Wi-Fi/network vendors by OUI (first 3 MAC bytes) | IEEE data compiled by the Nmap project: `nmap-mac-prefixes` | `tools/gen_ouidb.py` | `src/wifi/ouidb.h` (about 936 KB) |
| Stars, constellation lines and names | [d3-celestial](https://github.com/ofrohn/d3-celestial) (c) Olaf Frohn, BSD-3 (Hipparcos stars up to magnitude 5) | `tools/gen_astro.py` | `src/gallery/astrodata.h` |
| Land outlines for the globe | [world-atlas](https://github.com/topojson/world-atlas) (c) Mike Bostock, ISC, based on Natural Earth 1:110m (public domain) | `tools/gen_astro.py` | `src/gallery/astrodata.h` |
| Satellite orbits (TLE) | [CelesTrak](https://celestrak.org) | not generated: downloaded at run time | none |

Details:

- `gen_btdb.py` shortens vendor names (drops words like Inc, Ltd, GmbH, Technology and city names such as Shenzhen) to fit the device's memory and screen. `gen_ouidb.py` reuses `short()` from it, keeps only full 24-bit OUIs, and stores sorted keys plus a deduplicated name table.
- Inputs of `gen_astro.py` are in `tools/astro_src/` (`stars.mag5.json`, `constellations.json`, `constellations.lines.json`, `starnames.proper.json`, `land-110m.json`) together with `LICENSE-d3-celestial.txt` and `LICENSE-world-atlas.txt`. It writes the header to stdout: `python3 tools/gen_astro.py > src/gallery/astrodata.h`.
- Orbits: `tools/pc_monitor/pc_monitor.py` downloads TLEs for ISS, Tiangong and Hubble from CelesTrak every 6 hours, hands them to the device once a minute and keeps a cache file `tle_cache.txt` next to the program (it works without internet from the cache; the file is in `.gitignore`).

## Bundled and build-time software

| Component | Where | Role | License |
|---|---|---|---|
| [esptool-js](https://github.com/espressif/esptool-js) 0.5.4 (c) Espressif | `tools/flasher/esptool-js-0.5.4.bundle.js`, `esptool-js.LICENSE` | Flashing over Web Serial inside `flash_tengen.html` | Apache 2.0 |
| esptool 4.8.1 | installed by CI (`pip`) | Merging `tengen_full_0x0.bin` | not stored in the repository |
| pywebview | `tools/desktop/requirements*.txt` | Window of TENGEN Desktop (macOS: system WebKit via pyobjc; Linux: Qt WebEngine via PySide6/QtPy) | see the project |
| PyInstaller | installed in CI | Packaging Desktop | see the project |
| Other Desktop libraries | `requirements*.txt` | pyserial, psutil, pystray, Pillow; Windows: pycaw, comtypes, numpy, PyAudioWPatch, winrt-*; Linux: PySide6, QtPy, python-xlib, numpy, sounddevice | see each project |
| Capacitor 7.6.10 (`@capacitor/android`, `@capacitor/core`), `@capacitor/geolocation` 7.1.8, `@capacitor/cli` | `app/package.json` | Android app shell and GPS | see the project |
| AndroidX (`appcompat`, `coordinatorlayout`, `core-splashscreen`, ...), Cordova Android 10.1.1 | `app/android/variables.gradle` | Android dependencies | see the project |
| Pillow | `tools/gen_*.py`, `tools/sim`, `tools/make_docs.py` | Image and font rasterization | see the project |
| fonttools, brotli | `tools/webapp/make_fonts.py` | Font subsetting for the page | see the project |

The README lists only licenses that are stated in the repository files (MIT for the project, OFL for Exo 2, Oswald and JetBrains Mono, Apache 2.0 for esptool-js, IPA Font License, BSD-3, ISC). For other packages the license is the one published by the corresponding project; check it before redistributing a bundle.

## Receive-only principle and responsible use

TENGEN is built to look at the radio environment, not to interfere with it:

- It transmits nothing on the air: the CC1101 and IR are never switched to transmit, NFC only reads. The only emission is the device's own Wi-Fi access point for the tablet while the Web section is open.
- It shows only what devices already broadcast to everyone around. Wi-Fi packet contents are not read, remote-control codes are not stored, bank card numbers and holder names are not read.
- It works offline: no cloud, data stays on the device and on your tablet or computer. The one network request is on the PC side: the PC monitor downloads orbit elements from CelesTrak for the Globe.
- Laws on radio reception and privacy differ between countries. Use TENGEN where and how it is legal and respect other people's privacy.
- Limits worth knowing: with a single antenna the device can only tell "closer / farther"; meters are a rough estimate from signal strength; the angle on the radar is for visual clarity; 5 GHz networks are not visible (ESP32-S3 is 2.4 GHz only); modern phones rotate addresses and broadcast little, so many devices stay "unknown".
