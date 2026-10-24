# -*- coding: utf-8 -*-
"""Загружает тексты молитв с azbyka.ru и сохраняет их в data/blocks.json.

Результат: список блоков вида {"type": "h2|h3|p|li", "text": "..."}.
"""

import html
import json
import os
import re
import sys
import urllib.request

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(BASE, "data")

UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36"

PAGES = [
    ("morning", "Утренние молитвы", "https://azbyka.ru/molitvoslov/molitvy-utrennie-bez-tolkovanij.html"),
    ("evening", "Молитвы на сон грядущим", "https://azbyka.ru/molitvoslov/molitvy-na-son-gryadushhim-bez-tolkovanij.html"),
    ("symbol", "Символ веры", "https://azbyka.ru/simvol_very"),
    ("otche_nash", "Отче наш", "https://azbyka.ru/molitvoslov/molitva-gospodnya-otche-nash.html"),
    ("bogorodica", "Молитвы ко Пресвятой Богородице", "https://azbyka.ru/molitvoslov/molitvy-ko-presvyatoj-bogorodice.html"),
    ("psalm50", "Псалом 50", "https://azbyka.ru/molitvoslov/psalom-50.html"),
    ("kratkie", "Краткие молитвы", "https://azbyka.ru/molitvoslov/kratkie-molitvy.html"),
    ("kanon_pokayanny", "Покаянный канон", "https://azbyka.ru/molitvoslov/kanon-pokayannyj-ko-gospodu-nashemu-iisusu-xristu.html"),
    ("zhivye_usopshie", "Помянник о живых и усопших", "https://azbyka.ru/molitvoslov/pomyannik-o-zhivyx-i-usopshix.html"),
    ("prosfora_voda", "Молитва на принятие просфоры и святой воды", "https://azbyka.ru/molitvoslov/molitva-na-prinyatie-prosfory-i-svyatoj-vody.html"),
    ("chaso_slov", "Часослов", "https://azbyka.ru/chaso-slov/"),
    ("psaltir", "Псалтирь", "https://azbyka.ru/molitvoslov/psaltir-po-kafizmam-s-perevodom.html"),
    ("troparia", "Тропари праздников",
     "https://azbyka.ru/molitvoslov/pravoslavnyj-molitvennik-s-perevodom-na-russkij-jazyk.html"),
    ("chin12", "Чин 12 псалмов",
     "https://azbyka.ru/molitvoslov/chin-peniya-dvenadcati-psalmov.html"),
    ("akafist", "Акафист Богородице",
     "https://azbyka.ru/molitvoslov/akafist-presvjatoj-bogorodice-velikij-akafist-chitaemyj-v-subbotu-akafista.html"),
    ("prichastie", "Последование ко Святому Причащению",
     "https://azbyka.ru/molitvoslov/posledovanie-ko-svyatomu-prichashheniyu.html"),
    ("pasxa", "Пасхальный канон",
     "https://azbyka.ru/molitvoslov/pasxalnyj-kanon-tvorenie-ioanna-damaskina.html"),
    ("kanon_angel", "Канон Ангелу-Хранителю",
     "https://azbyka.ru/molitvoslov/kanon-angelu-xranitelyu.html"),
    ("kanon_bogorodice", "Канон молебный ко Богородице",
     "https://azbyka.ru/molitvoslov/kanon-molebnyj-ko-presvyatoj-bogorodice.html"),
]

SKIP = re.compile(r"(?is)<script.*?</script>|<style.*?</style>|<noscript.*?</noscript>|<sup[^>]*>.*?</sup>")

