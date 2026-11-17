# Sound, sky, alerts and display

This section covers everything that is neither a radio scanner nor a protocol: how sound becomes a spectrum, how the sky and satellites are computed on a microcontroller, how "radio weather" is derived from the airwaves, where the alert thresholds live and how they are applied, and how the 320×170 screen is drawn. Paths are relative to the repository root.

| Topic | Files |
|---|---|
| Sound (mic, speaker, laptop audio) | `src/audio/audio.cpp`, `audio_model.h`, `audio_screens.cpp`, `tools/pc_monitor/pc_monitor.py` |
| Sky, Globe, satellites | `src/gallery/astro.cpp`, `sat.cpp`, `gallery_sky.cpp`, `astrodata.h`, `tools/gen_astro.py` |
| Radio weather | `src/gallery/weather.cpp`, `weather.h` |
| Alerts | `src/core/settings.cpp`, `tools/webapp/src/core/store.js`, `tools/webapp/src/views/hq.js`, `tools/desktop/events.py` |
| Drawing, themes, fonts | `src/core/display.cpp`, `theme.cpp`, `ui.cpp`, `psram.h`, `tools/gen_gfxfont.py` |
| Screen simulator | `tools/sim/` |

## 1. Sound

The device (ESP32-S3) has a PDM microphone and an I2S speaker amplifier. They use two separate I2S peripherals and two separate FreeRTOS tasks, so they never block each other or the UI.

### 1.1 Pins and peripherals

| Function | Peripheral | Pins (GPIO) | Mode |
|---|---|---|---|
| Microphone | I2S0 (PDM RX is only available on I2S0) | CLK 39, DATA 42 | PDM, mono, 16 kHz, 16 bit |
| Speaker | I2S1 | BCLK 46, LRCLK 40, DIN 7 | Philips standard, mono, 16 kHz, 16 bit |

The pins and the driver settings come from the LilyGO `test_mic_speaker` example (comment at the top of `audio.cpp`).

### 1.2 Microphone pipeline

The microphone runs **only while the Sound section is open** (or while a tablet tab asks for sound data, or the Weather screen is open). Nothing is recorded or stored: a block is analysed and overwritten by the next one.

```
 PDM mic --I2S0--> task "mic" (core 1, prio 2, stack 6 KB)
                   block of 512 samples = 32 ms
                   |  remove DC (mean)  -> level (RMS, dBFS)
                   |  Hann window -> FFT 512 (radix-2, in place, float)
                   |  32 log bands 60 Hz .. 8 kHz -> dB per band
                   |  dominant frequency, 256-point waveform
                   v
              MicBlock (copied under a spinlock, with a sequence number)
                   |
 main loop  auUpdate(): AGC, smoothing, peak caps, history  -> AudioView g_au
                   |
        screens (audio_screens.cpp), tablet stream, LED strip
```

Details:

- **Block and FFT.** `N = 512` samples at 16 kHz: 32 ms per block, FFT bin width 16000/512 = 31.25 Hz. The mean of the block is subtracted first (PDM microphones carry a DC offset), then a Hann window is applied. The FFT is a small hand-written radix-2 routine (`fft()` in `audio.cpp`), no external library.
- **Level.** `20·log10(RMS/32768)` in dBFS (0 = full scale). `levelWord()` turns it into QUIET (< −55), NORMAL (< −40), LOUD (< −25), VERY LOUD.
- **32 log bands.** Band `k` covers `60·(8000/60)^(k/32)` .. `60·(8000/60)^((k+1)/32)` Hz. Band edges are converted to FFT bins once at microphone start (`s_bandLo/s_bandHi`); each band takes the **maximum** magnitude of its bins, not the sum, so wide high bands are not louder than narrow low ones. Magnitude is normalised by `N·8192` and converted to dB.
- **Dominant frequency.** The loudest bin above bin 2 (about 94 Hz) multiplied by the bin width.
- **Handoff.** The task publishes a `MicBlock` (level, 32 band dB, 256 waveform points, dominant Hz, raw peak, sequence number) inside a critical section; the main loop copies it and skips the work if the sequence number is unchanged.
- **Channel auto-probe.** A PDM mic can be wired to the left or right slot. The mic starts on the left; if for 1.5 s the raw peak stays below 8 (digital silence), the driver is restarted on the right slot. `MicDiag` (driver stage 0..4, slot, blocks, timeouts, raw peak) is shown by the "mic check" screen when something is wrong.

