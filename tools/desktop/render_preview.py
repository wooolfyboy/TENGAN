# -*- coding: utf-8 -*-
"""Demo popup and widget pages with made-up data (view in a browser without TENGAN).

    python3 tools/desktop/render_preview.py <dir>
Writes popup_<theme>_<lang>.html and widget_<theme>_<lang>.html (themes denko / kurenai, langs en / ru).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from overlay_html import POPUP_HTML, WIDGET_HTML  # noqa: E402
from events import Engine  # noqa: E402
from tengan_desktop import note_title  # noqa: E402

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
    for st in ("denko", "kurenai"):
        for lang in ("en", "ru"):
            notes = [dict(n, title=note_title(n["title"], lang)) for n in NOTES]
            adds = "".join("P.add(%s);" % json.dumps(n, ensure_ascii=False) for n in notes)
            with open(os.path.join(out, "popup_%s_%s.html" % (st, lang)), "w", encoding="utf-8") as f:
                f.write(POPUP_HTML.replace("</body>", '<script>P.style("%s");P.dur(60);%s</script></body>' % (st, adds)))
            with open(os.path.join(out, "widget_%s_%s.html" % (st, lang)), "w", encoding="utf-8") as f:
                f.write(WIDGET_HTML.replace("</body>", '<script>W.style("%s");W.lang("%s");W.update(%s)</script></body>'
                                            % (st, lang, json.dumps(snap, ensure_ascii=False))))
    print("готово:", out, "активность", snap["act"]["v"], "устройств", snap["n"])


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "preview")
