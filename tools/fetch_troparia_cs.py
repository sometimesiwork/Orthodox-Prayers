# -*- coding: utf-8 -*-
"""Загружает церковнославянские тропари, кондаки и величания с ударениями.

Источники:
  * https://azbyka.ru/days/<дата>  — блок «Тропари, кондаки, молитвы и величания»
  * https://azbyka.ru/velichanie     — список величаний

Результат: data/troparia_cs.json = {праздник: {"Тропарь": {"cs": ..., "ru": ...}, ...}}
"""

import datetime
import difflib
import html as _html
import io
import json
import os
import re
import sys
import unicodedata
import urllib.request

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(BASE, "data")
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36"

MONTHS = {"января": 1, "февраля": 2, "марта": 3, "апреля": 4, "мая": 5, "июня": 6,
          "июля": 7, "августа": 8, "сентября": 9, "октября": 10, "ноября": 11, "декабря": 12}

# год, к которому привязываются церковнославянские даты (старый стиль)
LIT_YEAR = 2026
JULIAN_OFFSET = 13   # старый стиль -> новый стиль (1900-2099)

# праздники, которых нет в старинном молитвеннике, но которые есть на azbyka
EXTRA_FEASTS = [("Покров Пресвятой Богородицы", 1, 10)]

# соответствие названий на странице /velichanie названиям из молитвенника
VEL_ALIAS = {
    "рождество пресвятой богородицы": "Рождество Пресвятой Богородицы",
    "воздвижение честного и животворящего креста": "Воздвижение Животворящего Креста",
    "введение во храм пресвятой богородицы": "Введение во храм Пресвятой Богородицы",
    "рождество господа нашего иисуса христа": "Рождество Христово",
    "по плоти обрезание господа нашего иисуса христа": "Обрезание Господне",
    "крещение господа нашего иисуса христа": "Богоявление Господне",
    "сретение господа нашего иисуса христа": "Сретение Господне",
    "благовещение пресвятой богородицы": "Благовещение Пресвятой Богородицы",
    "вход господень в иерусалим": "Вербное Воскресение",
    "вознесение господа нашего иисуса христа": "Вознесение Господне",
    "рождество св иоанна предтечи": "Рождество Иоанна Предтечи",
    "святых первоверховных апостолов петра и павла": "день св. Апостолов Петра и Павла",
    "пятидесятница день святой троицы": "Троицын день или Сошествия Святаго Духа",
    "преображение господа нашего иисуса христа": "Преображение Господне",
    "успение пресвятой богородицы": "Успение Пресвятой Богородицы",
    "усекновение главы иоанна предтечи": "день Усекновения главы Предтечи",
    "покров пресвятой богородицы": "Покров Пресвятой Богородицы",
}


# ---------------------------------------------------------------- утилиты

def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=45) as resp:
        return resp.read().decode("utf-8", "replace")


def strip_tags(html):
    return _html.unescape(re.sub(r"(?is)<[^>]+>", "", html))


def clean(text):
    text = text.replace("\u00a0", " ").replace("\u200b", "")
    text = re.sub(r"\[\d+\]", "", text)
    text = re.sub(r"[ \t]+", " ", text)
    return text.strip()


def strip_accents(text):
    """Убирает ударения (только острое/grave), сохраняя breve у «й»."""
    text = unicodedata.normalize("NFD", text)
    text = "".join(c for c in text if c not in ("\u0301", "\u0300", "\u0304"))
    return unicodedata.normalize("NFC", text)


def norm(text):
    """Ключ для сопоставления: без знаков препинания и регистра."""
    text = strip_accents(text).lower().replace("ё", "е")
    text = re.sub(r"[^\w\s]", " ", text)
    return re.sub(r"\s+", " ", text).strip()


def feast_key(name):
    """Ключ для сопоставления названий праздников."""
    text = strip_accents(name).lower()
    text = re.sub(r"\([^)]*\)", " ", text)
    text = re.sub(r"\d+", " ", text)
    text = re.sub(r"[^\w\s]", " ", text)
    return re.sub(r"\s+", " ", text).strip()


