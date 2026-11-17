#!/usr/bin/env python3
"""Fake TENGAN stick for developing the tablet app without hardware.

    python3 tools/webapp/build.py --html /tmp/tablet.html
    python3 tools/webapp/mock.py /tmp/tablet.html [8090]
    open http://127.0.0.1:8090/tablet?host=127.0.0.1:8090&sse=8091

:8090 serves /tablet and /api/*, :8091 serves the SSE /stream (like the firmware's second port 81).
Data is made up but has the same shape as the firmware sends.
"""
import http.server, json, math, random, sys, threading, time
from urllib.parse import urlparse, parse_qs

HTML = open(sys.argv[1] if len(sys.argv) > 1 else "tablet.html", "rb").read()
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8090          # API; SSE stream on PORT+1
THEME = next((a for a in sys.argv[3:] if a in ("denko", "kurenai")), "denko")      # device theme: denko | kurenai
LANG = next((a for a in sys.argv[3:] if a in ("en", "ru")), "en")                  # firmware language: en | ru
T0 = time.time()
TYPES = ["phone", "audio", "watch", "tracker", "computer", "unknown", "TV", "sensor", "band", "phone"]
NAMES = ["iPhone", "AirPods Pro", "Galaxy Watch5", "AirTag", "ThinkPad X1", "", "[TV] Samsung Q7", "LYWSD03MMC", "Mi Band 7", "Pixel 7"]
VEND = ["Apple", "Apple", "Samsung", "Apple", "Lenovo", "private", "Samsung", "Xiaomi", "Xiaomi", "Google"]
BASE = [-44 - i * 2.6 for i in range(22)]


