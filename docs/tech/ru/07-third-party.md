# Сторонние компоненты, данные и лицензии

Код проекта распространяется по лицензии **MIT** (`LICENSE`, «Copyright (c) 2026 wolfyboy1488»). В разделе перечислено остальное: библиотеки, шрифты, наборы данных, встроенные скрипты; где они лежат в репозитории и каковы условия лицензий и ответственного использования. Тексты лицензий встроенных сторонних материалов хранятся рядом с ними (`fonts/`, `tools/astro_src/`, `tools/flasher/`).

## Библиотеки прошивки

Объявлены в `platformio.ini` (`lib_deps`; PlatformIO скачивает их сам, в репозитории их нет):

| Библиотека | Версия | Для чего |
|---|---|---|
| TFT_eSPI (`bodmer/TFT_eSPI`) | `^2.5.43` | Драйвер экрана (ST7789 170x320), спрайты, текст. Настраивается флагами сборки вместо `User_Setup.h`. Шрифты подключаются как массивы Adafruit GFX. |
| NimBLE-Arduino (`h2zero/NimBLE-Arduino`) | `^2.1.0` | Сканирование Bluetooth LE (пассивное; устройство ничего не передаёт). |
| Adafruit NeoPixel (`adafruit/Adafruit NeoPixel`) | `^1.12.3` | Светодиоды на плате. |

Платформа: PlatformIO `espressif32` с фреймворком Arduino; он приносит Arduino-ESP32 и ESP-IDF (Wi-Fi, lwIP, mbedTLS, OTA, API QR-кодов `qrcode.h`, который использует `src/web/web.cpp`, и другое). Профиль платы `boards/T_Embed_PN532.json` взят из репозитория LilyGO.

