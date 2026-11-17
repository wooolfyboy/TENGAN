# -*- coding: utf-8 -*-
"""
TENGAN desktop - alert/widget engine (no windows, no libraries; testable anywhere).

Takes the same stick events as the HQ page (ble / ir / nfc / sens / wx / st / snd) and applies the same
rules as the tablet UI (tools/webapp/src/core/store.js): al_warm warm-up after connect, "new" = not seen
this session, al_new / al_trk levels (0 off / 1 near only / 2 any), activity = mean of enabled sources
x al_sens/5 with al_warn / al_alarm levels. Alert settings come from the stick (/api/settings, page "alert");
the app's own settings (popup kinds, quiet hours, limit) are in prefs.
Output: a list of notice dicts for popups and a state snapshot for the widget.
"""
import json
import time
import zlib

CFG_DEF = {"al_near": -65, "al_new": 1, "al_trk": 1, "al_newfor": 30, "al_warm": 20, "al_sens": 5,
           "al_warn": 60, "al_alarm": 80, "al_cble": 1, "al_crf": 1, "al_csnd": 1, "al_cprb": 1,
           "al_blink": 1, "al_plate": 1}

KINDS = ["new", "tracker", "ir", "nfc", "sensor", "temp", "battery", "link"]

PREFS_DEF = {
    "popups": True,                 # popups on/off
    "kinds": {k: True for k in KINDS},
    "quiet": False, "quiet_from": 23, "quiet_to": 8,   # quiet hours (local time)
    "quiet_trackers": True,         # a nearby tracker overrides quiet hours
    "per_min": 6,                   # max popups per minute
    "merge_s": 2.0,                 # same-kind events within N s merge into one popup ("3 NEW DEVICES")
    "duration": 4.5,                # popup lifetime, s
    "corner": "br",                 # corner: br bl tr tl
    "style": "auto",                # auto (follow TENGAN theme) / denko / kurenai
    "native": False,                # native OS notifications instead of own popups
    "widget": False, "opacity": 1.0, "clickthrough": False,
    "widget_rect": None,            # [x, y, w, h]
    "tray": True,                   # closing the window goes to tray
    # app window ("Window" settings)
    "win_rect": None,               # [x, y, w, h], remembered automatically
    "win_max": False,               # was maximized
    "start_full": False,            # start fullscreen (like F11)
    "zoom": 1.0,                    # window UI scale: 0.8 .. 1.5
    "autostart": False,             # start with the OS
    "start_tray": True,             # on autostart go straight to tray
}

KJ = {"new": "接近警告", "tracker": "追跡警告", "ir": "赤外線", "nfc": "近接", "sensor": "感知",
      "temp": "外気", "battery": "電池", "link": "通信"}


def merged_prefs(p):
    out = json.loads(json.dumps(PREFS_DEF))
    for k, v in (p or {}).items():
        if k == "kinds" and isinstance(v, dict):
            out["kinds"].update({kk: bool(vv) for kk, vv in v.items() if kk in out["kinds"]})
        elif k in out:
            out[k] = v
    return out


def clamp(v, a=0.0, b=1.0):
    return a if v < a else b if v > b else v


def cfg_from_settings(d):
    """/api/settings -> al_* numbers (like S.cfgFromItem on the page). None if there is no alert page."""
    pages = (d or {}).get("pages") or []
    pg = next((p for p in pages if p.get("id") == "alert"), None)
    if not pg:
        return None
    c = dict(CFG_DEF)
    for it in pg.get("items") or []:
        k = it.get("k")
        if k not in CFG_DEF:
            continue
        v = it.get("v")
        t = it.get("type")
        try:
            if t == "enum":
                opts = [str(o).lower() for o in (it.get("opts") or [])]
                v = opts.index(str(v).lower()) if str(v).lower() in opts else (int(v) if isinstance(v, (int, float)) else CFG_DEF[k])
            elif t == "bool":
                v = 1 if v in (True, 1, "1", "on") else 0
            else:
                v = float(v)
                v = int(v) if v == int(v) else v
            c[k] = v
        except Exception:
            pass
    return c


