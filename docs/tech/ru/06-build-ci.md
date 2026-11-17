# Сборка и CI

В разделе описано, как каждая часть TENGEN собирается локально и в GitHub Actions и что входит в релиз. Части: прошивка (PlatformIO), страница планшета, APK для Android, TENGEN Desktop, установщик одним файлом и симулятор экрана.

## Версия

Версия одна на всё: файл `VERSION` в корне репозитория (сейчас одна строка вида `2.0-test6`).

| Потребитель | Как берёт `VERSION` |
|---|---|
| Прошивка | `tools/version.py` — pre-скрипт PlatformIO (`extra_scripts = pre:tools/version.py` в `platformio.ini`). Перед каждой сборкой пишет `src/core/version.h` со строкой `#define FW_VERSION "..."`, но только если текст изменился (файл зря не трогается). Руками `version.h` не править. |
| Страница планшета | `tools/webapp/build.py` подставляет версию вместо `@@VERSION@@` в HTML. Приложение сравнивает версию страницы с версией устройства (`/api/pagever`). |
| APK | `app/android/app/build.gradle`: `versionName` — переменная окружения `APP_VERSION_NAME`, иначе содержимое `VERSION` (`/` заменяется на `-`). `versionCode` — переменная `APP_VERSION_CODE`, иначе `1`. В CI — `github.run_number`. |
| Страница установщика | Версия — третий аргумент `tools/flasher/make_flasher.py`. В CI это тег релиза, если он есть, иначе `github.ref_name` (имя ветки). |

## Локальная сборка

### Прошивка

