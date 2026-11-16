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

Плюс (WIDGET.md): значок в трее (закрытие окна — в трей), всплывашки при событиях
(в стиле темы TENGAN: DENKO / KURENAI) и мини-виджет поверх всех окон. Решения «что показать» — events.py,
окна — overlay.py.
"""
import base64
import gzip
import hashlib
import http.server
import json
import re
import os
import socketserver
import sys
import threading
import time
import urllib.parse
import urllib.request
from collections import deque

APP_NAME = "TENGAN Desktop"
PORT = 47321                       # постоянный адрес страницы — чтобы планшетные настройки (вкладки) запоминались
STICK_URL = "http://192.168.4.1/full"

# ---------- куда писать журнал (у окна без консоли нет stdout) ----------
if os.name == "nt":                                   # Windows: %APPDATA%\TENGAN
    APPDIR = os.path.join(os.environ.get("APPDATA") or os.path.expanduser("~"), "TENGAN")
elif sys.platform == "darwin":                        # macOS: ~/Library/Application Support/TENGAN
    APPDIR = os.path.join(os.path.expanduser("~"), "Library", "Application Support", "TENGAN")
else:                                                 # Linux: ~/.config/TENGAN
    APPDIR = os.path.join(os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config"), "TENGAN")
os.makedirs(APPDIR, exist_ok=True)
if getattr(sys, "frozen", False) or sys.stdout is None:
    _log = open(os.path.join(APPDIR, "desktop.log"), "a", encoding="utf-8", buffering=1)
    sys.stdout = _log
    sys.stderr = _log

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "pc_monitor"))
import pc_monitor as pm            # noqa: E402  (сбор нагрузки, музыка, звук, медиа-клавиши, поиск палки)
sys.path.insert(0, HERE)
from events import Engine, cfg_from_settings, KINDS   # noqa: E402  («мозг» оповещений — без окон)


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


# ---------- язык окна: последний известный язык прошивки TENGAN, иначе язык Windows ----------
TX = {
    "en": {
        "sub": "desktop · HQ and PC monitor", "usb_s": "over the cable · internet stays on", "wifi_s": "via the TENGAN access point",
        "auto": "AUTO", "auto_s": "USB if TENGAN is on the cable", "hint": "F11 — full screen · F2 — back to this screen · open Web on TENGAN",
        "countdown": "starting {m} in {n} s · or pick another", "wait": "please wait…",
        "wifi_open": "opening 192.168.4.1 — the computer must be on the TENGAN Wi-Fi",
        "usb_find": "looking for TENGAN on USB…", "usb_find2": "looking for TENGAN on USB… connect it with a cable and turn it on",
        "usb_none": "TENGAN not found on USB — check the cable (or choose Wi-Fi)",
        "usb_load": "loading HQ from TENGAN… {p}%", "failed": "failed: {e}", "no_page": "no page",
        "not_on_usb": "TENGAN is not on USB",
        "no_answer": "TENGAN does not answer — is firmware v1.8-test4 or newer installed? Is the old PC monitor closed?",
        "no_send": "TENGAN did not send the page", "partial": "the page arrived incomplete ({a} of {b})",
        "pp_btn": "⚙ ALERTS & WIDGET", "pp_h": "ALERTS <b>警報</b> · WIDGET",
        "pp_hint": "Near / new / tracker thresholds are shared with TENGAN and the tablet (HQ → ⚙ ALERTS). Here: what to show on this computer and how.",
        "pp_test": "TEST POPUP", "pp_done": "DONE",
        "tray_open": "Open HQ", "tray_widget": "Widget on top", "tray_popups": "Popup alerts", "tray_native": "System notifications instead of popups",
        "usb_perm": "no access to {p} — Linux: run `sudo usermod -aG dialout $USER`, then log out and back in",
        "tray_click": "Click-through widget", "tray_style": "Style", "tray_corner": "Corner", "tray_mode": "Connection", "tray_prefs": "Alert settings…",
        "tray_quit": "Quit", "test_text": "this is how an alert looks",
    },
    "ru": {
        "sub": "desktop · штаб и монитор ПК", "usb_s": "по кабелю · интернет остаётся", "wifi_s": "через точку доступа TENGAN",
        "auto": "АВТО", "auto_s": "USB, если TENGAN на кабеле", "hint": "F11 — во весь экран · F2 — снова этот выбор · на TENGAN открой раздел Web",
        "countdown": "запуск: {m} через {n} с · или выбери другой", "wait": "подожди…",
        "wifi_open": "открываю 192.168.4.1 — компьютер должен быть подключён к Wi-Fi TENGAN",
        "usb_find": "ищу TENGAN на USB…", "usb_find2": "ищу TENGAN на USB… подключи кабелем и включи",
        "usb_none": "TENGAN не найден на USB — проверь кабель (или выбери Wi-Fi)",
        "usb_load": "загружаю штаб с TENGAN… {p}%", "failed": "не вышло: {e}", "no_page": "нет страницы",
        "not_on_usb": "TENGAN не на USB",
        "no_answer": "TENGAN не отвечает — на нём прошивка v1.8-test4 или новее? Старый монитор ПК закрыт?",
        "no_send": "TENGAN не прислал страницу", "partial": "страница пришла не целиком ({a} из {b})",
        "pp_btn": "⚙ ОПОВЕЩЕНИЯ И ВИДЖЕТ", "pp_h": "ОПОВЕЩЕНИЯ <b>警報</b> · ВИДЖЕТ",
        "pp_hint": "Пороги «близко», «новое» и трекеры — общие с TENGAN и планшетом (штаб → ⚙ ALERTS). Здесь — что и как показывать на этом компьютере.",
        "pp_test": "ПРОВЕРИТЬ ПЛАШКУ", "pp_done": "ГОТОВО",
        "tray_open": "Открыть штаб", "tray_widget": "Виджет поверх окон", "tray_popups": "Всплывающие оповещения", "tray_native": "Системные уведомления вместо плашек",
        "usb_perm": "нет доступа к {p} — Linux: выполни `sudo usermod -aG dialout $USER` и перезайди в систему",
        "tray_click": "Сквозные клики по виджету", "tray_style": "Стиль", "tray_corner": "Угол", "tray_mode": "Связь", "tray_prefs": "Настройки оповещений…",
        "tray_quit": "Выход", "test_text": "так будет выглядеть оповещение",
    },
}


# настройки оповещений и виджета (панель на стартовом экране) — по языку окна
PP_TX = {
    "en": {
        "h_pop": "Popups", "h_kinds": "What to show", "h_quiet": "Quiet hours", "h_widget": "Widget", "h_window": "Window",
        "rows": {"popups": ["Show alerts", "a card in the screen corner, above all windows"],
                 "native": ["Windows notifications instead", "regular system ones, in the notification center"],
                 "style": ["Style", "follows the TENGAN theme, or fixed"], "corner": ["Screen corner", ""],
                 "duration": ["Shown for, s", ""], "per_min": ["At most per minute", "alarms may exceed the limit (up to twice)"],
                 "quiet": ["Enable", "no popups during these hours"], "quiet_from": ["From (hour)", ""], "quiet_to": ["To (hour)", ""],
                 "quiet_trackers": ["Still show a nearby tracker", ""],
                 "widget": ["Widget on top", "mini radar, activity, temperature, remote, card · double click — HQ"],
                 "opacity": ["Opacity", "0.3 … 1"], "clickthrough": ["Click-through", "clicks pass through the widget (turn off in the tray menu)"],
                 "tray": ["Close button — to tray", "keeps running by the clock; quit from the tray icon menu"]},
        "styles": [["auto", "as the TENGAN theme"], ["denko", "DENKO"], ["kurenai", "KURENAI"]],
        "corners": [["br", "bottom right"], ["bl", "bottom left"], ["tr", "top right"], ["tl", "top left"]],
        "kinds": {"new": ["New device nearby", "per the TENGAN alert thresholds"], "tracker": ["Tracker nearby", "AirTag and similar tags"],
                  "ir": ["Remote", "which remote and which button"], "nfc": ["NFC card / tag", "card type"],
                  "sensor": ["New 433 MHz sensor", "thermometer, doorbell, gate remote"], "temp": ["Outdoor temperature", "if it changed by 1° or more"],
                  "battery": ["TENGAN battery", "below 15%"], "link": ["Link to TENGAN", "lost / restored"]},
    },
    "ru": {
        "h_pop": "Всплывающие плашки", "h_kinds": "Что показывать", "h_quiet": "Тихие часы", "h_widget": "Виджет", "h_window": "Окно",
        "rows": {"popups": ["Показывать оповещения", "плашка в углу экрана поверх всех окон"],
                 "native": ["Уведомления Windows вместо плашек", "обычные системные, в центре уведомлений"],
                 "style": ["Стиль", "как тема TENGAN или всегда один"], "corner": ["Угол экрана", ""],
                 "duration": ["Сколько висит, с", ""], "per_min": ["Не больше в минуту", "тревоги могут пройти сверх лимита (не больше двойного)"],
                 "quiet": ["Включить", "в эти часы плашек нет"], "quiet_from": ["С (час)", ""], "quiet_to": ["До (час)", ""],
                 "quiet_trackers": ["Трекер рядом — всё равно показать", ""],
                 "widget": ["Виджет поверх окон", "мини-радар, активность, температура, пульт, карта · двойной клик — штаб"],
                 "opacity": ["Непрозрачность", "0.3 … 1"], "clickthrough": ["Сквозные клики", "клики проходят сквозь виджет (выключить — в меню трея)"],
                 "tray": ["Крестик — в трей", "программа работает у часов; выход — из меню значка"]},
        "styles": [["auto", "как тема TENGAN"], ["denko", "DENKO"], ["kurenai", "KURENAI"]],
        "corners": [["br", "справа внизу"], ["bl", "слева внизу"], ["tr", "справа вверху"], ["tl", "слева вверху"]],
        "kinds": {"new": ["Новое устройство рядом", "по порогам тревог TENGAN"], "tracker": ["Трекер рядом", "AirTag и похожие метки"],
                  "ir": ["Пульт", "кто и какую кнопку нажал"], "nfc": ["Карта / метка NFC", "тип карты"],
                  "sensor": ["Новый датчик 433 МГц", "термометр, звонок, пульт ворот"], "temp": ["Температура за окном", "если изменилась на 1° и больше"],
                  "battery": ["Батарея TENGAN", "ниже 15%"], "link": ["Связь с TENGAN", "пропала / вернулась"]},
    },
}

# заголовки оповещений (events.py пишет по-английски) — перевод для русского окна
NOTE_RU = {"TRACKER NEARBY": "ТРЕКЕР РЯДОМ", "NEW DEVICE NEARBY": "НОВОЕ УСТРОЙСТВО РЯДОМ", "REMOTE": "ПУЛЬТ", "CARD": "КАРТА",
           "433 REMOTE / BELL": "ПУЛЬТ / ЗВОНОК 433", "NEW 433 SENSOR": "НОВЫЙ ДАТЧИК 433", "OUTSIDE": "ЗА ОКНОМ",
           "TENGAN BATTERY LOW": "TENGAN: БАТАРЕЯ РАЗРЯЖЕНА", "LINK LOST": "СВЯЗЬ С TENGAN ПРОПАЛА", "LINK RESTORED": "СВЯЗЬ С TENGAN ВЕРНУЛАСЬ",
           "TEST · NEW DEVICE NEARBY": "ПРОВЕРКА · НОВОЕ УСТРОЙСТВО РЯДОМ"}


def note_title(title, lang):
    if lang != "ru" or not title:
        return title
    if title in NOTE_RU:
        return NOTE_RU[title]
    m = re.match(r"^(\d+) NEW DEVICES$", title)
    return "НОВЫХ УСТРОЙСТВ: %s" % m.group(1) if m else title


def os_lang():
    """Язык интерфейса системы: русский → ru, остальное → en."""
    if os.name == "nt":
        try:
            import ctypes
            return "ru" if (ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3FF) == 0x19 else "en"
        except Exception:
            pass
    if sys.platform == "darwin":                      # приложение из Finder запускается без LANG
        out = pm.run_cmd(["defaults", "read", "-g", "AppleLanguages"]) or ""
        first = re.findall(r"[A-Za-z]{2}", out)
        if first:
            return "ru" if first[0].lower() == "ru" else "en"
    import locale
    loc = (locale.getlocale()[0] or os.environ.get("LANG") or "").lower()
    return "ru" if loc.startswith("ru") else "en"


class Msg(RuntimeError):
    """Ошибка с ключом из TX (текст подставляется на языке окна)."""
    def __init__(self, key, **kw):
        super().__init__(key)
        self.key, self.kw = key, kw


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
        self.on_event = None           # «мозг» оповещений: каждое событие палки (ev, json)
        self.extra = ""                # что ещё слушать для оповещений/виджета (даже если штаб на другой вкладке)
        self.page_q = ""               # подписка самой страницы
        self.on_port = None            # монитор ПК: порт открылся / пропал
        self.dev_lang = None           # язык прошивки TENGAN (из события st)
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
                        self.perm_err = ""
                        log("USB: TENGAN на", name)
                        threading.Thread(target=self._reader, args=(p,), daemon=True).start()
                        if self.on_port:
                            self.on_port(p)
                        if self.want_q:
                            self.send_line("@W " + self.want_q)
                        if self.on_event:
                            self.on_event("_link", "true")
                    except Exception as e:
                        log("USB: не открылся", name, e)
                        # Linux: порт есть, но нет прав (группа dialout) — покажем подсказку на экране запуска
                        self.perm_err = name if (isinstance(e, PermissionError) or getattr(e, "errno", None) == 13
                                                 or "permission" in str(e).lower()) else ""
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
            if self.on_event:
                self.on_event("_link", "false")

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
            if ev == "st" and '"lang"' in js:
                try:
                    self.dev_lang = json.loads(js).get("lang") or self.dev_lang
                except Exception:
                    pass
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
        if self.on_event and ev[0] != "_":
            try:
                self.on_event(ev, js)
            except Exception as e:
                log("оповещения:", e)
        with self.cv:
            if ev == "snd" and len(self.events) > 200:   # страница не успевает — старый звук не нужен
                return
            self.events.append((ev, js))
            self.cv.notify_all()

    # --- для страницы ---
    def want(self, q):
        self.page_q = q
        self._rewant()

    def set_extra(self, parts):
        if parts != self.extra:
            self.extra = parts
            self._rewant()

    def _rewant(self):
        """Подписка = то, что смотрит штаб, + то, что нужно оповещениям (ble, ir, nfc, sens, wx)."""
        q = self.page_q                                  # вид «want=ble,wifi&sm=0&sb=1» (как у SSE)
        if self.extra:
            head, sep, tail = q.partition("&")
            if head.startswith("want="):
                head = head[5:]
            have = [x for x in head.split(",") if x]
            for x in self.extra.split(","):
                if x and x not in have:
                    have.append(x)
            q = "want=" + ",".join(have) + (sep + tail if sep else "")
        if q == self.want_q:
            return
        self.want_q = q
        if q:
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
                return '{"ok":false,"err":"no device on USB"}'
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
                raise Msg("not_on_usb")
            t0 = time.time()
            while not pg["done"].wait(0.2):
                if progress and pg["size"]:
                    progress(pg["got"], pg["size"])
                if not pg["size"] and time.time() - t0 > 6:
                    raise Msg("no_answer")
                if time.time() - t0 > timeout:
                    raise Msg("no_send")
            data = b"".join(pg["chunks"][k] for k in sorted(pg["chunks"]))
            if pg["size"] and len(data) != pg["size"]:
                raise Msg("partial", a=len(data), b=pg["size"])
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


LAUNCHER = """<!doctype html><html lang="%LANG%"><head><meta charset="utf-8"><title>TENGAN Desktop</title>
<style>
:root{--bg:#000;--acc:#fff;--hot:#fff;--dim:#8a8a8a;--line:#2e2e2e}  /* строгий чёрно-белый старт: тема TENGAN ещё не известна */
html,body{margin:0;height:100%;background:var(--bg);color:var(--hot);font:16px/1.4 'Segoe UI',system-ui,sans-serif}
.w{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:28px}
h1{margin:0;font-weight:600;letter-spacing:.3em;font-size:40px}h1 b{color:var(--acc)}
.sub{color:var(--dim);letter-spacing:.15em;text-transform:uppercase;font-size:13px}
.row{display:flex;gap:16px}
button{background:#0a0a0a;color:var(--hot);border:1px solid var(--line);padding:22px 30px;min-width:190px;
 font:600 18px 'Segoe UI',sans-serif;letter-spacing:.12em;cursor:pointer;text-align:left}
button small{display:block;font-weight:400;font-size:12px;color:var(--dim);letter-spacing:.05em;margin-top:6px}
button:hover,button.on{border-color:var(--acc);box-shadow:0 0 18px rgba(255,255,255,.18)}
#st{min-height:22px;color:var(--acc);letter-spacing:.08em}
.bar{width:420px;height:3px;background:var(--line)}.bar i{display:block;height:100%;width:0;background:var(--acc)}
.hint{color:var(--dim);font-size:12px}
#pb2{background:none;border:1px solid var(--line);color:var(--dim);padding:8px 16px;min-width:0;font-size:13px;letter-spacing:.1em}
#pb2:hover{color:var(--hot)}
#pp{position:fixed;inset:0;background:#000;display:none;overflow:auto}
#pp.on{display:block}
#pp .in{max-width:760px;margin:40px auto;padding:0 20px}
#pp h2{font-weight:600;letter-spacing:.2em;font-size:22px;margin:0 0 6px}#pp h2 b{color:var(--acc)}
#pp h3{color:var(--acc);font-size:13px;letter-spacing:.2em;margin:22px 0 6px;text-transform:uppercase}
.it{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:9px 0;border-bottom:1px solid var(--line)}
.it span small{display:block;color:var(--dim);font-size:12px}
.it select,.it input[type=number]{background:#111;color:var(--hot);border:1px solid var(--line);padding:6px 8px;font:14px 'Segoe UI'}
.it input[type=number]{width:80px}
.tg{width:54px;height:26px;border:1px solid var(--line);background:#111;position:relative;cursor:pointer;min-width:0;padding:0}
.tg:after{content:'';position:absolute;top:3px;left:3px;width:18px;height:18px;background:var(--dim);transition:.15s}
.tg.on{border-color:var(--acc)}.tg.on:after{left:31px;background:var(--acc)}
.ppb{display:flex;gap:10px;margin:22px 0 40px}
.ppb button{min-width:0;padding:10px 18px;font-size:13px}
</style></head><body><div class="w">
<h1>TENGAN <b>天眼</b></h1><div class="sub">%SUB%</div>
<div class="row">
<button data-m="usb">USB<small>%USB_S%</small></button>
<button data-m="wifi">WI-FI<small>%WIFI_S%</small></button>
<button data-m="auto">%AUTO_T%<small>%AUTO_S%</small></button>
</div>
<div id="st"></div><div class="bar"><i id="pb"></i></div>
<div class="hint">%HINT%</div>
<button id="pb2">%PP_BTN%</button>
</div>
<div id="pp"><div class="in"><h2>%PP_H%</h2>
<div class="hint">%PP_HINT%</div>
<div id="ppl"></div>
<div class="ppb"><button id="ppt">%PP_TEST%</button><button id="ppc">%PP_DONE%</button></div></div></div>
<script>
(function(){
var X=%PP_TX%;
var L=document.getElementById('ppl'),P=null;
function set(k,v){fetch('/__app/prefs_set?k='+encodeURIComponent(k)+'&v='+encodeURIComponent(JSON.stringify(v))).then(function(r){return r.json()}).then(function(d){P=d.prefs;draw()})}
function row(t,h,el){var d=document.createElement('div');d.className='it';var sp=document.createElement('span');sp.textContent=t;if(h){var sm=document.createElement('small');sm.textContent=h;sp.appendChild(sm)}d.appendChild(sp);d.appendChild(el);L.appendChild(d)}
function tog(k,v){var b=document.createElement('button');b.className='tg'+(v?' on':'');b.setAttribute('aria-label',k);b.onclick=function(){set(k,!v)};return b}
function num(k,v,mn,mx,st){var i=document.createElement('input');i.type='number';i.min=mn;i.max=mx;i.step=st;i.value=v;i.onchange=function(){var x=Math.min(mx,Math.max(mn,+i.value));set(k,x)};return i}
function sel(k,v,opts){var s=document.createElement('select');opts.forEach(function(o){var e=document.createElement('option');e.value=o[0];e.textContent=o[1];if(o[0]===v)e.selected=true;s.appendChild(e)});s.onchange=function(){set(k,s.value)};return s}
function h3(t){var e=document.createElement('h3');e.textContent=t;L.appendChild(e)}
function r(k){return X.rows[k]||[k,'']}
function draw(){L.innerHTML='';var p=P;
 h3(X.h_pop);
 row(r('popups')[0],r('popups')[1],tog('popups',p.popups));
 row(r('native')[0],r('native')[1],tog('native',p.native));
 row(r('style')[0],r('style')[1],sel('style',p.style,X.styles));
 row(r('corner')[0],'',sel('corner',p.corner,X.corners));
 row(r('duration')[0],'',num('duration',p.duration,2,15,0.5));
 row(r('per_min')[0],r('per_min')[1],num('per_min',p.per_min,1,30,1));
 h3(X.h_kinds);
 Object.keys(X.kinds).forEach(function(k){row(X.kinds[k][0],X.kinds[k][1],tog('kinds.'+k,p.kinds[k]))});
 h3(X.h_quiet);
 row(r('quiet')[0],r('quiet')[1],tog('quiet',p.quiet));
 row(r('quiet_from')[0],'',num('quiet_from',p.quiet_from,0,23,1));row(r('quiet_to')[0],'',num('quiet_to',p.quiet_to,0,23,1));
 row(r('quiet_trackers')[0],'',tog('quiet_trackers',p.quiet_trackers));
 h3(X.h_widget);
 row(r('widget')[0],r('widget')[1],tog('widget',p.widget));
 row(r('opacity')[0],r('opacity')[1],num('opacity',p.opacity,0.3,1,0.05));
 row(r('clickthrough')[0],r('clickthrough')[1],tog('clickthrough',p.clickthrough));
 h3(X.h_window);
 row(r('tray')[0],r('tray')[1],tog('tray',p.tray));
}
function open_(){fetch('/__app/prefs').then(function(r){return r.json()}).then(function(d){P=d.prefs;draw();document.getElementById('pp').classList.add('on')})}
document.getElementById('pb2').onclick=open_;
document.getElementById('ppc').onclick=function(){document.getElementById('pp').classList.remove('on')};
document.getElementById('ppt').onclick=function(){fetch('/__app/test_popup')};
if(location.search.indexOf('prefs')>=0)open_();
})();
</script><script>
var last='%LAST%',auto=%AUTO%,cd=3,timer=null;
var st=document.getElementById('st');
function go(m){clearInterval(timer);st.textContent='…';fetch('/__app/start?mode='+m).then(function(r){return r.text()}).then(function(t){if(t)st.textContent=t});}
document.querySelectorAll('button[data-m]').forEach(function(b){if(b.dataset.m===last)b.classList.add('on');b.onclick=function(){go(b.dataset.m)}});
if(auto&&last){timer=setInterval(function(){if(cd<=0){go(last);return}st.textContent='%COUNTDOWN%'.replace('{m}',last.toUpperCase()).replace('{n}',cd);cd--},1000);}
setInterval(function(){fetch('/__app/status').then(function(r){return r.json()}).then(function(s){if(s.msg)st.textContent=s.msg;document.getElementById('pb').style.width=(s.pct||0)+'%'}).catch(function(){})},300);
</script></body></html>"""


class App:
    def __init__(self, link, open_window=True):
        self.link = link
        self.cfg = cfg_load()
        self.engine = Engine(prefs=self.cfg.get("alerts"))
        self.eng_lock = threading.Lock()
        self.mode_on = ""              # режим, в котором штаб реально запущен: usb / wifi
        self.popups = self.widget = self.tray = self.sse = None
        self.quitting = False
        self.hq_style = "denko"        # тема TENGAN (st.theme): denko / kurenai — по ней красятся плашки и виджет
        self.ev_t = 0.0                # когда пришло последнее событие палки
        link.on_event = self.on_stick_event
        self.window = None
        self.html = None
        self.msg = ""
        self.pct = 0
        self.busy = False
        self.first = True
        self.port = PORT

    @property
    def lang(self):
        d = self.link.dev_lang
        if d in TX and self.cfg.get("lang") != d:            # запомнить язык прошивки — следующий запуск сразу на нём
            self.cfg["lang"] = d
            cfg_save(self.cfg)
        return d if d in TX else self.cfg.get("lang") if self.cfg.get("lang") in TX else os_lang()

    def t(self, key, **kw):
        s = TX[self.lang][key]
        return s.format(**kw) if kw else s

    def launcher_html(self, last, auto):
        rep = {"%LANG%": self.lang, "%SUB%": self.t("sub"), "%USB_S%": self.t("usb_s"), "%WIFI_S%": self.t("wifi_s"),
               "%AUTO_T%": self.t("auto"), "%AUTO_S%": self.t("auto_s"), "%HINT%": self.t("hint"),
               "%COUNTDOWN%": self.t("countdown").replace("'", "\\'"), "%LAST%": last, "%AUTO%": auto,
               "%PP_BTN%": self.t("pp_btn"), "%PP_H%": self.t("pp_h"), "%PP_HINT%": self.t("pp_hint"),
               "%PP_TEST%": self.t("pp_test"), "%PP_DONE%": self.t("pp_done"),
               "%PP_TX%": json.dumps(PP_TX[self.lang], ensure_ascii=False).replace("</", "<\\/")}
        h = LAUNCHER
        for k, v in rep.items():
            h = h.replace(k, v)
        return h

    # --- переходы окна ---
    def launcher(self, autostart=False, prefs=False):
        if self.window:
            self.window.load_url("http://127.0.0.1:%d/" % self.port + ("?auto=1" if autostart else "?prefs=1" if prefs else ""))

    def start(self, mode):
        if self.busy:
            return self.t("wait")
        self.cfg["mode"] = mode
        cfg_save(self.cfg)
        if mode == "auto":
            mode = "usb" if (self.link.connected() or self.link.find_port()) else "wifi"
        if mode == "wifi":
            self.msg = self.t("wifi_open")
            self.mode_on = "wifi"
            self.apply_prefs()
            if self.window:
                self.window.load_url(STICK_URL)
            return self.msg
        self.mode_on = "usb"
        self.apply_prefs()
        threading.Thread(target=self._start_usb, daemon=True).start()
        return self.t("usb_find")

    def _start_usb(self):
        self.busy = True
        try:
            t0 = time.time()
            while not self.link.connected():
                perm = getattr(self.link, "perm_err", "")
                self.msg = self.t("usb_perm", p=perm) if perm else self.t("usb_find2")
                if time.time() - t0 > 30:
                    self.msg = self.t("usb_none")
                    return
                time.sleep(0.3)

            def prog(got, size):
                self.pct = int(100 * got / max(1, size))
                self.msg = self.t("usb_load", p=self.pct)
            self.html = self.link.get_page(progress=prog)
            self.pct = 100
            self.msg = ""
            if self.window:
                self.window.load_url("http://127.0.0.1:%d/full" % self.port)
        except Exception as e:
            self.msg = self.t("failed", e=self.t(e.key, **e.kw) if isinstance(e, Msg) else e)
            log("USB старт:", e)
        finally:
            self.busy = False

    # ---------- оповещения, виджет, трей ----------
    def on_stick_event(self, ev, js):
        with self.eng_lock:
            if ev == "_link":
                self.engine.link(js == "true")
                return
            self.ev_t = time.time()
            if ev == "st":                                   # тема устройства — стиль плашек и виджета
                try:
                    th = json.loads(js).get("theme")
                except Exception:
                    th = None
                if th in ("denko", "kurenai") and th != self.hq_style:
                    self.hq_style = th
                    if self.widget:
                        threading.Thread(target=self.widget.apply_style, daemon=True).start()
            self.engine.feed_raw(ev, js)

    def on_sse_link(self, ok):
        with self.eng_lock:
            self.engine.link(ok)

    def need_events(self):
        p = self.engine.prefs
        return bool(p.get("popups") or p.get("widget"))

    def apply_prefs(self):
        """Включить/выключить то, что нужно оповещениям, под текущий режим и настройки."""
        need = self.need_events()
        self.link.set_extra("ble,ir,nfc,sens,wx" if need and self.mode_on == "usb" else "")
        if need and self.mode_on == "wifi":
            if self.sse is None:
                import overlay
                self.sse = overlay.SseFeed(self.on_stick_event, self.on_sse_link)
            self.sse.start()
        elif self.sse:
            self.sse.stop()
        if self.widget:
            self.widget.set_shown(bool(self.engine.prefs.get("widget")))
        if self.tray:
            self.tray.refresh()

    def set_pref(self, k, v):
        p = dict(self.engine.prefs)
        if k.startswith("kinds."):
            kk = dict(p["kinds"])
            kk[k[6:]] = bool(v)
            p["kinds"] = kk
        else:
            p[k] = v
        self.engine.set_prefs(p)
        self.cfg["alerts"] = self.engine.prefs
        cfg_save(self.cfg)
        self.apply_prefs()
        if self.widget and k in ("opacity", "clickthrough", "style"):
            self.widget.apply_style()

    def toggle_pref(self, k):
        self.set_pref(k, not bool(self.engine.prefs.get(k)))

    def style(self):
        s = self.engine.prefs.get("style", "auto")
        s = {"nerv": "denko", "domain": "kurenai"}.get(s, s)          # старые значения из конфига
        return self.hq_style if s not in ("denko", "kurenai") else s

    def show_main(self, prefs=False):
        w = self.window
        if not w:
            return
        try:
            w.show()
            w.restore()
        except Exception:
            pass
        if prefs:
            self.launcher(prefs=True)

    def quit(self):
        self.quitting = True
        if self.tray:
            self.tray.stop()
        try:
            import webview
            for w in list(webview.windows):
                w.destroy()
        except Exception:
            pass
        os._exit(0)

    def _settings_from_stick(self):
        try:
            if self.mode_on == "usb" and self.link.connected():
                js = self.link.api("/api/settings", timeout=3)
            elif self.mode_on == "wifi":
                with urllib.request.urlopen("http://192.168.4.1/api/settings", timeout=3) as r:
                    js = r.read().decode("utf-8")
            else:
                return
            c = cfg_from_settings(json.loads(js))
            if c:
                with self.eng_lock:
                    self.engine.set_cfg(c)
        except Exception:
            pass

    def ticker(self):
        """5 раз в секунду: показать готовые плашки, обновить виджет; раз в 30 с — настройки тревог с TENGAN.
        Стиль плашек — тема устройства (событие st)."""
        last_cfg = 0.0
        while not self.quitting:
            time.sleep(0.2)
            now = time.time()
            if self.need_events() and now - last_cfg > 30:
                last_cfg = now
                threading.Thread(target=self._settings_from_stick, daemon=True).start()
            if self.mode_on == "usb":
                self.on_stick_event("_link", "true" if (self.link.connected() and now - self.ev_t < 8) else "false")
            with self.eng_lock:
                notes = self.engine.pop()
                snap = self.engine.snapshot() if (self.widget and self.widget.shown) else None
            for n in notes:
                self.show_note(n)
            if snap is not None:
                self.widget.update(snap)

    def show_note(self, n):
        p = self.engine.prefs
        n = dict(n, title=note_title(n.get("title", ""), self.lang))
        log("оповещение:", n["kind"], n["title"], n["text"])
        if p.get("native") or not self.popups:
            if self.tray and self.tray.notify("%s  %s" % (n.get("kj", ""), n["title"]), n["text"]):
                return
        if self.popups:
            self.popups.show(n, self.style(), p.get("duration", 4.5))

    def prefs_json(self):
        return json.dumps({"prefs": self.engine.prefs, "kinds": KINDS}, ensure_ascii=False)

    def full_html(self):
        h = self.html or "<html><body style='background:#000;color:#fff'>%s</body></html>" % self.t("no_page")
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
                self._send(app.launcher_html(last, auto), "text/html; charset=utf-8")
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
            elif p == "/__app/show":                      # второй запуск программы — показать это окно
                app.show_main()
                self._send('{"ok":true}')
            elif p == "/__app/prefs":
                self._send(app.prefs_json())
            elif p == "/__app/prefs_set":
                k = q.get("k", [""])[0]
                try:
                    v = json.loads(q.get("v", ["null"])[0])
                except Exception:
                    v = q.get("v", [""])[0]
                if k:
                    app.set_pref(k, v)
                self._send(app.prefs_json())
            elif p == "/__app/test_popup":                # кнопка «проверить» в настройках
                app.show_note({"kind": "new", "kj": "接近警告", "title": "TEST · NEW DEVICE NEARBY",
                               "text": app.t("test_text"), "sev": "alert"})
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
    import overlay
    log("=== TENGAN desktop ===")
    if not overlay.single_instance(PORT):
        log("уже запущено — показал окно первой копии")
        return
    link = Link()
    Monitor(link)
    app = App(link)
    serve(app)
    win = webview.create_window(APP_NAME, "http://127.0.0.1:%d/" % app.port, width=1600, height=1000,
                                background_color="#000000", min_size=(900, 600), js_api=JsApi(app))
    app.window = win
    # оповещения и виджет (WIDGET.md): свои окна поверх всех + значок в трее
    for name, make in (("popups", overlay.Popups), ("widget", overlay.Widget), ("tray", overlay.Tray)):
        try:
            setattr(app, name, make(app))
        except Exception as e:
            log("не включилось:", name, e)

    def on_closing():
        # крестик — в трей (палка и монитор ПК работают дальше); выход — из меню трея
        if app.tray and app.tray.icon and app.engine.prefs.get("tray", True) and not app.quitting:
            win.hide()
            return False
        app.quit()
    win.events.closing += on_closing
    threading.Thread(target=app.ticker, daemon=True).start()

    def on_loaded():
        try:
            win.evaluate_js(KEYS)             # F11 / F2 и на странице палки (режим Wi-Fi)
        except Exception:
            pass
    win.events.loaded += on_loaded
    # Linux: окно на Qt WebEngine (ставится из pip и надёжно упаковывается); Windows / macOS — системный движок
    gui = "qt" if sys.platform.startswith("linux") else None
    webview.start(private_mode=False, storage_path=os.path.join(APPDIR, "webview"), gui=gui)


if __name__ == "__main__":
    main()
