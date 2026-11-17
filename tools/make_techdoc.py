#!/usr/bin/env python3
"""Build the technical documentation PDF: docs/tech/<lang>/*.md -> one HTML (pandoc) -> PDF (headless Chrome/Chromium).

Usage: python tools/make_techdoc.py <en|ru> <out.pdf>
Needs: pandoc and a Chrome/Chromium binary (CHROME env var, or found on PATH / in the Playwright cache).
"""
import glob, os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TITLES = {"en": "TENGAN 天眼 — Technical documentation", "ru": "TENGAN 天眼 — Техническая документация"}
CSS = """
@page { size: A4; margin: 18mm 16mm; }
body { font-family: 'DejaVu Sans', 'Liberation Sans', sans-serif; font-size: 10pt; line-height: 1.45; color: #111; }
h1 { font-size: 20pt; border-bottom: 2px solid #111; padding-bottom: 4px; margin-top: 0; page-break-before: always; }
h1.title { page-break-before: avoid; font-size: 26pt; border: 0; margin-top: 60mm; }
h2 { font-size: 14pt; margin-top: 1.4em; } h3 { font-size: 11.5pt; }
pre, code { font-family: 'DejaVu Sans Mono', monospace; font-size: 8.4pt; }
pre { background: #f3f3f3; padding: 6px 8px; border-radius: 3px; white-space: pre-wrap; word-break: break-word; page-break-inside: avoid; }
code { background: #f3f3f3; padding: 0 2px; }
pre code { background: none; padding: 0; }
table { border-collapse: collapse; width: 100%; margin: 8px 0; font-size: 9pt; page-break-inside: auto; }
th, td { border: 1px solid #bbb; padding: 3px 6px; vertical-align: top; } th { background: #eee; }
tr { page-break-inside: avoid; }
nav#TOC { page-break-after: always; } nav#TOC ul { list-style: none; padding-left: 1.2em; }
img { max-width: 100%; }
"""

def find_chrome():
    c = os.environ.get("CHROME")
    if c and os.path.exists(c):
        return c
    for n in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser", "chrome"):
        p = shutil.which(n)
        if p:
            return p
    hits = sorted(glob.glob("/opt/pw-browsers/chromium-*/chrome-linux*/chrome") + glob.glob(os.path.expanduser("~/.cache/ms-playwright/chromium-*/chrome-linux*/chrome")))
    if hits:
        return hits[-1]
    sys.exit("no Chrome/Chromium found (set CHROME)")

def main():
    if len(sys.argv) != 3 or sys.argv[1] not in TITLES:
        sys.exit(__doc__)
    lang, out = sys.argv[1], os.path.abspath(sys.argv[2])
    files = sorted(glob.glob(os.path.join(ROOT, "docs", "tech", lang, "*.md")))
    if not files:
        sys.exit("no docs in docs/tech/" + lang)
    with tempfile.TemporaryDirectory() as tmp:
        css = os.path.join(tmp, "doc.css"); html = os.path.join(tmp, "doc.html")
        open(css, "w", encoding="utf-8").write(CSS)
        subprocess.check_call(["pandoc", "-f", "markdown", "-t", "html5", "--standalone", "--embed-resources",
                               "--toc", "--toc-depth=2", "-c", css, "--metadata", "title=" + TITLES[lang],
                               "--metadata", "lang=" + lang, "-o", html] + files, cwd=os.path.join(ROOT, "docs", "tech", lang))
        subprocess.check_call([find_chrome(), "--headless=new", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
                               "--print-to-pdf=" + out, "file://" + html], stderr=subprocess.DEVNULL)
    print("wrote", out, os.path.getsize(out), "bytes")

if __name__ == "__main__":
    main()
