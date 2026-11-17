# Radio sections: algorithms

This part describes how the radio sections of TENGAN turn raw radio events into what the screen shows. Everything here is **receive only**: Bluetooth never connects, Wi-Fi never transmits (passive scan, promiscuous listening), the CC1101 is never put into transmit (the `STX` strobe is not used), the IR LED (GPIO 2) is held low, and NFC only reads what a card announces to everybody.

| Section | Code | Hardware | Data in | Data out |
|---|---|---|---|---|
| Bluetooth | `src/ble/` | ESP32-S3 radio, NimBLE | advertising packets | physical devices, types, vendors, weather sensors |
| Wi-Fi | `src/wifi/` | ESP32-S3 radio, 2.4 GHz only | beacons, packet headers | networks, channel load, clients, signal finder |
| Sub-GHz | `src/subghz/` | CC1101 on the shared SPI bus | RSSI, demodulated OOK on GPIO 3 | spectrum, bursts, 433 MHz sensors |
| IR | `src/probe/ir*` | IR receiver (GPIO 1) via RMT | mark/space durations, µs | protocol, address, command, brand |
| NFC | `src/probe/nfc*` | PN532 over I2C (0x24) | ATQA/SAK/UID, PPSE and NDEF replies | card family, masked UID, NDEF records |

General rules shared by all sections:

- **One radio at a time.** Bluetooth and Wi-Fi share one antenna: entering Bluetooth calls `wifiRadioOff()`, entering Wi-Fi calls `bleRadioOff()` (`bleEnter` in `src/ble/ble_core.cpp`, `wifiEnter` in `src/wifi/wifi.cpp`). Leaving a section switches its radio off. IR, NFC and the CC1101 run only while their section is open or the tablet is subscribed to them (`src/web/web.cpp`).
- **Raw data are written by a callback or ISR, the main loop reads a snapshot.** Bluetooth uses a mutex, Wi-Fi uses atomics and a ring buffer, Sub-GHz and IR use a ring buffer / RMT.
- **Display values are "calm".** Raw RSSI jitters by several dB, so most screens show smoothed or hysteresis-filtered values (details below). Sorting and filtering use the smoothed value, the display uses a steadier one.

## Bluetooth (`src/ble/`)

### Files

| File | Role |
|---|---|
| `ble_core.cpp` | scanner callback, address merging, range filter, list order, history, JSON for the web page |
| `ble_model.h` | structures (`Dev`, `Ent`, `Sensor`) and constants |
| `devtypes.h` | advertisement parser, device type classification, fingerprint |
| `vendors.cpp`, `vendors.h` | vendor lookup (Company ID / service UUID), brand table |
| `btdb.h` | generated database (`tools/gen_btdb.py`): Company IDs and 16-bit UUID owners; included only from `vendors.cpp` |
| `sensors.h` | parsers of weather-sensor advertisements |
| `ble_screens.cpp`, `ble_city.cpp` | screens: Overview, Scope, City, Nearest, Devices, Weather |

### Scanning

`bleBegin()` initialises NimBLE with an active scan (`setActiveScan(true)`), interval 100, window 99 (almost continuous listening), callback for every packet (`setScanCallbacks(cb, true)`), `setMaxResults(0)` so NimBLE stores nothing and all processing happens in `ScanCB::onResult`. Scanning starts only when the section is entered (`bleRadioOn`). In web mode `bleLowDuty(true)` narrows the window to 45 so Wi-Fi gets more air.

### Advertisement parsing

`parseAdv()` walks the `[len][type][data]` records and extracts: 16-bit service UUIDs (types 0x02/0x03 and the UUID in service data 0x16, up to 8), name (0x08/0x09, complete name wins), Appearance (0x19), manufacturer data (0xFF: Company ID and the first 8 bytes plus total length).

### Device type

`classify()` returns one of: phone, computer, audio, watch, TV, tracker, beacon, input, sensor, unknown. Checks go in strict order, first hit wins:

