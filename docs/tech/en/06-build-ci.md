# Build and CI

This section describes how every part of TENGAN is built locally and in GitHub Actions, and what a release contains. Parts: firmware (PlatformIO), the tablet page, the Android APK, TENGAN Desktop, the single-file installer and the screen simulator.

## Versioning

There is one version for everything: the `VERSION` file in the repository root (currently one line like `2.0-test6`).

| Consumer | How it reads `VERSION` |
|---|---|
| Firmware | `tools/version.py` is a PlatformIO pre-script (`extra_scripts = pre:tools/version.py` in `platformio.ini`). Before each build it writes `src/core/version.h` with `#define FW_VERSION "..."`, but only if the text changed (the file is not touched needlessly). Do not edit `version.h` by hand. |
| Tablet page | `tools/webapp/build.py` replaces `@@VERSION@@` in the HTML. The page version is also compared by the app with the device (`/api/pagever`). |
| Android APK | `app/android/app/build.gradle`: `versionName` = `APP_VERSION_NAME` env variable, otherwise the contents of `VERSION` (`/` replaced by `-`). `versionCode` = `APP_VERSION_CODE` env variable, otherwise `1`. CI sets it to `github.run_number`. |
| Installer page | The version string is passed as the third argument to `tools/flasher/make_flasher.py`. In CI it is the release tag if there is one, otherwise `github.ref_name` (branch name). |

## Local builds

### Firmware

