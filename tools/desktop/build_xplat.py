#!/usr/bin/env python3
"""Собрать TENGAN Desktop для macOS или Linux (на соответствующей системе, CI: .github/workflows/desktop-xplat.yml).

    pip install -r tools/desktop/requirements-mac.txt pyinstaller      # или requirements-linux.txt
    python tools/desktop/build_xplat.py mac      ->  dist/TENGAN-Desktop-mac.zip  (внутри «TENGAN Desktop.app»)
    python tools/desktop/build_xplat.py linux    ->  dist/tengan_desktop (один файл; AppImage собирает CI)

Windows-версия собирается отдельно: tools/desktop/build_exe.py.
"""
import os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DIST = os.path.join(ROOT, "dist")
BUILD = os.path.join(ROOT, "build", "pyi-xplat")


def icons():
    """Значок 天眼 из tengan.ico: .icns (macOS) и .png 256 (Linux)."""
    from PIL import Image
    os.makedirs(BUILD, exist_ok=True)
    im = Image.open(os.path.join(HERE, "tengan.ico"))
    im.size = max(im.info.get("sizes", [im.size]))
    im = im.convert("RGBA").resize((512, 512), Image.LANCZOS)
    png = os.path.join(BUILD, "tengan.png")
    im.resize((256, 256), Image.LANCZOS).save(png)
    icns = os.path.join(BUILD, "tengan.icns")
    im.save(icns, sizes=[(16, 16), (32, 32), (64, 64), (128, 128), (256, 256), (512, 512)])
    return png, icns


def common():
    return [
        sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean",
        "--distpath", DIST, "--workpath", os.path.join(BUILD, "work"), "--specpath", BUILD,
        "--paths", os.path.join(ROOT, "tools", "pc_monitor"), "--paths", HERE,
        "--hidden-import", "pc_monitor", "--hidden-import", "events",
        "--hidden-import", "overlay", "--hidden-import", "overlay_html",
        "--collect-submodules", "webview", "--collect-data", "webview",
        "--collect-submodules", "serial", "--collect-submodules", "pystray", "--hidden-import", "PIL.Image",
        "--add-data", os.path.join(HERE, "tengan.ico") + os.pathsep + ".",
    ]


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else ("mac" if sys.platform == "darwin" else "linux")
    png, icns = icons()
    os.makedirs(DIST, exist_ok=True)
    if target == "mac":
        args = common() + ["--windowed", "--name", "TENGAN Desktop", "--icon", icns,
                           "--osx-bundle-identifier", "app.tengan.desktop",
                           os.path.join(HERE, "tengan_desktop.py")]
        print(" ".join(args), flush=True)
        if subprocess.call(args):
            sys.exit(1)
        app = os.path.join(DIST, "TENGAN Desktop.app")
        out = os.path.join(DIST, "TENGAN-Desktop-mac.zip")
        if os.path.exists(out):
            os.remove(out)
        # ditto сохраняет права и атрибуты .app (обычный zip ломает подпись/запуск)
        sys.exit(subprocess.call(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", app, out]))
    else:
        # onedir (не onefile): Qt WebEngine нужен QtWebEngineProcess и ресурсы рядом —
        # в одном файле они теряются. Папку dist/tengan_desktop/ упакует в AppImage сам CI.
        args = common() + ["--onedir", "--name", "tengan_desktop",
                           "--collect-all", "PySide6", "--collect-submodules", "qtpy",
                           "--hidden-import", "sounddevice",
                           os.path.join(HERE, "tengan_desktop.py")]
        print(" ".join(args), flush=True)
        if subprocess.call(args):
            sys.exit(1)
        shutil.copy(png, os.path.join(DIST, "tengan.png"))


if __name__ == "__main__":
    main()
