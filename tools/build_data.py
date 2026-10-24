# -*- coding: utf-8 -*-
"""Преобразует data/blocks.json в data/prayers.json — структурированный набор молитв."""

import json
import os
import re

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(BASE, "data")

ACC = "\u0301"
RU_WORD = re.compile(r"[а-яёА-ЯЁ]{8,}")



# Буквы, визуально неотличимые от кириллицы (частая беда при копировании с сайта).
LOOKALIKE = str.maketrans({
    "A": "А", "B": "В", "C": "С", "D": "Д", "E": "Е", "F": "Ф", "G": "Г",
    "H": "Н", "I": "И", "J": "Ј", "K": "К", "L": "Л", "M": "М", "N": "Н",
    "O": "О", "P": "Р", "Q": "Д", "R": "Р", "S": "С", "T": "Т", "U": "У",
    "V": "В", "W": "Ш", "X": "Х", "Y": "У", "Z": "З",
    "a": "а", "b": "в", "c": "с", "d": "д", "e": "е", "f": "ф", "g": "г",
    "h": "н", "i": "и", "k": "к", "l": "л", "m": "м", "n": "п", "o": "о",
    "p": "р", "q": "д", "r": "р", "s": "с", "t": "т", "u": "и", "v": "в",
    "w": "ш", "x": "х", "y": "у", "z": "з",
})

# Служебные пометки сайта, которые не являются молитвами.
NOTE_PATTERNS = (
    "Видеонот", "См. ", "Аудио:", "Скачать", "Краткий Молитвослов",
    "Читаем Псалтирь", "Текст молитвы", "Слушать", "Источник",
)

# Границы кафисм Псалтири.
KATHISMA = [
    (1, 8), (9, 16), (17, 23), (24, 31), (32, 36), (37, 45), (46, 54), (55, 63),
    (64, 69), (70, 76), (77, 84), (85, 90), (91, 100), (101, 104), (105, 108),
    (109, 117), (118, 118), (119, 133), (134, 142), (143, 151),
]


def cyrillize(text):
    """Приводит ударения к знаковому виду и меняет латинские омоглифы на кириллицу."""
    import unicodedata

    # предсоставленные буквы (é, ó, Ё) -> буква + знак ударения
    text = text.replace("\u0451", "\uE000")
    text = unicodedata.normalize("NFD", text)
    text = text.replace("\uE000", "\u0451")

    def repl(m):
        word = m.group(0)
        if not re.search(r"[\u0410-\u044f\u0401]", word):
            return word
        return word.translate(LOOKALIKE)

    return re.sub(r"[\w\u0300-\u036f]+", repl, text, flags=re.UNICODE)


def join_split_words(text):
    """Склеивает слова, разорванные вёрсткой: «Ж ите́йское» -> «Жите́йское»."""
    def repl(m):
        letter, word = m.group(1), m.group(2)
        if ACC in letter or ACC not in word:
            return m.group(0)
        return letter + word
    return re.sub(r"(?<![А-Яа-яЁё])([А-ЯЁ]) ([а-яё́]+)", repl, text)


def strip_footnote_numbers(text):
    """Убирает номера сносок, прилипшие к слову («псалти́рь1»)."""
    return re.sub(r"([А-Яа-яЁё])(\d{1,2})(?=[\s.,;:!?»)]|$)", r"\1", text)


def is_service_note(text):
    stripped = text.strip()
    if not stripped:
        return True
    if stripped.startswith("^"):
        return True
    for pattern in NOTE_PATTERNS:
        if stripped.startswith(pattern):
            return True
    if re.fullmatch(r"\d{1,3}", stripped):
        return True
    return False


# Император → Государ (актуализация обращения к правителю в тропарях).
def _letters(word):
    """Слово с необязательными знаками ударения после каждой буквы."""
    return "".join(ch + r"\u0301?" for ch in word)


_IMP_TS = _letters("Императриц")
_IMP_T = _letters("Император")


def modernize_imperial(text):
    def repl_ts(m):
        return "Государын" + m.group(2)

    def repl_t(m):
        return "Государ" + m.group(2)

    text = re.sub("(" + _IMP_TS + ")([а-яё]*)", repl_ts, text)
    text = re.sub("(" + _IMP_T + ")([а-яё]*)", repl_t, text)
    return text