1. tracker UUIDs (Tile `FEED`/`FEEC`, Samsung SmartTag `FD5A`, Chipolo `FE33`);
2. Appearance, by category (`value >> 6`);
3. name keywords (order matters: tracker, audio, watch, TV, input, sensor, computer, phone, so "Galaxy Buds" is audio, not a phone);
4. service UUIDs (Eddystone, Fast Pair, LE Audio, HID, heart rate, environmental sensing, BTHome, Xiaomi);
5. manufacturer data: for Apple (Company ID `0x004C`) the first byte is the message type (iBeacon, AirPods proximity pairing, AirPlay, Apple Watch, HomeKit, Find My, Nearby/Handoff). For Find My the status byte distinguishes AirTag/third-party tag, AirPods and phones. Also Windows (`0x0006`), Garmin, Huami, Bose, Harman.

If the packet is a recognised weather-sensor format, the type is forced to "sensor". The type of an address is only updated by a known (non-unknown) result, so one packet without a name does not erase the type.

### Vendor

`resolveVendor()` considers the Company ID and the owner of every non-shared service UUID (`btdb.h`). Each candidate gets a confidence score:

| Score | Meaning |
|---|---|
| 3 | a brand from the `BRANDS` table (Apple, Microsoft, Samsung, Google, Xiaomi, ...), matched by the first word of the name |
| 2 | another named company |
| 1 | a Bluetooth chip maker (`isChipMaker`: the code names the silicon, not the product) or an unnamed code |

The best candidate is kept. UUIDs used by any brand (`FD6F` contact tracing, `FEAA` Eddystone, `FE2C` Fast Pair) are ignored. The label shown is the brand, else the company name, else `0xXXXX`. The database is generated, not edited by hand.

### Address kinds and merging rotating MACs

Phones and many accessories periodically change their random address, so counting MACs overcounts devices. TENGAN groups addresses into one **physical device** (`Ent`).

Address kind comes from the type bit and the two top bits of the address: public; random static (`11`); resolvable private, i.e. rotating (`01`); non-resolvable (`00`). Rotating and non-resolvable ones are candidates for merging (`rnd`).

**Fingerprint** (`advSignature`): an FNV-1a hash over what stays constant between packets of one device: Company ID, manufacturer data length and its first byte (message type), sorted service UUIDs, Appearance, name. An empty advertisement gives 0 and is never merged.

**Merge rule.** When a new address with a non-zero fingerprint appears, the code looks for a predecessor among already known addresses that:

- is also random (`rnd`) and has not already been merged into something;
- has the same fingerprint;
- has been silent for `STALE_MIN`..`STALE_MAX` = 1.5..12 s (not silent yet means it is probably another device; silent too long means it left);
- has the closest RSSI, within `MERGE_DB` = 10 dB.

If found, the new address inherits the physical id (`ent`), first-seen time, packet count, min/max RSSI, and, if missing, type, vendor and name; the old one is marked `merged` and no longer takes part in merging. Otherwise a new physical id is allocated. Limitations: devices with identical fingerprints near each other with similar RSSI can be mistaken for one another; a device that was silent for more than 12 s is counted as new; an empty advertisement cannot be merged. The number of raw MACs is shown next to the number of devices ("MAC" in Overview, `macs` in JSON).

### Range filter and hysteresis

The Bluetooth "Range" setting is a threshold on the smoothed RSSI (`src/core/settings.h`):

| Name | Threshold, dBm |
|---|---|
| all | -127 |
| far | -90 |
| room | -80 |
| near | -70 |
| very near | -60 |

Smoothed RSSI: `rssiS = (3·rssiS + rssi) / 4`. An address enters the "in range" state when `rssiS >= threshold` and leaves only when `rssiS < threshold - 5` (`RANGE_HYST_DB`), so a device at the edge does not blink. Addresses with fewer than `MIN_PKTS` = 2 packets are dropped (single stray packets). Silent addresses are forgotten after `REMEMBER_MS` = 30 s; at most `MAX_DEVS` = 600 addresses are held. Every frame `bleUpdate()` rebuilds the physical-device map from the addresses that pass the filter: packets = max, first = earliest, RSSI = from the freshest address, `nMacs` = number of addresses.

### "Calm" signal and the nearest device

