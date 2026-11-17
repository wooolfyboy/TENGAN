# -*- coding: utf-8 -*-
"""Test the alert engine without windows or a stick:  python3 tools/desktop/test_events.py"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from events import Engine, cfg_from_settings  # noqa: E402


class Clock:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def ble(devs):
    return {"on": 1, "n": len(devs), "d": [[i, r, typ, "", nm, 1, 1] for i, r, typ, nm in devs]}


def run_for(e, clk, sec, step=1.0):
    out = []
    end = clk.t + sec
    while clk.t < end:
        clk.t += step
        out += e.pop()
    return out


def wall_at(hour):
    return lambda: time.struct_time((2026, 11, 16, hour, 0, 0, 0, 320, 0))


def test_warmup_and_churn():
    clk = Clock()
    e = Engine(clock=clk)
    base = [(i, -60 - i % 30, "phone", "p%d" % i) for i in range(1, 76)]
    e.feed("ble", ble(base))                       # first list: before warm-up
    for k in range(40):                            # 75 devices drop out and come back
        clk.t += 1
        e.feed("ble", ble(base[: 40 + k % 30]))
        assert not e.pop(), "во время разогрева/текучки не должно быть всплывашек"
    clk.t += 120
    for k in range(20):                            # after warm-up: same churn again
        clk.t += 1
        e.feed("ble", ble(base[: 30 + k % 40]))
    assert not run_for(e, clk, 5), "вернувшиеся устройства — не новые"
    return "warm-up + churn: 0 popups"


def test_new_near_device():
    clk = Clock()
    e = Engine(clock=clk)
    e.feed("ble", ble([(1, -70, "phone", "a")]))
    clk.t += 30
    e.feed("ble", ble([(1, -70, "phone", "a"), (99, -50, "phone", "Новый")]))
    out = run_for(e, clk, 4)
    assert len(out) == 1 and out[0]["kind"] == "new" and out[0]["sev"] == "alert", out
    # far new device with al_new=near: no popup
    e.feed("ble", ble([(1, -70, "phone", "a"), (99, -50, "phone", "x"), (100, -88, "phone", "far")]))
    assert not run_for(e, clk, 4)
    # with al_new=any: far one too
    e.set_cfg({"al_new": 2})
    e.feed("ble", ble([(101, -89, "phone", "far2")]))
    out = run_for(e, clk, 4)
    assert len(out) == 1 and out[0]["sev"] == "info", out
    return "new near device → 1 alert; far → none (near), 1 info (any)"


def test_tracker_and_burst_merge():
    clk = Clock()
    e = Engine(clock=clk)
    e.feed("ble", ble([(1, -70, "phone", "a")]))
    clk.t += 30
    e.feed("ble", ble([(1, -70, "phone", "a"), (7, -55, "tracker", "tag")]))
    out = run_for(e, clk, 4)
    assert [n["kind"] for n in out] == ["tracker"], out
    # 4 new near devices within a second -> one "4 NEW DEVICES" popup
    e.feed("ble", ble([(1, -70, "phone", "a"), (7, -55, "tracker", "tag")] + [(200 + i, -50, "phone", "n") for i in range(4)]))
    out = run_for(e, clk, 4)
    assert len(out) == 1 and out[0]["title"] == "4 NEW DEVICES", out
    return "tracker → 1; burst of 4 → one merged popup"


def test_rate_limit():
    clk = Clock()
    e = Engine(clock=clk, prefs={"per_min": 3, "merge_s": 0})
    out = []
    for i in range(10):
        e.feed("ir", {"p": "NEC", "addr": 4, "cmd": i, "bits": 32, "brand": "LG TV", "rep": 0})
        out += run_for(e, clk, 2)
    assert len(out) == 3, len(out)
    return "rate limit: 10 remotes in 20 s → 3 popups"


def test_quiet_hours():
    clk = Clock()
    e = Engine(clock=clk, prefs={"quiet": True, "quiet_from": 23, "quiet_to": 8}, wall=wall_at(2))
    e.feed("nfc", {"fam": "NTAG213", "uid": "04A2••"})
    assert not run_for(e, clk, 4)
    e.feed("ble", ble([(1, -70, "phone", "a")]))
    clk.t += 30
    e.feed("ble", ble([(1, -70, "phone", "a"), (7, -50, "tracker", "tag")]))
    out = run_for(e, clk, 4)
    assert [n["kind"] for n in out] == ["tracker"], out      # tracker overrides quiet hours
    e2 = Engine(clock=clk, prefs={"quiet": True, "quiet_from": 23, "quiet_to": 8}, wall=wall_at(14))
    e2.feed("nfc", {"fam": "NTAG213", "uid": "04A2••"})
    assert len(run_for(e2, clk, 4)) == 1
    return "quiet hours: card muted at 02:00, tracker passes, card shown at 14:00"


def test_ir_nfc_sensor_temp_battery_link():
    clk = Clock()
    e = Engine(clock=clk)
    e.feed("ir", {"p": "NEC", "addr": 4, "cmd": 8, "brand": "LG TV", "rep": 0})
    e.feed("ir", {"p": "NEC", "addr": 4, "cmd": 8, "brand": "LG TV", "rep": 1})     # key held
    e.feed("nfc", {"fam": "MIFARE Classic 1K", "hint": "transit card (likely)", "uid": "04A2••"})
    e.feed("nfc", {"fam": "MIFARE Classic 1K", "uid": "04A2••"})                   # same card again at once
    e.feed("sens", {"pk": 1, "sensors": [{"id": "nexus-1-0x5a", "p": "Nexus", "ch": 1, "t": 4.2}]})
    e.feed("sens", {"pk": 3, "sensors": [{"id": "nexus-1-0x5a", "p": "Nexus", "ch": 1, "t": 4.2},
                                         {"id": "remote-7a0b", "p": "Remote", "kind": "remote", "btn": 1}]})
    e.feed("wx", {"temp": 4.2, "hum": 81, "tsrc": "Nexus"})
    clk.t += 700
    e.feed("wx", {"temp": 2.9, "hum": 85, "tsrc": "Nexus"})
    e.feed("st", {"bat": 20, "usb": 0})
    e.feed("st", {"bat": 14, "usb": 0})
    e.link(True); clk.t += 6; first = e.pop()
    e.link(False); clk.t += 2; e.link(True)                                            # link blip: no popup
    clk.t += 6
    e.link(False)
    out = first + run_for(e, clk, 8)
    kinds = sorted(n["kind"] for n in out)
    assert kinds == sorted(["ir", "nfc", "sensor", "temp", "battery", "link"]), kinds
    snap = e.snapshot()
    assert snap["temp"]["t"] == 2.9 and snap["ir"]["brand"] == "LG TV" and snap["nfc"]["fam"].startswith("MIFARE")
    return "ir/nfc/sensor/temp/battery/link: one each, repeats and blinks suppressed"


def test_cfg_from_settings_and_activity():
    d = {"pages": [{"id": "alert", "items": [
        {"k": "al_near", "type": "range", "v": -80}, {"k": "al_new", "type": "enum", "v": "any", "opts": ["off", "near", "any"]},
        {"k": "al_csnd", "type": "bool", "v": False}, {"k": "al_sens", "type": "range", "v": 10}]}]}
    c = cfg_from_settings(d)
    assert c["al_near"] == -80 and c["al_new"] == 2 and c["al_csnd"] == 0 and c["al_sens"] == 10, c
    clk = Clock()
    e = Engine(clock=clk)
    e.set_cfg(c)
    for i in range(3):
        e.feed("ir", {"p": "NEC", "addr": 1, "cmd": i, "rep": 0})
        clk.t += 2
    a = e.activity()
    assert a["parts"]["prb"] == 1.0 and not a["en"]["snd"] and a["v"] > 0, a
    return "settings parsed; activity: IR/NFC part full, sound disabled → v=%.2f state=%d" % (a["v"], a["state"])


if __name__ == "__main__":
    ok = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            print("OK  ", fn())
            ok += 1
    print("%d tests passed" % ok)