def split_verses(text, limit=420):
    """Делит стих на части по номерам стихов, иначе — по предложениям."""
    parts = re.split(r"(?<=[.,:;!?)»«])\s+(?=\d{1,3}\s+\w)", text)
    if len(parts) > 1:
        # стихи идут жёсткими переносами внутри одного абзаца
        return ["\n".join(p.strip() for p in parts if p.strip())]
    if len(text) <= limit:
        return [text]
    parts = re.split(r"(?<=[.!?…])\s+", text)
    out, cur = [], ""
    for part in parts:
        if cur and len(cur) + len(part) + 1 > limit:
            out.append(cur)
            cur = part
        else:
            cur = (cur + " " + part) if cur else part
    if cur:
        out.append(cur)
    return out

def strip_accents(t):
    return t.replace(ACC, "")


def classify(text):
    """Определяет язык абзаца: cs / ru / both."""
    accents = text.count(ACC)
    plain = strip_accents(text)
    if accents == 0:
        # короткие пояснения в церковнославянском стиле («Или более краткий…:»)
        if text.rstrip().endswith(":") and len(text) < 80:
            return "cs"
        return "ru"
    if accents == 1 and len(text) >= 40:
        # единичная опечатка в русском переводе
        return "ru"
    if RU_WORD.search(plain) and re.search(r"\([^)]*[а-яёА-ЯЁ]{10,}[^)]*\)", plain):
        return "both"
    return "cs"


def make_paras(raw_text, force_lang=None):
    """Разбирает абзац источника на список абзацев (текст, язык, тип)."""
    out = []
    text = raw_text.strip()
    if not text:
        return out

    text = cyrillize(text)
    text = join_split_words(text)
    text = strip_footnote_numbers(text)
    # в источнике перенос строки иногда помечен косой чертой
    text = re.sub(r"\s*/+\s*", "\n", text)
    if is_service_note(text):
        return []

    if re.match(r"^Перевод\s*\d*\s*[:.]", text):
        match = re.match(r"^Перевод\s*(\d*)\s*[:.]", text)
        number = match.group(1) if match else ""
        parts = re.split(r"^Перевод\s*\d*\s*[:.]", text, maxsplit=1)
        head = parts[0].strip()
        tail = parts[1].strip()
        if head:
            out.append({"t": head, "l": classify(head), "k": "text"})
        lines = [l.strip() for l in tail.split("\n") if l.strip()]
        if lines:
            # первый вариант — основной перевод, остальные — примечания
            kind = "text" if number in ("", "1") else "rubric"
            out.append({"t": "\n".join(lines), "l": "ru", "k": kind})
        return out

    kind = "text"
    if len(text) < 70 and (text.startswith("(") or text.startswith("Слава")
                           or text.startswith("И ныне") or text.startswith("Тропари сия")
                           or text.startswith("Глас ")):
        kind = "rubric"

    if classify(text) == "ru":
        # убираем редкие осечки-ударения, оставшиеся в русском переводе
        text = text.replace(ACC, "")

    groups = []  # (язык, список строк, тип)
    for line in text.split("\n"):
        line = line.strip()
        if not line or is_service_note(line):
            continue
        line = re.sub(r"\s*\*+\s*$", "", line)   # знаки сносок в конце строки
        line = re.sub(r"([а-яё])\s+:", r"\1:", line)  # «Ирмос :» -> «Ирмос:»
        for piece in split_verses(line):
            piece = piece.strip()
            if not piece:
                continue
            lang = force_lang or classify(piece)
            if groups and groups[-1][0] == lang:
                groups[-1][1].append(piece)
            else:
                groups.append((lang, [piece], kind))

    for lang, pieces, k in groups:
        text = "\n".join(pieces)
        if text.count("(") > text.count(")"):
            text += ")" * (text.count("(") - text.count(")"))
        para = {"t": text, "l": lang, "k": k}
        if force_lang:
            para["lock"] = True
        out.append(para)
    return out


def is_heading(block):
    return block["type"] in ("h2", "h3", "h4")


def clean_title(t):
    t = t.strip().lstrip("^ ").strip()
    t = re.sub(r"\s+", " ", t)
    t = re.sub(r"(?<=[А-Яа-яЁёA-Za-z])\d+$", "", t)   # сноски вида «Богородице1»
    t = t.rstrip("*").strip()
    return t