Per physical device the displayed RSSI follows `steadyRssi()`: a target value that moves only when the smoothed signal differs from it by 4 dB (`STEADY_DB`); the shown value eases to the target with time constant 0.7 s. The "nearest" device changes only if another one is stronger by 3 dB (`BEST_HYST_DB`). Distance (`estDistance`, `src/ble/ble_screens.cpp`) uses the log-distance model with 1 m reference -59 dBm and exponent 2.5, clamped to 0.1..99 m. It is a rough hint, not a measurement.

### List order

The device list is "sticky": nearest on top, but a pair of neighbours swaps only if the lower one is stronger by more than `ORDER_HYST_DB` = 8 dB; the order is recomputed every `ORDER_EVERY` = 2 s and frozen while the details are open or for `ORDER_FREEZE` = 4 s after a wheel move. New devices are inserted at their place, vanished ones removed.

### History and the LED event detector

`bleSample()` runs once a second and stores the physical device count in a 150-sample ring (`HIST_N`, 2.5 minutes). A baseline follows the count with an exponential average (coefficient 0.15). If the count differs from the baseline by at least the "LED event" threshold (`g_thresh`, default 7, 0 = off), an event +1 (group arrived) or -1 (group left) is raised, the baseline jumps to the current count and a 3 s cooldown starts. After the radio is switched on the baseline is re-learned and events are muted for 10 s. LED feedback is sent only inside the Bluetooth section.

### Labels

On the tablet a device can be given a name (up to 20 bytes, cut on a UTF-8 boundary). Labels are stored per MAC in NVS namespace `labels` (up to 32, the oldest is evicted). Because a rotating MAC changes, the API reports `stable=false` for such devices: the label lasts until the address changes.

### Weather sensors (`sensors.h`)

`parseSensor()` scans the raw packet and tries all formats. Packets are not encrypted and are readable by anyone:

| Format | Where | Fields |
|---|---|---|
| PVVX (15 bytes) / ATC1441 (13 bytes) | service data `181A` | temperature (PVVX s16 LE /100, ATC s16 BE /10), humidity, battery |
| BTHome v2 | service data `FCD2` | objects by id: battery `01`, temperature `02`/`45`, humidity `03`/`2E`, pressure `04`, CO2 `12`; encrypted packets (bit 0 of the info byte) and other versions are skipped |
| Xiaomi MiBeacon | `FE95` | unencrypted only; objects `1004` temperature, `1006` humidity, `100A` battery, `100D` both |
| Qingping | `FDCD` | temperature+humidity, battery, pressure, CO2 |
| SwitchBot Meter | `FD3D` | models `T`, `i`, `w`: battery, temperature, humidity |
| Govee | manufacturer `0xEC88` | 24-bit packed value: temperature and humidity; accepted only if -40 < T < 80 |
| RuuviTag format 5 | manufacturer `0x0499` | temperature, humidity, pressure; "not available" markers are skipped |

The latest values are kept per address (up to `MAX_SENSORS` = 24) for `SENSOR_REMEMBER_MS` = 5 minutes, because sensors transmit rarely. They are shown on the Weather screen and sent to the web page (`sens` in `/api/ble`).

### Web output

`bleJson` (`/api/ble`, up to 150 devices), `bleStreamJson` (SSE stream for the tablet, up to 80 devices as arrays) and `bleDevJson` (`/api/bledev`, all addresses of one device with kind, RSSI min/max, Company ID, UUIDs with their owners, first manufacturer bytes) read the same snapshot.

## Wi-Fi (`src/wifi/`)

The ESP32-S3 has a 2.4 GHz radio only: 5 GHz networks are invisible. Channels 1..13 (`CH_MAX`) are used.

### Screens and radio modes

| Screen | Radio mode |
|---|---|
| Networks (list + details) | passive scan, all channels |
| Channels | passive scan, all channels |
| Activity | promiscuous, hopping |
| Clients | promiscuous, hopping |
| Finder (FIND) | passive scan on one channel |

Scan and promiscuous modes are mutually exclusive: `wifiSetMode` calls `promOn()` for Activity/Clients (stops the scan first) and `promOff()` otherwise. Switching into or out of FIND cancels a running scan because all-channel and single-channel scans cannot be mixed. In the section submenu (before opening a screen) a plain all-channel scan runs to keep the numbers live.

