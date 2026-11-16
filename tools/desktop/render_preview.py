# -*- coding: utf-8 -*-
"""Демо-страницы всплывашек и виджета с выдуманными данными (для просмотра в браузере без палки).

    python3 tools/desktop/render_preview.py <папка>
Создаёт popup_nerv.html, popup_domain.html, widget_nerv.html, widget_domain.html.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from overlay_html import POPUP_HTML, WIDGET_HTML  # noqa: E402
from events import Engine  # noqa: E402

NOTES = [dict(kind="new", kj="接近警告", title="NEW DEVICE NEARBY", text="JBL TUNE770NC · -52 dBm", sev="alert"),
         dict(kind="ir", kj="赤外線", title="REMOTE", text="Samsung TV · NEC 0x07", sev="info"),
         dict(kind="nfc", kj="近接", title="CARD", text="MIFARE Classic 1K · transit card (likely)", sev="info")]


def demo_snapshot():
    e = Engine()
    devs = [[i, -48 - i * 2, "tracker" if i == 3 else "phone", "", "dev%d" % i, 1, 1, "наушники" if i == 2 else ""] for i in range(1, 22)]
    e.feed("ble", {"d": devs}, now=0)
    for d in list(e.devs.values())[:3]:
        d["first"] = False
    e.feed("wx", {"temp": 3.8, "hum": 81, "tsrc": "Nexus"}, now=1)
    for k in range(2):
        e.feed("ir", {"p": "NEC", "brand": "Samsung TV", "cmd": k, "addr": 7}, now=1 + k * 2)
    e.feed("nfc", {"fam": "NTAG213", "uid": "04A2"}, now=5)
    e.feed("st", {"bat": 74}, now=5)
    e.link_ok = True
    return e.snapshot(now=6)


def main(out):
    os.makedirs(out, exist_ok=True)
    snap = demo_snapshot()
    for st in ("nerv", "domain"):
        adds = "".join("P.add(%s);" % json.dumps(n, ensure_ascii=False) for n in NOTES)
        with open(os.path.join(out, "popup_%s.html" % st), "w", encoding="utf-8") as f:
            f.write(POPUP_HTML.replace("</body>", '<script>P.style("%s");P.dur(60);%s</script></body>' % (st, adds)))
        with open(os.path.join(out, "widget_%s.html" % st), "w", encoding="utf-8") as f:
            f.write(WIDGET_HTML.replace("</body>", '<script>W.style("%s");W.update(%s)</script></body>' % (st, json.dumps(snap, ensure_ascii=False))))
    print("готово:", out, "активность", snap["act"]["v"], "устройств", snap["n"])


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "preview")
