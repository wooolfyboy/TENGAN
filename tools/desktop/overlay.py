# -*- coding: utf-8 -*-
"""
TENGAN desktop — трей, всплывашки и виджет поверх всех окон.

  Tray     — значок у часов (pystray): штаб / виджет / всплывашки / стиль / угол / режим / выход.
  Popups   — одно маленькое окно без рамки поверх всех; в нём стопка до 3 плашек (NERV / DOMAIN),
             окно подгоняется по высоте стопки и прячется, когда плашки ушли. Клик — открыть штаб.
  Widget   — окошко без рамки поверх всех (мини-радар, активность, температура, пульт, карта);
             таскается мышкой, помнит место и размер; прозрачность и «сквозные клики» — через Win32.
  SseFeed  — в режиме Wi-Fi приложение само слушает поток палки (порт 81) для «мозга» оповещений.

Всё, что зависит от Windows / pystray / pywebview, импортируется лениво — модуль грузится и на Linux.
"""
import json
import os
import threading
import time
import urllib.request

from overlay_html import POPUP_HTML, WIDGET_HTML

IS_WIN = os.name == "nt"
HERE = os.path.dirname(os.path.abspath(__file__))
POP_W = 380


def log(*a):
    print(time.strftime("%H:%M:%S"), "[overlay]", *a, flush=True)


# ---------- Win32: прозрачность, сквозные клики, «не в панели задач» ----------
GWL_EXSTYLE = -20
WS_EX_LAYERED = 0x00080000
WS_EX_TRANSPARENT = 0x00000020
WS_EX_TOOLWINDOW = 0x00000080
WS_EX_NOACTIVATE = 0x08000000
LWA_ALPHA = 0x2


def hwnd_of(win):
    """HWND окна pywebview на Windows (бэкенд WinForms: window.native — это Form)."""
    try:
        return int(win.native.Handle.ToInt64())
    except Exception:
        return None


def win_style(win, opacity=None, clickthrough=None, tool=True):
    if not IS_WIN:
        return False
    h = hwnd_of(win)
    if not h:
        return False
    try:
        import ctypes
        u = ctypes.windll.user32
        u.GetWindowLongW.restype = ctypes.c_long
        ex = u.GetWindowLongW(h, GWL_EXSTYLE)
        if tool:
            ex |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE        # не в панели задач / Alt-Tab, не крадёт фокус
        layered = (opacity is not None and opacity < 0.999) or bool(clickthrough)
        ex = (ex | WS_EX_LAYERED) if layered else (ex & ~WS_EX_LAYERED)
        if clickthrough is not None:
            ex = (ex | WS_EX_TRANSPARENT) if clickthrough else (ex & ~WS_EX_TRANSPARENT)
        u.SetWindowLongW(h, GWL_EXSTYLE, ex)
        if layered:
            u.SetLayeredWindowAttributes(h, 0, int(255 * (opacity if opacity is not None else 1.0)), LWA_ALPHA)
        return True
    except Exception as e:
        log("win32 style:", e)
        return False


def screen_size():
    try:
        import webview
        s = webview.screens[0]
        return int(s.width), int(s.height)
    except Exception:
        return 1920, 1080


# ============================================================
#  Всплывашки
# ============================================================
class _PopupApi:
    def __init__(self, owner):
        self._o = owner

    def popup_size(self, h):
        self._o._size(int(h or 0))

    def popup_click(self):
        self._o.app.show_main()