### Passive scan

`WiFi.scanNetworks(async=true, show_hidden=true, passive=true, ms_per_channel, channel)`: nothing is transmitted, the radio only listens to beacons. The full pass uses 150 ms per channel (about 2 s for 13 channels); FIND uses 110 ms on the target's channel only. Scans are chained: when one finishes, the next starts (on failure, retry after 500 ms or 1 s).

For every result (keyed by BSSID) the code stores: SSID (hidden = empty), channel, auth mode, secondary channel (`sec`: +1 above, -1 below, 0 = plain 20 MHz), generation (`phy_11ax` = Wi-Fi 6, `phy_11n` = Wi-Fi 4, `phy_11g`, else 11b), vendor (by OUI), min/max RSSI, smoothed RSSI `rssiS = (rssiS + rssi) / 2`. The displayed `rssi` is steadied: it follows `rssiS` only when they differ by 3 dB (`WIFI_STEADY_DB`). Networks not heard for `AP_REMEMBER_MS` = 60 s are dropped after a full pass.

**Vendor by OUI**: first 3 bytes of the BSSID, binary search in the generated table `src/wifi/ouidb.h` (`tools/gen_ouidb.py`, IEEE data via Nmap).

**Phone hotspot** (`looksLikeHotspot`): SSID contains a keyword (iphone, galaxy, redmi, pixel, hotspot, ...) or the OUI vendor is Apple, Samsung, Xiaomi, Motorola or OnePlus. The screen marks it "PHONE". Open networks are marked "OPEN".

### List order

Same idea as Bluetooth: sticky order, swap only if the difference exceeds 6 dB; frozen while details are open or for 4 s after a wheel move. The Wi-Fi "Range" setting hides networks whose `rssiS` is below its threshold (the same dBm table as in Bluetooth; the selected network stays). The signal history of the opened network gets one sample per full scan (60 points).

### Channels and recommendation

Each network occupies a band centred on `ch + 2·sec` channel numbers (a 40 MHz network is centred between two channels) with a half-width of 2 (20 MHz) or 4 (40 MHz). The interference of network *a* with channel *k* is linear:

```
d     = |center(a) - k|
reach = half(a) + 2 + 1          // + our own 20 MHz (±2) + 1 channel margin
I     = d >= reach ? 0 : 1 - d / reach
```

`bestChannel()` evaluates the three non-overlapping channels 1, 6 and 11. For each, the score is `sum(I · (rssiS + 100))` over all known networks; the lowest score wins. Stronger networks weigh more. The result is only shown (as "best N"); the device never changes anything. Limitation: only beacons the scan has seen are counted; traffic load is not part of the score.

### Activity (promiscuous mode)

`promOn()` stops the scan, sets the filter to management and data frames, registers `promCb` and starts on channel 1 (`WIFI_SECOND_CHAN_NONE`). The handler (runs in the Wi-Fi task, in IRAM) only:

- counts management and data frames (atomic counters);
- accumulates the noise floor reported by the radio (accepted if between -120 and -20 dBm);
- from frame headers (never from payloads) pushes events into a 128-slot ring: a data frame with ToDS/FromDS gives a (device, network) pair; a probe request gives the source of a device that is searching for Wi-Fi. Frames shorter than 24 bytes and group addresses are ignored.

The main loop (`wifiHopTick`) hops to the next channel every `HOP_MS` = 200 ms (a full 13-channel cycle takes about 2.6 s). On each hop it converts the packet counts for the channel it just left into per-second rates (`pps`, smoothed 0.4/0.6 old/new, separately for management and data) and noise (average, smoothed 0.5/0.5). Once a second the total pps over all channels is appended to a 120-sample history (2 minutes). The screen warns when noise on a channel is above -86 dBm (normally -90..-95). Because only one channel is listened to at a time, the numbers for other channels are the last measured values, not simultaneous.

### Clients