# remote settings (/api/settings, /api/set, /api/label, /api/act), kept in memory
SETTINGS = json.loads(('{"pages":[{"id":"general","name":"General","items":[{"k":"theme","name":"Theme","hint":"change it on TENGAN: Settings › Theme","type":"info","v":"__THEME__"},{"k":"screen","name":"Screen","hint":"screen backlight","type":"range","v":100,"min":20,"max":100,"step":10,"unit":"%"},{"k":"dim","name":"Auto dim","hint":"dim screen when idle","type":"enum","v":"off","opts":["off","30 s","1 min","5 min"]},{"k":"ledbr","name":"LED brightness","hint":"led strip brightness","type":"range","v":50,"min":10,"max":100,"step":10,"unit":"%"},{"k":"volume","name":"Volume","hint":"all sounds: off/quiet/loud","type":"enum","v":"quiet","opts":["off","quiet","loud"]},{"k":"clicks","name":"Wheel clicks","hint":"click on every wheel step","type":"bool","v":1},{"k":"city","name":"City","hint":"for sky map and satellites","type":"enum","v":"Moscow","opts":["Moscow","Saint Petersburg","Novosibirsk","Yekaterinburg","Kazan","Nizhny Novgorod","Samara","Krasnodar","Rostov-on-Don","Vladivostok","Irkutsk","Minsk","Kyiv","Almaty","Tbilisi","Yerevan","Berlin","London","New York","Tokyo"]},{"k":"battery","name":"Battery","hint":"","type":"info","v":"87%  3.98 V"},{"k":"version","name":"Version","hint":"TENGAN firmware","type":"info","v":"v1.7-test3"}]},{"id":"ble","name":"Bluetooth","items":[{"k":"ble_range","name":"Range","hint":"ignore weaker devices","type":"enum","v":"all","opts":["all","far","room","near","very near"]},{"k":"ble_ledev","name":"LED events","hint":"leds flash on count jump","type":"range","v":7,"min":0,"max":30,"step":1,"unit":""},{"k":"ble_ticks","name":"Find ticks","hint":"ticks on nearest screen","type":"bool","v":1}]},{"id":"wifi","name":"Wi-Fi","items":[{"k":"wifi_range","name":"Range","hint":"hide weaker networks","type":"enum","v":"all","opts":["all","far","room","near","very near"]},{"k":"wifi_ticks","name":"Find ticks","hint":"ticks on find screen","type":"bool","v":1}]},{"id":"sub","name":"Sub-GHz","items":[{"k":"sub_band","name":"Start band","hint":"band when section opens","type":"enum","v":"433 MHz","opts":["315 MHz","433 MHz","868 MHz","915 MHz"]}]},{"id":"snd","name":"Sound","items":[{"k":"mic","name":"Microphone","hint":"mic for sound section","type":"bool","v":1},{"k":"au_leds","name":"LED meter","hint":"leds show loudness","type":"bool","v":0},{"k":"au_src","name":"Source","hint":"auto / mic / PC audio","type":"enum","v":"auto","opts":["auto","mic","PC"]}]}]}').replace('__THEME__', THEME))
SETTINGS["pages"].append({"id": "alert", "name": "Alerts", "items": [   # like Settings > Alerts in the firmware
    {"k": "al_near", "name": "Near", "hint": "stronger than this = near", "type": "range", "v": -65, "min": -90, "max": -40, "step": 5, "unit": "dBm"},
    {"k": "al_new", "name": "New device", "hint": "red + alert: off/near/any", "type": "enum", "v": "near", "opts": ["off", "near", "any"]},
    {"k": "al_trk", "name": "Tracker", "hint": "tracker alert: off/near/any", "type": "enum", "v": "near", "opts": ["off", "near", "any"]},
    {"k": "al_newfor", "name": "New for", "hint": "how long a device is new", "type": "range", "v": 30, "min": 10, "max": 300, "step": 10, "unit": "s"},
    {"k": "al_warm", "name": "Warm-up", "hint": "after connect nothing is new", "type": "range", "v": 20, "min": 0, "max": 120, "step": 5, "unit": "s"},
    {"k": "al_sens", "name": "Sensitivity", "hint": "activity level gain", "type": "range", "v": 5, "min": 1, "max": 10, "step": 1, "unit": ""},
    {"k": "al_warn", "name": "Attention at", "hint": "activity level: attention", "type": "range", "v": 60, "min": 20, "max": 95, "step": 5, "unit": "%"},
    {"k": "al_alarm", "name": "Alarm at", "hint": "activity level: alarm", "type": "range", "v": 80, "min": 30, "max": 100, "step": 5, "unit": "%"},
    {"k": "al_cble", "name": "Count BLE", "hint": "new devices raise activity", "type": "bool", "v": 1},
    {"k": "al_crf", "name": "Count 433", "hint": "433 packets raise activity", "type": "bool", "v": 1},
    {"k": "al_csnd", "name": "Count sound", "hint": "loudness raises activity", "type": "bool", "v": 1},
    {"k": "al_cprb", "name": "Count IR/NFC", "hint": "remotes/cards raise activity", "type": "bool", "v": 1},
    {"k": "al_blink", "name": "Blinking", "hint": "blink on alerts", "type": "bool", "v": 1},
    {"k": "al_plate", "name": "Alert plate", "hint": "show proximity warning", "type": "bool", "v": 1}]})
LABELS = {}


def set_item(k, v):
    for pg in SETTINGS["pages"]:
        for it in pg["items"]:
            if it["k"] != k:
                continue
            if it["type"] == "info":
                return {"ok": False, "err": "read-only"}
            if it["type"] == "enum":
                opts = it["opts"]
                m = [o for o in opts if o.lower() == v.lower()]
                if not m and v.isdigit() and int(v) < len(opts):
                    m = [opts[int(v)]]
                if not m:
                    return {"ok": False, "err": "bad option"}
                it["v"] = m[0]
            elif it["type"] == "bool":
                it["v"] = 1 if v.lower() in ("1", "true", "on") else 0
            else:
                try:
                    it["v"] = max(it["min"], min(it["max"], int(v)))
                except ValueError:
                    return {"ok": False, "err": "bad number"}
            if k in ("al_warn", "al_alarm"):                  # alarm is always above warning (as in firmware)
                its = {x["k"]: x for x in pg["items"]}
                if k == "al_warn" and its["al_alarm"]["v"] <= it["v"]:
                    its["al_alarm"]["v"] = min(100, it["v"] + 5)
                if k == "al_alarm" and its["al_warn"]["v"] >= it["v"]:
                    its["al_warn"]["v"] = max(20, it["v"] - 5)
            return {"ok": True, "item": it}
    return {"ok": False, "err": "unknown key"}