def parse_blocks(blocks):
    """Разбирает блоки в список заголовков с абзацами."""
    items = []
    cur = None
    for block in blocks:
        if is_heading(block):
            cur = {"level": int(block["type"][1]), "title": clean_title(block["text"]), "paras": []}
            items.append(cur)
        elif block["type"] in ("p", "li"):
            if cur is None:
                continue
            if block["type"] == "li":
                for para in make_paras(block["text"]):
                    para["k"] = "rubric"
                    cur["paras"].append(para)
            else:
                cur["paras"].extend(make_paras(block["text"], block.get("lang")))
    return items


def dedupe(items):
    """Убирает повторное копирование блоков (страница дублирует содержимое)."""
    if len(items) < 4:
        return items
    first = items[0]["title"]
    for i in range(1, len(items)):
        if items[i]["title"] == first:
            return items[:i]
    return items


def group_and_prayers(items):
    """Пустые заголовки становятся названиями групп, непустые — молитвами."""
    prayers = []
    group = ""
    pending_group = ""
    for item in items:
        if not item["paras"]:
            pending_group = item["title"]
            continue
        if pending_group:
            group = pending_group
            pending_group = ""
        title = item["title"]
        own_group = group
        if title.startswith("(") and group:
            title = group + " " + title
            own_group = ""
        prayers.append({"group": own_group, "title": title, "paras": item["paras"]})
    return prayers


def split_intro(blocks):
    """Абзацы до первого заголовка (без служебной навигации в начале)."""
    out = []
    for block in blocks:
        if is_heading(block):
            break
        if block["type"] == "p":
            out.extend(make_paras(block["text"]))
    for i, para in enumerate(out):
        if para["l"] == "cs":
            return out[i:]
    return out


EVENING_INTRO = [
    ("во имя отца", "Во имя Отца, и Сына, и Святаго Духа"),
    ("молитвами святых отец", "Молитвами святых отец наших"),
    ("царю небесный", "Молитва к Святому Духу"),
    ("святый боже", "Трисвятое"),
    ("пресвятая троице", "Молитва ко Пресвятой Троице"),
    ("отче наш", "Отче наш"),
]


def pair_prayers(intro):
    """Группирует вводные абзацы по известным молитвам.

    Вечерняя страница не имеет заголовков, поэтому начало каждой молитвы
    определяется по её первому слову, а служебные строки («Слава…») относятся
    к предыдущей молитве.
    """
    groups = []
    current = None
    for para in intro:
        head = strip_accents(para["t"]).strip().lower()
        title = None
        if para["l"] == "cs":
            for prefix, name in EVENING_INTRO:
                if head.startswith(prefix):
                    title = name
                    break
        if title is not None:
            current = {"group": "", "title": title, "paras": [para]}
            groups.append(current)
        elif current is not None:
            current["paras"].append(para)
    return groups


TROPARIA_KINDS = {"Тропарь": 2, "Кондак": 2, "Величание": 1}


def parse_troparia_blocks(blocks):
    """Тропари/кондаки/величания: h3 — праздник, h4 — вид песнопения.

    Молитвенник заканчивается длинным приложением (молитвы святым, сноски,
    навигация сайта) — оно отбрасывается: у каждого праздника берём только
    тропарь, кондак и величание.
    """
    feasts = []
    cur = None
    kind = None
    for b in blocks:
        t = b.get("type")
        if t == "h3":
            cur = {"group": clean_title(b["text"]), "groups": []}
            feasts.append(cur)
            kind = None
            continue
        if cur is None:
            continue
        if t == "h4":
            if len(cur["groups"]) >= 3:
                kind = None
                continue
            kind = clean_title(b["text"])
            cur["groups"].append({"kind": kind, "paras": []})
            continue
        if t == "p" and kind:
            grp = cur["groups"][-1]
            if len(grp["paras"]) >= TROPARIA_KINDS.get(kind, 2):
                continue
            text = (b.get("text") or "").strip()
            if text:
                grp["paras"].append({"t": text, "lang": b.get("lang")})
    return feasts


def strip_imperial(text):
    """«Императору» -> «Государю», убрать имя государя."""
    text = modernize_imperial(text)
    text = re.sub(r"\s*Александр[а-я]* Николаевич[а-я]*", "", text)
    return text.strip()


def liturgical_key(date_str):
    """Порядок праздников по церковному году (начинается с сентября)."""
    m = re.match(r"(\d{2})-(\d{2})", date_str or "")
    if not m:
        return (99, 0)
    month, day = int(m.group(1)), int(m.group(2))
    return ((month - 8) % 12, day)


