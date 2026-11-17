# Overview and architecture

TENGEN is a receive-only radio "spectrum viewer" for the LilyGO T-Embed CC1101 (ESP32-S3). The
device (the "stick") listens to Bluetooth, Wi-Fi (2.4 GHz), Sub-GHz (CC1101), IR, NFC and sound, shows the
result on its own 320x170 screen and, in parallel, streams the same data to a tablet (TENGEN Tablet),
a phone browser or a computer (TENGEN Desktop). All heavy drawing in the "HQ" view happens on the
tablet or computer; the stick only sends numbers.

This section describes the big picture: who talks to whom, what the main loop does, how the device saves
power, how a crash is diagnosed, and how the code is organised. Pin-level details are in
[02-hardware.md](02-hardware.md).

## 1. Components and data flow

```
                         +-----------------------------------------+
   radio / sensors       |        STICK  (ESP32-S3, firmware)       |
 +----------------+      |                                         |
 | BLE advertisers| ---> | NimBLE scan (ble/)                       |
 | Wi-Fi beacons  | ---> | passive scan, promiscuous counters (wifi/)|
 | 315..915 MHz   | ---> | CC1101 RSSI / OOK edges  (subghz/)       |
 | IR remotes     | ---> | RMT receiver              (probe/ir)     |
 | NFC cards      | ---> | PN532 over I2C            (probe/nfc)    |
 | sound          | ---> | PDM microphone + FFT      (audio/)       |
 +----------------+      |                 |                       |
                         |   section models + screens              |
   wheel, side button -> |   core/ (menu, UI, input, power)        |--> 320x170 screen, 8 LEDs, speaker
                         |                 |                       |
                         |        web/  (Web section)              |
                         +---------+-----------------+-------------+
                                   |                 |
                       Wi-Fi AP "SSID from MAC"   USB-CDC (Serial, 115200)
                       192.168.4.1                       |
                       :80  HTTP (page, /api/*, OTA)     |  lines: "@W/@Q/@G ..." from PC
                       :81  SSE stream (events)          |         "@E/@R/@g ..." to PC
                       :53  DNS (catch-all)              |         PC monitor: "KEY ...", "PA ...", ...
                                   |                     |
                    +--------------+------+    +---------+----------------+
                    | tablet / phone      |    | computer                 |
                    | TENGEN Tablet (APK) |    | TENGEN Desktop           |
                    | or any browser      |    | (pywebview) + PC monitor |
                    | page = tablet_page.h|    | same page, USB instead   |
                    +---------------------+    | of Wi-Fi                 |
                                               +--------------------------+
```

Key facts:

- **One page, two transports.** The tablet page (`src/web/tablet_page.h`, a gzip blob generated from
  `tools/webapp/`) is stored in the firmware. A tablet fetches it over HTTP and then opens an SSE stream
  on port 81 (`/stream?want=ble,wifi,snd,...`). TENGEN Desktop asks for the same page over USB (`@G`) and
  receives the same JSON events as `@E <event> <json>` lines. On the firmware side the USB link is just a
  third "SSE client" (`USB_SLOT` in `src/web/web.cpp`; `SSE_NET = 2` Wi-Fi tablets + 1 USB client).
- **The stick is the data source, the client is the renderer.** The stick sends compact JSON (arrays, not
  objects, capped lists: for example up to 80 Bluetooth devices in the stream) and the page draws.
- **Subscriptions.** A client says what it wants (`want=` list). Radios that nobody watches are switched
  off (see section 5).
- **Everything is local.** The access point has no uplink and there is no cloud; the DNS server answers
  every name with the stick's address only so that phones open the page by themselves.
- **USB carries two protocols on one wire.** Old PC-monitor lines (`PM`, `PV`, `KEY ...`, `AUD ON`,
  `PA ...`; `src/pc/pc.cpp`) and the newer `@`-prefixed app link (`webLinkLine()` in `src/web/web.cpp`).
  All output to the PC goes through one queue (`src/link/serout.cpp`): a 64 KB ring buffer in PSRAM, lines
  are stored whole, written non-blocking in chunks as the port has room, and "droppable" lines (audio and
  spectrum frames) are refused when more than 12 KB is already queued. Result: lines never interleave and a
  stalled PC cannot freeze the stick.