def ble():
    t = time.time()
    d = []
    for i in range(22):
        if i == 21 and int(t / 9) % 2:            # one device comes and goes
            continue
        drift = 6 * math.sin(t * 0.05 + i)        # slow drift
        r = int(BASE[i] + drift + random.uniform(-3, 3))   # raw noise +-3 dB, so smoothing has work to do
        d.append([1000 + i, r, TYPES[i % 10], VEND[i % 10], NAMES[i % 10], 30 + i * 41, 1 + i % 3, LABELS.get(1000 + i, "")])
    k = int(t / 25)                                # guests: every 25 s new devices (near and far)
    for j in range(3):
        d.append([5000 + k * 3 + j, [-52, -68, -84][j], ["phone", "tracker", "watch"][j], "private", "", 5, 1, ""])
    return {"on": 1, "n": len(d), "m": len(d) + 7, "d": d}


WIFI_N = [("Keenetic-7741", 1, -48, "WPA2"), ("MGTS_GPON_5A2C", 6, -62, "WPA2"), ("TP-Link_3F10", 11, -71, "WPA2"),
          ("DIRECT-7B-HP", 6, -80, "WPA2"), ("Beeline_2G", 1, -76, "WPA2"), ("iPhone", 11, -58, "WPA3"),
          ("RT-WiFi-8820", 3, -84, "WPA2"), ("Xiaomi_A1F2", 9, -69, "WPA3"), ("eduroam", 13, -88, "WPA2-ENT"),
          ("Кафе Ромашка", 4, -74, "open"), ("", 8, -90, "WPA2"), ("Galaxy A54", 11, -66, "WPA3")]


def wifi():
    return {"scanned": True, "nets": [{"s": s, "b": "AA:BB:CC:DD:EE:%02X" % i, "v": ["TP-Link", "Huawei", "Xiaomi", ""][i % 4],
                                       "r": int(r + random.uniform(-3, 3)), "c": c, "e": e} for i, (s, c, r, e) in enumerate(WIFI_N)]}


def snd():
    t = time.time()
    beat = max(0, math.sin(t * 2 * math.pi * 2.05)) ** 10
    b = "".join("%02x" % int(255 * min(1, max(0, math.exp(-k / 14) * (0.5 + 0.3 * math.sin(t * 1.7 + k * 0.37)) + (beat * 0.5 if k < 6 else 0) + random.random() * 0.05))) for k in range(32))
    w = "".join("%02x" % (int(100 * math.sin(i * 0.4 + t * 20) * (0.4 + beat)) & 255) for i in range(64))
    lv = -34 + beat * 20
    return {"en": 1, "ok": 1, "lv": round(lv, 1), "pk": round(lv + 4, 1), "hz": 120 + int(300 * beat), "e": round(0.3 + beat * 0.6, 2), "b": b, "w": w}


SUB = {"burst": None}