def cs_punctuation(text):
    """Убирает церковнославянские знаки азбуки («/», «//»).

    В исходнике azbyka современная пунктуация стоит рядом с церковнославянской
    («...Господне, Предтече:/ показал...»), поэтому знаки просто удаляются —
    остаётся читаемый текст, как в старинном молитвеннике.
    """
    text = re.sub(r"\s*/+\s*", " ", text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"\s+([.,;:])", r"\1", text)
    return text.strip()


def julian_pascha(year):
    """Пасха по юлианскому календарю (алгоритм Meeus)."""
    a, b, c = year % 4, year % 7, year % 19
    d = (19 * c + 15) % 30
    e = (2 * a + 4 * b - d + 34) % 7
    month = (d + e + 114) // 31
    day = ((d + e + 114) % 31) + 1
    return datetime.date(year, month, day)


def to_gregorian(dt):
    return (dt + datetime.timedelta(days=JULIAN_OFFSET)).strftime("%Y-%m-%d")


def resolve_date(hint, pasxa):
    """'8 сентября' / 'за неделю до Пасхи' -> '2026-09-21'."""
    hint = (hint or "").strip().lower()
    m = re.match(r"(\d+)\s+([а-яё]+)", hint)
    if m and MONTHS.get(m.group(2)):
        return to_gregorian(datetime.date(LIT_YEAR, MONTHS[m.group(2)], int(m.group(1))))
    if "неделю до пасхи" in hint or "вербное" in hint:
        return to_gregorian(pasxa - datetime.timedelta(days=7))
    if "40" in hint:
        return to_gregorian(pasxa + datetime.timedelta(days=39))
    if "50" in hint:
        return to_gregorian(pasxa + datetime.timedelta(days=49))
    return None


def resolve_julian(hint, pasxa):
    """'8 сентября' -> '09-08' (старый стиль — по нему идёт порядок праздников)."""
    hint = (hint or "").strip().lower()
    m = re.match(r"(\d+)\s+([а-яё]+)", hint)
    if m and MONTHS.get(m.group(2)):
        return "%02d-%02d" % (MONTHS[m.group(2)], int(m.group(1)))
    if "неделю до пасхи" in hint or "вербное" in hint:
        d = pasxa - datetime.timedelta(days=7)
    elif "40" in hint:
        d = pasxa + datetime.timedelta(days=39)
    elif "50" in hint:
        d = pasxa + datetime.timedelta(days=49)
    else:
        return "00-00"
    return "%02d-%02d" % (d.month, d.day)


# ---------------------------------------------------------------- разбор

def parse_feasts(blocks):
    """blocks.json -> [{name, date, items:[{kind, cs, ru}]}]"""
    feasts = []
    cur = None
    kind = None
    for b in blocks:
        t = b.get("type")
        if t == "h3":
            cur = {"name": b["text"].strip(), "date": b.get("date", ""), "items": []}
            feasts.append(cur)
            kind = None
        elif t == "h4" and cur is not None:
            kind = b["text"].strip()
            # у некоторых праздников несколько величаний — берём только первое
            if kind == "Величание" and any(i["kind"] == "Величание" for i in cur["items"]):
                kind = None
        elif t == "p" and cur is not None and kind:
            text = b.get("text", "")
            lang = b.get("lang")
            if lang == "ru" and cur["items"] and cur["items"][-1].get("cs"):
                cur["items"][-1]["ru"] = text
            elif lang in (None, "cs"):
                if cur["items"] and cur["items"][-1]["kind"] == kind and not cur["items"][-1].get("cs"):
                    cur["items"][-1]["cs"] = text
                else:
                    cur["items"].append({"kind": kind, "cs": text, "ru": ""})
    return feasts


