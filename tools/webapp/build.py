#!/usr/bin/env python3
"""Сборка приложения для планшета в один файл -> src/web/tablet_page.h

  исходники: tools/webapp/src (index.html с метками /*@INLINE путь*/)
  результат: всё внутри одного HTML (без интернета), сжато gzip -> массив байт в прошивке.
  Палка отдаёт его по /tablet с заголовком Content-Encoding: gzip.

    python3 tools/webapp/build.py            # собрать
    python3 tools/webapp/build.py --html out.html   # ещё и сохранить несжатый HTML (для проверки)
"""
import gzip, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src")
OUT = os.path.join(HERE, "..", "..", "src", "web", "tablet_page.h")


def slim(code, kind):
    """Лёгкая минификация: убрать строки-комментарии и отступы (без риска сломать код)."""
    out = []
    for line in code.splitlines():
        t = line.strip()
        if not t or (kind == "js" and t.startswith("//")) or (kind == "css" and t.startswith("/*") and t.endswith("*/")):
            continue
        out.append(t)
    return "\n".join(out)


def build():
    html = open(os.path.join(SRC, "index.html"), encoding="utf-8").read()
    def inline(m):
        path = m.group(1)
        code = open(os.path.join(SRC, path), encoding="utf-8").read()
        return slim(code, "css" if path.endswith(".css") else "js")
    html = re.sub(r"/\*@INLINE ([^*]+?)\*/", inline, html)
    assert "@INLINE" not in html
    assert not re.search(r'(src|href)="https?://', html), "внешние ссылки запрещены: страница должна работать без интернета"
    return html


def main():
    html = build()
    if "--html" in sys.argv:
        open(sys.argv[sys.argv.index("--html") + 1], "w", encoding="utf-8").write(html)
    gz = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)   # mtime=0 — одинаковый результат при пересборке
    lines = ["// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/webapp/build.py из tools/webapp/src — не править руками.",
             "// Приложение для планшета (один HTML, gzip). Отдаётся по /tablet.",
             "#pragma once", "#include <Arduino.h>", "",
             "static const uint8_t TABLET_GZ[] PROGMEM = {"]
    for i in range(0, len(gz), 20):
        lines.append("  " + ",".join("0x%02x" % b for b in gz[i:i + 20]) + ",")
    lines += ["};", "static const size_t TABLET_GZ_LEN = %d;" % len(gz), ""]
    data = "\n".join(lines)
    if "--check" in sys.argv:                          # для CI: исходники и прошивка не разъехались
        cur = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if cur != data:
            sys.exit("src/web/tablet_page.h устарел: запусти python3 tools/webapp/build.py")
        print("tablet_page.h актуален"); return
    open(OUT, "w", encoding="utf-8").write(data)
    print("tablet: %d КБ HTML -> %d КБ gzip -> %s" % (len(html) // 1024, len(gz) // 1024, os.path.relpath(OUT)))


if __name__ == "__main__":
    main()
