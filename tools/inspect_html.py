# -*- coding: utf-8 -*-
import pathlib

h = pathlib.Path("data/azbyka_sample.html").read_text(encoding="utf-8")
print(h[46500:51000])