Needs [PlatformIO](https://platformio.org/).

```bash
pio run            # -> .pio/build/t-embed-cc1101/firmware.bin
```

Key points of `platformio.ini` (single environment `t-embed-cc1101`):

- `platform = espressif32`, `framework = arduino`, board `T_Embed_PN532` from the project's own profile `boards/T_Embed_PN532.json` (taken from the LilyGO repository: ESP32-S3, `qio_opi` memory, PSRAM, flash QIO 80 MHz, 240 MHz CPU, `default_16MB.csv` partitions). The generic `feather-s3` board is not used: it drives GPIO21 (screen backlight) before `setup()`, which gives a red flash.
- `lib_deps`: `bodmer/TFT_eSPI ^2.5.43`, `h2zero/NimBLE-Arduino ^2.1.0`, `adafruit/Adafruit NeoPixel ^1.12.3`.
- Optimization is for speed: `-Os` is removed and `-O2` added ("flash has headroom").
- `-DARDUINO_USB_CDC_ON_BOOT=1`; the board profile adds `-DBOARD_HAS_PSRAM` and `-DARDUINO_USB_MODE=1`.
- TFT_eSPI is configured entirely by build flags (`-DUSER_SETUP_LOADED=1`, ST7789, 170x320, pins MOSI 9, MISO 10, SCLK 11, CS 41, DC 16, BL 21, RST not used, SPI 40 MHz, read 20 MHz, inversion on, HSPI port), so no `User_Setup.h` is edited inside the library.

One firmware serves both languages; the language is chosen on the device (`L("English", "Русский")` in `src/core/lang.h`).

A single USB image for address `0x0` is merged from four parts (the same command CI runs):

```bash
esptool --chip esp32s3 merge-bin -o tengan_full_0x0.bin \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

`bootloader.bin`, `partitions.bin` and `firmware.bin` are in `.pio/build/t-embed-cc1101/`; `boot_app0.bin` is in `~/.platformio/packages/` (CI finds it with `find`). The NVS settings area (`0x9000..0xE000`) is not in the image.

### Tablet page

Sources are in `tools/webapp/src` (`index.html`, `hud.css`, `fonts.css`, `core/*.js`, `views/*.js`). The build inlines everything into one offline HTML file, gzips it and writes it as a C array into `src/web/tablet_page.h`, which the firmware serves.

```bash
python3 tools/webapp/build.py                 # build src/web/tablet_page.h
python3 tools/webapp/build.py --check         # CI: the committed page matches the sources
python3 tools/webapp/build.py --html out.html --only-html [--lang ru]   # HTML only
python3 tools/webapp/mock.py page.html 8090 [ru] [kurenai]              # test without a device
```

Important: `tablet_page.h` is committed. After changing anything in `tools/webapp/src` or `tools/webapp/i18n/ru.json` you must run `build.py` and commit the result, otherwise CI fails at "Tablet page is up to date". UI text is written in English; Russian comes from `tools/webapp/i18n/ru.json` embedded in the page. Fonts (`fonts.css`, WOFF2 in base64) are regenerated only by `tools/webapp/make_fonts.py` (needs `fonttools`, `brotli`); CI does not rebuild them.

### Android APK (TENGAN Tablet)

`app/` is a Capacitor 7 project (`@capacitor/android`, `@capacitor/core`, `@capacitor/geolocation`, all 7.x pinned in `app/package.json`; app id `dev.tengan.app`, `minSdk 23`, `compileSdk`/`targetSdk 35`). The app shows the same page as `/full` on the device and adds GPS.

```bash
cd app
npm ci
npm run sync               # = build.py --html www/index.html --only-html + npx cap sync android
cd android && ./gradlew assembleRelease
# result: app/android/app/build/outputs/apk/release/app-release.apk
```

Needs Node 22 and JDK 21 (what CI uses). Signing:

- With env variables `TENGAN_KEYSTORE` (path to a PKCS12 file) and `TENGAN_KEYSTORE_PASSWORD`, the release is signed with the TENGAN key (alias `tengan`, the same password for store and key).
- Without them (fork, your own build) the Android debug key is used. The APK works, but Android treats it as a different app: it does not install as an update over the official APK.

### TENGAN Desktop

`tools/desktop` is Python with pywebview; it also includes the PC monitor from `tools/pc_monitor`.

| Platform | Install | Build | Result |
|---|---|---|---|
| Windows | `pip install -r tools/desktop/requirements.txt pyinstaller` | `python tools/desktop/build_exe.py` | `dist/tengan_desktop.exe` (PyInstaller `--onefile --windowed`, icon and tray icon from `tengan.ico`) |
| macOS | `requirements-mac.txt` (pywebview with Cocoa/WebKit) | `python tools/desktop/build_xplat.py mac` | `dist/TENGAN-Desktop-mac.zip` with `TENGAN Desktop.app` |
| Linux | `requirements-linux.txt` (pywebview + PySide6/Qt WebEngine, sounddevice, python-xlib) | `python tools/desktop/build_xplat.py linux` | `dist/tengan_desktop/` (onedir); CI packs it into an AppImage |

On Linux `build_xplat.py` also prunes what the window does not need from the Qt bundle (QML, translations, Chromium locales other than en-US/ru, Qt libraries no remaining binary links to) to keep the size down. Tests: `python tools/desktop/test_events.py`; `tools/desktop/smoke_run.py <exe> [seconds]` starts the built app and checks `desktop.log` for a startup crash (the usual cause is a module PyInstaller missed).

### Single-file installer

```bash
python tools/flasher/make_flasher.py tengan_full_0x0.bin flash_tengan.html vX.Y
```

`make_flasher.py` fills `tools/flasher/flasher_template.html` with the firmware image (base64) and the bundled `esptool-js-0.5.4.bundle.js`. The page has English and Russian text (by browser language, with a switch) and writes everything except the NVS area (`0x9000..0xE000`), so device settings survive. It works in Chrome/Edge (Web Serial).

### Screen simulator

```bash
bash tools/sim/build.sh        # -> tools/sim/out/*.png and contact.png
```

It compiles the real drawing code (`src/core`, `src/ble`, `src/wifi`, `src/subghz`, `src/gallery`, `src/pc`, `src/probe`, ...) together with `tools/sim/sim.cpp` and stubs from `tools/sim/stubs` using `g++`, and writes every screen at 2x. Requirements: `g++`, Python 3 with Pillow, and PlatformIO libraries already downloaded (run `pio run` once), because it copies TFT_eSPI from `.pio/libdeps/t-embed-cc1101` and patches `uint32_t flash_address` to `uintptr_t` for 64-bit. Environment variables: `SIM_THEME=kurenai`, `SIM_LANG=ru`, `SIM_ANIM=1` (animation frames), `SIM_WORK`, `SIM_OUTDIR`. `tools/make_docs.py` turns the frames into the README GIFs and galleries in `docs/`.

### Other tools

- `tools/ook_test/run_tests.py` checks the 433 MHz decoders on captures from the `rtl_433_tests` set (see section 07).
- `tools/gen_*.py` generate the data headers; they are run by hand, not by CI (see section 07).

## GitHub Actions

Two workflows in `.github/workflows/`.

### `build.yml` ("build")

Triggers:

| Event | Condition |
|---|---|
| `push` | any branch, and tags `v*` |
| `pull_request` | any |
| `workflow_dispatch` | input `release_tag` (empty = just build) |

`paths-ignore` for push and pull request: `**.md`, `docs/**`, `LICENSE`, `fonts/*.txt`, `.gitignore`. A change touching only these files does not start a build. Path filters do not apply to tags, so a `v*` tag always builds. The commit message tag `[skip ci]` (GitHub's built-in feature) is used for comment-only or text-only commits.

Permissions: `contents: write` (needed to create releases). Action versions: checkout v5, setup-python v6 (3.12), upload-artifact v5, softprops/action-gh-release v2.

Jobs:

```
firmware (ubuntu-latest)
  |-- android (ubuntu-latest, needs: firmware)
  `-- desktop (windows-latest, needs: firmware)
```

**firmware**

1. Cache of `~/.platformio` and `.pio/libdeps`, keyed by `platformio.ini` and `boards/*.json`.
2. `pip install platformio "esptool==4.8.1"`.
3. `python tools/webapp/build.py --check`: the committed `tablet_page.h` must match the sources.
4. `pio run`.
5. Merge `dist/tengan_full_0x0.bin` (see above), copy `firmware.bin` to `dist/tengan_firmware_ota.bin`, build `dist/flash_tengan.html`.
6. Size budgets (the job fails if exceeded; raise the limit deliberately when growth is intended):

| File | Limit, bytes | Note in the workflow |
|---|---|---|
| `dist/tengan_firmware_ota.bin` | 2 830 000 | 2.69 MB on v2.0-test5 |
| `dist/flash_tengan.html` | 4 150 000 | 3.94 MB |
| tablet page gzip (`src/web/tablet_page.h`, counted as number of `0x..` bytes) | 231 000 | |

7. Upload artifact `firmware-<sha>` (`dist/*`).
8. Release step (see below).

**android** (needs `firmware`, so the release already exists): Node 22, JDK 21 (Temurin), `npm ci` and `npm run sync` in `app/`, signing key, `./gradlew --no-daemon assembleRelease`, copy to `dist/tengan_tablet.apk`, artifact `tablet-apk-<sha>`, attach to the release.

Signing (secrets `TENGAN_KEYSTORE_B64`, `TENGAN_KEYSTORE_PASSWORD`): the base64 PKCS12 key is decoded to `$RUNNER_TEMP/tengan.p12` and its path is exported as `TENGAN_KEYSTORE`. If the secret is absent:

- on a release (tag or `release_tag` set) the job fails with an error: a release APK must be signed with the TENGAN key, otherwise users could not update over it;
- otherwise it only warns and signs with a throwaway debug key.

`APP_VERSION_CODE` is `github.run_number`, which grows monotonically, so each new build can be installed over the previous one.

**desktop** (Windows, needs `firmware`): installs `tools/desktop/requirements.txt` and PyInstaller, checks that `webview, serial, psutil, tengan_desktop` import, runs `tools/desktop/build_exe.py`, artifact `desktop-exe-<sha>`, attaches `dist/tengan_desktop.exe` to the release.

### Releases

A release is created by the `firmware` job in either of two ways:

1. Push a tag `v*` (for example `v2.0`).
2. Actions, build, Run workflow, `release_tag = v1.1`. The tag is created on the built commit (`target_commitish: github.sha`).

The release is named `TENGAN <tag>`, notes are generated automatically (`generate_release_notes`) and a bilingual EN/RU install table is added to the body. Files are added by three jobs to the same tag.

| File | Added by | Purpose |
|---|---|---|
| `flash_tengan.html` | firmware | Easiest install: open in Chrome/Edge, press Install firmware |
| `tengan_full_0x0.bin` | firmware | Full USB image for address 0x0 (esptool, esptool-js) |
| `tengan_firmware_ota.bin` | firmware | Wireless update from the device's Web section |
| `tengan_tablet.apk` | android | TENGAN Tablet for Android |
| `tengan_desktop.exe` | desktop | TENGAN Desktop for Windows |
| `TENGAN-Desktop-mac.zip` | desktop-xplat (manual) | TENGAN Desktop for macOS |
| `TENGAN-Desktop-linux-x86_64.AppImage` | desktop-xplat (manual) | TENGAN Desktop for Linux |

### `desktop-xplat.yml` ("desktop-xplat")

Only `workflow_dispatch` (input `release_tag`; if set, the files are attached to that existing release). It never runs on push: a macOS minute counts as 10 minutes of the Actions limit (the workflow comment), so the builds are made on demand, for a release. Both jobs have `timeout-minutes: 40`.

- **mac** (`macos-latest`): install `requirements-mac.txt` and PyInstaller; `py_compile` of the Python files and `test_events.py`; `build_xplat.py mac`; smoke run for 25 s (`TENGAN_NO_TRAY=1`); artifact `desktop-mac-<sha>`; release step uploads `TENGAN-Desktop-mac.zip`.
- **linux** (`ubuntu-22.04`): installs system libraries for Qt WebEngine, audio, AppImage and Xvfb; `requirements-linux.txt`; the same logic tests; `build_xplat.py linux`; self-test of the packaged binary (`TENGAN_SELFTEST=1`, offscreen Qt: exit code 0 and `selftest OK` in `~/.config/TENGAN/desktop.log`); smoke run under `xvfb-run`; packing into an AppImage with `appimagetool` (downloaded in the job; `AppRun` and `.desktop` file generated); artifact `desktop-linux-<sha>`; release step uploads `TENGAN-Desktop-linux-x86_64.AppImage`.

## Release checklist

1. Set the version in `VERSION`; make sure `build.py --check` passes (tablet page rebuilt and committed).
2. Run the `build` workflow with `release_tag`, or push a tag `v*`.
3. Make sure the `TENGAN_KEYSTORE_*` secrets are set (otherwise the android job fails on a release).
4. Optionally run `desktop-xplat` with the same `release_tag` to add the macOS and Linux builds.
5. If a size budget fails, check what grew; raise the limit in `build.yml` only on purpose.
