#!/usr/bin/env python3
"""CI: запустить собранное приложение на N секунд и проверить журнал — нет ли падения при старте
(например, PyInstaller не положил какой-то модуль). Окно не трогаем: просто ждём и закрываем.

    python tools/desktop/smoke_run.py <путь к исполняемому файлу> [секунд]
"""
import os, subprocess, sys, time

exe, secs = sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 25
if sys.platform == "darwin":
    logdir = os.path.expanduser("~/Library/Application Support/TENGAN")
else:
    logdir = os.path.join(os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config"), "TENGAN")
log = os.path.join(logdir, "desktop.log")
if os.path.exists(log):
    os.remove(log)
p = subprocess.Popen([exe], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
try:
    out, _ = p.communicate(timeout=secs)
    early = True
except subprocess.TimeoutExpired:
    p.kill()
    out, _ = p.communicate()
    early = False
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