### 1.3 AGC and smoothing (`auUpdate`)

The spectrum has to look good in a quiet room and at a concert alike, so band values are shown **relative to the current loudest band**, not in absolute dB.

- `s_agcTop` is the "top of the scale" in dB. It jumps up to the loudest band instantly and sinks by 0.15 dB per block (about 4.7 dB/s). It never goes below −70 dB, so silence is not amplified into noise.
- A band is mapped as `(dB − (top − 45)) / 45`, clamped to 0..1: a 45 dB window under the top.
- Smoothing is asymmetric: rise is instant, fall is `0.82·old + 0.18·new`. Peak "caps" fall by 0.012 per block. `g_au.energy` is the mean of the bands (0..1) and drives the animations.
- The overall peak (`g_au.peak`) falls 0.4 dB per block; a 120-point history takes one level value every 0.5 s (1 minute). `g_auAvg` is an exponential average (0.97/0.03).
- Optional LED meter (Settings › Sound › LED meter): level −70..−15 dB is mapped onto the 8 WS2812 LEDs.

The Terrain screen has its own second normalisation (`s_trTop`, decay 0.985 per push) so that a loud sound does not turn the mountains into a wall.

### 1.4 Laptop audio over USB

With TENGAN Desktop / `pc_monitor.py` running, the laptop can supply its own speaker output ("what is playing") instead of the microphone. The heavy work is done on the laptop:

1. While the Sound section is open the stick sends `AUD ON` every 1.5 s (`AUD OFF` when it no longer wants it).
2. The laptop captures loopback audio, runs an FFT (2048 points, 30 frames/s), and sends the **finished** frame as one text line: `PA <level×10> <Hz> <32 bands hex> <64 wave samples hex> <64 L,R pairs hex> <stereo 0/1>` (format commented in `src/pc/pc.cpp`, produced by `Analyzer.frame()` in `pc_monitor.py`). The band edges are the same 60 Hz..8 kHz log scale as on the stick, so frequency labels match; the laptop does its own AGC (48 dB window, floor −60 dB).
3. `parseAudio()` in `pc.cpp` decodes the hex and calls `audioPcFrame()`, which fills the same `g_au` structure (fall factor 0.78/0.22, waveform expanded from 64 to 256 points).

Source selection (Settings › Sound › Source, `g_auSrc`): `auto` (laptop if frames arrived in the last 0.6 s, otherwise microphone), `mic`, `laptop`. In laptop-only mode the microphone is not touched; if a stick is in `auto` and the laptop audio stops, the mic is started on the fly. The XY scope uses real L/R from the laptop; with the microphone (mono) it draws a phase portrait: X = sample `i`, Y = sample `i+10`.

### 1.5 Speaker

`audioBegin()` opens I2S1 once at boot and starts task `spk` (core 1, priority 1) with a queue of 8 events. UI sounds are tiny synthesised sine bursts with a decaying envelope:

| Sound | Frequency | Length | Envelope |
|---|---|---|---|
| Wheel click (`audioClick`) | 2600 Hz | 6 ms | cubic fall, a "tick" |
| Geiger tick (`audioGeiger(pct)`) | 1800 + 12·pct Hz | 4 ms | cubic fall |
| Beep (`audioBeep`) | given | given | linear fall |

The Geiger interval is `900 − 8·pct` ms (900 ms far, 100 ms close), used by finder screens. Volume is Settings › General › Volume: off / quiet (amplitude 2500) / loud (9000) of int16. If the queue is full the sound is silently dropped. `auto_clear` makes the channel output silence, not a stuck tone, when the queue is empty.

### 1.6 Tablet / web output

`audioStreamJson()` builds the compact frame for the tablet's SSE stream (about 25 per second): level, peak, dominant Hz, energy, 32 bands and 64 waveform samples as hex strings, written into a stack buffer without temporary `String`s. `audioJson()` is the slower variant for the web page. `audioWebUse/Tick/Stop` start and stop the microphone on behalf of a remote client.

