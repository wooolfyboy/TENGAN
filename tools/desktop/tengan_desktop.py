# -*- coding: utf-8 -*-
"""
TENGAN — приложение для компьютера (Windows).

Одно окно со штабом палки (та же страница FULL, что на планшете) + монитор ПК
(tools/pc_monitor) в одной программе и на одном USB-кабеле.

Режимы связи (выбор при запуске, последний запоминается):
  USB   — всё по кабелю: страницу и данные палка отдаёт строками «@…»
          (протокол — в src/web/web.cpp, «Приложение для компьютера по USB»).
          Интернет на компьютере остаётся.
  Wi-Fi — как планшет: компьютер подключён к точке палки, окно открывает
          http://192.168.4.1/full. Если кабель тоже воткнут — монитор ПК шлёт
          нагрузку и музыку по нему.
  Авто  — USB, если палка на кабеле, иначе Wi-Fi.

Клавиши: F11 — во весь экран, F2 — снова выбор режима.
"""
import base64
import gzip
import hashlib
import http.server
import json
import os
import socketserver
import sys
import threading
import time
import urllib.parse
from collections import deque

APP_NAME = "TENGAN"
PORT = 47321                       # постоянный адрес страницы — чтобы планшетные настройки (вкладки) запоминались
STICK_URL = "http://192.168.4.1/full"

# ---------- куда писать журнал (у окна без консоли нет stdout) ----------
APPDIR = os.path.join(os.environ.get("APPDATA") or os.path.expanduser("~"), "TENGAN")
os.makedirs(APPDIR, exist_ok=True)
if getattr(sys, "frozen", False) or sys.stdout is None:
    _log = open(os.path.join(APPDIR, "desktop.log"), "a", encoding="utf-8", buffering=1)
    sys.stdout = _log
    sys.stderr = _log

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "pc_monitor"))
import pc_monitor as pm            # noqa: E402  (сбор нагрузки, музыка, звук, медиа-клавиши, поиск палки)


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


# ---------- настройки приложения ----------
CFG_PATH = os.path.join(APPDIR, "desktop.json")


