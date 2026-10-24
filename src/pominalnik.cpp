// Поминальник и сорокоусты: списки о здравии и о упокоении, расчёт 3-го,
// 9-го, 40-го дня, полугода, годовщин и Вселенских родительских суббот,
// а также полные молитвы с автоподстановкой имён.

#include "pominalnik.h"

#include "calendar_init.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace pom {

std::vector<std::wstring>& Live() {
    static std::vector<std::wstring> v;
    return v;
}

std::vector<DeadRow>& Dead() {
    static std::vector<DeadRow> v;
    return v;
}

std::vector<std::wstring> SplitBy(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::wstring Sanitize(std::wstring s) {
    std::wstring out;
    for (wchar_t c : s) {
        out += (c == L';' || c == L'|') ? L' ' : c;
    }
    const size_t b = out.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return std::wstring();
    const size_t e = out.find_last_not_of(L" \t\r\n");
    return out.substr(b, e - b + 1);
}

bool ParseDate(const std::wstring& s, int& y, int& m, int& d) {
    int parts[3] = {0, 0, 0};
    int idx = 0;
    int cur = 0;
    bool digits = false;
    for (wchar_t c : s) {
        if (c >= L'0' && c <= L'9') {
            digits = true;
            if (idx > 2) return false;
            cur = cur * 10 + (c - L'0');
            if (cur > 9999) return false;
        } else if (c == L'.' || c == L'/' || c == L'-') {
            if (!digits) return false;
            if (idx >= 2) return false;
            parts[idx++] = cur;
            cur = 0;
            digits = false;
        } else if (iswspace(c)) {
            continue;
        } else {
            return false;
        }
    }
    if (!digits || idx != 2) return false;
    parts[2] = cur;
    y = parts[2];
    m = parts[1];
    d = parts[0];
    return y >= 1000 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

void FromSettings(const std::wstring& live, const std::wstring& dead) {
    Live().clear();
    for (const std::wstring& item : SplitBy(live, L';')) {
        const std::wstring name = Sanitize(item);
        if (!name.empty()) {
            Live().push_back(name);
        }
    }
    Dead().clear();
    for (const std::wstring& item : SplitBy(dead, L';')) {
        const std::wstring s = Sanitize(item);
        if (s.empty()) continue;
        DeadRow row;
        const size_t paren = s.find(L'(');
        if (paren != std::wstring::npos) {
            const size_t close = s.find(L')', paren);
            const std::wstring datestr =
                close == std::wstring::npos
                    ? s.substr(paren + 1)
                    : s.substr(paren + 1, close - paren - 1);
            int y = 0, m = 0, d = 0;
            if (ParseDate(datestr, y, m, d)) {
                row.jdn = JdnGreg(y, m, d);
            }
            row.name = Sanitize(s.substr(0, paren));
        } else {
            row.name = s;
        }
        if (!row.name.empty()) {
            Dead().push_back(row);
        }
    }
}

std::wstring SettingsLive() {
    std::wstring out;
    for (size_t i = 0; i < Live().size(); ++i) {
        if (i) out += L";";
        out += Live()[i];
    }
    return out;
}

std::wstring SettingsDead() {
    std::wstring out;
    for (size_t i = 0; i < Dead().size(); ++i) {
        if (i) out += L";";
        out += Dead()[i].name;
        if (Dead()[i].jdn != 0) {
            const YMD d = FromJdnGreg(Dead()[i].jdn);
            wchar_t buf[32];
            wsprintfW(buf, L" (%02d.%02d.%04d)", d.d, d.m, d.y);
            out += buf;
        }
    }
    return out;
}

int LiveCount() { return static_cast<int>(Live().size()); }

int DeadCount() { return static_cast<int>(Dead().size()); }

namespace {

// Димитриевская родительская суббота: суббота перед 8 ноября по новому стилю
long DimitrievSaturdayJdn(int gregYear) {
    const long nov8 = JdnGreg(gregYear, 11, 8);
    const int dow = DowFromJdn(nov8);  // 0 = Вс .. 6 = Сб
    const int backToSat = (dow == 6) ? 7 : ((dow + 1) % 7);
    return nov8 - backToSat;
}

std::wstring JoinNames(const std::vector<std::wstring>& v, const wchar_t* emptyFallback) {
    if (v.empty()) return emptyFallback;
    std::wstring out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += L", ";
        out += v[i];
    }
    return out;
}

std::wstring JoinDeadNames(const std::vector<DeadRow>& v, const wchar_t* emptyFallback) {
    if (v.empty()) return emptyFallback;
    std::wstring out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += L", ";
        out += v[i].name;
    }
    return out;
}

}  // namespace