def build_troparia(raw):
    """Раздел «Тропари праздников».

    Церковнославянские тексты берутся из data/troparia_cs.json (azbyka, с
    ударениями), русские переводы — из молитвенника; если перевода нет,
    берётся он же с azbyka.
    """
    cs_path = os.path.join(DATA, "troparia_cs.json")
    cs_data = {}
    if os.path.exists(cs_path):
        with open(cs_path, encoding="utf-8") as fh:
            cs_data = json.load(fh)

    feasts = parse_troparia_blocks(raw["troparia"]["blocks"])
    # праздники, которых нет в молитвеннике (Покров) — добавляем из azbyka
    for name, entry in cs_data.items():
        if name in ("Покров Пресвятой Богородицы",) and not any(f["group"] == name for f in feasts):
            feasts.append({"group": name, "groups": [], "date": entry.get("date", "")})

    prayers = []
    for f in feasts:
        entry = cs_data.get(f["group"], {})
        items = entry.get("items", {})
        for grp in f["groups"]:
            kind = grp["kind"]
            src = items.get(kind, {})
            cs_text = src.get("cs", "")
            ru_text = ""
            for p in grp["paras"]:
                if p["lang"] == "cs":
                    if not cs_text:
                        cs_text = p["t"]
                elif p["lang"] == "ru":
                    if not ru_text:
                        ru_text = p["t"]
                elif not ru_text:
                    ru_text = p["t"]
            if not ru_text:
                ru_text = src.get("ru", "")
            paras = []
            if cs_text:
                paras.extend(make_paras(strip_imperial(cs_text), "cs"))
            if ru_text:
                paras.extend(make_paras(strip_imperial(ru_text), "ru"))
            if paras:
                prayers.append({"group": f["group"], "title": kind, "paras": paras})
        # величание/кондак, которых не было в молитвеннике
        for kind in ("Тропарь", "Кондак", "Величание"):
            if any(p["title"] == kind and p["group"] == f["group"] for p in prayers):
                continue
            src = items.get(kind)
            if not src or not src.get("cs"):
                continue
            paras = make_paras(strip_imperial(src["cs"]), "cs")
            if src.get("ru"):
                paras.extend(make_paras(strip_imperial(src["ru"]), "ru"))
            if paras:
                prayers.append({"group": f["group"], "title": kind, "paras": paras})

    # порядок — по церковному году (с сентября), внутри праздника:
    # тропарь, кондак, величание
    order = {f["group"]: liturgical_key(cs_data.get(f["group"], {}).get("jdate", ""))
             for f in feasts}
    kind_order = {"Тропарь": 0, "Кондак": 1, "Величание": 2}
    grouped = []
    for f in feasts:
        grouped.extend([p for p in prayers if p["group"] == f["group"]])
    grouped.sort(key=lambda p: (order.get(p["group"], (99, 0)),
                                 kind_order.get(p["title"], 9)))
    return grouped


