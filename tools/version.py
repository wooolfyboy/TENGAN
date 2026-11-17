# TENGAN version — single source: the VERSION file in the repo root.
# PlatformIO runs this before the build: writes src/core/version.h (only if changed).
# build.py uses the same version for the tablet page, and the app for its versionName.
import os
try:
    Import("env")  # noqa: F821  (defined when run by PlatformIO)
    ROOT = env["PROJECT_DIR"]  # noqa: F821
except Exception:
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ver = open(os.path.join(ROOT, "VERSION")).read().strip()
out = os.path.join(ROOT, "src", "core", "version.h")
text = ('// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/version.py из файла VERSION — не править руками.\n'
        '#pragma once\n#define FW_VERSION "%s"\n' % ver)
if not os.path.exists(out) or open(out).read() != text:
    open(out, "w").write(text)