def extract_daily_tropari(doc):
    """Страница дня -> [{feast, items:[{kind, cs, ru}]}]"""
    i = doc.find('id="tropari"')
    if i < 0:
        return []
    start = doc.rfind("<div", 0, i)
    nxt = re.search(r'(?is)<div[^>]*\bid="', doc[i + 200:])
    end = i + 200 + nxt.start() if nxt else len(doc)
    seg = doc[start:end]
    out = []
    for chunk in re.split(r'(?is)<div class="tropary-item"', seg)[1:]:
        tm = re.search(r'(?is)<h2 class="block_title">(.*?)</h2>', chunk)
        title = clean(strip_tags(tm.group(1))) if tm else ""
        items = []
        for fb in re.split(r'(?is)<div class="frame"', chunk)[1:]:
            km = re.search(r'(?is)<span style="color:[^"]*">([^<]{2,20})</span>', fb)
            if not km:
                continue
            kind = clean(km.group(1))
            body = re.sub(r"(?is)<p[^>]*>\s*<p[^>]*>", "<p>", fb)
            body = re.sub(r"(?is)</p>\s*</p>", "</p>", body)
            cm = re.search(r"(?is)<p>(.*?)</p>", body)
            cs = clean(strip_tags(cm.group(1))) if cm else ""
            ru = ""
            rm = re.search(r'(?is)<div class="taks-explanation-info"[^>]*>(.*?)</div>', body)
            if rm:
                ru = clean(strip_tags(rm.group(1)))
            if cs:
                items.append({"kind": kind, "cs": cs, "ru": ru})
        if items:
            out.append({"feast": title, "items": items})
    return out


def extract_velichanie(doc):
    """Страница /velichanie -> [{feast, cs, ru}]"""
    m = re.search(r'(?is)<p[^>]*>\s*\*\*\*\s*</p>', doc)
    seg = doc[m.end():] if m else doc
    out = []
    for mm in re.finditer(r"(?is)<p([^>]*)>(.*?)</p>", seg):
        attrs, inner = mm.group(1), mm.group(2)
        if "inline-player" in inner:
            continue
        txt = clean(strip_tags(inner))
        if not txt:
            continue
        if "<strong" in inner:
            # «Рождество Пресвятой Богородицы. 8 (21) сентября» -> название без даты
            title = re.split(r"[\d(]", txt)[0].strip().rstrip(".").strip()
            out.append({"feast": title, "cs": "", "ru": ""})
            continue
        if out and not out[-1]["cs"]:
            out[-1]["cs"] = txt
        elif out and out[-1]["cs"] and not out[-1]["ru"] and "translate" in attrs:
            out[-1]["ru"] = txt
    return out


def best_match(items, text):
    """Найти в items пару с тем же текстом (без учёта ударений)."""
    key = norm(text)
    if not key:
        return None
    for it in items:
        if norm(it["cs"]) == key:
            return it
    best, score = None, 0.0
    for it in items:
        ratio = word_similarity(key, norm(it["cs"]))
        if ratio > score:
            best, score = it, ratio
    if best and score >= 0.72:
        return best
    for it in items:
        other = norm(it["cs"])
        if key and (key in other or other in key):
            return it
    return None


def word_similarity(a, b):
    """Доля общих слов (мультимножество) — устойчиво к опечаткам и вариантам."""
    wa, wb = a.split(), b.split()
    if not wa or not wb:
        return 0.0
    from collections import Counter
    ca, cb = Counter(wa), Counter(wb)
    common = sum((ca & cb).values())
    return 2.0 * common / (len(wa) + len(wb))