std::vector<MemoryDay> UpcomingMemoryDays(const DayInfo& day, int maxRows) {
    std::vector<MemoryDay> out;
    auto Add = [&](long jdn, std::wstring label) {
        if (jdn < day.jdn) return;
        MemoryDay md;
        md.jdn = jdn;
        md.label = std::move(label);
        out.push_back(std::move(md));
    };

    for (const DeadRow& row : Dead()) {
        if (row.jdn == 0) continue;
        // 3-й, 9-й, 40-й дни (считая день кончины первым днём) и полгода
        Add(row.jdn + 2, L"3-й день по кончине — " + row.name);
        Add(row.jdn + 8, L"9-й день по кончине — " + row.name);
        Add(row.jdn + 39, L"40-й день (окончание сорокоуста) — " + row.name);
        Add(row.jdn + 182, L"Полгода со дня кончины — " + row.name);

        // Ближайшие годовщины памяти (для любого года кончины)
        const YMD dg = FromJdnGreg(row.jdn);
        const int startY = std::max(dg.y + 1, day.g.y);
        for (int y = startY; y <= startY + 1; ++y) {
            const int yrs = y - dg.y;
            if (yrs <= 0) continue;
            const long ann = JdnGreg(y, dg.m, std::min(dg.d, 28));
            if (ann >= day.jdn) {
                Add(ann, std::to_wstring(yrs) + L"-я годовщина памяти — " + row.name);
                break;
            }
        }
    }

    // Вселенские и особенные дни поминовения усопших
    for (int py = day.g.y; py <= day.g.y + 1; ++py) {
        const long p = PaschaJdn(py);
        Add(p - 57, L"Вселенская мясопустная родительская суббота");
        Add(p - 36, L"Родительская суббота 2-й седмицы Великого поста");
        Add(p - 29, L"Родительская суббота 3-й седмицы Великого поста");
        Add(p - 22, L"Родительская суббота 4-й седмицы Великого поста");
        Add(p + 9,  L"Радоница — пасхальное поминовение усопших");
        Add(JdnGreg(py, 5, 9), L"Поминовение усопших воинов (9 мая)");
        Add(p + 48, L"Вселенская Троицкая родительская суббота");
        Add(DimitrievSaturdayJdn(py), L"Димитриевская родительская суббота");
    }

    std::sort(out.begin(), out.end(),
              [](const MemoryDay& a, const MemoryDay& b) { return a.jdn < b.jdn; });
    if (static_cast<int>(out.size()) > maxRows) {
        out.resize(static_cast<size_t>(maxRows));
    }
    return out;
}