The wire formats themselves (HTTP API, SSE events, `@`-lines, OTA) are described in the protocol section.

## 2. Firmware startup (`setup()`)

`src/main.cpp`, in this order:

1. `crumbBoot()` - read the previous reset reason and the last logged step (see section 7).
2. `checkWakeHold()` - if the board woke from deep sleep ("power off"), require the side button to be
   held for `PWR_ON_MS` = 1 s; otherwise go straight back to sleep (accidental touch in a bag).
3. Deselect every device on the shared SPI bus (all CS pins HIGH) and raise `BOARD_PWR_EN` (GPIO 15,
   peripheral power).
4. `Serial` with enlarged buffers (RX 2048, TX 8192) at 115200 on the native USB-CDC.
5. `inputBegin`, `displayBegin`, `ledsBegin`, `settingsBegin`, `batBegin`, `bootScreen`, `bleBegin`,
   `audioBegin`, `satBegin`, `navBegin`, first `navDraw`.
6. `crumbWatchdogStart()` - the watchdog starts only now, so a slow boot animation cannot trip it.

Two details exist to avoid a visible flash of garbage on the screen: `initVariant()` and a global
constructor with priority 101 (`earlyBacklightOff()`) both drive the backlight pin LOW before the Arduino
core initialises; the backlight is switched on only after the first `fillScreen` (`displayBegin()`).

Another start-up rule: BLE is initialised (`bleBegin()`) but **not scanning**; scanning starts when the
Bluetooth section is entered.

OTA rollback: a firmware installed over Wi-Fi boots as "pending verify". `confirmFirmware()` calls
`esp_ota_mark_app_valid_cancel_rollback()` after 15 s of uptime; if the new image hangs or reboots earlier,
the bootloader returns to the previous slot. After a USB flash it is a no-op.

## 3. The main loop

`loop()` in `src/main.cpp` is a single cooperative loop on the Arduino task (core 0 per the board profile;
the microphone and speaker tasks are pinned to core 1). One pass:

| Step | Call | What it does |
|---|---|---|
| 1 | `crumbFeed()` | feed the task watchdog |
| 2 | `inputPoll()` | wheel steps, wheel press, side button (short = back, hold = power off) |
| 3 | `bleSample(now)` | sample the Bluetooth device count for the LED/event logic |
| 4 | `ledsTick()`, `batTick(now)` | LED animation; battery re-read every 2 s |
| 5 | `pcPoll(now)` | read lines from USB (budget 2048 bytes per pass), dispatch to PC monitor or `@` link |
| 6 | `webLinkPump()` | page transfer in chunks and the USB write queue |
| 7 | `navTick(now)` | `tick()` of the open section (scans, sweeps, the Web server) |
| 8 | `powerIdleTick(now)`, `confirmFirmware(now)` | auto-dim, OTA confirmation |
| 9 | draw (if the frame interval has passed) | `bleUpdate`, `navDraw` (or the power-hold gauge), `settingsAutosave` |
| 10 | `perfLoop`, `perfTick`, `delay(5)` | timing statistics; 5 ms yield |

Every phase of interest is preceded by `crumb("...")` (about 24 call sites in the code), so the black box
knows where the loop was.

**Frame pacing.** Drawing is decoupled from polling:

| State | Frame interval |
|---|---|
| normal | `REDRAW_MS` = 66 ms (about 15 fps), `src/core/board.h` |
| a screen set `g_fastFrame` (3D scenes, gallery, sound) | 33 ms (about 30 fps) |
| dimmed (idle) | `REDRAW_MS * 4` = 264 ms |

`g_fastFrame` is reset after each frame, so a screen must ask again on every draw - when the user leaves
the 3D screen the rate falls back by itself.