Написано для проекта (чужой код не копировался): драйвер CC1101, декодеры OOK, декодер ИК, чтение NFC на PN532, расчёт орбит (`src/gallery/sat.cpp`: разбор TLE, приближение Кеплер + J2) и т. д. Декодеры OOK следуют открытой документации протоколов [rtl_433](https://github.com/merbanan/rtl_433) (правила нарезки битов, форматы, контрольные суммы; см. `src/subghz/ook.h`), но код оригинальный. `tools/ook_test/` сверяет их с набором записей `rtl_433_tests` (файлы `.cu8` с ожидаемым `.json`), который скачивается отдельно.

## Шрифты

Шрифты лежат в `fonts/` вместе с файлами лицензий. Латиницу и кириллицу рисует одно и то же семейство, поэтому при смене языка вёрстка не сдвигается.

| Шрифт | Файлы | Лицензия по `fonts/` и README | Где используется |
|---|---|---|---|
| Exo 2 (SemiBold, Bold) | `Exo2-*.ttf`, `OFL-Exo2.txt` | SIL OFL 1.1, (c) 2013 The Exo 2 Project Authors | Заголовки и крупные цифры (тема DENKO) |
| Oswald (SemiBold) | `Oswald-SemiBold.ttf`, `OFL-Oswald.txt` | SIL OFL 1.1, (c) 2016 The Oswald Project Authors | Узкие заголовки и цифры (тема KURENAI) |
| JetBrains Mono (Medium, SemiBold) | `JetBrainsMono-*.ttf`, `OFL-JetBrainsMono.txt` | SIL OFL 1.1, (c) 2020 The JetBrains Mono Project Authors (font version 2.304) | Весь остальной текст |
| IPAGothic | в репозитории нет; `LICENSE-IPAFont.txt` | IPA Font License v1.0, (c) 2007-2011 Information-technology Promotion Agency, Japan | Только два иероглифа печати 天眼 |


Как шрифты попадают в продукт:

- **Экран устройства.** `tools/gen_gfxfont.py` растеризует TTF через Pillow в растровый шрифт Adafruit GFX (`src/core/fonts/`: `JBM10/11/12/14/19`, `EXO20/37`, `OSW20/36`). Параметры: `--latin1` (добавляет `· « » ° ×`), `--cyr` (плюс кириллица до U+045F), `--thr=N` (порог жирности, по умолчанию 110). Диапазон глифов сплошной, поэтому коды между латиницей и кириллицей пусты. TFT_eSPI сама разбирает UTF-8.
- **Печать.** `tools/gen_seal.py <путь к ipag.ttf>` рисует иероглифы 天眼 как однобитную «ханко» (32x60) в `src/core/seal.h`. Шрифт нужен только для растеризации, в поставку не входит.
- **Страница планшета.** `tools/webapp/make_fonts.py` оставляет в шрифтах только нужные глифы и встраивает их как WOFF2 в base64 в `tools/webapp/src/fonts.css`, поэтому страница работает полностью офлайн. IPAGothic здесь — системный шрифт `fonts-ipafont-gothic` (`/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf`).

## Наборы данных

Все сгенерированные данные закоммичены как заголовки C (или встроены в страницу); при обычной сборке источники не скачиваются. Генераторы запускаются вручную.

| Данные | Источник | Генератор | Результат |
|---|---|---|---|
| Производители Bluetooth по Company ID | Bluetooth SIG assigned numbers через [bluetooth-numbers-database](https://github.com/NordicSemiconductor/bluetooth-numbers-database) Nordic (`company_ids.json`) | `tools/gen_btdb.py` | `src/ble/btdb.h` (около 127 КБ) |
| Производители Bluetooth по 16-битному UUID участника | Bluetooth SIG `member_uuids.yaml` (assigned_numbers) | `tools/gen_btdb.py` | `src/ble/btdb.h` |
| Производители сетевых устройств по OUI (первые 3 байта MAC) | данные IEEE, собранные проектом Nmap: `nmap-mac-prefixes` | `tools/gen_ouidb.py` | `src/wifi/ouidb.h` (около 936 КБ) |
| Звёзды, линии и названия созвездий | [d3-celestial](https://github.com/ofrohn/d3-celestial) (c) Olaf Frohn, BSD-3 (звёзды Hipparcos до величины 5) | `tools/gen_astro.py` | `src/gallery/astrodata.h` |
| Контуры суши для глобуса | [world-atlas](https://github.com/topojson/world-atlas) (c) Mike Bostock, ISC, на основе Natural Earth 1:110m (общественное достояние) | `tools/gen_astro.py` | `src/gallery/astrodata.h` |
| Орбиты спутников (TLE) | [CelesTrak](https://celestrak.org) | не генерируется: скачивается во время работы | нет |

Подробности:

- `gen_btdb.py` сокращает названия (отбрасывает слова вроде Inc, Ltd, GmbH, Technology и названия городов вроде Shenzhen), чтобы уложиться в память и экран устройства. `gen_ouidb.py` использует его функцию `short()`, оставляет только полные 24-битные OUI и хранит отсортированные ключи и таблицу уникальных названий.
- Входные файлы `gen_astro.py` лежат в `tools/astro_src/` (`stars.mag5.json`, `constellations.json`, `constellations.lines.json`, `starnames.proper.json`, `land-110m.json`) вместе с `LICENSE-d3-celestial.txt` и `LICENSE-world-atlas.txt`. Заголовок пишется в stdout: `python3 tools/gen_astro.py > src/gallery/astrodata.h`.
- Орбиты: `tools/pc_monitor/pc_monitor.py` раз в 6 часов скачивает TLE для ISS, Tiangong и Hubble с CelesTrak, раз в минуту отдаёт их устройству и хранит кэш `tle_cache.txt` рядом с программой (без интернета работает из кэша; файл в `.gitignore`).

## Встроенное и сборочное ПО

| Компонент | Где | Роль | Лицензия |
|---|---|---|---|
| [esptool-js](https://github.com/espressif/esptool-js) 0.5.4 (c) Espressif | `tools/flasher/esptool-js-0.5.4.bundle.js`, `esptool-js.LICENSE` | Прошивка через Web Serial внутри `flash_tengan.html` | Apache 2.0 |
| esptool 4.8.1 | ставится в CI (`pip`) | Склейка `tengan_full_0x0.bin` | в репозитории не хранится |
| pywebview | `tools/desktop/requirements*.txt` | Окно TENGAN Desktop (macOS: системный WebKit через pyobjc; Linux: Qt WebEngine через PySide6/QtPy) | см. проект |
| PyInstaller | ставится в CI | Упаковка Desktop | см. проект |
| Прочие библиотеки Desktop | `requirements*.txt` | pyserial, psutil, pystray, Pillow; Windows: pycaw, comtypes, numpy, PyAudioWPatch, winrt-*; Linux: PySide6, QtPy, python-xlib, numpy, sounddevice | см. каждый проект |
| Capacitor 7.6.10 (`@capacitor/android`, `@capacitor/core`), `@capacitor/geolocation` 7.1.8, `@capacitor/cli` | `app/package.json` | Оболочка Android-приложения и GPS | см. проект |
| AndroidX (`appcompat`, `coordinatorlayout`, `core-splashscreen` и др.), Cordova Android 10.1.1 | `app/android/variables.gradle` | Зависимости Android | см. проект |
| Pillow | `tools/gen_*.py`, `tools/sim`, `tools/make_docs.py` | Растеризация картинок и шрифтов | см. проект |
| fonttools, brotli | `tools/webapp/make_fonts.py` | Подрезка шрифтов для страницы | см. проект |

В README перечислены только лицензии, указанные в файлах репозитория (MIT для проекта, OFL для Exo 2, Oswald и JetBrains Mono, Apache 2.0 для esptool-js, IPA Font License, BSD-3, ISC). Для остальных пакетов действует лицензия, опубликованная соответствующим проектом; проверьте её перед распространением сборки.

## Принцип «только приём» и ответственное использование

TENGAN сделан, чтобы смотреть на радиообстановку, а не вмешиваться в неё:

- Он ничего не передаёт в эфир: CC1101 и ИК никогда не переключаются на передачу, NFC только читает. Единственное излучение — собственная точка доступа Wi-Fi для планшета, пока открыт раздел «Веб».
- Он показывает только то, что устройства и так вещают всем вокруг. Содержимое пакетов Wi-Fi не читается, коды пультов не сохраняются, номера и имена держателей банковских карт не считываются.
- Всё работает офлайн: без облака, данные остаются на устройстве и на вашем планшете или компьютере. Единственный сетевой запрос — на стороне ПК: монитор ПК скачивает элементы орбит с CelesTrak для Globe.
- Законы о приёме радиосигналов и о приватности отличаются в разных странах. Используйте TENGAN там и так, как это законно, и уважайте чужую приватность.
- Ограничения: с одной антенной устройство различает только «ближе / дальше»; метры — грубая оценка по уровню сигнала; угол на радаре нужен для наглядности; сети 5 ГГц не видны (ESP32-S3 работает только на 2,4 ГГц); современные телефоны меняют адреса и почти ничего не вещают, поэтому многие устройства остаются «неизвестными».
