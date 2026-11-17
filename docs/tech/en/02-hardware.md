# Hardware and pins

Target board: **LilyGO T-Embed CC1101 Plus** (the non-Plus T-Embed CC1101 works too, without the NFC
section). The firmware is built for the board profile `boards/T_Embed_PN532.json` (taken from the LilyGO
repository, name "LilyGo T-Embed-PN532 (16M Flash 8M QSPI PSRAM)"). Pin numbers come from LilyGO's
`utilities.h`; in this repository they live in `src/core/board.h` (shared pins), in
`platformio.ini` (display) and next to each driver (everything else).

## 1. Platform summary

| Item | Value | Source |
|---|---|---|
| MCU | ESP32-S3, 240 MHz | `boards/T_Embed_PN532.json` (`f_cpu`) |
| Flash | 16 MB, QIO 80 MHz | board profile |
| PSRAM | 8 MB, octal (`memory_type: qio_opi`), `-DBOARD_HAS_PSRAM` | board profile, `src/core/psram.h` |
| Internal RAM | about 320 KB (`maximum_ram_size` = 327680) | board profile |
| Partitions | `default_16MB.csv`: `nvs` 0x5000, `otadata` 0x2000, two app slots `app0`/`app1` of 0x640000 (6.25 MB) each, `spiffs`, `coredump` | board profile; Arduino core table |
| Arduino task core | 0 (`-DARDUINO_RUNNING_CORE=0`) | board profile |
| USB | native USB-CDC on boot (`-DARDUINO_USB_CDC_ON_BOOT=1`, `-DARDUINO_USB_MODE=1`) (the `Serial.begin(115200)` value is nominal for CDC) | `platformio.ini`, `main.cpp` |
| Build | `-O2` (the default `-Os` is removed; flash has headroom), about 36 % flash used at the time the option was chosen | `platformio.ini` |

The board profile is used instead of `feather-s3`: the Feather profile drives GPIO 21 (the screen backlight)
before `setup()`, which gives a red flash on start.

## 2. Pin map

All GPIO numbers are ESP32-S3 GPIO numbers.

| GPIO | Signal | Peripheral | Defined in |
|---|---|---|---|
| 0 | `PIN_ENC_KEY` | wheel press (active LOW, pull-up) | `core/board.h` |
| 1 | `IR_RX_PIN` | IR receiver | `probe/ir.cpp` |
| 2 | `IR_TX_PIN` | IR LED (held LOW, never used for transmitting) | `probe/ir.cpp` |
| 3 | `CC_GDO0` | CC1101 GDO0: demodulated OOK data (input) | `subghz/sens_rx.cpp` |
| 4 | `PIN_ENC_A` | wheel phase A | `core/board.h` |
| 5 | `PIN_ENC_B` | wheel phase B | `core/board.h` |
| 6 | `PIN_USER_KEY` | side button (active LOW); also the deep-sleep wake-up pin | `core/board.h`, `core/power.cpp` |
| 7 | `SPK_DIN` | speaker amplifier data (I2S1 DOUT) | `audio/audio.cpp` |
| 8 | `BAT_I2C_SDA` | I2C SDA: BQ27220, BQ25896, PN532 | `core/battery.cpp` |
| 9 | `TFT_MOSI` | shared SPI MOSI | `platformio.ini` |
| 10 | `TFT_MISO` = `CC_MISO` | shared SPI MISO | `platformio.ini`, `subghz/cc1101.h` |
| 11 | `TFT_SCLK` | shared SPI clock | `platformio.ini` |
| 12 | `PIN_LORA_CS` = `CC_CS` | CC1101 chip select | `core/board.h`, `subghz/cc1101.h` |
| 13 | `PIN_SD_CS` | SD card chip select (only deselected, not used) | `core/board.h` |
| 14 | `PIN_WS2812` | LED strip data (8 x WS2812, `WS2812_N`) | `core/board.h` |
| 15 | `BOARD_PWR_EN` | peripheral power enable (CC1101, LEDs, ...) | `core/board.h` |
| 16 | `TFT_DC` | display data/command | `platformio.ini` |
| 18 | `BAT_I2C_SCL` | I2C SCL | `core/battery.cpp` |
| 21 | `TFT_BL` | display backlight (PWM via `analogWrite`) | `platformio.ini` |
| 39 | `MIC_CLK` | PDM microphone clock | `audio/audio.cpp` |
| 40 | `SPK_LRCLK` | speaker I2S word select | `audio/audio.cpp` |
| 41 | `PIN_TFT_CS` = `TFT_CS` | display chip select | `core/board.h`, `platformio.ini` |
| 42 | `MIC_DATA` | PDM microphone data | `audio/audio.cpp` |
| 44 | `PIN_NRF_CS` | nRF24 chip select on the board (only deselected, not used) | `core/board.h` |
| 45 | `PN_RST` | PN532 reset / sleep (LOW = asleep) | `probe/nfc.cpp` |
| 46 | `SPK_BCLK` | speaker I2S bit clock | `audio/audio.cpp` |
| 47 | `CC_SW1` | CC1101 antenna band switch | `subghz/cc1101.h` |
| 48 | `CC_SW0` | CC1101 antenna band switch | `subghz/cc1101.h` |