class Popups:
    def __init__(self, app):
        import webview
        self.app = app
        self.ready = False
        self.pending = []
        self.visible = False
        self.lock = threading.Lock()
        self.win = webview.create_window("TENGAN alert", html=POPUP_HTML, width=POP_W, height=120, frameless=True,
                                         on_top=True, resizable=False, hidden=True, focus=False,
                                         background_color="#000000", js_api=_PopupApi(self))
        self.win.events.loaded += self._loaded

    def _loaded(self):
        win_style(self.win, tool=True)
        self.ready = True
        with self.lock:
            p, self.pending = self.pending, []
        for style, dur, n in p:
            self._send(style, dur, n)

    def show(self, note, style, dur):
        if not self.ready:
            with self.lock:
                self.pending.append((style, dur, note))
            return
        self._send(style, dur, note)

    def _send(self, style, dur, n):
        try:
            self.win.evaluate_js("P.style(%s);P.dur(%s);P.add(%s)" % (json.dumps(style), float(dur), json.dumps(n, ensure_ascii=False)))
        except Exception as e:
            log("popup:", e)

    def _size(self, h):
        if h <= 0:
            if self.visible:
                self.visible = False
                self.win.hide()
            return
        W, H = screen_size()
        corner = self.app.engine.prefs.get("corner", "br")
        m, bar = 12, 56                                    # отступ и место под панель задач
        x = W - POP_W - m if corner in ("br", "tr") else m
        y = H - h - bar if corner in ("br", "bl") else m
        try:
            self.win.resize(POP_W, h)
            self.win.move(x, y)
            if not self.visible:
                self.visible = True
                self.win.show()
                win_style(self.win, tool=True)
        except Exception as e:
            log("popup place:", e)


# ============================================================
#  Виджет
# ============================================================
class _WidgetApi:
    def __init__(self, owner):
        self._o = owner

    def widget_open(self):
        self._o.app.show_main()


class Widget:
    def __init__(self, app):
        import webview
        self.app = app
        p = app.engine.prefs
        r = p.get("widget_rect") or [None, None, 320, 200]
        W, H = screen_size()
        x = r[0] if r[0] is not None else W - 340
        y = r[1] if r[1] is not None else 80
        self.ready = False
        self.shown = bool(p.get("widget"))
        self.win = webview.create_window("TENGAN widget", html=WIDGET_HTML, width=int(r[2]), height=int(r[3]), x=int(x), y=int(y),
                                         frameless=True, easy_drag=True, on_top=True, resizable=True, hidden=not self.shown,
                                         focus=False, min_size=(240, 150), background_color="#050302", js_api=_WidgetApi(self))
        self.win.events.loaded += self._loaded
        try:
            self.win.events.moved += self._moved
            self.win.events.resized += self._resized
        except Exception:
            pass
        self._rect = [x, y, r[2], r[3]]
        self._save_t = 0

    def _loaded(self):
        self.ready = True
        self.apply_style()

    def apply_style(self):
        p = self.app.engine.prefs
        win_style(self.win, opacity=float(p.get("opacity", 1.0)), clickthrough=bool(p.get("clickthrough")), tool=True)
        if self.ready:
            try:
                self.win.evaluate_js("W.style(%s)" % json.dumps(self.app.style()))
            except Exception:
                pass

    def _moved(self, x, y):
        self._rect[0], self._rect[1] = x, y
        self._save_later()

    def _resized(self, w, h):
        self._rect[2], self._rect[3] = w, h
        self._save_later()

    def _save_later(self):
        t = time.time()
        self._save_t = t

        def go():
            time.sleep(1.0)
            if self._save_t == t:
                self.app.set_pref("widget_rect", list(self._rect))
        threading.Thread(target=go, daemon=True).start()

    def set_shown(self, on):
        self.shown = on
        try:
            if on:
                self.win.show()
                self.apply_style()
            else:
                self.win.hide()
        except Exception as e:
            log("widget:", e)

    def update(self, snap):
        if not (self.ready and self.shown):
            return
        try:
            self.win.evaluate_js("W.update(%s)" % json.dumps(snap, ensure_ascii=False))
        except Exception:
            pass


