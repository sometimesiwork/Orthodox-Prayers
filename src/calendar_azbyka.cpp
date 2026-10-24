// Раздел «Азбука веры и порталы»: онлайн-чтения дня, полные святцы,
// тропари и молитвы святым дня (ЦС и РУ), цитата, притча, основы
// православия, практический совет, пролог, день в истории и каталог
// православных порталов.

#include "calendar_init.h"
#include "calendar_dates.h"
#include "calendar_sections.h"
#include "online.h"

#include <windows.h>
#include <shellapi.h>

#include <utility>

namespace {

void SplitLinesToParas(std::vector<std::pair<unsigned char, std::wstring>>& ps,
                       const std::wstring& text) {
    size_t start = 0;
    while (start < text.size()) {
        size_t e = text.find(L'\n', start);
        if (e == std::wstring::npos) e = text.size();
        std::wstring line = text.substr(start, e - start);
        if (!line.empty()) {
            ps.push_back(Txt(std::move(line)));
        }
        start = e + 1;
    }
}

}  // namespace

Section BuildAzbykaSection() {
    Section s;
    s.id = L"azbyka";
    s.title = L"Азбука веры и порталы";
    s.shortTitle = L"Азбука веры";
    s.group = L"Сегодня и Устав";
    s.desc = L"чтения, святцы, тропари дня, притча, катехизис и православные сайты";

    const online::Data& d = online::Get();
    const std::wstring dateKey = d.dateKey.empty() ? online::DateKey() : d.dateKey;
    const std::wstring dayUrl = L"https://azbyka.ru/days/" + dateKey;

    // 1. Чтения Священного Писания дня
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(d.status));
        if (!d.readings.empty()) {
            ps.push_back(Rub(L"Божественные чтения суточного круга:"));
            SplitLinesToParas(ps, d.readings);
        } else {
            ps.push_back(
                Txt(L"Данные загружаются с портала azbyka.ru. Нажмите кнопку "
                    L"«Открыть на сайте» или «Обновить» для загрузки."));
        }
        AddPrayer(s, L"Чтения Священного Писания дня", std::move(ps));
        s.prayers.back().group = L"ДЕНЬ НА АЗБУКЕ ВЕРЫ";
        s.prayers.back().url = L"https://azbyka.ru/biblia/days/" + dateKey;
    }

    // 2. Полные святцы дня с сайта
    if (!d.saints.empty()) {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Полный месяцеслов дня по календарю Азбуки веры:"));
        SplitLinesToParas(ps, d.saints);
        AddPrayer(s, L"Святые и праздники дня (полные святцы)", std::move(ps));
        s.prayers.back().group = L"ДЕНЬ НА АЗБУКЕ ВЕРЫ";
        s.prayers.back().url = dayUrl;
    }

    // 3. Цитата дня и Пролог в поучениях
    if (!d.quote.empty() || !d.prologue.empty()) {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        if (!d.quote.empty()) {
            ps.push_back(Rub(L"Цитата дня святых отцов:"));
            SplitLinesToParas(ps, d.quote);
        }
        if (!d.prologue.empty()) {
            ps.push_back(Rub(L"Пролог в поучениях:"));
            SplitLinesToParas(ps, d.prologue);
        }
        AddPrayer(s, L"Цитата дня и Пролог", std::move(ps));
        s.prayers.back().group = L"ДУХОВНОЕ ЧТЕНИЕ ДНЯ";
        s.prayers.back().url = L"https://azbyka.ru/quotes/";
    }

    // 4. Притча дня
    if (!d.parable.empty()) {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Душеполезная притча дня:"));
        SplitLinesToParas(ps, d.parable);
        AddPrayer(s, L"Притча дня", std::move(ps));
        s.prayers.back().group = L"ДУХОВНОЕ ЧТЕНИЕ ДНЯ";
        s.prayers.back().url = L"https://azbyka.ru/pritchi";
    }

    // 5. Основы православия (тема дня)
    if (!d.osnovy.empty()) {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Катехизическая тема дня:"));
        SplitLinesToParas(ps, d.osnovy);
        AddPrayer(s, L"Основы православия", std::move(ps));
        s.prayers.back().group = L"ДУХОВНОЕ ЧТЕНИЕ ДНЯ";
        s.prayers.back().url = L"https://azbyka.ru/1/";
    }

    // 6. Практический совет дня
    if (!d.sovet.empty()) {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Совет христианину на каждый день:"));
        SplitLinesToParas(ps, d.sovet);
        AddPrayer(s, L"Практический совет дня", std::move(ps));
        s.prayers.back().group = L"ДУХОВНОЕ ЧТЕНИЕ ДНЯ";
        s.prayers.back().url = dayUrl;
    }

    // 7. Этот день в истории
    if (!d.history.empty()) {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Памятные даты церковной истории:"));
        SplitLinesToParas(ps, d.history);
        AddPrayer(s, L"Этот день в истории Церкви", std::move(ps));
        s.prayers.back().group = L"ДУХОВНОЕ ЧТЕНИЕ ДНЯ";
        s.prayers.back().url = dayUrl;
    }

    // 8. Тропари, кондаки, молитвы и величания святым дня (ЦС + РУ)
    for (const online::TroparBlock& tb : d.tropari) {
        Prayer p;
        p.group = L"ТРОПАРИ И МОЛИТВЫ ДНЯ";
        p.title = tb.title;
        p.url = dayUrl;
        if (!tb.group.empty()) {
            PrayerPara rub;
            rub.text = tb.group;
            rub.lang = ParaLang::Both;
            rub.style = kStyleRubric;
            p.paras.push_back(std::move(rub));
        }
        if (!tb.cs.empty()) {
            PrayerPara csPara;
            csPara.text = tb.cs;
            csPara.lang = tb.ru.empty() ? ParaLang::Both : ParaLang::Cs;
            csPara.style = kStyleText;
            p.paras.push_back(std::move(csPara));
        }
        if (!tb.ru.empty()) {
            PrayerPara ruPara;
            ruPara.text = tb.ru;
            ruPara.lang = ParaLang::Ru;
            ruPara.style = kStyleText;
            p.paras.push_back(std::move(ruPara));
        }
        s.prayers.push_back(std::move(p));
    }

    // 9. Навигатор по порталу «Азбука веры» и православным сайтам
    struct LinkRow {
        const wchar_t* group;
        const wchar_t* title;
        const wchar_t* desc;
        const wchar_t* url;
    };
    const std::wstring worshipUrl = L"https://azbyka.ru/worships/?date=" + dateKey;
    const std::wstring ukazUrl = L"https://azbyka.ru/bogosluzhebnye-ukazaniya?date=" + dateKey;

    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Полные богослужебные тексты и уставные указания на выбранный день:"));
        ps.push_back(Txt(L"Откройте полное чинопоследование вечерни, утрени, часов и литургии "
                         L"с изменяемыми песнопениями Октоиха, Минеи и Триоди на портале."));
        AddPrayer(s, L"Богослужение на этот день (полный текст)", std::move(ps));
        s.prayers.back().group = L"БОГОСЛУЖЕНИЕ И КАЛЕНДАРЬ ОНЛАЙН";
        s.prayers.back().url = worshipUrl;
    }
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Официальные богослужебные указания Русской Православной Церкви:"));
        ps.push_back(Txt(L"Подробные уставные заметки для священнослужителей, диаконов и чтецов "
                         L"на текущий день."));
        AddPrayer(s, L"Богослужебные указания Патриархии", std::move(ps));
        s.prayers.back().group = L"БОГОСЛУЖЕНИЕ И КАЛЕНДАРЬ ОНЛАЙН";
        s.prayers.back().url = ukazUrl;
    }

    static const LinkRow kLinks[] = {
        {L"БОГОСЛУЖЕНИЕ И КАЛЕНДАРЬ ОНЛАЙН", L"Молимся по часам (портал Азбуки)",
         L"Тропари и молитвы каждого часа дня и ночи по церковному уставу.",
         L"https://azbyka.ru/chaso-slov/"},
        {L"БОГОСЛУЖЕНИЕ И КАЛЕНДАРЬ ОНЛАЙН", L"Видеотрансляция из храма",
         L"Прямые трансляции православных богослужений из монастырей и соборов.",
         L"https://azbyka.ru/bogosluzhenie-online"},
        {L"БОГОСЛУЖЕНИЕ И КАЛЕНДАРЬ ОНЛАЙН", L"Аудиокалендарь и жития святых",
         L"Ежедневные аудиозаписи житий святых, песнопений и духовных советов.",
         L"https://azbyka.ru/days/p-audio-calendar"},
        {L"БОГОСЛУЖЕНИЕ И КАЛЕНДАРЬ ОНЛАЙН", L"Православное радио и песнопения",
         L"Круглосуточное православное радио, литургические песнопения и чтение Писания.",
         L"https://azbyka.ru/audio/radio.html"},
        {L"БИБЛИОТЕКИ И ТОЛКОВАНИЯ", L"Библия и святоотеческие толкования",
         L"Священное Писание Ветхого и Нового Завета с параллельными местами и толкованиями.",
         L"https://azbyka.ru/biblia/"},
        {L"БИБЛИОТЕКИ И ТОЛКОВАНИЯ", L"Отечник — библиотека святых отцов",
         L"Собрание творений святых отцов, богословов и подвижников благочестия.",
         L"https://azbyka.ru/otechnik/"},
        {L"БИБЛИОТЕКИ И ТОЛКОВАНИЯ", L"Православная энциклопедия «Азбука веры»",
         L"Алфавитный богословский, литургический и церковно-исторический словарь.",
         L"https://azbyka.ru/dictionary"},
        {L"БИБЛИОТЕКИ И ТОЛКОВАНИЯ", L"Экзегет.ру — Библия и лекции",
         L"Построчное толкование Библии, переводы, аудиобиблия и библейские курсы.",
         L"https://ekzeget.ru/"},
        {L"ПРАВОСЛАВНЫЕ ПОРТАЛЫ", L"Православие.Ru (Сретенский монастырь)",
         L"Православный календарь, статьи, проповеди, ответы священников и аналитика.",
         L"https://pravoslavie.ru/"},
        {L"ПРАВОСЛАВНЫЕ ПОРТАЛЫ", L"Предание.ру — медиатека и аудиокниги",
         L"Крупнейший православный архив аудиокниг, лекций, проповедей и музыки.",
         L"https://predanie.ru/"},
        {L"ПРАВОСЛАВНЫЕ ПОРТАЛЫ", L"Оптина пустынь — официальный сайт",
         L"Наследие преподобных Оптинских старцев, поучения, расписание богослужений.",
         L"https://www.optina.ru/"},
        {L"ПРАВОСЛАВНЫЕ ПОРТАЛЫ", L"Свято-Троицкая Сергиева Лавра",
         L"Официальный портал Лавры: прямые трансляции у раки прп. Сергия, новости, устав.",
         L"https://stsl.ru/"},
    };

    for (const LinkRow& row : kLinks) {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Txt(row.desc));
        ps.push_back(Rub(std::wstring(L"Адрес: ") + row.url));
        AddPrayer(s, row.title, std::move(ps));
        s.prayers.back().group = row.group;
        s.prayers.back().url = row.url;
    }

    return s;
}