## 2. Sky, Globe and satellites

Files: `src/gallery/astro.cpp` (time, planets, Sun, Moon), `sat.cpp` (satellites), `gallery_sky.cpp` (both screens), `astrodata.h` (generated data). There is no network and no ephemeris file on the stick: positions are computed from compact formulas. The accuracy goal is "a correct picture", in fractions of a degree for the Sun, Moon and planets.

### 2.1 Time and place

- **Time** comes from the laptop: the `PC` line carries `ts=` (UTC) and a time-zone offset. `astroSetTime()` writes it to the chip clock if it differs by more than 2 s; the offset is saved to settings (`g_tzOff`, default 10800 = UTC+3). The ESP32 clock keeps running while the stick is "off" as long as the battery lives. `astroTimeKnown()` is `time > 1700000000`; otherwise Sky and Globe show "TIME?" and ask to connect an app once.
- **Place** is Settings › General › City: 20 built-in cities (`CITIES[]`) plus one extra "GPS" entry whose coordinates are pushed by the tablet app (`/api/loc` → `setGpsCity()`).

### 2.2 Coordinates

Julian date from Unix time: `JD = t/86400 + 2440587.5`. Greenwich mean sidereal time `gmstDeg(jd)` uses the standard polynomial. Equatorial → horizontal conversion is done in two flavours:

- `toAltAz()` in double, for single objects (Sun, Moon, planets);
- `skyFrame()` + `toAltAzF()` in **float**: local sidereal time and sin/cos of latitude are computed once per frame, and each of the hundreds of stars costs only a few float operations. The ESP32-S3 FPU does single precision in hardware, while `double` is emulated in software and is several times slower.

### 2.3 Sun, planets, Moon

- **Planets** (Mercury, Venus, Mars, Jupiter, Saturn): JPL "mean elements" (E. M. Standish, valid 1800–2050): for each body `a, e, I, L, ϖ, Ω` and their rates per century. The code takes `T` in centuries from J2000, solves Kepler's equation `E − e·sin E = M` by 8 Newton iterations, builds heliocentric ecliptic coordinates, subtracts the Earth's position (also from elements), and converts ecliptic → equatorial with a fixed obliquity 23.43928°. The code applies no light-time, aberration, nutation or refraction.
- **Sun**: the Earth's heliocentric vector with the opposite sign.
- **Moon**: a shortened series (Meeus, "Astronomical Algorithms"): mean longitude plus five periodic terms, one latitude term. Phase is `(1 − cos D)/2` from the mean elongation `D`.

### 2.4 Star data (generated)

`astrodata.h` is generated by `python3 tools/gen_astro.py > src/gallery/astrodata.h` from `tools/astro_src/` (d3-celestial, BSD-3, Hipparcos catalogue; world-atlas / Natural Earth 1:110m land, ISC). Do not edit it by hand. Contents:

| Array | Size | Encoding |
|---|---|---|
| `STARS` | 1627 stars, magnitude ≤ 5 | RA, Dec in degrees×100 (`uint16`/`int16`), magnitude `(m+2)·10` in one byte; **sorted brightest first** |
| `STAR_NAMES` | 25 stars with mag ≤ 1.6 | index into `STARS` + name |
| `CL_PTS`/`CL_START` | 150 constellation polylines | points + start offsets |
| `CON_NAMES` | 46 labels (constellation ranks 1–2) | position, Latin abbreviation, Russian name |
| `LAND_PTS`/`LAND_START` | 130 land polylines | lon/lat ×100, simplified with Ramer–Douglas–Peucker (ε = 0.45°) |

Because stars are sorted by brightness, the draw loop simply `break`s at the first star dimmer than the current magnitude limit.

### 2.5 Sky screen (planetarium)