# Избранные акафисты.
AKAFISTY = [
    ("ak_pokrov", "Акафист Покрову Богородицы",
     "https://azbyka.ru/molitvoslov/akafist-pokrovu-presvjatoj-bogorodicy.html"),
    ("ak_voskresenie", "Акафист Воскресению Христову",
     "https://azbyka.ru/molitvoslov/akafist-voskreseniju-hristovu.html"),
    ("ak_iisus", "Акафист Иисусу Сладчайшему",
     "https://azbyka.ru/molitvoslov/akafist-iisusu-sladchajshemu.html"),
    ("ak_russkie", "Акафист всем святым, в земле Русской просиявшим",
     "https://azbyka.ru/molitvoslov/akafist-vsem-svjatym-v-zemle-russkoj-prosijavshim.html"),
    ("ak_strasti", "Акафист Божественным Страстям Христовым",
     "https://azbyka.ru/molitvoslov/akafist-bozhestvennym-strastjam-hristovym.html"),
    ("ak_novomuch", "Акафист новомученикам и исповедникам Церкви Русской",
     "https://azbyka.ru/molitvoslov/akafist-novomuchenikam-i-ispovednikam-cerkvi-russkoj.html"),
    ("ak_nikola", "Акафист святителю Николаю Чудотворцу",
     "https://azbyka.ru/molitvoslov/akafist-svjatitelju-nikolaju-chudotvorcu.html"),
    ("ak_krest", "Акафист на Воздвижение Креста Господня",
     "https://azbyka.ru/molitvoslov/akafist-na-vozdvizhenie-kresta-gospodnja.html"),
]

# Молитвы на разные случаи жизни.
DAILY = [
    ("blagodarenie", "Благодарение за всякое благодеяние Божие",
     "https://azbyka.ru/molitvoslov/blagodarenie-za-vsyakoe-blagodeyanie-bozhie.html"),
    ("prizyvanie", "Призывание помощи Духа Святаго",
     "https://azbyka.ru/molitvoslov/prizyvanie-pomoshhi-duxa-svyatago-na-vsyakoe-delo-dobroe.html"),
    ("vyhod_iz_doma", "Перед выходом из дома",
     "https://azbyka.ru/molitvoslov/molitva-pered-vyxodom-iz-doma.html"),
    ("eda", "Перед и после вкушения пищи",
     "https://azbyka.ru/molitvoslov/molitvy-pered-i-posle-vkusheniya-pishhi.html"),
    ("delo", "Перед началом и по окончании всякого дела",
     "https://azbyka.ru/molitvoslov/molitva-pered-nachalom-i-po-okonchanii-vsyakogo-dela.html"),
    ("v_hram", "По пути в храм и при входе в него",
     "https://azbyka.ru/molitvoslov/molitvy-po-puti-v-xram-i-pri-vxode-v-nego.html"),
    ("evangelie", "Пред и по чтении Евангелия",
     "https://azbyka.ru/molitvoslov/molitva-pred-i-po-chtenii-evangeliya.html"),
    ("internet", "Перед выходом в интернет",
     "https://azbyka.ru/molitvoslov/molitva-pered-vyxodom-v-internet.html"),
    ("optinskie", "Молитва Оптинских старцев",
     "https://azbyka.ru/molitvoslov/molitva-poslednix-optinskix-starcev-na-nachalo-dnya.html"),
    ("ioasaf", "Ежечасная молитва Иоасафа Белгородского",
     "https://azbyka.ru/molitvoslov/ezhechasnaja-molitva-svjatitelja-ioasafa-episkopa-belgorodskogo.html"),
    ("serafim", "Молитвенное правило Серафима Саровского",
     "https://azbyka.ru/molitvoslov/molitvennoe-pravilo-prp-serafima-sarovskogo.html"),
    ("deti", "Молитвы о детях и крестниках",
     "https://azbyka.ru/molitvoslov/molitvy-o-detyax-i-krestnikax.html"),
    ("spirodon", "Молитва святителю Спиридону Тримифунтскому",
     "https://azbyka.ru/molitvoslov/karta-sajta"),
    ("troica", "Три молитвы в День Святой Троицы",
     "https://azbyka.ru/molitvoslov/tri-molitvy-v-den-svjatoj-troicy.html"),
]