def cfg_load():
    try:
        with open(CFG_PATH, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def cfg_save(c):
    try:
        with open(CFG_PATH, "w", encoding="utf-8") as f:
            json.dump(c, f)
    except Exception:
        pass


# ============================================================
#  Связь с палкой по USB: один порт на всё.
#  Строки «@…» — приложение (поток, ответы, страница), остальные — монитор ПК
#  (KEY …, AUD …). Пишем через pm.send (общий замок): строки не перемешиваются.
# ============================================================
class Link:
    def __init__(self, find_port=None, open_port=None):
        self.find_port = find_port or pm.find_port
        self.open_port = open_port or pm.open_port
        self.port = None
        self.port_name = ""
        self.lock = threading.Lock()
        self.cv = threading.Condition()
        self.events = deque(maxlen=3000)
        self.want_q = ""
        self.want_t = 0.0
        self.req_id = 0
        self.pending = {}
        self.pages = {}
        self.on_line = None            # монитор ПК: строки без «@» (KEY / AUD)
        self.on_port = None            # монитор ПК: порт открылся / пропал
        self.stop = False
        threading.Thread(target=self._keeper, daemon=True).start()

    # --- порт ---
    def connected(self):
        return self.port is not None

    def _keeper(self):
        while not self.stop:
            if self.port is None:
                name = self.find_port()
                if name:
                    try:
                        p = self.open_port(name)
                        p.timeout = 0.2
                        self.port, self.port_name = p, name
                        log("USB: палка на", name)
                        threading.Thread(target=self._reader, args=(p,), daemon=True).start()
                        if self.on_port:
                            self.on_port(p)
                        if self.want_q:
                            self.send_line("@W " + self.want_q)
                    except Exception as e:
                        log("USB: не открылся", name, e)
                        time.sleep(2)
                        continue
                else:
                    time.sleep(1)
                    continue
            # «я здесь» раз в 2 с — палка держит подписку, пока слышит приложение
            if self.want_q and time.time() - self.want_t > 2:
                self.send_line("@W " + self.want_q)
            time.sleep(0.25)

    def _lost(self, p):
        if self.port is p:
            log("USB: связь пропала")
            self.port = None
            try:
                p.close()
            except Exception:
                pass
            if self.on_port:
                self.on_port(None)
            self._push("_x", '"nostick"')

    def send_line(self, s):
        p = self.port
        if p is None:
            return False
        try:
            pm.send(p, (s + "\n").encode("utf-8"))
            if s.startswith("@W"):
                self.want_t = time.time()
            return True
        except Exception:
            self._lost(p)
            return False

    def _reader(self, p):
        buf = b""
        while self.port is p and not self.stop:
            try:
                chunk = p.read(p.in_waiting or 1)
            except Exception:
                self._lost(p)
                return
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").rstrip("\r")
                if line:
                    try:
                        self.handle(line)
                    except Exception as e:
                        log("строка не разобрана:", e, line[:80])

    # --- входящие строки ---
    def handle(self, line):
        if line.startswith("@E "):
            ev, _, js = line[3:].partition(" ")
            self._push(ev, js)
        elif line.startswith("@R "):
            rid, _, js = line[3:].partition(" ")
            w = self.pending.get(rid)
            if w:
                w[1] = js
                w[0].set()
        elif line.startswith("@g "):
            self._page_part(line[3:])
        elif line.startswith("@X "):
            self._push("_x", json.dumps(line[3:].strip()))
        elif self.on_line:
            self.on_line(line)

    def _push(self, ev, js):
        with self.cv:
            if ev == "snd" and len(self.events) > 200:   # страница не успевает — старый звук не нужен
                return
            self.events.append((ev, js))
            self.cv.notify_all()

    # --- для страницы ---
    def want(self, q):
        self.want_q = q
        self.send_line("@W " + q)

    def poll(self, wait=0.25):
        with self.cv:
            if not self.events:
                self.cv.wait(wait)
            items = list(self.events)
            self.events.clear()
        # готовый JSON без перекодирования: данные палки вставляются как есть
        return "[" + ",".join('["%s",%s]' % (ev, js) for ev, js in items) + "]"

    def api(self, path, timeout=3.0):
        with self.lock:
            self.req_id += 1
            rid = str(self.req_id)
        w = [threading.Event(), None]
        self.pending[rid] = w
        try:
            if not self.send_line("@Q %s %s" % (rid, path)):
                return '{"ok":false,"err":"no stick on USB"}'
            if not w[0].wait(timeout):
                return '{"ok":false,"err":"timeout"}'
            return w[1]
        finally:
            self.pending.pop(rid, None)

    # --- страница с палки (gzip кусками) ---
    def _page_part(self, rest):
        parts = rest.split(" ")
        pid = parts[0]
        pg = self.pages.get(pid)
        if pg is None:
            return
        if parts[1] == "begin":
            pg["size"] = int(parts[2])
            pg["sha"] = parts[3] if len(parts) > 3 else ""
        elif parts[1] == "end":
            pg["done"].set()
        else:
            pg["chunks"][int(parts[1])] = base64.b64decode(parts[2])
            pg["got"] = sum(len(c) for c in pg["chunks"].values())

    def get_page(self, timeout=60, progress=None):
        with self.lock:
            self.req_id += 1
            pid = str(self.req_id)
        pg = {"chunks": {}, "size": 0, "got": 0, "sha": "", "done": threading.Event()}
        self.pages[pid] = pg
        try:
            if not self.send_line("@G " + pid):
                raise RuntimeError("палка не на USB")
            t0 = time.time()
            while not pg["done"].wait(0.2):
                if progress and pg["size"]:
                    progress(pg["got"], pg["size"])
                if time.time() - t0 > timeout:
                    raise RuntimeError("палка не прислала страницу")
            data = b"".join(pg["chunks"][k] for k in sorted(pg["chunks"]))
            if pg["size"] and len(data) != pg["size"]:
                raise RuntimeError("страница пришла не целиком (%d из %d)" % (len(data), pg["size"]))
            html = gzip.decompress(data).decode("utf-8")
            if pg["sha"] and hashlib.sha256(html.encode("utf-8")).hexdigest()[:len(pg["sha"])] != pg["sha"]:
                log("страница: контрольная сумма не совпала (", pg["sha"], ") — показываю как есть")
            return html
        finally:
            self.pages.pop(pid, None)


# ============================================================
#  Монитор ПК — тот же, что tools/pc_monitor, только на общем порту
# ============================================================
class Monitor:
    def __init__(self, link):
        self.link = link
        self.col = pm.Collector()
        self.media = pm.NowPlaying()
        self.tap = pm.AudioTap()
        self.orbits = pm.Orbits()
        self.wake = threading.Event()
        link.on_line = self.on_line
        link.on_port = self.on_port
        threading.Thread(target=self.run, daemon=True).start()

    def on_port(self, p):
        self.tap.port = p

    def on_line(self, line):
        if line == "AUD ON":
            self.tap.request()
        elif line == "AUD OFF":
            self.tap.stop()
        elif line.startswith("KEY "):
            pm.press(line[4:].strip())
            self.wake.set()

    def run(self):
        last_tle = 0.0
        next_t = time.monotonic()
        while True:
            port = self.link.port
            if port is not None:
                try:
                    if self.wake.is_set():
                        self.wake.clear()
                        time.sleep(0.05)
                        v = pm.read_volume()
                        if v:
                            pm.send(port, ("PV vol=%d mute=%d\n" % (v[0], 1 if v[1] else 0)).encode())
                        self.wake.wait(max(0.0, next_t - time.monotonic()))
                        continue
                    line, _cpu, _ram = self.col.line()
                    pm.send(port, line.encode("ascii", "replace"))
                    v = pm.read_volume()
                    if v:
                        pm.send(port, ("PV vol=%d mute=%d\n" % (v[0], 1 if v[1] else 0)).encode())
                    if self.media.line:
                        pm.send(port, self.media.line.encode("utf-8"))
                    if self.col.city:
                        pm.send(port, self.col.city.encode("ascii", "replace"))
                    if self.orbits.lines and time.time() - last_tle > 60:
                        last_tle = time.time()
                        for l in self.orbits.lines:
                            pm.send(port, l.encode("ascii", "ignore"))
                except Exception as e:
                    log("монитор: ошибка записи", e)
                    self.link._lost(port)
            next_t += 1.0
            wait = next_t - time.monotonic()
            if wait < 0:
                next_t = time.monotonic()
                wait = 0
            self.wake.wait(wait)


# ============================================================
#  Страницы окна: выбор режима и штаб (локальный сервер 127.0.0.1)
# ============================================================
BRIDGE = """<script>
window.TENGAN_DESKTOP={mode:'usb'};
(function(){var f=window.fetch.bind(window);var t=function(u){return f(u,{cache:'no-store'}).then(function(r){return r.text()})};
window.TenganUSB={want:function(q){return t('/__link/want?q='+encodeURIComponent(q))},
 api:function(p){return t('/__link/api?p='+encodeURIComponent(p))},
 poll:function(){return t('/__link/poll')}};})();
</script>"""

# F11 / F2 — через мост pywebview (он есть на любой странице окна, и на странице палки по Wi-Fi)
KEYS = """(function(){if(window.__tgKeys)return;window.__tgKeys=1;
document.addEventListener('keydown',function(e){var a=window.pywebview&&window.pywebview.api;if(!a)return;
 if(e.key==='F11'){e.preventDefault();a.fullscreen();}
 if(e.key==='F2'){e.preventDefault();a.home();}
});})();"""


class JsApi:
    """То, что страница окна может попросить у приложения (pywebview js_api)."""
    def __init__(self, app):
        self._app = app

    def fullscreen(self):
        if self._app.window:
            self._app.window.toggle_fullscreen()

    def home(self):
        self._app.launcher()


LAUNCHER = """<!doctype html><html><head><meta charset="utf-8"><title>TENGAN</title>
<style>
:root{--bg:#05080d;--acc:#3ee6ff;--hot:#ebfaff;--dim:#5a7080;--line:#203240}
html,body{margin:0;height:100%;background:var(--bg);color:var(--hot);font:16px/1.4 'Segoe UI',system-ui,sans-serif}
.w{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:28px}
h1{margin:0;font-weight:600;letter-spacing:.3em;font-size:40px}h1 b{color:var(--acc)}
.sub{color:var(--dim);letter-spacing:.15em;text-transform:uppercase;font-size:13px}
.row{display:flex;gap:16px}
button{background:rgba(14,42,52,.4);color:var(--hot);border:1px solid var(--line);padding:22px 30px;min-width:190px;
 font:600 18px 'Segoe UI',sans-serif;letter-spacing:.12em;cursor:pointer;text-align:left}
button small{display:block;font-weight:400;font-size:12px;color:var(--dim);letter-spacing:.05em;margin-top:6px}
button:hover,button.on{border-color:var(--acc);box-shadow:0 0 18px rgba(62,230,255,.25)}
#st{min-height:22px;color:var(--acc);letter-spacing:.08em}
.bar{width:420px;height:3px;background:var(--line)}.bar i{display:block;height:100%;width:0;background:var(--acc)}
.hint{color:var(--dim);font-size:12px}
</style></head><body><div class="w">
<h1>TENGAN <b>天眼</b></h1><div class="sub">desktop · штаб и монитор ПК</div>
<div class="row">
<button data-m="usb">USB<small>по кабелю · интернет остаётся</small></button>
<button data-m="wifi">WI-FI<small>через точку палки</small></button>
<button data-m="auto">АВТО<small>USB, если палка на кабеле</small></button>
</div>
<div id="st"></div><div class="bar"><i id="pb"></i></div>
<div class="hint">F11 — во весь экран · F2 — снова этот выбор · на палке открой раздел Web</div>
</div><script>
var last='%LAST%',auto=%AUTO%,cd=3,timer=null;
var st=document.getElementById('st');
function go(m){clearInterval(timer);st.textContent='…';fetch('/__app/start?mode='+m).then(function(r){return r.text()}).then(function(t){if(t)st.textContent=t});}
document.querySelectorAll('button').forEach(function(b){if(b.dataset.m===last)b.classList.add('on');b.onclick=function(){go(b.dataset.m)}});
if(auto&&last){timer=setInterval(function(){if(cd<=0){go(last);return}st.textContent='запуск: '+last.toUpperCase()+' через '+cd+' с · или выбери другой';cd--},1000);}
setInterval(function(){fetch('/__app/status').then(function(r){return r.json()}).then(function(s){if(s.msg)st.textContent=s.msg;document.getElementById('pb').style.width=(s.pct||0)+'%'}).catch(function(){})},300);
</script></body></html>"""


class App:
    def __init__(self, link, open_window=True):
        self.link = link
        self.cfg = cfg_load()
        self.window = None
        self.html = None
        self.msg = ""
        self.pct = 0
        self.busy = False
        self.first = True
        self.port = PORT

    # --- переходы окна ---
    def launcher(self, autostart=False):
        if self.window:
            self.window.load_url("http://127.0.0.1:%d/" % self.port + ("?auto=1" if autostart else ""))

    def start(self, mode):
        if self.busy:
            return "подожди…"
        self.cfg["mode"] = mode
        cfg_save(self.cfg)
        if mode == "auto":
            mode = "usb" if (self.link.connected() or self.link.find_port()) else "wifi"
        if mode == "wifi":
            self.msg = "открываю 192.168.4.1 — компьютер должен быть подключён к Wi-Fi палки"
            if self.window:
                self.window.load_url(STICK_URL)
            return self.msg
        threading.Thread(target=self._start_usb, daemon=True).start()
        return "ищу палку на USB…"

    def _start_usb(self):
        self.busy = True
        try:
            t0 = time.time()
            while not self.link.connected():
                self.msg = "ищу палку на USB… подключи её кабелем и включи"
                if time.time() - t0 > 30:
                    self.msg = "палка не найдена на USB — проверь кабель (или выбери Wi-Fi)"
                    return
                time.sleep(0.3)

            def prog(got, size):
                self.pct = int(100 * got / max(1, size))
                self.msg = "загружаю штаб с палки… %d%%" % self.pct
            self.html = self.link.get_page(progress=prog)
            self.pct = 100
            self.msg = ""
            if self.window:
                self.window.load_url("http://127.0.0.1:%d/full" % self.port)
        except Exception as e:
            self.msg = "не вышло: %s" % e
            log("USB старт:", e)
        finally:
            self.busy = False

    def full_html(self):
        h = self.html or "<html><body style='background:#05080d;color:#fff'>нет страницы</body></html>"
        i = h.find("<head>")
        inj = BRIDGE + "<script>" + KEYS + "</script>"
        return h[:i + 6] + inj + h[i + 6:] if i >= 0 else inj + h


def make_handler(app):
    link = app.link

    class H(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def _send(self, body, ctype="application/json; charset=utf-8", code=200):
            b = body.encode("utf-8") if isinstance(body, str) else body
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(b)))
            self.end_headers()
            self.wfile.write(b)

        def do_GET(self):
            u = urllib.parse.urlparse(self.path)
            q = urllib.parse.parse_qs(u.query)
            p = u.path
            if p == "/":
                last = app.cfg.get("mode", "")
                auto = "true" if ("auto" in q or app.first) and last else "false"
                app.first = False
                self._send(LAUNCHER.replace("%LAST%", last).replace("%AUTO%", auto), "text/html; charset=utf-8")
            elif p in ("/full", "/tablet"):
                self._send(app.full_html(), "text/html; charset=utf-8")
            elif p == "/__link/want":
                link.want(q.get("q", [""])[0])
                self._send('{"ok":true}')
            elif p == "/__link/api":
                self._send(link.api(q.get("p", [""])[0]))
            elif p == "/__link/poll":
                self._send(link.poll())
            elif p == "/__app/start":
                self._send(app.start(q.get("mode", ["auto"])[0]), "text/plain; charset=utf-8")
            elif p == "/__app/status":
                self._send(json.dumps({"msg": app.msg, "pct": app.pct, "usb": link.connected(), "port": link.port_name}))
            elif p == "/__app/fs":
                if app.window:
                    app.window.toggle_fullscreen()
                self._send('{"ok":true}')
            elif p == "/__app/home":
                app.launcher()
                self._send('{"ok":true}')
            else:
                self._send('{"ok":false}', code=404)

    return H


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def serve(app, port=PORT):
    """Локальный сервер окна. Порт постоянный (настройки страницы запоминаются);
    если занят (второе окно приложения) — следующий свободный."""
    for p in range(port, port + 20):
        try:
            srv = Server(("127.0.0.1", p), make_handler(app))
        except OSError:
            continue
        app.port = p
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        return srv
    raise RuntimeError("нет свободного порта для окна")


def main():
    import webview
    log("=== TENGAN desktop ===")
    link = Link()
    Monitor(link)
    app = App(link)
    serve(app)
    win = webview.create_window(APP_NAME, "http://127.0.0.1:%d/" % app.port, width=1600, height=1000,
                                background_color="#05080D", min_size=(900, 600), js_api=JsApi(app))
    app.window = win

    def on_loaded():
        try:
            win.evaluate_js(KEYS)             # F11 / F2 и на странице палки (режим Wi-Fi)
        except Exception:
            pass
    win.events.loaded += on_loaded
    webview.start(private_mode=False, storage_path=os.path.join(APPDIR, "webview"))


if __name__ == "__main__":
    main()