- **Projection**: stereographic. Zenith is the dome centre, horizon is the edge of the circle; radius `r = R·tan((90° − alt)/2)` (equals `R` at the horizon). North is up and east is on the left, like a sky chart (looking up). Centre (86, 88), `R = 58`. The wheel rotates the dome by 15° per step with smoothing; click cycles labels: constellations + planets / bright stars / none.
- **Day/night**: Sun altitude > −6° counts as day, < −12° as night, in between twilight. The dome colour and the **magnitude limit** depend on it: 1.5 (day), 3.0 (twilight), 5.0 (night).
- Star glyph by brightness: filled circle (< 0.5 mag), 3×3 square, 2×2 square, single pixel with a dimming colour.
- Moon is drawn as a disc with the lit part as a vertical strip according to the phase; the Sun with 8 rays; planets with a ring and a three-letter label.
- **Label collision**: placed labels are remembered in a list of up to 64 rectangles, and a new label is skipped if it overlaps one.
- Right panel: local time, Sun altitude and day/night state, Moon phase and altitude, up to three planets above the horizon.

### 2.6 Satellites: Keplerian orbit + J2 instead of SGP4

**How the data arrives.** The laptop app downloads TLEs from CelesTrak for ISS (25544), Tiangong (48274) and Hubble (20580) every 6 hours (retry after 15 minutes on failure), keeps a cache file `tle_cache.txt`, and sends the stick lines `TLE\t<name>\t<line1>\t<line2>` (`Orbits` in `pc_monitor.py`). `satFromLine()` parses them, keeps up to `SAT_MAX = 4` satellites, and saves to NVS (namespace `sats`) **only if the epoch changed**, to spare flash. At boot `satBegin()` reloads them, so Globe works without the laptop for days.

**Why not SGP4.** The full SGP4 model (drag term B*, deep-space terms, short-period corrections) is large and not needed to draw a dot on a 60-pixel globe or to predict a pass within minutes. The stick uses a simplified two-body propagator (`satPos()`):

1. Orbital elements from the TLE: inclination, RAAN, eccentricity, argument of perigee, mean anomaly, mean motion `n0` (rev/day → rad/min); the semi-major axis `a` comes from Kepler's third law (`a³ = μ/n²`, μ = 398600.4418 km³/s²). The first derivative of mean motion (`ndot2`) is used; the **B\* drag term is not parsed**.
2. Secular J2 perturbation (Earth oblateness): with `p = a(1−e²)` and `k = 1.5·J2·(Re/p)²·n0`, the node regresses `Ω = Ω0 − k·cos i·t` and the perigee precesses `ω = ω0 + k·(2 − 2.5·sin²i)·t`.
3. Mean anomaly `M = M0 + n0·t + ndot2·t²`, Kepler's equation by 10 Newton iterations, true anomaly `ν`, radius `r = a(1 − e·cos E)`.
4. Position in the inertial frame (ECI), then rotation by GMST into the Earth-fixed frame (ECEF). Latitude is geocentric on a spherical Earth (`asin(z/r)`), height `r − Re`; speed from the vis-viva equation.

The header of `sat.h` states the claimed accuracy: minutes, good for days from the epoch. The screen shows the age of the orbit data and turns it orange after 7 days. The tablet receives the raw elements (`satJson()`) and runs the same Kepler + J2 model in the browser.

**Look angles.** `satLook()` converts the satellite ECEF vector minus the observer vector into east/north/up axes: elevation `asin(up/range)`, azimuth `atan2(east, north)`.

**Pass prediction.** `satNextPass()` walks the next 24 hours in 30-second steps looking for elevation ≥ 10°. On the first step above the threshold it backs up and refines the rise moment in 2-second steps, tracking maximum elevation and the rise azimuth; the set time is the first step below the threshold. The Globe screen recomputes it once a minute (or when the satellite or city changes, or the pass has ended).

### 2.7 Globe screen

- **Orthographic** projection: a point on the back side (`cos c < 0`) is not drawn. The view follows the ISS (or the home city, or another satellite, by click); the wheel adds a longitude offset, the camera latitude is `0.6·lat + 10°` clamped to −50..60, with smoothing.
- **Day/night shading** is computed per pixel of the disc (about 11 000 pixels): each pixel is converted back to a point on the sphere and compared with the sub-solar point (`RA_sun − GMST`, `Dec_sun`) by dot product: > 0.05 day, within ±0.05 twilight, otherwise night.
- Land outlines are polylines from `LAND_PTS`; a 30° grid is drawn as dots.
- **Ground track** −30..+90 minutes in 2-minute steps (61 orbit evaluations in `double`) is recomputed only every 20 s or when the orbit data changes; the satellite marker itself is evaluated every frame.
- **Visibility footprint** (area from which the main satellite is above 10°): angular radius `λ = acos(Re·cos(el)/(Re+h)) − el`, drawn as a dashed circle.

