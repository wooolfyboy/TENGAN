# Версия TENGAN — одна на всё: файл VERSION в корне репозитория.
# PlatformIO запускает этот скрипт перед сборкой: пишет src/core/version.h (только если изменилось).
# build.py берёт ту же версию для страницы планшета, а приложение — для своего versionName.
import os
try:
    Import("env")  # noqa: F821  (есть, когда запускает PlatformIO)
    ROOT = env["PROJECT_DIR"]  # noqa: F821
except Exception:
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ver = open(os.path.join(ROOT, "VERSION")).read().strip()
out = os.path.join(ROOT, "src", "core", "version.h")
text = ('// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/version.py из файла VERSION — не править руками.\n'
        '#pragma once\n#define FW_VERSION "%s"\n' % ver)
if not os.path.exists(out) or open(out).read() != text:
    open(out, "w").write(text)