def sub(mode, band):
    t = time.time()
    lo, hi = [(314.0, 318.0), (431.92, 435.92), (866.0, 870.0), (913.0, 917.0)][band % 4]
    if SUB["burst"] is None and random.random() < 0.05:
        SUB["burst"] = [random.randint(10, 85), random.uniform(-60, -40), t + random.uniform(0.2, 0.7)]
    b = SUB["burst"]
    if b and t > b[2]:
        SUB["burst"] = b = None
    cur = []
    for i in range(96):
        v = -106 + random.random() * 6
        v = max(v, -64 - abs(i - 48) * 9)
        if b:
            v = max(v, b[1] - (i - b[0]) ** 2 * 1.6)
        cur.append(int(v))
    if mode == 0:
        return {"ok": True, "mode": 0, "band": band, "lo": lo, "hi": hi, "mon": (lo + hi) / 2, "cur": cur, "peak": [c + 4 for c in cur]}
    return {"ok": True, "mode": 1, "band": band, "lo": lo, "hi": hi, "mon": (lo + hi) / 2, "floor": -100, "burst": b is not None,
            "events": 7, "rssi": [int(-100 + (40 if b and k > 110 else 0) + random.random() * 4) for k in range(120)],
            "ev": [{"ago": 3, "ms": 120, "pk": -52}, {"ago": 41, "ms": 800, "pk": -61}]}


APPS = [("chrome", 18, 2100, 1), ("Code", 9, 1200, 0), ("Telegram", 2, 420, 0), ("Yandex Music", 6, 380, 0), ("explorer", 1, 160, 0),
        ("python", 4, 90, 0), ("Discord", 3, 520, 0), ("svchost", 1, 300, 0), ("MsMpEng", 2, 240, 0), ("obs64", 12, 610, 0)]


def pc():
    t = time.time()
    cores = [int(max(2, min(100, 25 + 20 * math.sin(t * 0.3 + i) + random.uniform(-6, 6) + (55 if i == 3 and int(t / 20) % 2 else 0)))) for i in range(8)]
    cpu = sum(cores) / len(cores)
    pos = int(t - T0) % 214
    return {"live": 1, "age": 0, "host": "THINKPAD", "cpu": round(cpu, 1), "ghz": 2.8, "c": cores, "ram": 61.3, "ru": 9.8, "rt": 16.0,
            "disk": 71, "dr": round(abs(math.sin(t)) * 30, 1), "dw": 2.0, "nd": int(400 + 300 * math.sin(t)), "nu": 40, "bat": 77, "plug": 1,
            "up": 7300 + int(t - T0), "top": "chrome", "vol": 40, "mute": 0,
            "trk": {"t": "Ночной проспект", "a": "Neon Rain", "app": "Yandex Music", "play": 1, "pos": pos, "dur": 214},
            "city": [[n, round(c * (0.6 + 0.4 * abs(math.sin(t * 0.2 + i))), 1), m, fg, [14, 15, 21, 20, 8, 9, 16, 22, 27, 26][i]] for i, (n, c, m, fg) in enumerate(APPS)]}


def sat():
    # ISS: elements in the firmware's Sat units (radians, rad/min, km)
    n0 = 15.5 * 2 * math.pi / 1440
    a = (398600.4418 / (n0 / 60) ** 2) ** (1 / 3)
    epoch = time.time() - 3600 * 5
    r = math.radians
    return {"city": "Moscow", "lat": 55.756, "lon": 37.617,
            "sats": [["ISS", int(epoch), n0, 1e-10, r(51.64), r(120.0), 0.0003, r(90.0), r(270.0), a],
                     ["HUBBLE", int(epoch), 15.09 * 2 * math.pi / 1440, 0, r(28.47), r(200.0), 0.0002, r(40.0), r(10.0), 6917.0]]}


WX = {"hist": [], "histT": 0}


IR_FAKE = [("NEC", 0x04, 0x08, 32, "LG TV"), ("SAMSUNG", 0x07, 0x02, 32, "Samsung TV"), ("SONY12", 1, 0x15, 12, "Sony TV"),
           ("RC5", 0, 0x0C, 13, "Philips TV"), ("NEC-EXT", 0xEF00, 0x05, 32, "RGB LED strip / lamp"), ("RAW", 0, 0x76D1A5F3, 71, "long frame: air conditioner?")]