# ============================================================
#  Трей
# ============================================================
class Tray:
    def __init__(self, app):
        self.app = app
        self.icon = None
        try:
            import pystray
            from PIL import Image
        except Exception as e:
            log("трей недоступен:", e)
            return
        self.pystray = pystray
        img = Image.open(os.path.join(HERE, "tengan.ico"))
        M, I = pystray.Menu, pystray.MenuItem
        a = app
        p = lambda k: (lambda item: bool(a.engine.prefs.get(k)))
        radio = lambda k, v: (lambda item: a.engine.prefs.get(k) == v)
        self.icon = pystray.Icon("TENGAN", img, "TENGAN 天眼", menu=M(
            I("Открыть штаб", lambda: a.show_main(), default=True),
            M.SEPARATOR,
            I("Виджет поверх окон", lambda: a.toggle_pref("widget"), checked=p("widget")),
            I("Всплывающие оповещения", lambda: a.toggle_pref("popups"), checked=p("popups")),
            I("Уведомления Windows вместо плашек", lambda: a.toggle_pref("native"), checked=p("native")),
            I("Сквозные клики по виджету", lambda: a.toggle_pref("clickthrough"), checked=p("clickthrough")),
            I("Стиль", M(*[I(n, (lambda v: lambda: a.set_pref("style", v))(v), checked=radio("style", v), radio=True)
                           for n, v in (("как в штабе", "auto"), ("NERV", "nerv"), ("DOMAIN", "domain"))])),
            I("Угол", M(*[I(n, (lambda v: lambda: a.set_pref("corner", v))(v), checked=radio("corner", v), radio=True)
                          for n, v in (("справа внизу", "br"), ("слева внизу", "bl"), ("справа вверху", "tr"), ("слева вверху", "tl"))])),
            I("Режим связи", M(*[I(n, (lambda v: lambda: a.start(v))(v), checked=(lambda v: lambda item: a.cfg.get("mode") == v)(v), radio=True)
                                 for n, v in (("USB", "usb"), ("Wi-Fi", "wifi"), ("Авто", "auto"))])),
            I("Настройки оповещений…", lambda: a.show_main(prefs=True)),
            M.SEPARATOR,
            I("Выход", lambda: a.quit()),
        ))
        threading.Thread(target=self.icon.run, daemon=True).start()

    def refresh(self):
        if self.icon:
            try:
                self.icon.update_menu()
            except Exception:
                pass

    def notify(self, title, text):
        if self.icon:
            try:
                self.icon.notify(text or " ", title)
                return True
            except Exception as e:
                log("notify:", e)
        return False

    def stop(self):
        if self.icon:
            try:
                self.icon.stop()
            except Exception:
                pass


# ============================================================
#  Wi-Fi: свой поток событий палки (SSE, порт 81) для «мозга» оповещений
# ============================================================
class SseFeed:
    WANT = "ble,ir,nfc,sens,wx"

    def __init__(self, on_event, on_link, host="192.168.4.1", port=81):
        self.on_event, self.on_link = on_event, on_link
        self.url = "http://%s:%d/stream?want=%s" % (host, port, self.WANT)
        self.run_flag = False
        self.th = None

    def start(self):
        if self.run_flag:
            return
        self.run_flag = True
        self.th = threading.Thread(target=self._loop, daemon=True)
        self.th.start()

    def stop(self):
        self.run_flag = False

    def _loop(self):
        while self.run_flag:
            try:
                with urllib.request.urlopen(self.url, timeout=8) as r:
                    self.on_link(True)
                    ev, data = "", []
                    for raw in r:
                        if not self.run_flag:
                            break
                        line = raw.decode("utf-8", "replace").rstrip("\r\n")
                        if line.startswith("event:"):
                            ev = line[6:].strip()
                        elif line.startswith("data:"):
                            data.append(line[5:].lstrip())
                        elif line == "":
                            if ev and data:
                                self.on_event(ev, "\n".join(data))
                            ev, data = "", []
            except Exception:
                pass
            self.on_link(False)
            for _ in range(30):
                if not self.run_flag:
                    return
                time.sleep(0.1)


# ---------- один экземпляр программы ----------
_mutex = None


def single_instance(port):
    """True — мы первые. Иначе просим уже запущенную показать окно и выходим."""
    global _mutex
    if IS_WIN:
        try:
            import ctypes
            _mutex = ctypes.windll.kernel32.CreateMutexW(None, False, "TENGAN_desktop_single")
            if ctypes.windll.kernel32.GetLastError() != 183:      # ERROR_ALREADY_EXISTS
                return True
        except Exception:
            return True
    else:
        import socket
        s = socket.socket()
        try:
            s.settimeout(0.3)
            s.connect(("127.0.0.1", port))
        except Exception:
            return True
        finally:
            s.close()
    for p in range(port, port + 20):
        try:
            urllib.request.urlopen("http://127.0.0.1:%d/__app/show" % p, timeout=1).read()
            break
        except Exception:
            continue
    return False