## 3. Radio weather

Files: `src/gallery/weather.cpp`, `weather.h`. The surrounding airwaves are rendered as weather. It receives only and does not scan Wi-Fi. All quantities are smoothed — the comment says "beauty matters more than accuracy".

| Weather | Source | Normalisation (before smoothing) |
|---|---|---|
| clouds | number of Bluetooth devices now | `1 − exp(−n/25)` (≈ 0.9 at 60 devices) |
| wind | devices that appeared + disappeared in the last 60 s | `turn/30`, clamped to 0..1 |
| rain | bursts of packets at 433.92 MHz per minute (remotes, sensors), CC1101 in monitor mode | `bursts/6`, clamped |
| storm | rain + loud seconds | `0.6·rain + 0.5·loudSeconds/20 + 0.3 if thunder` |
| thunder | microphone | level jump > 18 dB above a slow average and level > −30 dBFS; or a 433 burst with peak > −55 dBm; shown for 2 s |
| barometer | clouds now vs 10 minutes ago | `(clouds − old)·3`, clamped to ±1 |
| outside | outdoor 433 MHz thermometer (decoded in `sensors.cpp`) | °C, humidity, protocol |

How it is computed (`wxUpdate()`, once per second):

- Device identity sets are compared as sorted arrays of `uint32` ids between consecutive seconds (a merge walk) to count arrivals and departures; per-second counters live in ring buffers of 60 entries (`s_turnSec`, `s_burstSec`, `s_loudSec`).
- Rain and storm count bursts via the `g_events` counter of the Sub-GHz monitor.
- Normalised values go through an exponential filter `v += (target − v)·(1 − e^(−dt/τ))` with τ = 8 s (clouds), 6 (wind), 5 (rain), 4 (storm).
- History for the chart: one point per 10 s, 60 points (10 min).
- The weather word (calm / clear / cloudy / overcast / windy / rain / storm) is chosen by thresholds in `condFor()` and switches only after a new candidate has held for **5 s**.

Sources are enabled only while needed: on the stick by `wxStart()` (Bluetooth on, CC1101 on 433.92 MHz, microphone), on behalf of a tablet by `web.cpp` when a client subscribed to the `wx` event. The same `WxState` is sent as JSON (`wxJson()`, event `wx`, once a second). The screen draws a procedural scene: sun or moon with stars, number and speed of clouds from clouds/wind, rain slant from wind, lightning bolt and flash on thunder, grass swaying with loudness. The second view (wheel) is a 10-minute chart of clouds and rain.

## 4. Alerts

### 4.1 Where they are configured

Alerts are **settings stored on the stick** and exposed as a page "Alerts" (id `alert`). The same table `DEFS[]` in `src/core/settings.cpp` serves the stick screen (Settings › Alerts), the tablet page and the HQ panel (via `/api/settings` and `/api/set`), so the logic is not duplicated. Values are in NVS namespace `radar` (keys `alnear`, `alnew`, …) and are clamped on load.

| Key (`/api/set`) | Default | Range | Meaning |
|---|---|---|---|
| `al_near` | −65 dBm | −90..−40, step 5 | stronger than this counts as "near" |
| `al_new` | near (1) | off / near / any | alert on a new device |
| `al_trk` | near (1) | off / near / any | alert on a tracker |
| `al_newfor` | 30 s | 10..300 | how long a device stays "new" |
| `al_warm` | 20 s | 0..120 | warm-up after connecting: nothing is new |
| `al_sens` | 5 | 1..10 | gain of the activity scale |
| `al_warn` | 60 % | 20..95 | activity level "attention" |
| `al_alarm` | 80 % | 30..100 | activity level "alarm" |
| `al_cble`, `al_crf`, `al_csnd`, `al_cprb` | on | bool | count Bluetooth / 433 MHz / sound / IR+NFC in activity |
| `al_blink` | on | bool | blinking on alerts (off = steady glow) |
| `al_plate` | on | bool | show the proximity warning banner |

