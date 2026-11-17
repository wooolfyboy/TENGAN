#!/usr/bin/env python3
"""Fake USB stick for testing the desktop app without hardware (Linux/macOS).

Creates a virtual serial port (pty) and answers the "@..." protocol like the firmware
(src/web/web.cpp); data comes from the mock stick tools/webapp/mock.py.

    python3 tools/webapp/build.py --html /tmp/tablet.html
    python3 tools/webapp/mock.py /tmp/tablet.html 8090 &
    python3 tools/desktop/fake_stick.py /tmp/tablet.html 8090 /tmp/fake_port.txt
    (the port name, e.g. /dev/pts/5, is written to /tmp/fake_port.txt)

Lines not starting with "@" (PC monitor stats) are printed, to show what the monitor sends.
"""
import base64, gzip, hashlib, json, os, pty, sys, threading, time, tty, urllib.request

HTML = open(sys.argv[1], "rb").read()
MOCK = int(sys.argv[2]) if len(sys.argv) > 2 else 8090
OUT = sys.argv[3] if len(sys.argv) > 3 else None
GZ = gzip.compress(HTML)
SHA = hashlib.sha256(HTML).hexdigest()

master, slave = pty.openpty()
tty.setraw(slave)
name = os.ttyname(slave)
if OUT:
    open(OUT, "w").write(name)
print("fake stick on", name, flush=True)

wlock = threading.Lock()


def out(line):
    with wlock:
        os.write(master, (line + "\n").encode("utf-8"))


state = {"want": None, "gen": 0}


def stream(want, gen):
    """Subscribe: read the mock SSE stream and forward it as "@E event json"."""
    try:
        r = urllib.request.urlopen("http://127.0.0.1:%d/stream?%s" % (MOCK + 1, want), timeout=10)
    except Exception as e:
        print("mock sse:", e, flush=True)
        return
    ev = None
    for raw in r:
        if state["gen"] != gen:
            break
        line = raw.decode("utf-8").rstrip("\n")
        if line.startswith("event: "):
            ev = line[7:]
        elif line.startswith("data: ") and ev:
            out("@E %s %s" % (ev, line[6:]))
            ev = None


def handle(line):
    if line.startswith("@W "):
        q = line[3:]
        if q != state["want"]:
            state["want"] = q
            state["gen"] += 1
            threading.Thread(target=stream, args=(q, state["gen"]), daemon=True).start()
    elif line.startswith("@Q "):
        rid, _, path = line[3:].partition(" ")
        try:
            body = urllib.request.urlopen("http://127.0.0.1:%d%s" % (MOCK, path), timeout=5).read().decode("utf-8")
        except Exception as e:
            body = json.dumps({"ok": False, "err": str(e)})
        out("@R %s %s" % (rid, body.replace("\n", " ")))
    elif line.startswith("@G "):
        pid = line[3:].strip()
        out("@g %s begin %d %s" % (pid, len(GZ), SHA))
        for off in range(0, len(GZ), 1536):
            out("@g %s %d %s" % (pid, off, base64.b64encode(GZ[off:off + 1536]).decode()))
        out("@g %s end" % pid)
    else:
        print("monitor ->", line[:100], flush=True)


buf = b""
while True:
    try:
        chunk = os.read(master, 4096)
    except OSError:
        time.sleep(0.05)
        continue
    buf += chunk
    while b"\n" in buf:
        raw, buf = buf.split(b"\n", 1)
        s = raw.decode("utf-8", "replace").strip()
        if s:
            handle(s)