Section BuildPominalnikSection(const DayInfo& day) {
    Section s;
    s.id = L"pominalnik";
    s.title = L"Поминальник и сорокоусты";
    s.shortTitle = L"Поминальник";
    s.group = L"Сегодня и Устав";
    s.desc = L"списки о здравии и о упокоении, молитвы с именами, годовщины и родительские субботы";

    // 1. Списки о здравии и о упокоении
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Записей о здравии: " + std::to_wstring(LiveCount()) +
                         L"  ·  Записей о упокоении: " + std::to_wstring(DeadCount()) +
                         L" (нажмите кнопку «Изменить списки» на карточке)."));
        ps.push_back(Rub(L"О здравии и спасении (живые):"));
        if (Live().empty()) {
            ps.push_back(
                Txt(L"Список пуст. Нажмите кнопку «Изменить списки» на этой карточке, "
                    L"чтобы добавить имена родных и близких."));
        } else {
            for (const std::wstring& name : Live()) {
                ps.push_back(Txt(L"\u2720 " + name));
            }
        }
        ps.push_back(Rub(L"О упокоении (усопшие):"));
        if (Dead().empty()) {
            ps.push_back(
                Txt(L"Список пуст. Добавьте имена усопших и даты их кончины в формате "
                    L"«Имя (дд.мм.гггг)» для расчёта 9-го, 40-го дня и годовщин."));
        } else {
            for (const DeadRow& r : Dead()) {
                std::wstring line = L"\u271D " + r.name;
                if (r.jdn != 0) {
                    const YMD dg = FromJdnGreg(r.jdn);
                    const YMD dj = FromJdnJul(r.jdn);
                    line += L" — кончина: " + DateRu(dg) + L" нов. ст. (" +
                            DateRu(dj) + L" ст. ст.)";
                }
                ps.push_back(Txt(std::move(line)));
            }
        }
        AddPrayer(s, L"Ваши поминальные списки", std::move(ps));
        s.prayers.back().group = L"ПОМИНАЛЬНЫЕ СПИСКИ И МОЛИТВЫ";
    }

    // 2. Молитва о живых (с автоподстановкой имён)
    {
        const std::wstring names = JoinNames(Live(), L"(имена)");
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Ежедневная молитва о здравии с вашими именами:"));
        ps.push_back(
            Txt(L"Спаси́, Го́споди, и поми́луй раб Твои́х: " + names +
                L", и всех правосла́вных христиа́н, да́руй им здра́вие душевное и теле́сное, "
                L"мир, долгоде́нствие и во всем благо́е поспеше́ние, сохрани́ их от вся́кия "
                L"ско́рби, беды́, гне́ва и ну́жды, и прости́ им вся́кое согреше́ние, во́льное же "
                L"и нево́льное, и моли́твами их поми́луй мя гре́шнаго."));
        AddPrayer(s, L"Молитва о живых (с вашими именами)", std::move(ps));
        s.prayers.back().group = L"ПОМИНАЛЬНЫЕ СПИСКИ И МОЛИТВЫ";
    }

    // 3. Молитва об усопших (с автоподстановкой имён)
    {
        const std::wstring names = JoinDeadNames(Dead(), L"(имена)");
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(Rub(L"Ежедневная молитва об упокоении с вашими именами:"));
        ps.push_back(
            Txt(L"Упоко́й, Го́споди, ду́ши усо́пших раб Твои́х: " + names +
                L", и всех правосла́вных христиа́н, и прости́ им вся согреше́ния во́льная и "
                L"нево́льная, и да́руй им Ца́рствие Небе́сное и прича́стие ве́чных Твои́х благи́х "
                L"и Твоея́ безконе́чныя и блаже́нныя жи́зни наслажде́ние."));
        ps.push_back(
            Txt(L"Со святы́ми упоко́й, Христе́, ду́ши раб Твои́х, иде́же несть боле́знь, "
                L"ни печа́ль, ни воздыха́ние, но жизнь безконе́чная. Ве́чная па́мять!"));
        AddPrayer(s, L"Молитва об усопших (с вашими именами)", std::move(ps));
        s.prayers.back().group = L"ПОМИНАЛЬНЫЕ СПИСКИ И МОЛИТВЫ";
    }

    // 4. Календарь памятных дат, сорокоустов и родительских суббот
    {
        std::vector<std::pair<unsigned char, std::wstring>> ps;
        ps.push_back(
            Rub(L"Годовщины кончины, 3-й, 9-й, 40-й дни и Вселенские родительские субботы:"));
        auto days = UpcomingMemoryDays(day, 14);
        for (const MemoryDay& md : days) {
            const YMD g = FromJdnGreg(md.jdn);
            const long diff = md.jdn - day.jdn;
            std::wstring when =
                (diff == 0) ? L"сегодня"
                            : (diff == 1 ? L"завтра"
                                         : (L"через " + std::to_wstring(diff) + L" дн."));
            ps.push_back(Txt(L"\u2022 " + DateRu(g) + L" (" + when + L") — " + md.label));
        }
        ps.push_back(
            Rub(L"В дни особого поминовения усопших подают записки на Проскомидию (Литургию) "
                L"и заказывают панихиду или сорокоуст."));
        AddPrayer(s, L"Дни поминовения и родительские субботы", std::move(ps));
        s.prayers.back().group = L"КАЛЕНДАРЬ ПОМИНОВЕНИЯ";
    }

    return s;
}

}  // namespace pom
