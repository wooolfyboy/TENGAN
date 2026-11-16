#!/usr/bin/env python3
"""Собрать приложение для компьютера в один файл dist/tengan_desktop.exe (на Windows, CI).

    pip install -r tools/desktop/requirements.txt pyinstaller
    python tools/desktop/build_exe.py
"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
args = [
    sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--onefile", "--windowed",
    "--name", "tengan_desktop",
    "--icon", os.path.join(HERE, "tengan.ico"),          # значок TENGAN 天眼 (файл, панель задач, окно)
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
    os.path.join(HERE, "tengan_desktop.py"),
]
print(" ".join(args), flush=True)
sys.exit(subprocess.call(args))
