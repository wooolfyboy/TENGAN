#!/usr/bin/env python3
"""Build TENGEN Desktop for macOS or Linux (on that OS; CI: .github/workflows/desktop-xplat.yml).

    pip install -r tools/desktop/requirements-mac.txt pyinstaller      # or requirements-linux.txt
    python tools/desktop/build_xplat.py mac      ->  dist/TENGEN-Desktop-mac.zip  (contains "TENGEN Desktop.app")
    python tools/desktop/build_xplat.py linux    ->  dist/tengen_desktop (onedir; CI packs it into an AppImage)

Windows build: tools/desktop/build_exe.py.
"""
import os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DIST = os.path.join(ROOT, "dist")
BUILD = os.path.join(ROOT, "build", "pyi-xplat")


def icons():
    """Make .icns (macOS) and a 256 px .png (Linux) from tengen.ico."""
    from PIL import Image
    os.makedirs(BUILD, exist_ok=True)
    im = Image.open(os.path.join(HERE, "tengen.ico"))
    im.size = max(im.info.get("sizes", [im.size]))
    im = im.convert("RGBA").resize((512, 512), Image.LANCZOS)
    png = os.path.join(BUILD, "tengen.png")
    im.resize((256, 256), Image.LANCZOS).save(png)
    icns = os.path.join(BUILD, "tengen.icns")
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
        "--add-data", os.path.join(HERE, "tengen.ico") + os.pathsep + ".",
    ]


def prune_qt(root):
    """Linux: drop Qt parts the window does not need (PyInstaller hooks pull them in with QtWebEngine):
    QML modules, Qt translations, Chromium locales except en-US/ru (others fall back to en-US),
    and Qt libs no remaining binary links to (closure over ELF NEEDED)."""
    qt = os.path.join(root, "_internal", "PySide6", "Qt")
    if not os.path.isdir(qt):
        return
    shutil.rmtree(os.path.join(qt, "qml"), ignore_errors=True)
    tr = os.path.join(qt, "translations")
    for f in os.listdir(tr) if os.path.isdir(tr) else []:
        if f.endswith(".qm"):
            os.remove(os.path.join(tr, f))
    loc = os.path.join(tr, "qtwebengine_locales")
    for f in os.listdir(loc) if os.path.isdir(loc) else []:
        if f not in ("en-US.pak", "ru.pak"):
            os.remove(os.path.join(loc, f))
    if not shutil.which("readelf"):
        print("prune_qt: нет readelf — библиотеки Qt не чищу", flush=True)
        return
    lib = os.path.join(qt, "lib")

    def needed(path):
        out = subprocess.run(["readelf", "-d", path], capture_output=True, text=True).stdout
        return {l.split("[", 1)[1].split("]", 1)[0] for l in out.splitlines() if "(NEEDED)" in l}

    def is_elf(path):
        with open(path, "rb") as fh:
            return fh.read(4) == b"\x7fELF"

    libs = {f: os.path.join(lib, f) for f in os.listdir(lib)}
    todo = [os.path.join(d, f) for d, _, fs in os.walk(root) if not d.startswith(lib)
            for f in fs if not os.path.islink(os.path.join(d, f)) and is_elf(os.path.join(d, f))]
    keep = set()
    while todo:
        for n in needed(todo.pop()):
            if n in libs and n not in keep:
                keep.add(n)
                todo.append(os.path.realpath(libs[n]))
    keep |= {os.path.basename(os.path.realpath(libs[n])) for n in keep}
    gone = [f for f in libs if f not in keep]
    for f in gone:
        os.remove(libs[f])
    print("prune_qt: убрал библиотек Qt: %d, оставил: %d" % (len(gone), len(libs) - len(gone)), flush=True)


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else ("mac" if sys.platform == "darwin" else "linux")
    png, icns = icons()
    os.makedirs(DIST, exist_ok=True)
    if target == "mac":
        args = common() + ["--windowed", "--name", "TENGEN Desktop", "--icon", icns,
                           "--osx-bundle-identifier", "app.tengen.desktop",
                           os.path.join(HERE, "tengen_desktop.py")]
        print(" ".join(args), flush=True)
        if subprocess.call(args):
            sys.exit(1)
        app = os.path.join(DIST, "TENGEN Desktop.app")
        out = os.path.join(DIST, "TENGEN-Desktop-mac.zip")
        if os.path.exists(out):
            os.remove(out)
        # ditto keeps .app permissions/attributes (plain zip breaks signing and launch)
        sys.exit(subprocess.call(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", app, out]))
    else:
        # onedir, not onefile: QtWebEngineProcess and resources must sit next to the exe; CI packs the folder into an AppImage.
        # List only the Qt modules pywebview really loads (qtpy imports them dynamically); PyInstaller hooks add the rest.
        # --collect-all PySide6 pulled all of Qt (~690 MB); qtpy tests import every module, so exclude them.
        qt = ["QtCore", "QtGui", "QtWidgets", "QtNetwork", "QtWebChannel", "QtPrintSupport",
              "QtOpenGL", "QtOpenGLWidgets", "QtWebEngineCore", "QtWebEngineWidgets"]
        hidden = []
        for m in qt:
            hidden += ["--hidden-import", "PySide6." + m, "--hidden-import", "qtpy." + m]
        args = common() + ["--onedir", "--strip", "--name", "tengen_desktop", "--hidden-import", "sounddevice",
                           "--exclude-module", "qtpy.tests", "--exclude-module", "PySide6.QtDataVisualization"] + hidden + [
                           os.path.join(HERE, "tengen_desktop.py")]
        print(" ".join(args), flush=True)
        if subprocess.call(args):
            sys.exit(1)
        prune_qt(os.path.join(DIST, "tengen_desktop"))
        shutil.copy(png, os.path.join(DIST, "tengen.png"))


if __name__ == "__main__":
    main()