**Drawing model.** Every screen draws into a full-frame sprite `spr` (`TFT_eSprite`, 16 bit, 320x170,
about 108 KB; `src/core/display.cpp`) and ends with `present()` (`src/core/theme.cpp`). `present()` pushes
only the changed rows: each row is hashed (FNV over 32-bit words, about 1 ms per frame), changed rows are
merged into bands (gaps up to 6 rows are bridged to save display commands) and sent. A full frame is sent
at least every 2 seconds as a safety net. A full frame takes about 22 ms over SPI, so calm screens (menu,
settings) cost very little bus time, which matters because the same bus also serves the CC1101.
`perfRows()` records how many rows were really pushed (`pushPct` in the stream, field `lp`).

**Timing helper.** `msSince(now, t)` (`src/core/timeutil.h`) returns 0 instead of about 4 billion when a
timestamp was taken after `now` (a USB line or a BLE packet handled by another task). Use it instead of
`now - t` on `uint32_t`.

**Performance counters.** `src/core/perf.cpp` accumulates loop and draw times (two `micros()` calls per
pass) and folds them every 2 s into `g_perf`: average and maximum loop time, draw time, fps and the share of
rows pushed. They are exposed in the stream.

## 4. Sections: one table, one menu

The UI has three levels: **main menu** (cards) -> **section submenu** (screens with live numbers) ->
**screen**. Everything in the menu code is generic; a section describes itself with one `Section` struct
(`src/core/section.h`):

| Field | Purpose |
|---|---|
| `title`, `r, g, b`, `icon`, `subtitle` | card title, colour, icon, live caption under the card ("23 devices") |
| `enter()` / `leave()` | section opened from / closed to the main menu: start and stop radios here |
| `nScreens`, `screenInfo(i, item)`, `screenNow()` | submenu rows (0 screens = straight to the screen) |
| `open(i)` / `close()` | submenu -> screen and back |
| `click()`, `rotate(dir)`, `back()` | wheel press, wheel step, side button on a screen; `back()` returns true if the section handled it (for example closed a detail view) |
| `tick(now)` | called every `loop()` pass **only while the section is open** |
| `draw(now)` | draw the current screen |

Optional function pointers may be null (for example Bluetooth has no `tick`).

`src/core/menu.cpp` keeps the state (`s_cur` open section or -1, `s_menuSel`, `s_sub`, `s_subSel`) and
routes input: `navClick`, `navRotate`, `navBack`, `navTick`, `navDraw`. The card order is the array
`SECTIONS[]` (Bluetooth, Wi-Fi, Sub-GHz, Sound, IR, NFC, Web, PC, Gallery, Settings). The main menu shows
5 cards at a time and scrolls the strip with the selection. `navOpen(const Section*)` lets code outside
the menu open a section - the desktop app uses it to open Web when it connects over USB.

**Adding a section** = a new file with a `const Section xxxSection = {...}` table, an `extern` in its
header, and one entry in `SECTIONS[]` in `menu.cpp`.

Input rules worth knowing (`src/core/input.cpp`):

- The wheel is decoded in an interrupt (`encIsr`) with a transition table; four valid transitions make one
  click, bounce yields invalid transitions that cancel out. The main loop only reads an atomic step counter.
- Wheel press: 250 ms lock-out. Side button: a release between 30 ms and `PWR_SHOW_MS` (600 ms) is "back";
  holding past 600 ms shows the power gauge; 2 s (`PWR_OFF_MS`) powers off.
- When the screen is dimmed, the first press or wheel step only wakes it and is not passed to the UI.

On first boot (no language stored) `navClick/Rotate/Draw` are redirected to the language picker
(`g_langAsk`). Settings live in NVS (`Preferences`, namespace `"radar"`) and are saved 3 s after the last
change (`settingsAutosave`) and on power-off.

## 5. Principle: the radio works only inside its own section

TENGEN's radios, microphone and CC1101 are **off in the main menu**. A section turns on what it needs in
`enter()` and turns it off in `leave()`. This saves battery and removes interference: the ESP32-S3 has one
2.4 GHz radio shared by Bluetooth and Wi-Fi.