def best_feast(name, names):
    """Найти в names наиболее похожее название праздника."""
    key = feast_key(name)
    best, score = None, 0.0
    for cand in names:
        ratio = word_similarity(key, feast_key(cand))
        if ratio > score:
            best, score = cand, ratio
    if best and score >= 0.5:
        return best
    return None


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "blocks.json"), encoding="utf-8") as fh:
        raw = json.load(fh)
    feasts = parse_feasts(raw["troparia"]["blocks"])
    for name, day, month in EXTRA_FEASTS:
        feasts.append({"name": name, "date": "%d %s" % (day, [k for k, v in MONTHS.items() if v == month][0]),
                       "items": [{"kind": "Тропарь", "cs": "", "ru": ""},
                                 {"kind": "Кондак", "cs": "", "ru": ""},
                                 {"kind": "Величание", "cs": "", "ru": ""}]})

    pasxa = julian_pascha(LIT_YEAR)
    print("Пасха %d года: %s" % (LIT_YEAR, to_gregorian(pasxa)))

    # ---- величания: одна страница на все праздники
    try:
        vel = extract_velichanie(fetch("https://azbyka.ru/velichanie"))
    except Exception as exc:  # noqa: BLE001
        print("FAIL velichanie:", exc)
        vel = []
    vel_names = [v["feast"] for v in vel]
    # соответствие «название на azbyka» -> «название в молитвеннике»
    vel_by_feast = {}
    for x in vel:
        key = feast_key(x["feast"])
        if key in VEL_ALIAS:
            vel_by_feast[VEL_ALIAS[key]] = x

    result = {}
    for f in feasts:
        date = resolve_date(f["date"], pasxa)
        if not date:
            print("SKIP  %-45s (не разобрана дата %r)" % (f["name"], f["date"]))
            continue
        url = "https://azbyka.ru/days/" + date
        try:
            doc = fetch(url)
        except Exception as exc:  # noqa: BLE001
            print("FAIL  %-45s %s (%s)" % (f["name"], url, exc))
            continue
        daily = extract_daily_tropari(doc)
        entry = {}

        # сначала находим блок нужного праздника на странице дня
        dnames = [d["feast"] for d in daily]
        dname = best_feast(f["name"], dnames) if dnames else None
        named = []
        for d in daily:
            if d["feast"] == dname:
                named = d["items"]
                break
        allframes = [it for d in daily for it in d["items"]]

        for kind in ("Тропарь", "Кондак"):
            src_text = next((i["cs"] for i in f["items"] if i["kind"] == kind), "")
            hit = best_match(allframes, src_text) if src_text else None
            if hit is None:
                hit = best_match(named, src_text) if src_text else None
            if hit is None:
                hit = next((x for x in named if x["kind"] == kind), None)
            if hit is None:
                hit = next((x for x in allframes if x["kind"] == kind), None)
            if hit:
                entry[kind] = {"cs": cs_punctuation(hit["cs"])}
                if hit.get("ru"):
                    entry[kind]["ru"] = hit["ru"]
                print("OK    %-45s %-10s %s" % (f["name"], kind, date))
            else:
                print("MISS  %-45s %-10s %s" % (f["name"], kind, date))

        # величание — со страницы /velichanie (там всегда есть ударения)
        v = vel_by_feast.get(f["name"])
        if v and v["cs"]:
            entry["Величание"] = {"cs": cs_punctuation(v["cs"])}
            ru = v.get("ru", "")
            if not ru:
                hit = next((x for x in allframes if x["kind"] == "Величание"), None)
                ru = hit.get("ru", "") if hit else ""
            if ru:
                entry["Величание"]["ru"] = ru
            print("OK    %-45s Величание  /velichanie" % f["name"])
        else:
            hit = next((x for x in named if x["kind"] == "Величание"), None)
            if hit:
                entry["Величание"] = {"cs": cs_punctuation(hit["cs"])}
                if hit.get("ru"):
                    entry["Величание"]["ru"] = hit["ru"]
                print("OK    %-45s Величание  %s" % (f["name"], date))
            else:
                print("MISS  %-45s Величание" % f["name"])

        result[f["name"]] = {"date": date, "jdate": resolve_julian(f["date"], pasxa),
                             "items": entry}

    path = os.path.join(OUT_DIR, "troparia_cs.json")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(result, fh, ensure_ascii=False, indent=1)
    print("saved:", path, len(result), "праздников")


if __name__ == "__main__":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    main()