def ir_ev():
    """IR: random remote (like the stick's "ir" event)."""
    p, a, c, b, br = random.choice(IR_FAKE)
    raw = [9000, 4500] + [x for _ in range(31) for x in (560, random.choice((560, 1690)))]
    return {"p": p, "addr": a, "cmd": c, "bits": b, "brand": br, "rep": int(random.random() < 0.3), "raw": raw[:64],
            "t": int(time.time() * 1000) & 0x7FFFFFFF}


NFC_FAKE = [{"fam": "MIFARE Classic 1K", "hint": "transit pass / door key - likely", "uid": "3A91\u2022\u2022", "atqa": "0004", "sak": "08", "ndef": []},
            {"fam": "NTAG213", "hint": "sticker / tag - 144 bytes", "uid": "04A2\u2022\u2022", "atqa": "0044", "sak": "00",
             "ndef": ["https://tengan.dev/hello", "Hello, TENGAN"]},
            {"fam": "Bank card (EMV)", "hint": "tap-to-pay card or phone", "uid": "084F\u2022\u2022", "atqa": "0004", "sak": "20", "ndef": []}]


def nfc_ev():
    """NFC: random card (like the stick's "nfc" event)."""
    d = dict(random.choice(NFC_FAKE)); d["present"] = 1; d["t"] = int(time.time() * 1000) & 0x7FFFFFFF
    return d


def wx():
    # radio weather: slowly drifting values + a clear > clouds > rain > storm cycle (~2 min)
    t = time.time()
    ph = (t / 120) % 1
    if __import__('os').environ.get('WX') == 'storm': ph = 0.44        # for screenshots: storm right now
    storm = max(0.0, math.sin(ph * math.pi * 2 - 1.2)) ** 2
    clouds = min(1, 0.25 + 0.5 * (0.5 + 0.5 * math.sin(t / 37)) + storm * 0.4)
    rain = min(1, storm * 1.1 + max(0, math.sin(t / 23)) * 0.15)
    wind = 0.3 + 0.3 * math.sin(t / 17) + storm * 0.3
    thunder = 1 if storm > 0.55 and int(t) % 7 == 0 else 0
    cond = "storm" if storm > 0.5 else "rain" if rain > 0.35 else "windy" if wind > 0.6 else "overcast" if clouds > 0.75 else "cloudy" if clouds > 0.4 else "clear" if clouds > 0.1 else "calm"
    if t - WX["histT"] > 1:
        WX["histT"] = t; WX["hist"].append([round(clouds, 2), round(rain, 2)]); WX["hist"] = WX["hist"][-60:]
    return {"ok": 1, "cond": cond, "clouds": round(clouds, 3), "wind": round(wind, 3), "rain": round(rain, 3), "storm": round(storm, 3),
            "thunder": thunder, "trend": round(-math.cos(ph * math.pi * 2 - 1.2), 2), "ble": int(8 + clouds * 40), "turn": int(wind * 20),
            "sub": int(rain * 9), "subPk": int(-70 + rain * 25), "snd": int(-50 + storm * 30), "pk": int(-40 + storm * 30), "hist": WX["hist"],
            "temp": round(4.2 + 0.4 * math.sin(t / 300), 1), "hum": 81, "tsrc": "Nexus"}   # outdoor: 433 sensor (null if not heard)


SENS = {"t0": time.time(), "pk": 0, "last": 0.0, "lastKind": "sensor"}


