# -*- coding: utf-8 -*-
"""Генерирует src/prayers_data.cpp из data/prayers.json."""

import json
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(BASE, "data")
OUT = os.path.join(BASE, "src", "prayers_data.cpp")

LANG = {"cs": 0, "ru": 1, "both": 2}
STYLE = {"text": 0, "rubric": 1, "verse": 2}


def esc(text):
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif ord(ch) < 32:
            out.append("\\%03o" % ord(ch))
        else:
            out.append(ch)
    return "".join(out)


def chunked_literal(text, indent="    "):
    """Строка в виде последовательности смежных литералов (лимит MSVC — 65535 байт)."""
    pieces = []
    current = []
    size = 0
    for ch in text:
        enc = ch.encode("utf-8")
        if size + len(enc) > 3000 and current:
            pieces.append("".join(current))
            current = []
            size = 0
        current.append(ch)
        size += len(enc)
    if current:
        pieces.append("".join(current))
    if not pieces:
        pieces = [""]
    lines = []
    for piece in pieces:
        lines.append('%s"%s"' % (indent, esc(piece)))
    return "\n".join(lines)


def main():
    with open(os.path.join(DATA, "prayers.json"), encoding="utf-8") as fh:
        data = json.load(fh)

    paras = []
    prayers = []
    sections = []

    for sec in data["sections"]:
        first_p = len(prayers)
        for prayer in sec["prayers"]:
            first = len(paras)
            for para in prayer["paras"]:
                paras.append((para["t"], LANG.get(para["l"], 1), STYLE.get(para["k"], 0)))
            prayers.append((prayer["group"], prayer["title"], first, len(paras) - first))
        sections.append((sec["id"], sec["title"], sec.get("shortTitle", sec["title"]),
                         sec.get("group", ""), sec["desc"], first_p, len(prayers) - first_p))

    lines = []
    lines.append("// Этот файл создан автоматически: tools/gen_cpp.py <- data/prayers.json")
    lines.append("// Не редактируйте вручную.")
    lines.append("")
    lines.append('#include "prayers.h"')
    lines.append("")
    lines.append("namespace {")
    lines.append("")
    lines.append("struct RawPara { const char* text; unsigned char lang; unsigned char style; };")
    lines.append("struct RawPrayer { const char* group; const char* title; int first; int count; };")
    lines.append("struct RawSection { const char* id; const char* title; const char* shortTitle; "
                 "const char* group; const char* desc; int first; int count; };")
    lines.append("")
    lines.append("const RawPara kParas[] = {")
    for text, lang, rubric in paras:
        lines.append("    {%s, %d, %d}," % (chunked_literal(text).replace("\n", "\n    "), lang, rubric))
    lines.append("};")
    lines.append("")
    lines.append("const RawPrayer kPrayers[] = {")
    for group, title, first, count in prayers:
        lines.append("    {%s, %s, %d, %d}," % (chunked_literal(group).replace("\n", "\n    "),
                                                 chunked_literal(title).replace("\n", "\n    "),
                                                 first, count))
    lines.append("};")
    lines.append("")
    lines.append("const RawSection kSections[] = {")
    for sid, title, short_title, group, desc, first, count in sections:
        lines.append("    {%s, %s, %s, %s, %s, %d, %d}," % (
            chunked_literal(sid).replace("\n", "\n    "),
            chunked_literal(title).replace("\n", "\n    "),
            chunked_literal(short_title).replace("\n", "\n    "),
            chunked_literal(group).replace("\n", "\n    "),
            chunked_literal(desc).replace("\n", "\n    "),
            first, count))
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace")
    lines.append("")
    lines.append("""const std::vector<Section>& LoadSections() {
    static std::vector<Section> sections;
    static bool ready = false;
    if (ready) {
        return sections;
    }
    ready = true;

    sections.reserve(sizeof(kSections) / sizeof(kSections[0]));
    for (const RawSection& rs : kSections) {
        Section sec;
            sec.id = Utf8ToWide(rs.id);
            sec.title = Utf8ToWide(rs.title);
            sec.shortTitle = Utf8ToWide(rs.shortTitle);
            sec.group = Utf8ToWide(rs.group);
            sec.desc = Utf8ToWide(rs.desc);
        sec.prayers.reserve(rs.count);
        for (int i = 0; i < rs.count; ++i) {
            const RawPrayer& rp = kPrayers[rs.first + i];
            Prayer prayer;
            prayer.group = Utf8ToWide(rp.group);
            prayer.title = Utf8ToWide(rp.title);
            prayer.paras.reserve(rp.count);
            for (int k = 0; k < rp.count; ++k) {
                const RawPara& rp2 = kParas[rp.first + k];
                PrayerPara para;
                para.text = Utf8ToWide(rp2.text);
                para.lang = static_cast<ParaLang>(rp2.lang);
                para.style = rp2.style;
                prayer.paras.push_back(std::move(para));
            }
            sec.prayers.push_back(std::move(prayer));
        }
        sections.push_back(std::move(sec));
    }
    return sections;
}""")
    lines.append("")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))

    size = os.path.getsize(OUT)
    print("sections: %d, prayers: %d, paras: %d" % (len(sections), len(prayers), len(paras)))
    print("written:", OUT, "%.1f KB" % (size / 1024.0))


if __name__ == "__main__":
    main()