| Resource | On | Off |
|---|---|---|
| Bluetooth scan | `bleEnter()` -> `bleRadioOn()` | `bleLeave()` -> `bleRadioOff()` |
| Wi-Fi scan / capture | `wifiEnter()` (first turns BLE off: "one antenna shared by both"); packet capture only on screens that need it (Activity, Clients) | `wifiLeave()`: `promOff()`, `wifiRadioOff()` |
| CC1101 | `subEnter()` (init on first use, antenna switch by band) | `subLeave()`: stop 433 MHz receiver, `ccSleep()` (SPWD) |
| Microphone (PDM, I2S0) | Sound section open | Sound section left |
| IR receiver, PN532 | IR / NFC section open | section left |
| LED strip feed | only inside the Bluetooth section (`navActive(&bleSection)`) | `ledsOff()` on leave |

The exception is the **Web section**. It starts the access point (Wi-Fi mode `AP_STA`, channel 6, up to 4
stations) and then decides per client subscription what to run (`webTick()` in `src/web/web.cpp`):

- Bluetooth and Wi-Fi scanning follow the subscribed streams; with no client, only the tab open on the
  stick's own screen matters.
- CC1101, microphone, IR and NFC run while some client asks for them and stop 4 s after the last request
  (a client must keep asking; the USB app repeats `@W` every 2 s as "I am here", and 6 s of silence means
  the app is closed).
- While Web is open Bluetooth scans with a shorter window (`bleLowDuty(true)`, window 45 instead of 99) to
  share airtime with the access point; during an OTA upload Bluetooth is switched off entirely.
- The Wi-Fi network scan under an active AP goes one channel at a time (90 ms per channel) so the AP does
  not disappear for the clients.

TENGEN never transmits: CC1101 is never put into TX (no `STX`), the IR LED pin is held LOW, the PN532 only
reads, Wi-Fi capture is passive and payloads are not read. The only emission is the stick's own access
point while Web is open.

## 6. Power saving, idle mode and power off

- **Auto dim** (Settings, off by default): after 30 s, 1 min or 5 min (`DIM_SECS` in
  `src/core/settings.h`) without wheel or button input, `powerIdleTick()` sets the backlight to a quarter of
  the chosen brightness and the frame interval becomes 4x longer (264 ms). Any input wakes it
  (`powerTouch()`); that first input is swallowed. A tablet can also turn the stick's screen off
  (`g_screenOff`), work continues and any stick input turns it back on.
- **Section gating** (section 5) is the main saver: nothing runs in the main menu except the loop itself,
  battery polling and the LED/count bookkeeping.
- **Dirty-row pushing** (section 3) reduces SPI traffic on calm screens.
- **Power off** (side button, hold 2 s): `powerOff()` saves settings, clears the LEDs, plays a 1.45 s
  shutdown animation, puts the display to sleep (`0x28`, `0x10`), waits for the button to be released, drives
  the backlight and `BOARD_PWR_EN` LOW, latches them with `gpio_hold_en` / `gpio_deep_sleep_hold_en`, enables
  EXT0 wake-up on the side button (GPIO 6, low level) and enters deep sleep. Peripherals are unpowered; only
  the button wakes the stick (and `checkWakeHold()` requires 1 s of holding).
- **Battery** is read-only (`src/core/battery.cpp`): BQ27220 fuel gauge and BQ25896 charger over I2C
  every 2 s; the stick shows percentage, charge state, time to empty/full.

## 7. Black box (crumb) and watchdog

Purpose: after a hang or crash on a device in the field, show **what the firmware was doing** without a
serial cable (`src/core/crumb.cpp`).

- `crumb("name")` copies a short tag (up to 47 characters) into `RTC_NOINIT_ATTR` memory together with a
  magic value (`0x7E6A0001`). RTC no-init RAM survives a software reset (panic, watchdog, `esp_restart`), so
  the tag is still there after the reboot. The call is cheap enough to put before every loop phase and in
  risky operations (`"ble sample"`, `"web http"`, `"ir frame"`, ...).
- `crumbBoot()` runs first in `setup()`: if the magic is valid it copies the last tag, and it translates
  `esp_reset_reason()` into a text: `power on`, `restart`, `panic`, `int watchdog`, `task watchdog`,
  `watchdog`, `brownout`, `wake`, `other`. Panic, the three watchdogs and brownout set `crumbWasCrash()`.