def sens():
    """433 sensors (like the stick's "sens" event): three thermometers and a doorbell; a packet every ~4 s."""
    t = time.time()
    el = t - SENS["t0"]
    nx_t = round(4.2 + 0.4 * math.sin(t / 300), 1)
    lc_t = round(21.4 + 0.2 * math.sin(t / 200), 1)
    def ago(period, off): return int((el + off) % period)
    rows = [
        {"id": "nexus-1-0x5a", "p": "Nexus", "ch": 1, "kind": "th", "t": nx_t, "h": 81, "bat": 1, "rssi": -62, "ago": ago(57, 3), "n": 5 + int(el / 57)},
        {"id": "lacrosse-tx141th-1-0x9b", "p": "LaCrosse-TX141TH", "ch": 1, "kind": "th", "t": lc_t, "h": 38, "bat": 1, "rssi": -78, "ago": ago(50, 20), "n": 3 + int(el / 50)},
        {"id": "nexus-3-0x2c", "p": "Nexus", "ch": 3, "kind": "t", "t": -1.6, "h": None, "bat": 0, "rssi": -88, "ago": 700 + int(el), "n": 1},
        {"id": "remote-7a0b", "p": "Remote", "ch": 0, "kind": "remote", "t": None, "h": None, "bat": None, "btn": 1, "rssi": -71, "ago": ago(40, 10), "n": 2 + int(el / 40)},
    ]
    if t - SENS["last"] > 4:                      # a packet arrived
        SENS["last"] = t; SENS["pk"] += 1
        SENS["lastKind"] = "remote" if SENS["pk"] % 4 == 0 else "sensor"
    if SENS["lastKind"] == "sensor":             # Nexus: 36-bit PPM, drawn as 48 pulse/gap pairs
        raw = int(round(nx_t * 10)) & 0xFFF
        b = [0x5A, 0x80 | (raw >> 8), raw & 0xFF, 0xF0 | (81 >> 4), (81 << 4) & 0xFF]
        bits = [(b[i >> 3] >> (7 - (i & 7))) & 1 for i in range(36)]
        pt = []
        for bit in bits + bits: pt += [500, 2000 if bit else 1000]
        pt = pt[:72]; pt[71] = 4000
        last = {"p": "Nexus", "cls": "sensor", "pulses": 444, "bits": 36, "rows": 12, "mod": "PPM", "hex": "".join("%02x" % x for x in b),
                "short": 500, "long": 500, "gs": 1000, "gl": 2000, "gap": 4000, "hash": "e30b", "pt": pt}
    else:                                         # doorbell: only the start of the packet, no code
        pt = [350, 4000] + [350, 1050, 1050, 350, 350, 1050, 1050, 350, 1050, 350, 350, 1050, 1050, 350, 1050, 350, 350, 1050, 350, 1050, 1050, 350]
        last = {"p": "Remote", "cls": "remote", "pulses": 150, "bits": 25, "rows": 6, "mod": "PWM", "hex": "",
                "short": 350, "long": 1050, "gs": 350, "gl": 1050, "gap": 10850, "hash": "7a0b", "pt": pt}
    last.update({"rssi": -62 if SENS["lastKind"] == "sensor" else -71, "ago": int(t - SENS["last"]), "us": 902000})
    rows.sort(key=lambda r: (r["kind"] == "remote", r["ago"] > 600, -r["rssi"]))
    return {"on": 1, "pk": SENS["pk"], "noise": 3, "sensors": rows, "last": last}


def st():
    return {"name": "TENGAN", "up": int(time.time() - T0) + 3600, "bat": 87, "mv": 3980, "usb": 0, "chg": 0, "cli": 1, "heap": 180, "radio": "bwsmp", "theme": THEME, "lang": LANG}