Devices seen in data frames are kept as (device MAC → network BSSID, time) up to `STA_MAX` = 500, forgotten after `STA_REMEMBER_MS` = 120 s. Probe-request sources are kept separately as "looking for Wi-Fi". Only counts are shown; MACs are not shown and payloads are not read. Randomised phone MACs are counted as separate devices. Per-network client counts (`clientsOf`) only include devices heard on the channel while hopping, so they are lower bounds.

### FIND

The target is the selected network (or the strongest if none). Scan is restricted to its channel (110 ms), so the result arrives several times a second. Each result adds a point (100-point history); a miss adds -127. The screen shows the mean of the last 5 samples, converts it to a 0..100 % gauge (`(rssi + 95) · 100 / 60`), a word (EXCELLENT above -55, GOOD above -67, FAIR above -75, WEAK above -85, BAD below) and a trend: the difference between the last 5 and the previous 5 samples (±3 dB gives "warmer" / "colder"). "NO SIGNAL" appears with no hits for 4 s. The distance estimate uses a different reference (`10^((-45 - rssi) / 25)` m). Optional Geiger-style ticks speed up with the gauge (`g_wifiTicks`), and the LED strip shows the level. A click selects the next network as the target.

## Sub-GHz (`src/subghz/`)

### CC1101 driver (`cc1101.cpp`)

Minimal and receive only. The CC1101 shares the SPI bus with the display (4 MHz, mode 0; the display library configures SPI for writing only, so each transaction first calls `SET_BUS_READ_MODE`). Pins: CS 12, MISO 10, antenna switch SW1 47 / SW0 48, GDO0 3 (`src/subghz/cc1101.h`, `sens_rx.cpp`).

- `ccInit`: reset (`SRES`), checks the chip (PARTNUM `0x00`, VERSION `0x14`, `0x04` or `0x07`); on failure the section shows "no chip". Then loads a register table: ASK/OOK, no packets, GDO pins high-impedance, 58 kHz channel filter (`MDMCFG4 = 0xF5`).
- RSSI: register `0x34`, converted as `(signed)/2 - 74` dBm.
- Frequency: `FREQ = mhz · 65536 / 26` (26 MHz crystal); `TEST0` is switched between `0x0B` and `0x09` depending on the sub-band (as in the ELECHOUSE library).
- Antenna switch by frequency: below 350 MHz = 315 path (SW1/SW0 = 1/0), below 500 MHz = 433 path (1/1), above = 868/915 path (0/1).
- `ccMeasure(f)`: idle, set frequency, `SRX` (with auto-calibration), wait up to 2 ms for the RX state, wait 250 µs for RSSI to settle, read.
- `ccSleep` (`SPWD`) when the section is left.

### Bands

| Band | Sweep | Monitor frequency |
|---|---|---|
| 315 MHz | 313.0..317.0 | 315.00 |
| 433 MHz | 431.92..435.92 | 433.92 |
| 868 MHz | 866.3..870.3 | 868.30 |
| 915 MHz | 913.0..917.0 | 915.00 |

A click on the wheel switches to the next band (the band is saved in settings). Default is 433 MHz.

### Spectrum and waterfall

The band is divided into `SP_N` = 96 points (4 MHz span, about 42 kHz step). Each main-loop pass measures 12 points (about 15 ms), so one sweep takes roughly 8 passes. RSSI is clamped to -120..-10 dBm. `g_spCur` holds the latest value; `g_spPeak` the peak, which decays by 1 dB per sweep. At the end of each sweep a waterfall row is written (`dBm + 120`, 0..110) into a ring of `WF_ROWS` = 56 rows allocated in external RAM (`psAlloc`). In this mode the filter is narrow (`0xF5`, about 58 kHz) so that neighbouring points do not blur. The span is only ±2 MHz: this is a look at one band, not a wide scanner.

### Monitor: bursts

The chip listens on the band's frequency with a wider filter (`MDMCFG4 = 0x65`) because key fobs and sensors drift in frequency. Each pass takes the maximum of 8 RSSI reads 300 µs apart and appends it to a 300-sample history. Burst detection works against a noise floor that is a slow average (`floor = 0.98·floor + 0.02·x`), updated only outside bursts:

- a burst starts when `rssi > floor + 12 dB` (`BURST_ON_DB`);
- while `rssi > floor + 7 dB` (`BURST_OFF_DB`) the burst continues and its peak is tracked (hysteresis);
- after 150 ms below that, the burst ends and is recorded (start, duration up to 65 s, peak); the last 6 are kept (`EV_N`).

The LED strip flashes during a burst. Only timing and level are recorded, never the content.

### Sensors: OOK pulse capture (`sens_rx.cpp`)

In Monitor and Sensors modes the CC1101 is switched into asynchronous serial mode: demodulated OOK (1 = carrier, 0 = silence) goes to GDO0 (GPIO 3), the AGC is tuned for OOK. An ISR on both edges writes into a 2048-entry ring the length of the segment that has just ended in microseconds, with the level in bit 31. A ring overflow counts as noise and discards the buffer.

The main loop (`sensRxTick`) assembles **pulse trains**: each element is pulse + following gap (`OokTrain`, up to 1024 pairs). A train is finished when a gap exceeds `TRAIN_END_US` = 20 ms, or by timeout if no edge arrives, or when the buffer is full (the latter is counted as noise, not decoded). Segments shorter than `GLITCH_US` = 40 µs are glitches and are merged with their neighbours (`cleanup`). Trains with fewer than 8 pulses are dropped. The strongest RSSI seen during the train (from the monitor) is attached to it.

### Decoding (`ook.cpp`)

The code is hardware independent and is compiled by the simulator and by the PC tests (`tools/ook_test`). Bit slicing follows the rules of the open-source rtl_433 project (formats and checksums are described by its documentation; the code is original), so recordings from its test set decode the same way.

Two slicers, driven by a protocol table (short/long width, sync, gap limit, reset gap, tolerance):

- **PPM**: the bit is the gap length (short = 0, long = 1);
- **PWM**: the bit is the pulse length (short = 1, long = 0), with optional sync pulses and tolerance windows.

Bits are collected into rows (up to 48 rows of 96 bits). A decoder usually requires the same row to be repeated (`repeated(minRep, minBits)`), because sensors send a message several times; this is a strong filter against noise. Then it validates length, checksum/CRC and value ranges. One sensor per train is emitted (same protocol + id + channel is deduplicated).

| Protocol | Modulation (µs) | Check |
|---|---|---|
| Nexus / FreeTec / Solight / TFA 30.3209, Rubicson / TFA 30.3197 | PPM 1000/2000, 36 bits, repeated 3x | CRC-8 `0x31` for Rubicson; ranges for Nexus |
| Prologue / ThermoPro TX2 / NC-7104 | PPM 2000/4000, 36 bits, repeated 4x | type nibble 9/5, ranges |
| Acurite 609TXC | PPM 1000/2000, 40 bits | byte sum |
| LaCrosse TX141TH-Bv2 / TX141-Bv2/Bv3 / TFA 30.3221 | PWM 208/417, sync 833 | LFSR digest-8 |
| Bresser 3CH / Renkforce DM-7511 | PWM 250/500, sync 750, inverted | byte sum; temperature in °F |
| Acurite 592TXR / tower | PWM 220/408, sync 620, 7 bytes | sum + parity |
| Fine Offset WH2 / WH2A / WH5 / TFA 30.3157 | PWM 500/1500 ±160 | CRC-8 `0x31` |

Output of a sensor decoder: protocol name, id, channel, temperature (valid range roughly -40..70/80 °C), humidity (if present), battery flag.

**Remotes.** If no sensor matched, `decRemote` looks at the pulse-length clusters. Two significant clusters with a ratio 1.6..4.5 suggest a remote. A fixed-code remote (EV1527/PT2262 type: 24 or 25 bits, long about 3x short, repeated at least twice) is reported as "Remote". **The code itself is not stored**: only a 16-bit hash of the 20 address bits as a fingerprint, and the button number (the lowest 4 bits, mapped to 1..4). A rolling-code remote (8+ even preamble pulses, header gap 3..12 ms, 65..67 bits) is reported as "Rolling" with a hash of the fixed part. Remote recordings cannot be replayed from what is stored.