def build():
    with open(os.path.join(DATA, "blocks.json"), encoding="utf-8") as fh:
        raw = json.load(fh)

    sections = []

    # ---------------- Утренние молитвы ----------------
    psalm50_ru = []
    for block in raw["psalm50"]["blocks"]:
        if block["type"] != "p":
            continue
        text = block["text"].strip()
        if text.startswith("Перевод:"):
            psalm50_ru.extend(make_paras(text))
    mb = raw["morning"]["blocks"]
    morning = group_and_prayers(dedupe(parse_blocks(mb)))
    intro = split_intro(mb)
    morning.insert(0, {"group": "", "title": "Во имя Отца, и Сына, и Святаго Духа",
                       "paras": [p for p in intro if not p["t"].startswith("Затем немного")]})
    # различаем две молитвы с одинаковым названием
    seen_titles = set()
    for prayer in morning:
        title = prayer["title"]
        if title == "Молитва ко Пресвятой Тро́ице" and prayer["paras"] and \
                prayer["paras"][0]["t"].startswith("От сна воста́в"):
            title = "Молитва ко Пресвятой Троице (благодарственная)"
            prayer["title"] = title
        if title in seen_titles:
            seen_titles.add(title)
        else:
            seen_titles.add(title)
    # Псалом 50 в утреннем разделе приходит без перевода — берём его со страницы псалма
    for prayer in morning:
        if prayer["title"].startswith("Псалом 50") and \
                not any(p["l"] in ("ru", "both") for p in prayer["paras"]):
            for para in psalm50_ru:
                prayer["paras"].append(para)
    sections.append({"id": "morning", "title": "Утренние молитвы",
                     "desc": "Полное утреннее молитвенное последование",
                     "prayers": morning})

    # ---------------- Вечерние молитвы ----------------
    eb = raw["evening"]["blocks"]
    evening = group_and_prayers(dedupe(parse_blocks(eb)))
    intro = split_intro(eb)
    evening[0:0] = pair_prayers(intro)
    for prayer in evening:
        if prayer["title"] == "Молитва" and prayer["paras"]:
            head = prayer["paras"][0]["t"]
            if head.startswith("Осла́би"):
                prayer["title"] = "Молитва о прощении грехов"
            elif head.startswith("Ненави́дящих"):
                prayer["title"] = "Молитва об оскорбляющих нас"
    sections.append({"id": "evening", "title": "Вечерние молитвы",
                     "desc": "Молитвы на сон грядущим",
                     "prayers": evening})

    # ---------------- Псалтирь ----------------
    items = parse_blocks(raw["psaltir"]["blocks"])
    lead = []
    for block in raw["psaltir"]["blocks"]:
        if is_heading(block):
            break
        if block["type"] == "p":
            lead.extend(make_paras(block["text"]))
    if lead:
        items.insert(0, {"level": 3, "title": "Псалом 1", "paras": lead})
    psaltir = group_and_prayers(items)
    for prayer in psaltir:
        match = re.match(r"Псалом\s+(\d+)", prayer["title"])
        if not match:
            continue
        num = int(match.group(1))
        for index, (start, end) in enumerate(KATHISMA, 1):
            if start <= num <= end:
                prayer["group"] = "Кафисма %d-я" % index
                break
    sections.append({"id": "psaltir", "title": "Псалтирь",
                     "desc": "Псалтирь по кафизмам",
                     "prayers": psaltir})

    # ---------------- Покаянный канон ----------------
    kanon = group_and_prayers(dedupe(parse_blocks(raw["kanon_pokayanny"]["blocks"])))
    for item in kanon:
        if not item["group"]:
            item["group"] = "Покаянный канон"
    sections.append({"id": "kanon", "title": "Покаянный канон",
                     "desc": "Канон ко Господу нашему Иисусу Христу",
                     "prayers": kanon})

    # ---------------- Молитвы Богородице ----------------
    items = [it for it in parse_blocks(raw["bogorodica"]["blocks"])
             if it["title"] != "Рекомендуем молитвы"]
    sections.append({"id": "bogorodica", "title": "Молитвы Богородице",
                     "desc": "Песни и молитвы Пресвятой Богородице",
                     "prayers": group_and_prayers(items)})

    # ---------------- Краткие молитвы ----------------
    kratkie = []
    chunk = []
    for block in raw["kratkie"]["blocks"]:
        if block["type"] != "p":
            continue
        text = block["text"].strip()
        if text.startswith("Рекомендуем молитвы"):
            break
        if text in ("* * *", "***"):
            if chunk:
                kratkie.append(chunk)
                chunk = []
            continue
        if not text:
            continue
        if text.startswith("Эти молитвы можно совершать"):
            continue
        chunk.append(text)
    if chunk:
        kratkie.append(chunk)

    kratkie_prayers = []
    for chunk in kratkie:
        first = chunk[0].strip()
        if len(first) < 110 and first.endswith(":"):
            title = clean_title(first.rstrip(":"))
            body = chunk[1:]
        else:
            title = ""
            body = chunk
        paras = []
        for text in body:
            paras.extend(make_paras(text))
        if not paras:
            continue
        if not title:
            # короткая молитва без заголовка — первая строка становится названием
            head = paras[0]["t"]
            title = head if len(head) <= 70 else head[:68].rstrip() + "…"
            paras.pop(0)
            if not paras:
                paras = [{"t": head, "l": "cs", "k": "text"}]
        kratkie_prayers.append({"group": "Краткие молитвы", "title": title, "paras": paras})
    sections.append({"id": "kratkie", "title": "Краткие молитвы",
                     "desc": "Короткие молитвы на каждый день",
                     "prayers": kratkie_prayers})

    # ---------------- Часослов ----------------
    chas = group_and_prayers(dedupe(parse_blocks(raw["chaso_slov"]["blocks"])))
    # в источнике нет знаков ударения — текст заведомо церковнославянский
    for prayer in chas:
        for para in prayer["paras"]:
            if para["l"] == "ru":
                para["l"] = "cs"
    for item in chas:
        item["group"] = "Часослов"
    sections.append({"id": "chasoslov", "title": "Часослов",
                     "desc": "Тропари и молитвы часов",
                     "prayers": chas})

    # ---------------- Помянник ----------------
    sections.append({"id": "pomyannik", "title": "О живых и усопших",
                     "desc": "Помянник о живых и усопших",
                     "prayers": group_and_prayers(dedupe(parse_blocks(raw["zhivye_usopshie"]["blocks"])))})

    # ---------------- Основные молитвы ----------------
    def find(prayers_list, prefix):
        for p in prayers_list:
            if p["title"].startswith(prefix):
                return p
        return None

    basics = []
    for prefix, title in (("Молитва Господня", "Отче наш"),
                          ("Символ веры", "Символ веры"),
                          ("Молитва мытаря", "Молитва мытаря"),
                          ("Трисвятое", "Трисвятое"),
                          ("Молитва Святому Духу", "Молитва к Святому Духу")):
        src = find(morning, prefix)
        if src:
            basics.append({"group": "Основные молитвы", "title": title, "paras": src["paras"]})

    # Псалом 50 с переводом — с отдельной страницы
    pb = raw["psalm50"]["blocks"]
    cs, ru = [], []
    for block in pb:
        if block["type"] != "p":
            continue
        text = block["text"].strip()
        if text.startswith("Перевод:"):
            ru.extend(make_paras(text))
        elif ACC in text:
            cs.extend(make_paras(text))
    if cs or ru:
        basics.append({"group": "Основные молитвы", "title": "Псалом 50",
                       "paras": cs[:3] + ru[:3]})

    for p in group_and_prayers(dedupe(parse_blocks(raw["prosfora_voda"]["blocks"]))):
        basics.append({"group": "Основные молитвы",
                       "title": "Молитва на принятие просфоры и святой воды",
                       "paras": p["paras"]})
    sections.append({"id": "basics", "title": "Основные молитвы",
                     "desc": "Самые важные молитвы верующего",
                     "prayers": basics})

    # ---------------- Тропари праздников ----------------
    troparia = build_troparia(raw)
    sections.append({"id": "troparia", "title": "Тропари праздников",
                     "desc": "Тропари, кондаки и величания на праздники",
                     "prayers": troparia})

    # ---------------- Чин 12 псалмов ----------------
    chin12 = group_and_prayers(parse_blocks(raw["chin12"]["blocks"]))
    sections.append({"id": "chin12", "title": "Чин 12 псалмов",
                     "desc": "Кафисма девятая: псалмы, читаемые на утрене",
                     "prayers": chin12})

    # ---------------- Акафист Пресвятой Богородице ----------------
    akafist = group_and_prayers(parse_blocks(raw["akafist"]["blocks"]))
    sections.append({"id": "akafist", "title": "Акафист Богородице",
                     "desc": "Великий акафист Пресвятой Богородице",
                     "prayers": akafist})

    # ---------------- Последование ко Святому Причащению ----------------
    prichastie = group_and_prayers(parse_blocks(raw["prichastie"]["blocks"]))
    sections.append({"id": "prichastie", "title": "К Святому Причащению",
                     "desc": "Последование ко Святому Причащению",
                     "prayers": prichastie})

    # ---------------- Пасхальный канон ----------------
    pasxa = group_and_prayers(parse_blocks(raw["pasxa"]["blocks"]))
    sections.append({"id": "pasxa", "title": "Пасхальный канон",
                     "desc": "Канон на Светлую Пасху, творение преподобного Иоанна Дамаскина",
                     "prayers": pasxa})

    # ---------------- Прочие каноны ----------------
    for key, title, desc in (("kanon_angel", "Канон Ангелу-Хранителю",
                              "Канон Ангелу-Хранителю, читаемый при выходе из дома"),
                             ("kanon_bogorodice", "Канон молебный ко Богородице",
                              "Канон, поемый во всякой скорби душевной")):
        canons = group_and_prayers(parse_blocks(raw[key]["blocks"]))
        for prayer in canons:
            prayer["group"] = title
        sections.append({"id": key, "title": title, "desc": desc, "prayers": canons})

    # ---------------- Акафисты ----------------
    akafist_prayers = []
    for key in raw.get("akafisty", {}):
        data = raw["akafisty"][key]
        for prayer in group_and_prayers(parse_blocks(data["blocks"])):
            prayer["group"] = data["title"]
            akafist_prayers.append(prayer)
    sections.append({"id": "akafisty", "title": "Акафисты",
                     "desc": "Акафисты важнейшим праздникам и святым",
                     "prayers": akafist_prayers})

    # ---------------- Молитвы на разные случаи ----------------
    daily_prayers = []
    for key in sorted(raw.get("daily", {})):
        data = raw["daily"][key]
        for prayer in group_and_prayers(parse_blocks(data["blocks"])):
            prayer["group"] = data["title"]
            daily_prayers.append(prayer)
    sections.append({"id": "daily", "title": "Молитвы на разные случаи",
                     "desc": "Молитвы на каждый день и на разные случаи жизни",
                     "prayers": daily_prayers})

    # ---------------- Избранное ----------------
    sections.append({"id": "favorites", "title": "Избранное",
                     "desc": "Отмеченные вами молитвы",
                     "prayers": []})

    # --- финальная вычитка: дубли, «пустые» молитвы, переклассификация ---
    skip_titles = ("оглавление", "содержание", "оглавление раздела", "навигация по разделу")
    for sec in sections:
        cleaned = []
        for prayer in sec["prayers"]:
            if prayer["title"].strip().lower() in skip_titles:
                continue
            seen = set()
            paras = []
            for para in prayer["paras"]:
                key = para["t"].strip().lower()
                if key in seen and len(key) > 60:
                    continue
                seen.add(key)
                paras.append(para)
            if not paras:
                continue
            has_ru = any(p["l"] in ("ru", "both") for p in paras)
            if has_ru:
                for para in paras:
                    # «ЦС» без знаков ударения внутри молитвы с переводом — это пояснение
                    if para["l"] == "cs" and ACC not in para["t"] and not para.get("lock"):
                        para["l"] = "ru"
                        if para["k"] == "text":
                            para["k"] = "rubric"
            prayer["paras"] = paras
            cleaned.append(prayer)
        sec["prayers"] = cleaned

    order = ["morning", "evening", "psaltir", "chin12", "troparia", "akafist", "akafisty", "kanon",
             "kanon_angel", "kanon_bogorodice", "pasxa", "prichastie", "bogorodica",
             "daily", "kratkie", "chaso_slov", "pomyannik", "basics", "favorites"]
    sections.sort(key=lambda s: order.index(s["id"]) if s["id"] in order else len(order))

    # группа и короткое название для бокового меню
    meta = {
        "morning": ("Дневные молитвы", "Утренние"),
        "evening": ("Дневные молитвы", "Вечерние"),
        "psaltir": ("Псалтирь", "Псалтирь"),
        "chin12": ("Псалтирь", "Чин 12 псалмов"),
        "troparia": ("Службы", "Тропари"),
        "akafist": ("Службы", "Великий акафист"),
        "akafisty": ("Службы", "Акафисты"),
        "kanon": ("Службы", "Покаянный канон"),
        "kanon_angel": ("Службы", "Канон Ангелу"),
        "kanon_bogorodice": ("Службы", "Канон Богородицы"),
        "pasxa": ("Службы", "Пасхальный канон"),
        "prichastie": ("Службы", "К Причастию"),
        "bogorodica": ("Молитвы", "Песни Богородице"),
        "daily": ("Молитвы", "По случаю"),
        "kratkie": ("Молитвы", "Краткие молитвы"),
        "chasoslov": ("Молитвы", "Часослов"),
        "pomyannik": ("Молитвы", "Помянник"),
        "basics": ("Молитвы", "Основные молитвы"),
        "favorites": ("Избранное", "Мои молитвы"),
    }
    for sec in sections:
        group, short_title = meta.get(sec["id"], ("", sec["title"]))
        sec["group"] = group
        sec["shortTitle"] = short_title

    out = {"sections": sections}
    path = os.path.join(DATA, "prayers.json")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(out, fh, ensure_ascii=False, indent=1)

    total = 0
    for s in sections:
        n = sum(len(p["paras"]) for p in s["prayers"])
        total += n
        print("%-12s prayers=%3d paras=%5d" % (s["id"], len(s["prayers"]), n))
    print("total paragraphs:", total)
    print("saved:", path)


if __name__ == "__main__":
    build()