`TFT_RST` is -1 (not connected, software reset). `USB D+/D-` (GPIO 19/20) are the native USB port.

## 3. Peripherals

### 3.1 Display (ST7789, 320x170)

- Driver: TFT_eSPI, configured entirely through `build_flags` in `platformio.ini` (`USER_SETUP_LOADED=1`,
  `ST7789_DRIVER`, `TFT_WIDTH=170`, `TFT_HEIGHT=320`, `TFT_INVERSION_ON=1`, `USE_HSPI_PORT=1`). Rotation 3
  gives a 320x170 landscape screen.
- SPI clock 40 MHz for writes, 20 MHz for reads (`SPI_FREQUENCY`, `SPI_READ_FREQUENCY`).
- Rendering model: a full-frame 16-bit sprite (320 x 170 x 2 = 108 800 bytes), pushed to the panel only for
  the rows that changed (see the overview, section 3). `displayBegin()` falls back to an 8-bit sprite if
  memory is short.
- Backlight: GPIO 21 is kept LOW during early start (`initVariant()` and a constructor of priority 101 in
  `main.cpp`) and set HIGH only after the first clear, so display RAM garbage is never visible.
  Brightness is PWM with `analogWrite(TFT_BL, ...)`; dimmed mode uses a quarter of the set value.
- Fonts: bitmap fonts generated into `src/core/fonts/*.h` (JBM, OSW, EXO) plus TFT_eSPI built-ins
  (`LOAD_GLCD`, `LOAD_FONT2/4/6/7/8`, `LOAD_GFXFF`, `SMOOTH_FONT`).

### 3.2 CC1101 (Sub-GHz, receive only)

- Chip select GPIO 12, SPI at 4 MHz, mode 0 (`src/subghz/cc1101.cpp`).
- GDO0 (GPIO 3) carries the demodulated signal in asynchronous OOK mode; an interrupt records every edge
  (`src/subghz/sens_rx.cpp`). GDO0 is configured as high impedance (`IOCFG0 = 0x2E`) whenever it is not
  needed, as are GDO1/GDO2 (GDO1 shares the MISO pin).
- Antenna switch `CC_SW1`/`CC_SW0` selects the filter path by frequency (`ccAntenna()`):

  | Frequency | SW1 | SW0 |
  |---|---|---|
  | below 350 MHz (315) | 1 | 0 |
  | 350..500 MHz (433) | 1 | 1 |
  | above 500 MHz (868 / 915) | 0 | 1 |

- Bands in `src/subghz/subghz_model.h`: 315 (313.0-317.0), 433 (431.92-435.92, monitor 433.92), 868
  (866.3-870.3, monitor 868.30), 915 (913.0-917.0) MHz. Frequency word = `f * 65536 / 26` (26 MHz crystal).
- The chip is accepted if `PARTNUM = 0x00` and `VERSION` is 0x14, 0x04 or 0x07. If not, the section shows
  "no chip".
- Receive only: the code never sends the `STX` strobe; `ccSleep()` (SIDLE then SPWD) is used when the
  section is left.
