#!/usr/bin/env python3
"""Build the desktop app into a single dist/tengen_desktop.exe (Windows, CI).

    pip install -r tools/desktop/requirements.txt pyinstaller
    python tools/desktop/build_exe.py
"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
args = [
    sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--onefile", "--windowed",
    "--name", "tengen_desktop",
    "--icon", os.path.join(HERE, "tengen.ico"),          # app icon (file, taskbar, window)
    "--distpath", os.path.join(ROOT, "dist"),
    "--workpath", os.path.join(ROOT, "build", "pyi"),
    "--specpath", os.path.join(ROOT, "build", "pyi"),
    "--paths", os.path.join(ROOT, "tools", "pc_monitor"),
    "--hidden-import", "pc_monitor",
    "--collect-all", "webview",
    "--collect-submodules", "serial",
    "--collect-submodules", "comtypes",
    "--collect-submodules", "pycaw",
    "--collect-all", "pyaudiowpatch",
    "--collect-all", "winrt",
    "--hidden-import", "clr",
    # tray, popups, widget (see docs/tech/en/05-applications.md, section 3.4)
    "--paths", HERE,
    "--hidden-import", "events",
    "--hidden-import", "overlay",
    "--hidden-import", "overlay_html",
    "--collect-submodules", "pystray",
    "--hidden-import", "PIL.Image",
    "--add-data", os.path.join(HERE, "tengen.ico") + os.pathsep + ".",   # tray icon bundled inside the exe
    os.path.join(HERE, "tengen_desktop.py"),
]
print(" ".join(args), flush=True)
sys.exit(subprocess.call(args))
