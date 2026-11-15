#!/usr/bin/env python3
"""Сборка приложения для планшета в один файл -> src/web/tablet_page.h

  исходники: tools/webapp/src (index.html с метками /*@INLINE путь*/)
  результат: всё внутри одного HTML (без интернета), сжато gzip -> массив байт в прошивке.
  Палка отдаёт его по /tablet с заголовком Content-Encoding: gzip.

    python3 tools/webapp/build.py            # собрать
    python3 tools/webapp/build.py --html out.html   # ещё и сохранить несжатый HTML (для проверки)
    python3 tools/webapp/build.py --html out.html --only-html   # только HTML, прошивку не трогать
"""
import gzip, hashlib, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src")
OUT = os.path.join(HERE, "..", "..", "src", "web", "tablet_page.h")
FW = os.path.join(HERE, "..", "..", "src")


def c_array(text, name):
    """Числа из static const ... NAME[] = { ... }; в плоский список."""
    m = re.search(r"static const [^=;]*\b%s\b[^=;]*=\s*\{(.*?)\};" % re.escape(name), text, re.S)
    assert m, name
    return [int(x) for x in re.findall(r"-?\d+", re.sub(r'"[^"]*"', "", m.group(1)))]


def data_js():
    """Данные с палки для планшета — берутся прямо из заголовков прошивки, чтобы не разъезжались:
    звёзды, линии созвездий, контуры суши (gallery/astrodata.h)."""
    a = open(os.path.join(FW, "gallery", "astrodata.h"), encoding="utf-8").read()
    con = re.search(r"CON_NAMES\[\]\s*=\s*\{(.*?)\};", a, re.S).group(1)
    cons = [[int(r), int(d), ab, ru] for r, d, ab, ru in re.findall(r'\{\s*(-?\d+),\s*(-?\d+),\s*"([^"]*)",\s*"([^"]*)"\s*\}', con)]
    names = re.search(r"STAR_NAMES\[\]\s*=\s*\{(.*?)\};", a, re.S).group(1)
    snames = [[int(i), n] for i, n in re.findall(r'\{\s*(\d+),\s*"([^"]*)"\s*\}', names)]
    d = {
        "stars": c_array(a, "STARS"),            # тройки: ra*100, dec*100, (mag+2)*10
        "starNames": snames,
        "cl": c_array(a, "CL_PTS"),              # пары ra*100, dec*100 (uint16 -> знак восстановить)
        "clStart": c_array(a, "CL_START"),
        "con": cons,                             # [ra*100, dec*100, "Ori", "Орион"]
        "land": c_array(a, "LAND_PTS"),          # пары (как в gallery_sky.cpp)
        "landStart": c_array(a, "LAND_START"),
    }
    return "T.DATA=" + json.dumps(d, ensure_ascii=False, separators=(",", ":")) + ";"


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
    html = html.replace("/*@DATA*/", data_js())
    ver = open(os.path.join(HERE, "..", "..", "VERSION")).read().strip()
    html = html.replace("@@VERSION@@", ver)                      # версия TENGAN (файл VERSION)
    assert "@INLINE" not in html
    assert not re.search(r'(src|href)="https?://', html), "внешние ссылки запрещены: страница должна работать без интернета"
    return html


def main():
    html = build()
    if "--html" in sys.argv:
        out = sys.argv[sys.argv.index("--html") + 1]
        open(out, "w", encoding="utf-8").write(html)
        # рядом — версия страницы: приложение сравнивает её с палкой (/api/pagever)
        open(os.path.join(os.path.dirname(os.path.abspath(out)), "pagever.txt"), "w").write(hashlib.sha256(html.encode("utf-8")).hexdigest())
        if "--only-html" in sys.argv:                  # для разработки: прошивку не трогать
            print("tablet: %d КБ HTML" % (len(html) // 1024)); return
    gz = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)   # mtime=0 — одинаковый результат при пересборке
    digest = hashlib.sha256(html.encode("utf-8")).hexdigest()
    lines = ["// АВТОМАТИЧЕСКИ СГЕНЕРИРОВАНО tools/webapp/build.py из tools/webapp/src — не править руками.",
             "// Приложение для планшета (один HTML, gzip). Отдаётся по /tablet.",
             "// html-sha256: " + digest,
             "#pragma once", "#include <Arduino.h>", "",
             "static const uint8_t TABLET_GZ[] PROGMEM = {"]
    for i in range(0, len(gz), 20):
        lines.append("  " + ",".join("0x%02x" % b for b in gz[i:i + 20]) + ",")
    lines += ["};", "static const size_t TABLET_GZ_LEN = %d;" % len(gz),
              "// версия страницы (sha256 несжатого HTML): по ней приложение планшета понимает, что пора обновиться",
              'static const char TABLET_HTML_SHA[] = "%s";' % digest, ""]
    data = "\n".join(lines)
    if "--check" in sys.argv:                          # для CI: исходники и прошивка не разъехались
        # сравниваем сам HTML (по хэшу), а не сжатые байты: zlib разных версий жмёт чуть по-разному
        cur = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        m = re.search(r"html-sha256: ([0-9a-f]{64})", cur)
        if not m or m.group(1) != digest:
            sys.exit("src/web/tablet_page.h устарел: запусти python3 tools/webapp/build.py")
        print("tablet_page.h актуален"); return
    open(OUT, "w", encoding="utf-8").write(data)
    print("tablet: %d КБ HTML -> %d КБ gzip -> %s" % (len(html) // 1024, len(gz) // 1024, os.path.relpath(OUT)))


if __name__ == "__main__":
    main()