def fetch(url):
    req = urllib.request.Request(url, headers={
        "User-Agent": UA,
        "Accept": "text/html,application/xhtml+xml",
        "Accept-Language": "ru-RU,ru;q=0.9",
    })
    with urllib.request.urlopen(req, timeout=90) as resp:
        raw = resp.read()
    for enc in ("utf-8", "windows-1251"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", "replace")


def clean_text(t):
    t = html.unescape(t)
    t = t.replace("\u00a0", " ").replace("\u200b", "").replace("\u2026", "...")
    t = re.sub(r"\[\d+\]", "", t)
    t = re.sub(r"[ \t]+", " ", t)
    return t.strip()



def extract_festival_troparia(doc):
    """Тропари, кондаки и величания на праздники (таблица «славянский текст | перевод»)."""
    import html as _html
    import re as _re

    start = doc.rfind("Тропари, кондаки и величания на двунадесятые")
    if start < 0:
        return []
    body = doc[start:]
    # в разметке встречаются незакрытые <p> — из-за них «съедаются» заголовки
    body = re.sub(r"(?is)<p>\s*</div>", "</div>", body)

    def cell_text(html):
        text = _re.sub(r"(?is)<[^>]+>", "", html)
        text = _html.unescape(text).replace("\u00a0", " ")
        text = _re.sub(r"\[\d+\]", "", text)   # номера сносок
        return _re.sub(r"\s+", " ", text).strip()

    # «На <праздник> (<дата>) <вид песнопения>»; в скобках может быть и
    # относительная дата («за неделю до Пасхи»), и просто число.
    head_re = _re.compile(
        r"^На\s+([^\d,;:]{2,60}?)\s*\(([^)]{1,40})\)\s*"
        r"(Тропарь|Кондак|Величание)?\s*$")

    def head_of(text):
        """-> (праздник, дата, вид) либо None."""
        m = head_re.match(text)
        if not m:
            return None
        name, date, kind = m.group(1).strip(), m.group(2).strip(), m.group(3)
        return name, date, kind

    blocks = []
    cur_lang = None
    alt_mode = False
    for m in _re.finditer(r"(?is)<(td|p|h[2-5])[^>]*>(.*?)</\1>", body):
        tag = m.group(1)
        text = cell_text(m.group(2))
        if not text or text in ("&nbsp;", "nbsp"):
            continue
        if tag == "td":
            head = head_of(text)
            if head:
                blocks.append({"type": "h3", "text": head[0], "date": head[1]})
                blocks.append({"type": "h4", "text": head[2] or "Тропарь"})
                cur_lang = None
                alt_mode = False
                continue
            if text in ("Тропарь", "Кондак", "Величание"):
                blocks.append({"type": "h4", "text": text})
                cur_lang = None
                alt_mode = False
                continue
            if text == "Славянский текст":
                cur_lang = "cs"
                alt_mode = True
                continue
            if text == "Русский перевод":
                continue
            block = {"type": "p", "text": text}
            if alt_mode and cur_lang:
                block["lang"] = cur_lang
                cur_lang = "ru" if cur_lang == "cs" else "cs"
            blocks.append(block)
            continue
        # абзацы вне таблицы (например, величание)
        alt_mode = False
        cur_lang = None
        head = head_of(text)
        if head:
            blocks.append({"type": "h3", "text": head[0], "date": head[1]})
            blocks.append({"type": "h4", "text": head[2] or "Тропарь"})
            continue
        if text == "Величание":
            blocks.append({"type": "h4", "text": "Величание"})
            continue
        blocks.append({"type": "p", "text": text})
    return blocks


def extract_psaltir(doc):
    """Псалтирь на azbyka свёрстана таблицей: слева церковнославянский, справа перевод."""
    import html as _html
    import re as _re

    start = doc.find("Псало́м")
    if start < 0:
        start = doc.find("Псалом")
    if start < 0:
        return []
    body = doc[start:]

    def cell_text(html):
        text = _re.sub(r"(?is)<sup[^>]*>(.*?)</sup>", r"\1", html)
        text = _re.sub(r"(?is)<[^>]+>", "", text)
        text = _html.unescape(text)
        text = text.replace("\u00a0", " ").replace("\u200b", "")
        text = _re.sub(r"[ \t]+", " ", text)
        return text.strip()

    blocks = []
    for row in _re.finditer(r"(?is)<tr[^>]*>(.*?)</tr>", body):
        cells = _re.findall(r"(?is)<td([^>]*)>(.*?)</td>", row.group(1))
        if not cells:
            continue
        texts = [cell_text(c[1]) for c in cells]
        texts = [t for t in texts if t]
        if not texts:
            continue
        if len(cells) == 1 and "colspan" in cells[0][0].lower():
            head = texts[0]
            if _re.match(r"^Кафи́?сма", head) or _re.match(r"^Кафисма", head):
                blocks.append({"type": "h2", "text": head})
            elif _re.match(r"^Псало́?м\s*\d", head) or _re.match(r"^Псалом\s*\d", head):
                blocks.append({"type": "h3", "text": head})
            else:
                blocks.append({"type": "h3", "text": head})
            continue
        if len(texts) == 2:
            blocks.append({"type": "p", "text": texts[0]})
            blocks.append({"type": "p", "text": texts[1]})
        else:
            for t in texts:
                blocks.append({"type": "p", "text": t})
    return blocks

def to_blocks(doc):
    body = doc
    m = re.search(r'(?is)<div[^>]+class="[^"]*content[^"]*"[^>]*>(.*)', doc)
    if m:
        body = m.group(1)
    body = SKIP.sub(" ", body)
    for cut in (r'(?is)<div[^>]+class="[^"]*(footer|sidebar|related|comments)[^"]*"',
                r"(?is)<footer", r"(?is)<aside"):
        c = re.search(cut, body)
        if c:
            body = body[: c.start()]

    blocks = []
    # в разметке встречаются незакрытые <p> — из-за них «съедаются» заголовки
    body = re.sub(r"(?is)<p>\s*</div>", "</div>", body)
    pattern = re.compile(r"(?is)<h([2-6])[^>]*>(.*?)</h\1>|<p[^>]*>(.*?)</p>|<li[^>]*>(.*?)</li>|<br\s*/?>")
    for m in pattern.finditer(body):
        if m.group(1):
            txt = clean_text(re.sub(r"(?s)<[^>]+>", "", m.group(2)))
            if txt:
                blocks.append({"type": "h" + m.group(1), "text": txt})
        elif m.group(3) is not None:
            txt = clean_text(re.sub(r"(?s)<[^>]+>", " ", m.group(3)))
            if txt:
                blocks.append({"type": "p", "text": txt})
        elif m.group(4) is not None:
            txt = clean_text(re.sub(r"(?s)<[^>]+>", " ", m.group(4)))
            if txt:
                blocks.append({"type": "li", "text": txt})
        else:
            blocks.append({"type": "br", "text": ""})
    return blocks


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, "blocks.json")
    only = None
    if len(sys.argv) > 2 and sys.argv[1] == "--only":
        only = set(sys.argv[2].split(","))

    result = {}
    if only:
        # точечное обновление: остальные разделы берём из кэша
        if os.path.exists(path):
            with open(path, encoding="utf-8") as fh:
                result = json.load(fh)

    for key, title, url in PAGES:
        if only and key not in only:
            continue
        try:
            doc = fetch(url)
        except Exception as exc:  # noqa: BLE001
            print("FAIL %-18s %s" % (key, exc), file=sys.stderr)
            continue
        if key == "psaltir":
            blocks = extract_psaltir(doc)
        elif key == "troparia":
            blocks = extract_festival_troparia(doc)
        else:
            blocks = to_blocks(doc)
        result[key] = {"title": title, "url": url, "blocks": blocks}
        print("OK   %-18s %5d blocks" % (key, len(blocks)))

    if not only:
        akafisty = {}
        for key, title, url in AKAFISTY:
            try:
                doc = fetch(url)
            except Exception as exc:  # noqa: BLE001
                print("FAIL %-18s %s" % (key, exc), file=sys.stderr)
                continue
            akafisty[key] = {"title": title, "url": url, "blocks": to_blocks(doc)}
            print("OK   %-18s %6d chars" % (key, len(akafisty[key]["blocks"])))
        result["akafisty"] = akafisty

        daily = {}
        for key, title, url in DAILY:
            try:
                doc = fetch(url)
            except Exception as exc:  # noqa: BLE001
                print("FAIL %-18s %s" % (key, exc), file=sys.stderr)
                continue
            daily[key] = {"title": title, "url": url, "blocks": to_blocks(doc)}
            print("OK   %-18s %6d chars" % (key, len(daily[key]["blocks"])))
        result["daily"] = daily

    with open(path, "w", encoding="utf-8") as fh:
        json.dump(result, fh, ensure_ascii=False, indent=1)
    print("saved:", path)


if __name__ == "__main__":
    main()
