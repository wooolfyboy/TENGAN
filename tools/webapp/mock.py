#!/usr/bin/env python3
"""Пробная «палка» для разработки приложения планшета без железа.

    python3 tools/webapp/build.py --html /tmp/tablet.html
    python3 tools/webapp/mock.py /tmp/tablet.html [8090] [tengan|sumi]
    открыть http://127.0.0.1:8090/tablet?host=127.0.0.1:8090&sse=8091

:8090 — /tablet и /api/*, :8091 — поток SSE /stream (как «вторая дверь» на порту 81).
Данные выдуманные, но той же формы, что шлёт прошивка (web.cpp, ble_core.cpp, pc.cpp, subghz.cpp).
"""
import http.server, json, math, random, sys, threading, time
from urllib.parse import urlparse, parse_qs

HTML = open(sys.argv[1] if len(sys.argv) > 1 else "tablet.html", "rb").read()
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8090          # API; поток SSE — на PORT+1
THEME = sys.argv[3] if len(sys.argv) > 3 else "denko"         # тема, которую «выбрали на палке»: denko / sumi
T0 = time.time()
TYPES = ["phone", "audio", "watch", "tracker", "computer", "unknown", "TV", "sensor", "band", "phone"]
NAMES = ["iPhone", "AirPods Pro", "Galaxy Watch5", "AirTag", "ThinkPad X1", "", "[TV] Samsung Q7", "LYWSD03MMC", "Mi Band 7", "Pixel 7"]
VEND = ["Apple", "Apple", "Samsung", "Apple", "Lenovo", "private", "Samsung", "Xiaomi", "Xiaomi", "Google"]
BASE = [-44 - i * 2.6 for i in range(22)]


def ble():
    t = time.time()
    d = []
    for i in range(22):
        if i == 21 and int(t / 9) % 2:            # одно устройство то появляется, то пропадает
            continue
        drift = 6 * math.sin(t * 0.05 + i)        # медленный уход
        r = int(BASE[i] + drift + random.uniform(-3, 3))   # сырой шум ±3 dB — пусть сглаживание работает
        d.append([1000 + i, r, TYPES[i % 10], VEND[i % 10], NAMES[i % 10], 30 + i * 41, 1 + i % 3])
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
    # МКС: элементы в тех же единицах, что Sat в прошивке (радианы, рад/мин, км)
    n0 = 15.5 * 2 * math.pi / 1440
    a = (398600.4418 / (n0 / 60) ** 2) ** (1 / 3)
    epoch = time.time() - 3600 * 5
    r = math.radians
    return {"city": "Moscow", "lat": 55.756, "lon": 37.617,
            "sats": [["ISS", int(epoch), n0, 1e-10, r(51.64), r(120.0), 0.0003, r(90.0), r(270.0), a],
                     ["HUBBLE", int(epoch), 15.09 * 2 * math.pi / 1440, 0, r(28.47), r(200.0), 0.0002, r(40.0), r(10.0), 6917.0]]}


WX = {"hist": [], "histT": 0}


def wx():
    # радиопогода: медленно плывущие значения + цикл «ясно → облака → дождь → гроза» (~2 минуты)
    t = time.time()
    ph = (t / 120) % 1
    if __import__('os').environ.get('WX') == 'storm': ph = 0.44        # для картинок: гроза прямо сейчас
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
            "sub": int(rain * 9), "subPk": int(-70 + rain * 25), "snd": int(-50 + storm * 30), "pk": int(-40 + storm * 30), "hist": WX["hist"]}


def st():
    return {"name": "TENGAN", "up": int(time.time() - T0) + 3600, "bat": 87, "mv": 3980, "usb": 0, "chg": 0, "cli": 1, "heap": 180, "radio": "bwsmp", "theme": THEME}


class Api(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if u.path in ("/", "/full", "/tablet"):     # «/» — с выбором LITE / FULL, «/full» — сразу полная сцена
            body, ct = HTML, "text/html"
        elif u.path in ("/lite", "/phone"):
            body, ct = "<h1>phone page (на палке — web_page.h)</h1>".encode(), "text/html"
        elif u.path == "/api/bledev":
            body, ct = json.dumps({"addr": [{"mac": "4A:11:22:33:44:55", "kind": "rotating (private)", "r": -52, "ago": 1}], "n": "AirPods Pro", "t": "audio", "v": "Apple",
                                   "r": -52, "rmin": -71, "rmax": -44, "pk": 812, "age": 720, "cid": "0x004C", "uu": [{"u": "FD6F", "o": "Exposure"}], "mfr": "4C0012"}).encode(), "application/json"
        elif u.path == "/api/pagever":             # версия страницы — для самообновления приложения
            import hashlib
            body, ct = json.dumps({"sha": hashlib.sha256(HTML).hexdigest(), "gz": 0}).encode(), "application/json"
        elif u.path == "/api/loc":                 # GPS с приложения планшета
            print("loc", q.get("lat"), q.get("lon"), flush=True)
            body, ct = b'{"ok":true}', "application/json"
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
        tb = tw = ts = tsub = tpc = twx = 0
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
                time.sleep(0.04)
        except Exception:
            pass


threading.Thread(target=lambda: http.server.ThreadingHTTPServer(("127.0.0.1", PORT + 1), Sse).serve_forever(), daemon=True).start()
http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Api).serve_forever()