- Power: the CC1101 is supplied through `BOARD_PWR_EN` (GPIO 15).

### 3.3 IR receiver

- GPIO 1, captured by the ESP32 RMT peripheral at 1 us resolution (`rmtInit(..., 1000000)`), 4 memory
  blocks (192 symbols) with a fallback to 2 blocks.
- Glitch filter: pulses shorter than 3 us are noise; 12 ms of silence ends a burst.
- The IR LED on GPIO 2 is driven LOW and never modulated: TENGAN does not transmit IR.

### 3.4 PN532 (NFC, Plus version only)

- I2C address `0x24`, on the same I2C bus as the battery chips (SDA 8, SCL 18, 100 kHz).
- Reset/sleep line on GPIO 45: pulsed LOW then HIGH to start the chip, LOW to put it to sleep when the
  section is left or the chip disappears from the bus. No IRQ pin is used; readiness is polled over I2C
  (status byte, bit 0).
- A minimal own driver (`src/probe/nfc.cpp`) uses the PN532 information frame (preamble, length, checksums);
  polling is non-blocking: send `InListPassiveTarget`, return to the main loop, read the answer when ready.
- Read only: for payment cards only a `SELECT PPSE` is issued and the answer is discarded.

### 3.5 Microphone and speaker

| | Microphone | Speaker |
|---|---|---|
| Interface | PDM on I2S0 (PDM RX works only on I2S0) | standard I2S (Philips) on I2S1, mono |
| Pins | CLK 39, DATA 42 | BCLK 46, LRCLK 40, DOUT 7 |
| Rate | 16 kHz, 16 bit, mono | 16 kHz, 16 bit, mono |
| Processing | task on core 1 reads 512-sample blocks (32 ms), computes level and FFT | task on core 1 plays short tones from a queue of 8 |

- The microphone runs only while the Sound section is open (or a client subscribed to sound). If the left
  channel is silent (peak below 8 over the first 1.5 s) the code restarts the stream on the right channel
  (a self-check: the PDM data may sit in either slot).
- Sound output is limited to UI feedback: wheel click (2.6 kHz, 6 ms), a "Geiger counter" in the search
  screens, beeps. Volume is set by Settings > Sounds: off / quiet / loud (amplitude 2500 / 9000 of 32767).
  Nothing is recorded.

### 3.6 LED strip (WS2812 x 8)