Нужен [PlatformIO](https://platformio.org/).

```bash
pio run            # -> .pio/build/t-embed-cc1101/firmware.bin
```

Главное в `platformio.ini` (одно окружение `t-embed-cc1101`):

- `platform = espressif32`, `framework = arduino`, плата `T_Embed_PN532` из собственного профиля `boards/T_Embed_PN532.json` (взят из репозитория LilyGO: ESP32-S3, память `qio_opi`, PSRAM, флеш QIO 80 МГц, CPU 240 МГц, разметка `default_16MB.csv`). Обычная плата `feather-s3` не подходит: она включает GPIO21 (подсветка экрана) до `setup()`, из-за чего экран вспыхивает красным.
- `lib_deps`: `bodmer/TFT_eSPI ^2.5.43`, `h2zero/NimBLE-Arduino ^2.1.0`, `adafruit/Adafruit NeoPixel ^1.12.3`.
- Оптимизация на скорость: `-Os` убран, добавлен `-O2` («флеша хватает»).
- `-DARDUINO_USB_CDC_ON_BOOT=1`; профиль платы добавляет `-DBOARD_HAS_PSRAM` и `-DARDUINO_USB_MODE=1`.
- TFT_eSPI целиком настроен флагами сборки (`-DUSER_SETUP_LOADED=1`, ST7789, 170x320, пины MOSI 9, MISO 10, SCLK 11, CS 41, DC 16, BL 21, RST не используется, SPI 40 МГц, чтение 20 МГц, инверсия, порт HSPI), поэтому `User_Setup.h` внутри библиотеки не правится.

Одна прошивка на оба языка; язык выбирается на устройстве (`L("English", "Русский")` в `src/core/lang.h`).

Единый образ для USB по адресу `0x0` склеивается из четырёх частей (та же команда в CI):

```bash
esptool --chip esp32s3 merge-bin -o tengen_full_0x0.bin \
  0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

`bootloader.bin`, `partitions.bin` и `firmware.bin` лежат в `.pio/build/t-embed-cc1101/`; `boot_app0.bin` — в `~/.platformio/packages/` (CI находит его через `find`). Область настроек NVS (`0x9000..0xE000`) в образ не входит.

### Страница планшета

Исходники — `tools/webapp/src` (`index.html`, `hud.css`, `fonts.css`, `core/*.js`, `views/*.js`). Сборка встраивает всё в один офлайн-HTML, сжимает gzip и записывает как массив C в `src/web/tablet_page.h`, который отдаёт прошивка.

```bash
python3 tools/webapp/build.py                 # собрать src/web/tablet_page.h
python3 tools/webapp/build.py --check         # CI: закоммиченная страница совпадает с исходниками
python3 tools/webapp/build.py --html out.html --only-html [--lang ru]   # только HTML
python3 tools/webapp/mock.py page.html 8090 [ru] [kurenai]              # проверка без устройства
```

Важно: `tablet_page.h` лежит в репозитории. После любой правки в `tools/webapp/src` или `tools/webapp/i18n/ru.json` нужно запустить `build.py` и закоммитить результат, иначе CI упадёт на шаге «Tablet page is up to date». Тексты интерфейса пишутся по-английски; русский — словарь `tools/webapp/i18n/ru.json`, встроенный в страницу. Шрифты (`fonts.css`, WOFF2 в base64) пересобирает только `tools/webapp/make_fonts.py` (нужны `fonttools`, `brotli`); CI их не пересобирает.

### APK для Android (TENGEN Tablet)

`app/` — проект Capacitor 7 (`@capacitor/android`, `@capacitor/core`, `@capacitor/geolocation`, версии 7.x закреплены в `app/package.json`; id приложения `dev.tengen.app`, `minSdk 23`, `compileSdk`/`targetSdk 35`). Приложение показывает ту же страницу, что `/full` на устройстве, и добавляет GPS.

```bash
cd app
npm ci
npm run sync               # = build.py --html www/index.html --only-html + npx cap sync android
cd android && ./gradlew assembleRelease
# результат: app/android/app/build/outputs/apk/release/app-release.apk
```

Нужны Node 22 и JDK 21 (как в CI). Подпись:

- Если заданы переменные `TENGEN_KEYSTORE` (путь к файлу PKCS12) и `TENGEN_KEYSTORE_PASSWORD`, релиз подписывается ключом TENGEN (алиас `tengen`, один пароль на хранилище и ключ).
- Без них (форк, своя сборка) используется отладочный ключ Android. APK работает, но Android считает его другим приложением: поверх официального APK он не встанет как обновление.

### TENGEN Desktop

`tools/desktop` — Python и pywebview; внутрь также входит монитор ПК из `tools/pc_monitor`.

| Система | Установка | Сборка | Результат |
|---|---|---|---|
| Windows | `pip install -r tools/desktop/requirements.txt pyinstaller` | `python tools/desktop/build_exe.py` | `dist/tengen_desktop.exe` (PyInstaller `--onefile --windowed`, иконка файла и трея — `tengen.ico`) |
| macOS | `requirements-mac.txt` (pywebview с Cocoa/WebKit) | `python tools/desktop/build_xplat.py mac` | `dist/TENGEN-Desktop-mac.zip` с `TENGEN Desktop.app` |
| Linux | `requirements-linux.txt` (pywebview + PySide6/Qt WebEngine, sounddevice, python-xlib) | `python tools/desktop/build_xplat.py linux` | `dist/tengen_desktop/` (onedir); в CI упаковывается в AppImage |

Для Linux `build_xplat.py` дополнительно вырезает из пакета Qt то, что окну не нужно (QML, переводы, локали Chromium кроме en-US/ru, библиотеки Qt, на которые не ссылается ни один оставшийся бинарник), чтобы уменьшить размер. Тесты: `python tools/desktop/test_events.py`; `tools/desktop/smoke_run.py <exe> [секунд]` запускает собранное приложение и ищет в `desktop.log` падение при старте (обычная причина — модуль, который не подхватил PyInstaller).

### Установщик одним файлом

```bash
python tools/flasher/make_flasher.py tengen_full_0x0.bin flash_tengen.html vX.Y
```

`make_flasher.py` вставляет в шаблон `tools/flasher/flasher_template.html` образ прошивки (base64) и библиотеку `esptool-js-0.5.4.bundle.js`. Страница двуязычная (по языку браузера, есть переключатель) и пишет всё, кроме области NVS (`0x9000..0xE000`), поэтому настройки устройства сохраняются. Работает в Chrome/Edge (Web Serial).

### Симулятор экрана

```bash
bash tools/sim/build.sh        # -> tools/sim/out/*.png и contact.png
```

Скрипт компилирует настоящий код рисования (`src/core`, `src/ble`, `src/wifi`, `src/subghz`, `src/gallery`, `src/pc`, `src/probe` и др.) вместе с `tools/sim/sim.cpp` и заглушками из `tools/sim/stubs` через `g++` и сохраняет каждый экран с увеличением x2. Нужны: `g++`, Python 3 с Pillow и уже скачанные библиотеки PlatformIO (один раз выполните `pio run`): TFT_eSPI копируется из `.pio/libdeps/t-embed-cc1101`, и в копии `uint32_t flash_address` заменяется на `uintptr_t` для 64-битной системы. Переменные окружения: `SIM_THEME=kurenai`, `SIM_LANG=ru`, `SIM_ANIM=1` (кадры анимаций), `SIM_WORK`, `SIM_OUTDIR`. `tools/make_docs.py` превращает кадры в GIF и галереи README в `docs/`.

### Прочие инструменты

- `tools/ook_test/run_tests.py` проверяет декодеры 433 МГц на записях из набора `rtl_433_tests` (см. раздел 07).
- `tools/gen_*.py` генерируют заголовки с данными; запускаются вручную, не в CI (см. раздел 07).

## GitHub Actions

В `.github/workflows/` два workflow.

### `build.yml` («build»)

Запуск:

| Событие | Условие |
|---|---|
| `push` | любая ветка и теги `v*` |
| `pull_request` | любой |
| `workflow_dispatch` | параметр `release_tag` (пусто — просто собрать) |

`paths-ignore` для push и pull request: `**.md`, `docs/**`, `LICENSE`, `fonts/*.txt`, `.gitignore`. Изменение только этих файлов сборку не запускает. На теги фильтры путей не действуют, поэтому тег `v*` собирается всегда. Метка `[skip ci]` в сообщении коммита (встроенная возможность GitHub) используется для коммитов только с комментариями или текстами.

Права: `contents: write` (нужны для создания релиза). Версии действий: checkout v5, setup-python v6 (3.12), upload-artifact v5, softprops/action-gh-release v2.

Задания:

```
firmware (ubuntu-latest)
  |-- android (ubuntu-latest, needs: firmware)
  `-- desktop (windows-latest, needs: firmware)
```

**firmware**

1. Кэш `~/.platformio` и `.pio/libdeps` с ключом по `platformio.ini` и `boards/*.json`.
2. `pip install platformio "esptool==4.8.1"`.
3. `python tools/webapp/build.py --check`: закоммиченный `tablet_page.h` должен совпадать с исходниками.
4. `pio run`.
5. Склейка `dist/tengen_full_0x0.bin` (см. выше), копия `firmware.bin` в `dist/tengen_firmware_ota.bin`, сборка `dist/flash_tengen.html`.
6. Бюджеты размеров (задание падает при превышении; лимит поднимают осознанно, если рост задуман):

| Файл | Лимит, байт | Пометка в workflow |
|---|---|---|
| `dist/tengen_firmware_ota.bin` | 2 830 000 | 2,69 МБ на v2.0-test5 |
| `dist/flash_tengen.html` | 4 150 000 | 3,94 МБ |
| страница планшета в gzip (`src/web/tablet_page.h`, считается число байтов `0x..`) | 231 000 | |

7. Артефакт `firmware-<sha>` (`dist/*`).
8. Шаг Release (см. ниже).

**android** (`needs: firmware`, значит релиз уже создан): Node 22, JDK 21 (Temurin), `npm ci` и `npm run sync` в `app/`, ключ подписи, `./gradlew --no-daemon assembleRelease`, копия в `dist/tengen_tablet.apk`, артефакт `tablet-apk-<sha>`, прикрепление к релизу.

Подпись (секреты `TENGEN_KEYSTORE_B64`, `TENGEN_KEYSTORE_PASSWORD`): ключ PKCS12 в base64 декодируется в `$RUNNER_TEMP/tengen.p12`, путь передаётся как `TENGEN_KEYSTORE`. Если секрета нет:

- при релизе (тег или заполненный `release_tag`) задание падает с ошибкой: релизный APK обязан быть подписан ключом TENGEN, иначе пользователи не смогут обновиться поверх;
- иначе — только предупреждение, подпись одноразовым отладочным ключом.

`APP_VERSION_CODE` равен `github.run_number` и только растёт, так что каждая новая сборка ставится поверх предыдущей.

**desktop** (Windows, `needs: firmware`): ставит `tools/desktop/requirements.txt` и PyInstaller, проверяет импорт `webview, serial, psutil, tengen_desktop`, запускает `tools/desktop/build_exe.py`, артефакт `desktop-exe-<sha>`, прикрепляет `dist/tengen_desktop.exe` к релизу.

### Релизы

Релиз создаёт задание `firmware` одним из двух способов:

1. Отправить тег `v*` (например `v2.0`).
2. Actions, build, Run workflow, `release_tag = v1.1`. Тег создаётся на собранном коммите (`target_commitish: github.sha`).

Релиз называется `TENGEN <тег>`, описание генерируется автоматически (`generate_release_notes`) и к нему добавляется двуязычная таблица установки EN/RU. Файлы добавляют три задания в один и тот же тег.

| Файл | Кто добавляет | Назначение |
|---|---|---|
| `flash_tengen.html` | firmware | Самый простой способ: открыть в Chrome/Edge, нажать «Установить прошивку» |
| `tengen_full_0x0.bin` | firmware | Полный образ для USB с адреса 0x0 (esptool, esptool-js) |
| `tengen_firmware_ota.bin` | firmware | Обновление по Wi-Fi из раздела «Веб» на устройстве |
| `tengen_tablet.apk` | android | TENGEN Tablet для Android |
| `tengen_desktop.exe` | desktop | TENGEN Desktop для Windows |
| `TENGEN-Desktop-mac.zip` | desktop-xplat (вручную) | TENGEN Desktop для macOS |
| `TENGEN-Desktop-linux-x86_64.AppImage` | desktop-xplat (вручную) | TENGEN Desktop для Linux |

### `desktop-xplat.yml` («desktop-xplat»)

Только `workflow_dispatch` (параметр `release_tag`; если задан, файлы добавляются к этому существующему релизу). На push не запускается никогда: минута macOS списывается как 10 минут лимита Actions (комментарий в workflow), поэтому сборки делаются по требованию, к релизу. У обоих заданий `timeout-minutes: 40`.

- **mac** (`macos-latest`): `requirements-mac.txt` и PyInstaller; `py_compile` Python-файлов и `test_events.py`; `build_xplat.py mac`; пробный запуск на 25 с (`TENGEN_NO_TRAY=1`); артефакт `desktop-mac-<sha>`; шаг Release загружает `TENGEN-Desktop-mac.zip`.
- **linux** (`ubuntu-22.04`): системные библиотеки для Qt WebEngine, звука, AppImage и Xvfb; `requirements-linux.txt`; те же тесты логики; `build_xplat.py linux`; самопроверка собранного бинарника (`TENGEN_SELFTEST=1`, Qt offscreen: код выхода 0 и строка `selftest OK` в `~/.config/TENGEN/desktop.log`); пробный запуск под `xvfb-run`; упаковка в AppImage через `appimagetool` (скачивается в задании; `AppRun` и `.desktop` создаются на лету); артефакт `desktop-linux-<sha>`; шаг Release загружает `TENGEN-Desktop-linux-x86_64.AppImage`.

## Чек-лист релиза

1. Поставить версию в `VERSION`; убедиться, что `build.py --check` проходит (страница планшета пересобрана и закоммичена).
2. Запустить workflow `build` с `release_tag` или отправить тег `v*`.
3. Убедиться, что секреты `TENGEN_KEYSTORE_*` заданы (иначе задание android упадёт на релизе).
4. При желании запустить `desktop-xplat` с тем же `release_tag`, чтобы добавить сборки для macOS и Linux.
5. Если упал бюджет размера, выяснить, что выросло; лимит в `build.yml` поднимать только осознанно.