**Analyzer** (`ookAnalyze`) runs on every train: clusters pulse and gap lengths (±30 % plus 30 µs), takes significant clusters (at least 8 % of elements), guesses modulation (PWM if the period pulse+gap is constant in at least 70 % of pairs, otherwise Manchester; PPM if gaps have two widths), counts rows and bits (the longest row), computes a shape fingerprint (a hash of widths, bit count and rows, independent of the code) and classifies: `sensor`, `remote`, `rolling`, `unknown`, `noise`. For sensors the raw bytes are shown; for remotes and unknown signals only the beginning of the pulse picture (12 pairs, the tail gap clamped to 4 ms) is kept, so the code cannot be recovered.

### Sensor table

`src/subghz/sensors.cpp`: up to `SENS_MAX` = 16 entries keyed by (protocol, id, channel); the oldest is evicted when full. Each record has the last temperature, humidity, battery, RSSI, count and times. Nothing is saved to flash. `sensOutside()` returns the temperature from the strongest sensor heard in the last 10 minutes; it is used by "radio weather" as the outdoor temperature. Display order: thermometers first, then remotes, within a group by signal strength (stale entries sink).

### Web mode

While a Sub-GHz tab is open on the tablet, `subWebUse()` turns the CC1101 on (`src/web/web.cpp`); if the tab has not polled for 4 s it is switched off.

## IR (`src/probe/ir*`)

Question answered: "whose remote is this?".

### Capture (`ir.cpp`)

The IR receiver on GPIO 1 already removes the 38 kHz carrier. The ESP32 RMT peripheral records the durations: resolution 1 µs, 4 memory blocks (fallback 2), 192 symbols per burst. Pulses shorter than 3 µs are filtered out as glitches; silence longer than 12 ms ends the burst. The receiver output is inverted (0 = mark), so RMT symbols are converted into a flat sequence `mark, space, mark, ...` starting with a mark; two same-level segments in a row are merged; a trailing gap longer than 10 ms is dropped. Bursts of 3 or more durations go to the decoder. The buffer keeps up to `IR_RAW_MAX` = 128 durations. The IR LED (GPIO 2) is configured as output and held LOW. The receiver works only while the section is open or the tablet subscribes to `ir`.

### Decoding (`ir_decode.cpp`)

Durations are compared with nominal values with a wide tolerance, because cheap receivers lengthen marks by about 100 µs: `0.7·T - 100 <= v <= 1.3·T + 100`. Checks go in this order:

| Protocol | Header (µs) | Data |
|---|---|---|
| NEC repeat | 9000 + 2250, up to 4 durations | "key held": repeats the last NEC code |
| NEC / NEC-EXT | 8800 + 4400 | 32 bits LSB first, mark 560, space 1690 = 1 / 560 = 0; if the address (and command) byte is followed by its inverse, it is NEC with an 8-bit address; otherwise NEC-EXT with 16 bits |
| LG | 8800 + 4400 | 28 bits MSB first |
| JVC | 8800 + 4400 | 16 bits, mark 525, space 1575 / 525 |
| Samsung | 4500 + 4500 | 32 bits, 560/1690 |
| Kaseikyo (Panasonic and relatives) | 3456 + 1728 | 48 bits, 432/1296; address = vendor code |
| Sony SIRC | 2400 + 600 | 12, 15 or 20 bits; the pulse width carries the bit (1200 = 1, 600 = 0) |
| RC6 | 2666 + 889 | Manchester, unit 444 µs, 16/20/24/32 bits, toggle bit; mode 6 is the Windows MCE variant |
| RC5 | unit 889 µs | Manchester, 14 bits, toggle, 5-bit address, 6-bit command (+ the RC5X extension bit) |
| anything else | 7 durations or more | `RAW` with a hash of the quantised durations (the same button gives the same hash) |

The pulse-distance decoder (`pulseDist`) requires the exact number of marks: extra ones mean it is another protocol.

### Brand and repeats

`irBrand()` makes only honest guesses from protocol and address ("probably"): for example NEC address `0x04` = LG TV, Samsung address `0x07` = Samsung TV, Kaseikyo vendor `0x2002` = Panasonic, RC5 address 0 = Philips TV. Unknown NEC addresses give "NEC remote (many brands)"; long RAW frames (70+ bits) hint at an air conditioner.