class Api(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if u.path in ("/", "/full", "/tablet"):     # "/" offers LITE / FULL, "/full" goes straight to the full scene
            body, ct = HTML, "text/html"
        elif u.path in ("/lite", "/phone"):
            body, ct = "<h1>phone page (на палке — web_page.h)</h1>".encode(), "text/html"
        elif u.path == "/api/bledev":
            body, ct = json.dumps({"addr": [{"mac": "4A:11:22:33:44:55", "kind": "rotating (private)", "r": -52, "ago": 1}], "n": "AirPods Pro", "t": "audio", "v": "Apple",
                                   "r": -52, "rmin": -71, "rmax": -44, "pk": 812, "age": 720, "cid": "0x004C", "uu": [{"u": "FD6F", "o": "Exposure"}], "mfr": "4C0012"}).encode(), "application/json"
        elif u.path == "/api/pagever":             # page version, for the app's self-update
            import hashlib
            body, ct = json.dumps({"sha": hashlib.sha256(HTML).hexdigest(), "gz": 0}).encode(), "application/json"
        elif u.path == "/api/settings":
            body, ct = json.dumps(SETTINGS, ensure_ascii=False).encode(), "application/json"
        elif u.path == "/api/set":
            body, ct = json.dumps(set_item(q.get("k", [""])[0], q.get("v", [""])[0]), ensure_ascii=False).encode(), "application/json"
        elif u.path == "/api/label":
            i, n = int(q.get("id", ["0"])[0] or 0), q.get("n", [""])[0]
            if n:
                LABELS[i] = n[:20]
            else:
                LABELS.pop(i, None)
            body, ct = json.dumps({"ok": True, "stable": i % 2 == 0}).encode(), "application/json"
        elif u.path == "/api/act":
            print("act", q.get("a"), q.get("v"), flush=True)
            body, ct = b'{"ok":true}', "application/json"
        elif u.path == "/api/pckey":               # music remote
            print("key", q.get("k"), flush=True)
            body, ct = b'{"ok":true}', "application/json"
        elif u.path == "/api/loc":                 # GPS from the tablet app
            print("loc", q.get("lat"), q.get("lon"), flush=True)
            body, ct = b'{"ok":true}', "application/json"
        elif u.path == "/api/sens":
            body, ct = json.dumps(sens()).encode(), "application/json"
        elif u.path == "/api/sub":
            body, ct = json.dumps(sub(int(q.get("m", ["0"])[0]), int(q.get("b", ["1"])[0]))).encode(), "application/json"
        else:
            self.send_response(404); self.end_headers(); return
        self.send_response(200); self.send_header("Content-Type", ct); self.send_header("Access-Control-Allow-Origin", "*"); self.end_headers(); self.wfile.write(body)


class Sse(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a): pass

    def do_GET(self):
        q = parse_qs(urlparse(self.path).query)
        want = q.get("want", [""])[0]
        sm, sb = int(q.get("sm", ["0"])[0]), int(q.get("sb", ["1"])[0])
        self.send_response(200); self.send_header("Content-Type", "text/event-stream"); self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*"); self.end_headers()

        def ev(name, d): self.wfile.write(("event: %s\ndata: %s\n\n" % (name, json.dumps(d, ensure_ascii=False))).encode()); self.wfile.flush()
        tb = tw = ts = tsub = tpc = twx = tsens = 0
        tir, tnfc = time.time(), time.time()
        sent_sat = False
        try:
            while True:
                now = time.time()
                if now - ts > 2:
                    ts = now; ev("st", st())
                    if not sent_sat: sent_sat = True; ev("sat", sat())
                if "ble" in want and now - tb > 1: tb = now; ev("ble", ble())
                if "wifi" in want and now - tw > 4: tw = now; ev("wifi", wifi())
                if "snd" in want: ev("snd", snd())
                if "sub" in want and now - tsub > 0.14: tsub = now; ev("sub", sub(sm, sb))
                if "pc" in want and now - tpc > 1: tpc = now; ev("pc", pc())
                if "wx" in want and now - twx > 1: twx = now; ev("wx", wx())
                if "sens" in want.split(",") and now - tsens > 1: tsens = now; ev("sens", sens())
                if "ir" in want.split(",") and now - tir > 2.5: tir = now; ev("ir", ir_ev())
                if "nfc" in want.split(",") and now - tnfc > 6: tnfc = now; ev("nfc", nfc_ev())
                time.sleep(0.04)
        except Exception:
            pass


threading.Thread(target=lambda: http.server.ThreadingHTTPServer(("127.0.0.1", PORT + 1), Sse).serve_forever(), daemon=True).start()
http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Api).serve_forever()
