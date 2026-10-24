// Раздел «Православный календарь»: выбранная дата, богослужебный цвет
// облачений, глас, пост, месяцеслов, Пасхалия и ближайшие праздники.

#include "calendar_init.h"
#include "calendar_sections.h"
#include "online.h"

#include <algorithm>

Section BuildKalendarSection(const DayInfo& day) {
    Section s;
    s.id = L"kalendar";
    s.title = L"Православный календарь";
    s.shortTitle = L"Календарь";
    s.group = L"Сегодня и Устав";
    s.desc = L"даты, глас седмицы, облачения, пост, святцы и Пасхалия";

    const online::Data& od = online::Get();

    // 1. Сегодня / Выбранный день
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(day.period));

        std::wstring first = std::wstring(L"Новый стиль (григорианский): ") +
                             DateRu(day.g) + L" года, " +
                             kWeekdayNames[day.dow] + L".";
        ps.push_back(Txt(std::move(first)));

        std::wstring second = L"Старый стиль (церковный юлианский): " +
                              DateRu(day.j) + L" года.";
        ps.push_back(Txt(std::move(second)));

        if (day.glas > 0) {
            std::wstring gline = L"Октоих: глас " + std::to_wstring(day.glas) + L"-й.";
            if (day.matinsGospel > 0) {
                gline += L" Воскресное утреннее Евангелие: " +
                         std::to_wstring(day.matinsGospel) + L"-е.";
            }
            ps.push_back(Txt(std::move(gline)));
        } else {
            ps.push_back(
                Txt(L"Октоих: рядовой глас не поётся (особое праздничное последование)."));
        }

        if (!day.vestmentName.empty()) {
            ps.push_back(Rub(L"Цвет богослужебных облачений: " + day.vestmentName + L"."));
        }

        if (!od.readings.empty()) {
            ps.push_back(Rub(L"Рядовые чтения Священного Писания:"));
            ps.push_back(Txt(od.readings));
        }

        AddPrayer(s, L"Календарный лист дня", std::move(ps));
        s.prayers.back().group = L"ДЕНЬ ЦЕРКОВНОГО ГОДА";
    }

    // 2. Пост и трапеза
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        if (day.fast.continuous) {
            ps.push_back(Rub(L"Сплошная седмица — пост отменён."));
        } else if (day.fast.isFast) {
            ps.push_back(Rub(L"Постный день."));
        } else {
            ps.push_back(Rub(L"Непостный день (поста нет)."));
        }
        if (!day.fast.note.empty()) {
            ps.push_back(Txt(day.fast.note));
        }
        ps.push_back(Rub(MealLevelText(day.fast.level)));
        ps.push_back(
            Txt(L"Устав о трапезе приводится по монастырскому Типикону. В мирской "
                L"практике мера поста определяется по здоровью, трудам и благословению духовника."));
        AddPrayer(s, L"Пост и устав о трапезе", std::move(ps));
        s.prayers.back().group = L"ДЕНЬ ЦЕРКОВНОГО ГОДА";
    }

    // 3. Месяцеслов (святцы дня)
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        std::wstring head = L"Минея: " + std::to_wstring(day.j.d) + L" " +
                            kMonthNames[day.j.m - 1] + L" по старому стилю (" +
                            std::to_wstring(day.g.d) + L" " +
                            kMonthNames[day.g.m - 1] + L" по новому стилю).";
        ps.push_back(Rub(std::move(head)));

        for (const SaintRow& sr : day.saints) {
            std::wstring line = L"\u2720 " + sr.name;
            if (sr.rank == 3) {
                line += L" — великий / двунадесятый праздник (всенощное бдение)";
            } else if (sr.rank == 2) {
                line += L" — полиелейная служба";
            } else if (sr.rank == 1) {
                line += L" — славословная служба";
            }
            ps.push_back(Txt(std::move(line)));
        }

        if (!od.saints.empty()) {
            ps.push_back(Rub(L"Полный список святых дня (святцы портала Азбука веры):"));
            size_t start = 0;
            while (start < od.saints.size()) {
                size_t e = od.saints.find(L'\n', start);
                if (e == std::wstring::npos) e = od.saints.size();
                std::wstring ln = od.saints.substr(start, e - start);
                if (!ln.empty()) ps.push_back(Txt(std::move(ln)));
                start = e + 1;
            }
        } else if (day.saints.empty()) {
            ps.push_back(
                Txt(L"Рядовые памяти святых совершаются по вседневной Минее."));
        }

        AddPrayer(s, L"Месяцеслов: святые и праздники дня", std::move(ps));
        s.prayers.back().group = L"МЕСЯЦЕСЛОВ И ПАСХАЛИЯ";
    }

    // 4. Пасхалия и ближайшие праздники
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Переходящие вехи церковного года (" +
                         std::to_wstring(day.paschaYear) + L"):"));

        const YMD trG = FromJdnGreg(day.triodionStart);
        ps.push_back(Txt(L"Начало Постной Триоди (Неделя о мытаре и фарисее): " +
                         DateRu(trG) + L" (нов. ст.)."));

        const YMD lnG = FromJdnGreg(day.lentStart);
        ps.push_back(Txt(L"Начало Великого поста (Чистый понедельник): " +
                         DateRu(lnG) + L" (нов. ст.)."));

        const YMD vj = FromJdnJul(day.palmSun);
        const YMD vg = FromJdnGreg(day.palmSun);
        ps.push_back(Txt(L"Вход Господень в Иерусалим (Вербное воскресенье): " +
                         DateRu(vg) + L" нов. ст. (" + DateRu(vj) + L" ст. ст.)."));

        const YMD pj = FromJdnJul(day.pascha);
        const YMD pg = FromJdnGreg(day.pascha);
        ps.push_back(Txt(L"СВЕТЛОЕ ХРИСТОВО ВОСКРЕСЕНИЕ (ПАСХА): " +
                         DateRu(pg) + L" нов. ст. (" + DateRu(pj) + L" ст. ст.)."));

        const YMD ag = FromJdnGreg(day.ascen);
        ps.push_back(Txt(L"Вознесение Господне: " + DateRu(ag) + L" (нов. ст.)."));

        const YMD tg = FromJdnGreg(day.pentecost);
        ps.push_back(Txt(L"День Святой Троицы (Пятидесятница): " + DateRu(tg) +
                         L" (нов. ст.)."));

        ps.push_back(Rub(L"Ближайшие двунадесятые, великие и полиелейные праздники:"));
        auto upcoming = UpcomingFeasts(day, 8);
        for (const FeastRow& row : upcoming) {
            const YMD gj = FromJdnGreg(row.jdn);
            const long diff = row.jdn - day.jdn;
            std::wstring when =
                (diff == 0) ? L"сегодня"
                            : (diff == 1 ? L"завтра"
                                         : (L"через " + std::to_wstring(diff) + L" дн."));
            ps.push_back(Txt(L"\u2022 " + DateRu(gj) + L" (" + when + L") — " + row.name));
        }
        AddPrayer(s, L"Пасхалия и ближайшие праздники", std::move(ps));
        s.prayers.back().group = L"МЕСЯЦЕСЛОВ И ПАСХАЛИЯ";
    }

    return s;
}

const wchar_t* MealLevelText(int lv) {
    switch (lv) {
        case 0: return L"Устав трапезы: без ограничений (скоромная пища).";
        case 1: return L"Устав трапезы: горячая растительная пища с маслом (и рыба, если благословлено уставом).";
        case 2: return L"Устав трапезы: горячая растительная пища без елея (масла).";
        case 3: return L"Устав трапезы: сухоядение (хлеб, вода, овощи и плоды без варения и масла).";
        case 4: return L"Устав трапезы: полное воздержание от пищи до окончания вечернего богослужения.";
        default: return L"";
    }
}