Repeats (`irFeed`): the same protocol, address and command within 250 ms (RC5/RC6: 400 ms and the same toggle bit) is a repeat, not a new press. A NEC repeat frame repeats the last NEC code. The history keeps 8 presses; repeats update the time of the latest entry. A click on the wheel clears the history. Only the last burst (up to 64 durations in JSON) is sent to the tablet.

### Limitations

Air-conditioner remotes (long frames) are shown only as RAW; the decoder cannot identify them. Carrier frequency is not measured (the receiver module is built for 38 kHz). Nothing is transmitted.

## NFC (`src/probe/nfc*`)

Question answered: "what card is this?". Read only. The stick writes nothing and emulates nothing.

### Hardware and polling (`nfc.cpp`)

PN532 over I2C (address `0x24`, SDA 8, SCL 18), reset on GPIO 45 (LOW = sleep). `nfcStart()` resets the chip, queries firmware (`GetFirmwareVersion`, up to 3 tries because the first frame wakes the chip), sets normal SAM mode and limits passive-target retries to 2 so polling does not hang; if no answer, the state becomes "no reader" (a wheel click retries). `nfcStop()` switches the RF field off and the chip to sleep.

Polling never blocks the main loop: every 120 ms the code sends `InListPassiveTarget` (type A; every third poll type F / FeliCa at 212 kbps), then returns and collects the answer when the chip reports it is ready; after 400 ms without an answer the request is cancelled with an ACK frame and restarted. More than 8 failed exchanges in a row mean the reader disappeared from the bus.

Frames follow the PN532 format (preamble, length and its complement, TFI `D4`, data, checksum); replies are checked for the `D5`, `cmd+1` header and the checksum. The receive buffer is 64 bytes; a truncated reply is flagged.

If the same UID is still seen, the card counts as "still present" (it is considered removed after 900 ms without a reply) and is not read again.

### What is read when a card appears

For ISO 14443-A cards (ATQA, SAK, UID up to 10 bytes):

1. If SAK has the ISO-DEP bit (`0x20`), one command is sent: `SELECT 2PAY.SYS.DDF01` (PPSE). A positive answer means "payment card (EMV)". The answer is **not kept**: the receive buffer is wiped. No card number, name or history is read.
2. If SAK = `0x00` and ATQA low byte = `0x44` (Ultralight / NTAG), the capability container is read (page 3). If it starts with `0xE1` (NDEF formatted), pages from 4 onwards are read in 16-byte blocks up to 160 bytes, stopping at the terminator `0xFE`, and the NDEF message is parsed. Then `GET_VERSION` tells NTAG213/215/216 from Ultralight EV1 by the size byte.
3. Otherwise the family is determined from ATQA/SAK alone.

FeliCa: only the identifier (IDm) is read.

### Classification (`nfc_model.cpp`)

By SAK: `0x08`/`0x88` MIFARE Classic 1K, `0x18` Classic 4K, `0x09` Mini, `0x10`/`0x11` Plus 2K/4K, `0x28`/`0x38` Classic with a smart chip; SAK `0x00`: NTAG213 / 215 / 216 (by GET_VERSION size byte `0x0F`/`0x11`/`0x13`), Ultralight EV1 (`0x0B`/`0x0E`) or generic Ultralight/NTAG; ISO-DEP with ATQA `0x0344`: DESFire, otherwise a generic smart card; the rest: "ISO 14443-A tag". Each family has a hint ("transit pass / door key · likely"): wording is deliberately cautious, because the card type does not prove what it is used for.

### NDEF and privacy

NDEF records: URI (with the prefix table), text, MIME type only (Wi-Fi configuration is shown as "settings (hidden)", the content, which may contain a password, is not shown), external types. Up to 3 records, 64 characters each; control characters are replaced by spaces; incomplete UTF-8 tails are cut. The UID is shown masked: only the first 2 bytes (`04A2••`), also in JSON. The last 4 taps are kept in RAM only.

### Limitations

Only the short range of the PN532 antenna; only types A and F are polled (no type B or ISO 15693). MIFARE Classic sectors are not read (no keys are used or tried). Cards with a randomised UID show a different prefix each time.