Pairing rule (`setVal`): alarm is always above attention. Raising `al_warn` to or above `al_alarm` pushes `al_alarm` to `warn + 5` (max 100); lowering `al_alarm` to or below `al_warn` pushes `al_warn` to `alarm − 5` (min 20); the same is applied on load. In the HQ panel the "reset to defaults" button therefore sets `al_alarm` to 100 first, then the two thresholds in order.

Theme and language cannot be changed from the tablet (`/api/set` answers "change it on the device").

The firmware itself only **stores** these values. The decisions are made by the clients that see the event stream: the tablet page (`tools/webapp/src/core/store.js`, `views/hq.js`) and the desktop app (`tools/desktop/events.py`, which states in its docstring that it applies the same rules). In the HQ view of the tablet page the gear button "⚙ ALERTS" opens the same list; if the stick firmware has no such page (older than 1.8.2, per the HQ message), the page works with the defaults (`S.CFG_DEF`).

### 4.2 How "near / new / tracker / activity" are computed

**Near.** The signal strength is the smoothed RSSI of the device (`e.rs`). `T.steady(4, 0.8)` ignores jitter below 4 dB and glides to larger changes with a 0.8 s time constant. A device is "near" if `rs > al_near`.

**New** (`S.isNew`). A device is new if all hold: it is not marked `first`, `al_new` is not off, it was born less than `al_newfor` seconds ago, and either `al_new` = any or it is near. "Born" means: seen for the first time in this session. A device that vanished from the list and came back is **not** new (phones come and go), and during the first `al_warm` seconds after connecting everything is treated as already known, because the stick's list is still filling.

**Tracker** (`S.isTrk`). Same logic without the age limit: the device type is `tracker` and `al_trk` is on, near-only unless set to "any".

An event for a new/tracker device raises the red banner "PROXIMITY · NEW DEVICE" (or "TRACKER · NEARBY") with the name and RSSI for 20 s (blinking for the first 4 s), if `al_plate` is on; the entry in the event log is flagged as an alert.

**Activity** (`S.activity`, 0..1) is transparent: four sources, each normalised to 0..1, only enabled ones counted:

