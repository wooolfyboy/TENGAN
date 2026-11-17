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


def prune_qt(root):
    """Linux: убрать из сборки части Qt, которые окну не нужны (их затягивают хуки PyInstaller вместе
    с QtWebEngine): QML-модули, переводы Qt, локали Chromium кроме en-US/ru (остальные языки падают на en-US)
    и библиотеки Qt, на которые не ссылается ни один оставшийся бинарник (замыкание по ELF NEEDED)."""
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
        # Только модули Qt, которые реально грузит окно pywebview (qtpy подключает их динамически,
        # поэтому перечисляем явно); ресурсы и QtWebEngineProcess добавляют штатные хуки PyInstaller.
        # Раньше было --collect-all PySide6 — весь Qt (≈ 690 МБ). Тесты qtpy тянут все модули — исключаем.
        qt = ["QtCore", "QtGui", "QtWidgets", "QtNetwork", "QtWebChannel", "QtPrintSupport",
              "QtOpenGL", "QtOpenGLWidgets", "QtWebEngineCore", "QtWebEngineWidgets"]
        hidden = []
        for m in qt:
            hidden += ["--hidden-import", "PySide6." + m, "--hidden-import", "qtpy." + m]
        args = common() + ["--onedir", "--strip", "--name", "tengan_desktop", "--hidden-import", "sounddevice",
                           "--exclude-module", "qtpy.tests", "--exclude-module", "PySide6.QtDataVisualization"] + hidden + [
                           os.path.join(HERE, "tengan_desktop.py")]
        print(" ".join(args), flush=True)
        if subprocess.call(args):
            sys.exit(1)
        prune_qt(os.path.join(DIST, "tengan_desktop"))
        shutil.copy(png, os.path.join(DIST, "tengan.png"))


if __name__ == "__main__":
    main()
