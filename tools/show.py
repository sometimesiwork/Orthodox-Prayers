# -*- coding: utf-8 -*-
import json
import sys

sys.stdout.reconfigure(encoding="utf-8")
data = json.load(open("data/prayers.json", encoding="utf-8"))
want = sys.argv[1:] if len(sys.argv) > 1 else None
for s in data["sections"]:
    if want and s["id"] not in want:
        continue
    print("=== %s (%s)" % (s["title"], s["id"]))
    for i, p in enumerate(s["prayers"]):
        langs = {}
        for para in p["paras"]:
            langs[para["l"]] = langs.get(para["l"], 0) + 1
        print("  %2d. [%s] %-52s %s" % (i, p["group"][:18], p["title"][:52], langs))