| Source | Value | Maximum |
|---|---|---|
| BLE | new devices in the last 60 s | 5 per minute |
| 433 | packets in the last 60 s (growth of the sensors' packet counter) | 20 per minute |
| sound | `(level + 60)/40` | −60 dB → 0, −20 dB → 1 |
| IR/NFC | key presses and cards in the last 60 s | 3 per minute |

Result: `activity = mean(enabled sources) · al_sens / 5`, clamped to 0..1. The state: `≥ al_alarm` → alarm, `≥ al_warn` → attention, `≥ al_warn/2` → active, otherwise quiet. HQ shows the result and small per-source bars (disabled ones crossed out), so the user can see **why** the level is what it is.

## 5. Display and styling

### 5.1 Hardware and loop

ST7789 170×320 panel over SPI at 40 MHz (`platformio.ini`), used as 320×170 landscape (`setRotation(3)`). The backlight is switched on only after the screen is cleared, otherwise display memory garbage flashes at start. Every screen draws into a full-frame sprite (`spr`, 16 bpp = 320×170×2 = 108.8 KB; falls back to 8 bpp if memory is short) and then calls `present()`; nothing is drawn directly to the panel.

Frame period (`main.cpp`): `REDRAW_MS` = 66 ms (≈ 15 fps) normally; 33 ms (≈ 30 fps) if the screen set `g_fastFrame` this frame (animations: Tesseract, Torus, Life, Pendulum, Terrain, XY, Sky, Globe); four times slower (264 ms) when the power module reports idle (dimmed screen). `g_fastFrame` is reset after each frame, so the screen must ask again.

### 5.2 Push only changed rows (`pushChanged()` in `theme.cpp`)

A full frame is ~108 KB and takes ≈ 22 ms over SPI. Many screens (menu, settings, lists) change only a clock, a few digits or bars. So `present()` hashes each of the 170 rows (FNV-1a over 32-bit words, about 1 ms per frame) and compares with the previous frame's hash. Runs of changed rows are sent as bands with `pushSprite(0, y, 0, y, W, h)`; gaps up to 6 unchanged rows are merged into one band to save display commands. A full frame is sent anyway every 2 s as insurance, after start, and in the fallback cases (8-bit sprite). `perf.cpp` counts `pushPct`, the share of rows really sent, together with fps and loop/draw times (exposed in the `st` stream, field `lp`).

### 5.3 PSRAM

The board has 8 MB PSRAM against 320 KB of internal RAM. Large buffers are allocated on first use of a screen through `psAlloc<T>()` (`psram.h`: `heap_caps_calloc(MALLOC_CAP_SPIRAM)`, falling back to the normal heap) and are kept afterwards. Users: Terrain heights (24×52 floats), Torus points (56×18) and sort order, Life grids, the Sub-GHz waterfall, the serial output buffer. In the simulator `psAlloc` is plain `calloc`.

### 5.4 Themes DENKO and KURENAI

The theme (`g_theme`, saved under `th2`, chosen in Settings › General › Theme) changes colours and a few components; everything is a function of `themeKurenai()`.

- **DENKO** 電光: dark blue background, teal accent, near-white values, orange for alerts only.
- **KURENAI** 紅: flat black / grey / white with red as the accent and the strongest level; the title is a white plate with a cut corner, the selected row is a white plate with black text, the header underline is a solid red line instead of a fade.

**Palette discipline.** Screens never choose arbitrary colours. `C(r,g,b)` maps any RGB to the theme ramp by perceived brightness (`77r + 150g + 29b`) through a 256-entry lookup table that is rebuilt when the theme changes; `TC()` takes a literal colour (for KURENAI it is converted to grey); `heatV()/heat(rssi)` is the signal-strength ramp (steel → teal → white in DENKO; grey → white → red in KURENAI); `scale565()` darkens towards black. Semantic tokens: `C_BG`, `C_ACC` (selection, main value), `C_HOT` (values), `UI_LABEL`, `UI_DIM`, `UI_LINE`, `UI_PANEL`, `C_ALERT` (orange/red, **only** for alarms), `C_SEL*` for selected cards. Because every colour goes through the ramp, a new screen cannot accidentally leave the palette, and one drawing code works for both themes.

**UI grid** (`ui.h`): header y 0..19, content y 24..152 with 8 px margins, footer y 156..169; 4 px spacing step. Components in `ui.cpp`: `uiHeader` ("// TITLE" in upper case, status on the right, shortened with ".." so it does not overlap the title), `uiFooter` (control hints; drops one font size if too long), `uiFrame` (corner brackets), `uiReadout` (small label + value), `uiEmpty`, `uiRowHighlight`. `present()` also draws corner brackets and page dots.

### 5.5 Fonts and their generation

All UI fonts contain Latin **and** Cyrillic, so layout does not shift when the language changes and there is no per-language font switch. They are Adafruit-GFX bitmap fonts (TFT_eSPI parses UTF-8 itself and draws Russian text as is). `txt(s, x, y, size)` selects by size:

| size | Font | Use |
|---|---|---|
| 8 | JetBrains Mono 10 px | tiny captions that do not fit |
| 1 | JetBrains Mono 11 px | small labels (upper case) |
| 2 | JetBrains Mono 12 px | HUD text |
| 3 | JetBrains Mono 14 px | |
| 5 | JetBrains Mono 19 px | |
| 4 | DENKO: Exo 2 20 px; KURENAI: Oswald 20 px | titles, big values |
| 6 | DENKO: Exo 2 37 px; KURENAI: Oswald 36 px | the main number of a screen |
| 7 | built-in seven-segment font | |

Upper-casing is done by hand in `utf8Upper()` (ASCII and Cyrillic only), since the standard `toupper` ignores UTF-8. The built-in 6×8 font cannot show UTF-8, so `·` and `°` are remapped to its code-page codes.

**Generator** `tools/gen_gfxfont.py` (needs Pillow):

```
python3 tools/gen_gfxfont.py fonts/JetBrainsMono-Medium.ttf 12 JBM12 --cyr > src/core/fonts/JBM12.h
```

It renders each glyph with PIL at the given pixel size, thresholds it into a 1-bit bitmap (`--thr=N`, default 110: lower = bolder), and writes `Bitmaps[]`, `Glyphs[]` (offset, width, height, advance, x/y offset) and a `GFXfont` structure in PROGMEM. Glyphs: ASCII 0x20..0x7E; `--latin1` adds `· « » ° ×`; `--cyr` adds those plus U+0400..U+045F. A GFX font needs a contiguous table, so the codes between ASCII and Cyrillic are filled with empty glyphs. `--widespace` widens the space so words do not merge on the small screen. Source TTFs are in `fonts/` with their licences (OFL / Apache 2.0). Generated headers live in `src/core/fonts/`.

### 5.6 3D and animated screens

All are software-rendered into the sprite, with no GPU, using `float` and a few hundred primitives per frame.

- **Terrain** (Sound): the spectrum becomes a new ridge at the front every 45 ms; 24 older ridges recede to the horizon (52 columns each). Ridge height = smoothed band², normalised to the ridge itself, plus deterministic "rocky" noise (`rock()`, a hash of row and column) and mixed with the previous ridge for temporal smoothness. Rows are drawn far to near (painter's algorithm) as filled triangles; far ridges are dimmed as haze; high crests become "snow" (white).
- **Tesseract** (Gallery): 4D cube, 16 vertices, rotated in the XW, YW, ZW and XY planes, perspective 4D → 3D → 2D, colour by the 4D depth `w`; the trail is the last three frames.
- **Torus**: about 1000 points (56×18) with lighting; points are sorted by depth (`std::sort`) and drawn far to near.
- **Life** (Conway's game of life with afterglow; restarts when the population dies or stagnates), **Pendulum** (double pendulum with a glowing trail), **Reactor** and **XY scope** (Sound), **Sky** and **Globe** (section 2).

## 6. Screen simulator (`tools/sim`)

The simulator compiles the **real** drawing code on a PC and saves each screen as an image, so layout and themes can be checked without a device and the README screenshots (`docs/*.png`, GIFs) are generated from it.

- `tools/sim/build.sh` compiles `tools/sim/sim.cpp`, a patched copy of TFT_eSPI (64-bit host: `flash_address` as `uintptr_t`) and the real sources: `core/display.cpp`, `theme.cpp`, `ui.cpp`, `menu.cpp`, `settings.cpp`, all `*_screens.cpp`, `gallery/*.cpp`, `astro.cpp`, `sat.cpp`, `weather.cpp`, `pc/pc.cpp` and so on. Hardware APIs are replaced by stubs in `tools/sim/stubs/` (`Arduino.h`, `Preferences.h`, `SPI.h`, `Wire.h`, …). It needs g++, Python with Pillow and the TFT_eSPI library from `.pio/libdeps` (run `pio run` once).
- `sim.cpp` fills the models with plausible data (BLE list, Wi-Fi networks, a Sub-GHz capture with synthetic Nexus / TX141 / doorbell signals, fake weather values, two satellite TLEs, an IR/NFC test), then steps through the menu with `navRotate/navClick/navBack`, calls the `draw*()` functions and writes each frame with `snap()` as PPM → PNG at 2× (nearest neighbour, pixel art). Sound screens use a recorded real `PA ...` line from `tools/sim/pa_line.txt` fed through the real `pcPoll()` parser.
- Environment variables: `SIM_THEME=kurenai`, `SIM_LANG=ru`, `SIM_ANIM=1` (extra frames for GIFs), `SIM_OUTDIR`/`SIM_WORK`. Output: `tools/sim/out/*.png` and a contact sheet `contact.png` (3 columns). `tools/make_docs.py` turns the frames into README GIFs and screen galleries in `docs/`.
- `sim.cpp` also dumps `settingsJson()` to `settings.json` and tests `settingsSetKey()`, so the tablet-facing settings API is exercised without a device.

Limitations: no radio, buttons or timing — frame content, not performance, is verified. `psAlloc` is `calloc`, so PSRAM behaviour is not simulated.