- GPIO 14, `Adafruit_NeoPixel(8, 14, NEO_GRB + NEO_KHZ800)`, powered through `BOARD_PWR_EN`.
- Used for events in the Bluetooth section (a green wave on "someone arrived", an orange flash on "someone
  left", 1.2 s) and as a loudness meter in Sound (off by default). The strip is cleared on leaving a
  section. Brightness is Settings > LED brightness.

### 3.7 Wheel and buttons

- Wheel phases A (GPIO 4) and B (GPIO 5), wheel press on GPIO 0, side button on GPIO 6, all active LOW with
  internal pull-ups.
- A and B are decoded in an interrupt on both edges: the ISR reads `GPIO_IN_REG` directly, looks up the
  transition in a 16-entry table and accumulates; 4 transitions equal one detent. This is bounce-proof
  without timers.
- GPIO 6 is also an RTC pin used as the deep-sleep wake source (EXT0, low level), see
  [01-overview.md](01-overview.md), section 6.
- GPIO 0 is also the ESP32-S3 strapping pin (download mode if held LOW at reset); do not hold the wheel while
  resetting.

### 3.8 Power: BQ27220 and BQ25896

Both chips are on I2C (SDA 8, SCL 18, `Wire` at 100 kHz) and are only read; the firmware never configures
them (`src/core/battery.cpp`). A read happens every 2 s.

| Chip | Address | Registers read | Used for |
|---|---|---|---|
| BQ27220 fuel gauge | `0x55` | `0x2C` state of charge (%), `0x08` voltage (mV), `0x0C` current (mA, signed), `0x06` temperature (0.1 K), `0x16` time to empty, `0x18` time to full, `0x10` remaining and `0x12` full capacity (mAh) | percentage on the main menu, runtime and charge estimates, Settings summary |
| BQ25896 charger | `0x6B` | `0x0B` status: bits 7:5 power source (0 = no USB), bits 4:3 charge state (0 idle, 1 precharge, 2 fast charge, 3 done) | "USB power", "charging", "charged" |

A value of 0xFFFF in the time registers means unknown. If the fuel gauge does not answer, `batSummary()`
reports "battery gauge not found" and the percentage is hidden.

`BOARD_PWR_EN` (GPIO 15) must be HIGH for the peripherals above (it is raised in `setup()`); in power-off it
is driven LOW and latched with `gpio_hold_en` through deep sleep.

## 4. The shared SPI bus

The display and the CC1101 sit on the same three lines (SCLK 11, MOSI 9, MISO 10) and differ only by chip
select (display GPIO 41, CC1101 GPIO 12). The SD card (GPIO 13) and the nRF24 (GPIO 44) on the board also
share the bus; TENGAN does not use them but drives their CS lines HIGH at startup so they never respond.

Rules that make sharing work:

1. **All CS lines are raised before anything else** (`setup()`: `PIN_TFT_CS`, `PIN_SD_CS`, `PIN_LORA_CS`,
   `PIN_NRF_CS` HIGH), so no device listens to a transfer that is not meant for it.
2. **One SPI object.** The CC1101 driver does not create its own `SPIClass`; it uses the display library's
   instance (`g_ccSpi = &tft.getSPIinstance()`), so both go through the same hardware peripheral and
   settings are not fought over.
3. **Read mode.** TFT_eSPI configures the SPI peripheral for writing only (no MISO). Before every CC1101
   transaction the driver calls `SET_BUS_READ_MODE` (a TFT_eSPI macro), and the display library restores its
   own mode at the next screen transaction. Without it, CC1101 reads return garbage.
4. **Own transaction settings.** CC1101 transfers use `SPISettings(4 MHz, MSBFIRST, SPI_MODE0)` inside
   `beginTransaction/endTransaction`; the display stays at 40 MHz.
5. **Chip-ready wait.** After CS goes LOW the driver waits for MISO to fall (CC1101 "CHIP_RDYn") for up to
   3 ms before clocking; on timeout the transfer is skipped. MISO is the line that the display also owns
   (read-back), which is why GDO1 is kept high-impedance.
6. **Everything runs on the main loop task.** Display pushes and CC1101 accesses happen from `loop()`
   (the Sub-GHz section ticks in `navTick`), so no lock is needed. Only the edge capture of GDO0 is an
   interrupt (it does not use SPI).
7. **Bus time budget.** A full frame is about 22 ms of bus time; that is why only changed rows are pushed
   and why the Sub-GHz sweep measures a limited number of points per loop pass (12 points, about 15 ms).

## 5. I2C bus

One bus, `Wire` on GPIO 8/18 at 100 kHz, started once in `batBegin()`. Three devices: BQ27220 (`0x55`),
BQ25896 (`0x6B`) and PN532 (`0x24`). 100 kHz is chosen for margin on this shared bus; the PN532 is polled
with short reads, so battery reads (every 2 s) and NFC polling interleave without conflict because both
run on the main loop.

## 6. Memory budget in practice

| Consumer | Where | Size |
|---|---|---|
| frame sprite | PSRAM (TFT_eSPI, when PSRAM is present and DMA is off) | 108 800 bytes |
| USB output ring | PSRAM (`psAlloc`) | 64 KB |
| Sub-GHz waterfall | PSRAM | `WF_ROWS` x `SP_N` = 56 x 96 bytes |
| Sound terrain, gallery buffers (torus, Life) | PSRAM | allocated on first use of the screen |
| Bluetooth devices, Wi-Fi networks, strings | internal heap | bounded by lists (for example 150 devices in the JSON, 80 in the stream) |
| Firmware image | flash | OTA image 2.69 MB at v2.0-test5; CI budget 2 830 000 bytes |

Large arrays that are not `const` end up in `.data`/`.bss` in internal RAM, so they are moved to PSRAM with
`psAlloc`. Free heap and PSRAM are reported in `/api/info` (KB).
