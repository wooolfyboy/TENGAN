#!/usr/bin/env python3
"""CI: run the built app for N seconds and check the log for a startup crash
(e.g. PyInstaller missed a module). The window is not touched: just wait, then close.

    python tools/desktop/smoke_run.py <path to executable> [seconds]
"""
import os, signal, subprocess, sys, tempfile, time

exe, secs = sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 25
if sys.platform == "darwin":
    logdir = os.path.expanduser("~/Library/Application Support/TENGEN")
else:
    logdir = os.path.join(os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config"), "TENGEN")
log = os.path.join(logdir, "desktop.log")
if os.path.exists(log):
    os.remove(log)
# Output goes to a file, not a pipe: the packed app spawns a child, so the pipe stays open while any
# child lives (CI once hung for an hour). We kill the whole process group.
outf = tempfile.TemporaryFile()
p = subprocess.Popen([exe], stdout=outf, stderr=subprocess.STDOUT, start_new_session=True)
try:
    p.wait(timeout=secs)
    early = True
except subprocess.TimeoutExpired:
    early = False
    for sig in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(p.pid, sig)
        except ProcessLookupError:
            break
        try:
            p.wait(timeout=5)
            break
        except subprocess.TimeoutExpired:
            pass
outf.seek(0)
out = outf.read()
text = (out or b"").decode("utf-8", "replace")
if os.path.exists(log):
    text += "\n--- desktop.log ---\n" + open(log, encoding="utf-8", errors="replace").read()
print(text[-6000:])
bad = "Traceback" in text or "ModuleNotFoundError" in text or "ImportError" in text
if early:
    print("приложение завершилось само через < %d с (код %s)" % (secs, p.returncode))
if bad or (early and p.returncode not in (0, None)):
    sys.exit(1)
print("OK: приложение запустилось и проработало %d с без ошибок" % secs)
