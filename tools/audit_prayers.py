# -*- coding: utf-8 -*-
"""Аудит текстов молитв: ищет дефекты в данных."""
import json
import re
import sys
from collections import Counter

sys.stdout.reconfigure(encoding="utf-8")

ACC = "\u0301"
LATIN = re.compile(r"[A-Za-z]")
CYR = re.compile(r"[А-Яа-яЁё]")
MARKERS = re.compile(r"(Перевод:|Ирм|Prипев|Ирмос|Припев|\[\d+\]|\{|\}|<[a-z/]|https?://|\*\*\*)")

data = json.load(open("data/prayers.json", encoding="utf-8"))

problems = Counter()
samples = {}


def add(kind, where, text):
    problems[kind] += 1
    samples.setdefault(kind, []).append((where, text[:110]))


for sec in data["sections"]:
    for prayer in sec["prayers"]:
        where = f"{sec['id']}/{prayer['title'][:40]}"
        paras = prayer["paras"]
        if not paras:
            add("пустая молитва", where, "")
        for p in paras:
            t = p["t"]
            lines = [l for l in t.split("\n") if l.strip()]
            for line in lines:
                line = line.strip()
                if not line:
                    continue
                # латиница внутри кириллического слова
                for word in re.findall(r"\S+", line):
                    if LATIN.search(word) and CYR.search(word):
                        add("латиница в кириллице", where, line)
                        break
                if MARKERS.search(line):
                    add("служебные маркеры", where, line)
                if p["l"] == "cs" and ACC not in line and len(line) > 30:
                    add("ЦС без ударений", where, line)
            if t.count("(") != t.count(")"):
                add("непарные скобки", where, t[:110])
                if len(line) > 420:
                    add("слишком длинная строка", where, line)
                if len(line) > 25 and line[-1] not in ".:!?…»,;)]" and not line.endswith(")"):
                    add("обрыв на полуслове", where, line)
        # соответствие переводов
        cs = [p for p in paras if p["l"] in ("cs", "both")]
        ru = [p for p in paras if p["l"] in ("ru", "both")]
        if cs and not ru:
            add("нет перевода", where, prayer["title"])
        if ru and not cs:
            add("нет оригинала", where, prayer["title"])
        # дубли абзацев
        seen = set()
        for p in paras:
            key = p["t"].strip().lower()
            if key in seen and len(key) > 60:
                add("дубль абзаца", where, p["t"])
            seen.add(key)
    titles = [p["title"] for p in sec["prayers"]]
    for title, count in Counter(titles).items():
        if count > 1 and title:
            add("дубль заголовка", f"{sec['id']}", title)
    for p in sec["prayers"]:
        if not p["title"].strip():
            add("пустой заголовок", f"{sec['id']}", "")

print("=== СВОДКА ===")
if not problems:
    print("дефектов не найдено")
for kind, n in problems.most_common():
    print(f"{n:5d}  {kind}")
print()
for kind in problems:
    print(f"--- {kind} (примеры) ---")
    for where, text in samples[kind][:5]:
        print(f"  [{where}] {text}")
    print()
