# -*- coding: utf-8 -*-
"""Проверка новых файлов календаря на «загрязнение» текста:
китайские/японские базовые иероглифы, мягкие переносы, латиница внутри
кириллических слов, незакрытые кавычки в строковых литералах."""
import re
import sys
import pathlib

src = pathlib.Path(__file__).resolve().parent.parent / "src"
files = sorted(
    [p for p in src.iterdir() if p.suffix in (".cpp", ".h") and p.name != "prayers_data.cpp"]
)

CJK = re.compile("[\u4e00-\u9fff\u3040-\u30ff\uac00-\ud7af]")
SOFT = re.compile("\u00ad")
CYRCYR = re.compile("[\u0400-\u04ff][a-zA-Z]+[\u0400-\u04ff]")
STRAY = re.compile("(?:[\u0400-\u04ff])(?:[a-zA-Z](?:[\u0400-\u04ff]))")

exit_code = 0
for f in files:
    t = f.read_text(encoding="utf-8")
    issues = []
    for i, ln in enumerate(t.splitlines(), 1):
        tag = []
        if CJK.search(ln):
            tag.append("CJK")
        if SOFT.search(ln):
            tag.append("SOFT-HYPHEN")
        if CYRCYR.search(ln):
            tag.append("MIX-LATIN-IN-CYR")
        if tag:
            issues.append((i, "+".join(tag), ln.strip()[:95]))
    status = "OK " if not issues else "BAD"
    print("=== %s %s (%d lines)" % (status, f.name, len(t.splitlines())))
    for i, k, frag in issues[:60]:
        print("  L%d %s: %s" % (i, k, frag))
        exit_code = 1
    if len(issues) > 60:
        print("  ... +%d more" % (len(issues) - 60))

sys.exit(exit_code)