class Engine:
    def __init__(self, prefs=None, clock=time.monotonic, wall=time.localtime):
        self.prefs = merged_prefs(prefs)
        self.cfg = dict(CFG_DEF)
        self.clock = clock
        self.wall = wall
        self.reset()

    # ---------- state ----------
    def reset(self):
        """New link session: warm up again, forget what was seen."""
        self.devs = {}              # id -> {r, type, name, label, born, seen, first, trk_t}
        self.ever = set()
        self.warm_t = None
        self.act = {"ble": [], "rf": [], "prb": []}
        self.snd_lv = None
        self.sens_ids = None        # known 433 sensors (None until the first snapshot)
        self.sens_pk = None
        self.temp = None            # {t, h, src}
        self.temp_note = None       # (value, time) of the last temperature popup
        self.ir_last = None         # (p, addr, cmd, t)
        self.ir = None              # last one, for the widget
        self.nfc = None
        self.nfc_last = None        # (uid, t)
        self.bat = None
        self.link_ok = None
        self.link_t = 0.0
        self.link_pending = None    # (ok, since); link notice only if the state holds 5 s
        self.queue = []             # notices waiting to be merged
        self.sent = []              # times of shown popups (per-minute limit)

    def set_cfg(self, c):
        if c:
            self.cfg.update(c)

    def set_prefs(self, p):
        self.prefs = merged_prefs(p)

    # ---------- input ----------
    def feed_raw(self, ev, js):
        if ev in ("ble", "ir", "nfc", "sens", "wx", "st", "snd"):
            try:
                self.feed(ev, json.loads(js))
            except Exception:
                pass

    def feed(self, ev, d, now=None):
        now = self.clock() if now is None else now
        f = getattr(self, "_on_" + ev, None)
        if f and isinstance(d, dict):
            f(d, now)

    def _push_act(self, k, now, n=1):
        a = self.act[k]
        for _ in range(min(n, 30)):
            a.append(now)
        while a and now - a[0] > 60:
            a.pop(0)

    def _near(self, r):
        return r is not None and r > self.cfg["al_near"]

    def is_new(self, e, now):
        c = self.cfg
        if e["first"] or not c["al_new"] or now - e["born"] > c["al_newfor"]:
            return False
        return c["al_new"] == 2 or self._near(e["r"])

    def is_trk(self, e):
        c = self.cfg
        if e["type"] != "tracker" or not c["al_trk"]:
            return False
        return c["al_trk"] == 2 or self._near(e["r"])

    def _on_ble(self, d, now):
        if self.warm_t is None:
            self.warm_t = now + float(self.cfg["al_warm"])
        seen = set()
        fresh_new = []
        for a in d.get("d") or []:
            if not isinstance(a, list) or len(a) < 2:
                continue
            did = a[0]
            r = a[1]
            typ = a[2] if len(a) > 2 else ""
            name = a[4] if len(a) > 4 else ""
            label = a[7] if len(a) > 7 else ""
            seen.add(did)
            e = self.devs.get(did)
            if e is None:
                before = did in self.ever
                self.ever.add(did)
                fresh = (not before) and now > self.warm_t
                e = {"r": r, "type": typ, "name": name, "label": label, "born": now, "seen": now,
                     "first": not fresh, "trk_t": None, "alive": True}
                self.devs[did] = e
                if fresh:
                    self._push_act("ble", now)
                    if self.is_new(e, now):
                        fresh_new.append(e)
            else:
                e["r"] = r if e["r"] is None else round(e["r"] * 0.5 + r * 0.5, 1)
                e.update(type=typ or e["type"], name=name or e["name"], label=label or e["label"], seen=now, alive=True)
            # tracker nearby: once per 10 min per device
            if self.is_trk(e) and now > (self.warm_t or 0) and (e["trk_t"] is None or now - e["trk_t"] > 600):
                e["trk_t"] = now
                self._note("tracker", now, "TRACKER NEARBY", "%s · %d dBm" % (self._dname(e), e["r"]), sev="alert", dev=did)
        for did, e in list(self.devs.items()):
            if did not in seen:
                e["alive"] = False
                if now - e["seen"] > 60:
                    del self.devs[did]
        if len(self.ever) > 5000:
            self.ever = set(self.devs)
        for e in fresh_new:
            if e["type"] == "tracker" and e["trk_t"] == now:
                continue                                  # already reported as tracker nearby
            self._note("new", now, "NEW DEVICE NEARBY", "%s · %d dBm" % (self._dname(e), e["r"]),
                       sev="alert" if self._near(e["r"]) else "info")

    @staticmethod
    def _dname(e):
        return e.get("label") or e.get("name") or e.get("type") or "device"

    def _on_ir(self, d, now):
        key = (d.get("p"), d.get("addr"), d.get("cmd"))
        self.ir = {"p": d.get("p"), "brand": d.get("brand") or "", "cmd": d.get("cmd"), "addr": d.get("addr"), "t": time.time()}
        if d.get("rep") or (self.ir_last and self.ir_last[0] == key and now - self.ir_last[1] < 1.5):
            self.ir_last = (key, now)
            return                                        # key held: not a new press
        self.ir_last = (key, now)
        self._push_act("prb", now)
        who = d.get("brand") or d.get("p") or "IR"
        cmd = d.get("cmd")
        self._note("ir", now, "REMOTE", "%s · %s 0x%02X" % (who, d.get("p") or "", int(cmd) & 0xFFFF) if isinstance(cmd, int) else who)

    def _on_nfc(self, d, now):
        if d.get("err"):
            return
        uid = d.get("uid") or ""
        self.nfc = {"fam": d.get("fam") or "card", "hint": d.get("hint") or "", "uid": uid, "t": time.time()}
        if self.nfc_last and self.nfc_last[0] == uid and now - self.nfc_last[1] < 5:
            self.nfc_last = (uid, now)
            return
        self.nfc_last = (uid, now)
        self._push_act("prb", now)
        self._note("nfc", now, "CARD", "%s%s" % (d.get("fam") or "card", (" · " + d["hint"]) if d.get("hint") else ""))

    def _on_sens(self, d, now):
        pk = d.get("pk")
        if isinstance(pk, int):
            if self.sens_pk is not None and pk > self.sens_pk:
                self._push_act("rf", now, pk - self.sens_pk)
            self.sens_pk = pk
        ids = {}
        for s in d.get("sensors") or []:
            if isinstance(s, dict) and s.get("id"):
                ids[s["id"]] = s
        if self.sens_ids is None:                       # first snapshot: what was already there is not news
            self.sens_ids = set(ids)
            return
        for sid, s in ids.items():
            if sid in self.sens_ids:
                continue
            self.sens_ids.add(sid)
            if s.get("kind") == "remote":
                self._note("sensor", now, "433 REMOTE / BELL", "#%s · button %s" % (str(sid).split("-")[-1], s.get("btn", "?")))
            else:
                t = s.get("t")
                self._note("sensor", now, "NEW 433 SENSOR", "%s ch%s%s" % (s.get("p") or "?", s.get("ch", "?"),
                           (" · %+.1f°" % t) if isinstance(t, (int, float)) else ""))

    def _on_wx(self, d, now):
        t = d.get("temp")
        if not isinstance(t, (int, float)):
            return
        self.temp = {"t": t, "h": d.get("hum"), "src": d.get("tsrc") or ""}
        last = self.temp_note
        if last is None:
            self.temp_note = (t, now)                    # first value is only a baseline, no popup
            return
        if abs(t - last[0]) >= 1.0 and now - last[1] >= 600:
            self.temp_note = (t, now)
            self._note("temp", now, "OUTSIDE", "%+.1f°C%s · %s" % (t, (" · %s%%" % d["hum"]) if d.get("hum") is not None else "",
                                                                   "warmer" if t > last[0] else "colder"))

    def _on_st(self, d, now):
        b = d.get("bat")
        if isinstance(b, int) and b >= 0:
            if self.bat is not None and self.bat > 15 >= b and not d.get("usb"):
                self._note("battery", now, "TENGAN BATTERY LOW", "%d%%" % b, sev="alert")
            self.bat = b

    def _on_snd(self, d, now):
        if d.get("ok"):
            self.snd_lv = d.get("lv")

    def link(self, ok, now=None):
        """Stick link up/down. A popup shows only if the state holds 5 s (no flapping)."""
        now = self.clock() if now is None else now
        if ok == self.link_ok:
            self.link_pending = None
            return
        if self.link_pending is None or self.link_pending[0] != ok:
            self.link_pending = (ok, now)

    def _link_tick(self, now):
        lp = self.link_pending
        if lp and now - lp[1] >= 5:
            was = self.link_ok
            self.link_ok = lp[0]
            self.link_pending = None
            if was is not None:
                self._note("link", now, "LINK RESTORED" if lp[0] else "LINK LOST", "", sev="info" if lp[0] else "alert")
            if lp[0] and was is False:
                self.reset_soft()

    def reset_soft(self):
        """After the link returns, warm up again (the stick refills its list)."""
        self.warm_t = None

    # ---------- notices: merging, limit, quiet hours ----------
    def _note(self, kind, now, title, text, sev="info", **kw):
        self.queue.append(dict(kind=kind, kj=KJ.get(kind, ""), title=title, text=text, sev=sev, t=now, n=1, **kw))

    def _quiet(self):
        p = self.prefs
        if not p["quiet"]:
            return False
        h = self.wall().tm_hour
        a, b = int(p["quiet_from"]) % 24, int(p["quiet_to"]) % 24
        return (a <= h < b) if a < b else (h >= a or h < b) if a != b else False

    def pop(self, now=None):
        """Notices ready to show (merged; honoring prefs, quiet hours and the limit)."""
        now = self.clock() if now is None else now
        self._link_tick(now)
        p = self.prefs
        merge = float(p["merge_s"])
        ready, keep = [], []
        groups = {}
        for n in self.queue:
            if now - n["t"] < merge:
                keep.append(n)                             # wait: more of the same may arrive
            else:
                g = groups.get(n["kind"])
                if g is None:
                    groups[n["kind"]] = n
                    ready.append(n)
                else:
                    g["n"] += 1
                    if n["sev"] == "alert":
                        g["sev"] = "alert"
        # same kind still waiting: hold the whole batch
        waiting = {n["kind"] for n in keep}
        out = []
        for n in ready:
            if n["kind"] in waiting:
                keep.extend([n])
                continue
            if n["n"] > 1:
                if n["kind"] == "new":
                    n["title"] = "%d NEW DEVICES" % n["n"]
                else:
                    n["text"] = (n["text"] + "  · ×%d" % n["n"]).strip(" ·")
            out.append(n)
        self.queue = keep
        res = []
        quiet = self._quiet()
        self.sent = [t for t in self.sent if now - t < 60]
        for n in out:
            if not p["popups"] or not p["kinds"].get(n["kind"], True):
                continue
            if quiet and not (n["kind"] == "tracker" and p["quiet_trackers"]):
                continue
            if len(self.sent) >= int(p["per_min"]) and n["sev"] != "alert":
                continue
            if len(self.sent) >= int(p["per_min"]) * 2:
                continue                                   # even alerts: at most twice the limit
            self.sent.append(now)
            res.append(n)
        return res

    # ---------- widget snapshot ----------
    def activity(self, now=None):
        now = self.clock() if now is None else now
        c = self.cfg

        def cnt(k):
            a = self.act[k]
            while a and now - a[0] > 60:
                a.pop(0)
            return len(a)
        parts = {"ble": clamp(cnt("ble") / 5), "rf": clamp(cnt("rf") / 20),
                 "snd": clamp((self.snd_lv + 60) / 40) if isinstance(self.snd_lv, (int, float)) else 0.0,
                 "prb": clamp(cnt("prb") / 3)}
        en = {"ble": bool(c["al_cble"]), "rf": bool(c["al_crf"]), "snd": bool(c["al_csnd"]), "prb": bool(c["al_cprb"])}
        vals = [parts[k] for k in parts if en[k]]
        v = clamp(sum(vals) / len(vals) * c["al_sens"] / 5) if vals else 0.0
        warn, alarm = c["al_warn"] / 100.0, c["al_alarm"] / 100.0
        state = 3 if v >= alarm else 2 if v >= warn else 1 if v >= warn / 2 else 0
        return {"v": round(v, 3), "state": state, "parts": parts, "en": en, "warn": warn, "alarm": alarm}

    def snapshot(self, now=None):
        now = self.clock() if now is None else now
        devs = []
        for did, e in self.devs.items():
            if not e["alive"] or e["r"] is None:
                continue
            a = (zlib.crc32(str(did).encode()) % 3600) / 3600.0
            devs.append({"a": round(a, 4), "r": e["r"], "n": 1 if self.is_new(e, now) else 0,
                         "k": 1 if self.is_trk(e) else 0, "s": 1 if e["label"] else 0,
                         "nm": (e["label"] or e["name"] or "")[:16]})
        devs.sort(key=lambda x: -x["r"])
        return {"devs": devs[:60], "n": len(devs), "act": self.activity(now), "temp": self.temp, "ir": self.ir,
                "nfc": self.nfc, "bat": self.bat, "link": bool(self.link_ok), "near": self.cfg["al_near"],
                "blink": bool(self.cfg["al_blink"])}