- After a crash the reason and step appear in **Settings > Version** ("last reset: panic @ web http"),
  in `/api/info` (`rst`, `rstStep`) and in the SSE `st` event. A clean start shows only the version.
- **Watchdog.** `crumbWatchdogStart()` reconfigures the ESP-IDF task watchdog to 15 s with
  `trigger_panic = true` and adds the Arduino `loop()` task. `crumbFeed()` (`esp_task_wdt_reset`) is called
  once per loop pass and must be called inside any long operation. A loop stuck for more than 15 s causes a
  panic and reboot; the crumb written just before shows where it hung.
- The watchdog starts at the end of `setup()`, so long start-up work is not covered by it.

## 8. Memory

- Internal RAM is about 320 KB; the board has 8 MB of octal PSRAM and 16 MB flash (see
  [02-hardware.md](02-hardware.md)).
- Big buffers go to PSRAM through `psAlloc<T>(n)` (`src/core/psram.h`): `heap_caps_calloc` with
  `MALLOC_CAP_SPIRAM`, falling back to the normal heap when there is no PSRAM (and in the PC simulator).
  Used for: the USB write ring (64 KB), the Sub-GHz waterfall, the sound terrain, the gallery buffers.
  Buffers are allocated on first use of a screen and kept.
- The frame sprite is created by TFT_eSPI, which itself prefers PSRAM when it is available and DMA is not
  enabled; if allocation fails the code falls back to an 8-bit sprite.
- The stick reports free heap and PSRAM (`/api/info`, `heap`, `psram` in KB).
- Strings are bilingual: `L("English", "Русский")` (`src/core/lang.h`) picks one at run time by
  `g_lang`, so one firmware serves both languages.

## 9. Repository layout

```
VERSION                 single version for firmware, page, installer, apps (e.g. 2.0-test6)
platformio.ini          build: env t-embed-cc1101, TFT_eSPI pins/flags, -O2, libraries
boards/T_Embed_PN532.json   board profile (16 MB flash, qio_opi PSRAM, USB CDC)
src/main.cpp            setup() and loop()
src/core/               menu, section model, display/theme/UI primitives, input, power, battery,
                        settings (NVS), LEDs, crumb (black box), perf, boot animation, fonts
src/ble/                Bluetooth: scan core, types/vendors databases, "City" screen
src/wifi/               Wi-Fi: scan, channels, activity, clients, OUI database
src/subghz/             CC1101 driver, spectrum/waterfall/monitor, OOK decoders and 433 MHz sensors
src/probe/              IR receiver (RMT) and NFC (PN532), read only
src/audio/              microphone (PDM, FFT), speaker (UI sounds), Sound screens
src/web/                Web section: access point, HTTP API, SSE stream, OTA, USB app link, phone page
src/link/               USB output queue (serout)
src/pc/                 PC section: lines from the PC monitor, media control, process "city"
src/gallery/            Gallery, sky/planetarium, satellites (SGP4), radio weather
tools/webapp/           tablet page sources (HTML/CSS/JS), build.py -> src/web/tablet_page.h, mock server
tools/desktop/          TENGEN Desktop (Python, pywebview), widget/overlay, builders
tools/pc_monitor/       stand-alone laptop monitor script
tools/flasher/          single-file browser installer (esptool-js)
tools/sim/              PC simulator: compiles the real drawing code and renders screens to images
tools/ook_test/         tests for the OOK decoders
tools/gen_*.py          generators for databases and data headers (btdb, ouidb, astro, seal, fonts)
app/                    TENGEN Tablet: Capacitor wrapper for Android
.github/workflows/      CI: firmware + installer, APK, desktop builds, releases
docs/                   screenshots, README assets; docs/tech/ = this documentation (en/, ru/)
```

Generated files (`tablet_page.h`, `ouidb.h`, `btdb.h`, `astrodata.h`, `core/fonts/*.h`, `core/seal.h`)
are produced by the scripts in `tools/` and should be regenerated, not edited by hand.
