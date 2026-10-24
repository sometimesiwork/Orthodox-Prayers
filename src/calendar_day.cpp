#include "calendar_day.h"

#include <algorithm>

namespace {

std::wstring Ordinal(int n) {
    return std::to_wstring(n) + L"-я";
}

void ComputeVestmentColor(DayInfo& day) {
    // 0 — золотой, 1 — голубой, 2 — красный, 3 — зелёный, 4 — фиолетовый, 5 — белый
    const long j = day.jdn;
    const long p = day.pascha;
    const int jm = day.j.m;
    const int jd = day.j.d;

    if (day.inBrightWeek || (j >= p && j < day.ascen && day.isSunday && j == p)) {
        day.vestmentColor = 2;
        day.vestmentName = L"Красный (пасхальный)";
        return;
    }
    if (j == day.ascen || (jm == 12 && jd == 25) || (jm == 1 && jd == 6) ||
        (jm == 8 && jd == 6)) {
        day.vestmentColor = 5;
        day.vestmentName = L"Белый (господский праздник)";
        return;
    }
    if (j == day.pentecost || j == day.pentecost + 1 || j == day.palmSun) {
        day.vestmentColor = 3;
        day.vestmentName = L"Зелёный (праздничный)";
        return;
    }
    if ((jm == 9 && jd == 14) || j == p - 21) {
        day.vestmentColor = 4;
        day.vestmentName = L"Фиолетовый (Кресту Господню)";
        return;
    }
    // Богородичные праздники
    if ((jm == 9 && jd == 8) || (jm == 10 && jd == 1) || (jm == 11 && jd == 21) ||
        (jm == 2 && jd == 2) || (jm == 3 && jd == 25) || (jm == 8 && jd == 15) ||
        (jm == 7 && jd == 8) || (jm == 10 && jd == 22) || (jm == 11 && jd == 8)) {
        day.vestmentColor = 1;
        day.vestmentName = L"Голубой (Богородичный / ангельский)";
        return;
    }
    if (day.inPassionWeek) {
        if (day.dow == 4) {
            day.vestmentColor = 2;
            day.vestmentName = L"Тёмно-красный (Великий Четверг)";
        } else if (day.dow == 6) {
            day.vestmentColor = 5;
            day.vestmentName = L"Тёмный / Белый (Великая Суббота)";
        } else {
            day.vestmentColor = 4;
            day.vestmentName = L"Тёмный / фиолетовый (Страстная седмица)";
        }
        return;
    }
    if (day.inGreatLent) {
        day.vestmentColor = 4;
        day.vestmentName = L"Фиолетовый / тёмный (Великий пост)";
        return;
    }
    if (!day.saints.empty() && day.saints.front().rank >= 2) {
        const std::wstring& nm = day.saints.front().name;
        if (nm.find(L"Богородиц") != std::wstring::npos ||
            nm.find(L"Божией Матери") != std::wstring::npos ||
            nm.find(L"Покров") != std::wstring::npos) {
            day.vestmentColor = 1;
            day.vestmentName = L"Голубой (Богородичный)";
            return;
        }
        if (nm.find(L"Преподоб") != std::wstring::npos ||
            nm.find(L"преподоб") != std::wstring::npos ||
            nm.find(L"блажен") != std::wstring::npos) {
            day.vestmentColor = 3;
            day.vestmentName = L"Зелёный (преподобнический)";
            return;
        }
        if (nm.find(L"Мучени") != std::wstring::npos ||
            nm.find(L"мучени") != std::wstring::npos ||
            nm.find(L"Великомуч") != std::wstring::npos ||
            nm.find(L"Усекновен") != std::wstring::npos) {
            day.vestmentColor = 2;
            day.vestmentName = L"Красный (мученический)";
            return;
        }
    }
    day.vestmentColor = 0;
    day.vestmentName = L"Золотой (жёлтый, царский)";
}

}  // namespace

void SetSeasonDates(DayInfo& day) {
    const long p = day.pascha;
    day.palmSun = p - 7;
    day.ascen = p + 39;
    day.pentecost = p + 49;
    day.allSaints = p + 56;
    day.triodionStart = p - 70;
    day.lentStart = p - 48;

    const long d = day.jdn - p;
    day.dPascha = d;

    day.inBrightWeek = (d >= 0 && d <= 6);
    day.inPassionWeek = (d >= -6 && d < 0);
    day.inTriodion = (day.jdn >= day.triodionStart && day.jdn < day.pascha);
    day.inGreatLent =
        (day.jdn >= day.lentStart && day.jdn < day.pascha && !day.inPassionWeek &&
         !day.inBrightWeek && !day.isSunday);
}

void ComputeLiturgicalFields(DayInfo& day) {
    SetSeasonDates(day);
    const long p = day.pascha;

    if (day.inBrightWeek || day.jdn == day.pentecost) {
        day.glas = 0;
    } else {
        const long owner = day.jdn - day.dow;
        const long base =
            owner >= p + 7 ? p + 7 : PaschaJdn(day.paschaYear - 1) + 7;
        long w = (owner - base) / 7;
        w = ((w % 8) + 8) % 8;
        day.glas = static_cast<int>(w) + 1;
    }

    const long ownerMonday = day.jdn - ((day.dow + 6) % 7);
    const wchar_t* word = day.isSunday ? L"Неделя " : L"Седмица ";
    day.matinsGospel = 0;

    if (day.inBrightWeek) {
        day.period = (day.jdn == p) ? L"Светлое Христово Воскресение. ПАСХА"
                                    : L"Светлая седмица";
        day.sedmNum = 1;
    } else if (day.inPassionWeek) {
        static const wchar_t* kPassionDays[] = {
            L"Страстная седмица", L"Великий Понедельник", L"Великий Вторник",
            L"Великая Среда",     L"Великий Четверг",     L"Великая Пятница",
            L"Великая Суббота"};
        day.period = kPassionDays[std::clamp(day.dow, 0, 6)];
        day.sedmNum = 7;
    } else if (day.jdn == day.palmSun) {
        day.period = L"Вербное воскресенье: Вход Господень в Иерусалим";
        day.sedmNum = 6;
    } else if (day.jdn == p - 8) {
        day.period = L"Лазарева суббота (воскрешение праведного Лазаря)";
        day.sedmNum = 6;
    } else if (day.jdn >= day.lentStart && day.jdn < day.palmSun) {
        const long wk = (day.jdn - day.lentStart) / 7 + 1;
        day.sedmNum = static_cast<int>(wk);
        day.period = std::wstring(word) + Ordinal(static_cast<int>(wk)) +
                     L" Великого поста";
        if (day.isSunday) {
            static const wchar_t* kLentSundays[] = {
                L"",
                L" — Торжество Православия",
                L" — свт. Григория Паламы",
                L" — Крестопоклонная",
                L" — прп. Иоанна Лествичника",
                L" — прп. Марии Египетской"};
            if (wk >= 1 && wk <= 5) {
                day.period += kLentSundays[wk];
            }
        }
    } else if (day.inTriodion) {
        if (day.jdn < p - 63) {
            day.period = day.isSunday ? L"Неделя о мытаре и фарисее"
                                      : L"Седмица о мытаре и фарисее (сплошная)";
        } else if (day.jdn < p - 56) {
            day.period = day.isSunday ? L"Неделя о блудном сыне"
                                      : L"Седмица о блудном сыне";
        } else if (day.jdn < p - 49) {
            day.period = day.isSunday
                             ? L"Мясопустная Неделя о Страшном Суде"
                             : L"Мясопустная седмица";
        } else {
            day.period = day.isSunday
                             ? L"Сыропустная Неделя (Прощёное воскресенье)"
                             : L"Сыропустная седмица (Масленица)";
        }
    } else if (day.jdn == day.pentecost) {
        day.period = L"День Святой Троицы: Пятидесятница";
    } else if (day.jdn > day.pentecost && day.jdn < day.allSaints) {
        day.period = (day.jdn == day.pentecost + 1)
                         ? L"День Святого Духа (Троицкая седмица)"
                         : L"Троицкая седмица (сплошная)";
        day.sedmNum = 1;
    } else if (day.jdn >= day.allSaints) {
        const long pentMon = day.pentecost + 1;
        long w = (ownerMonday - pentMon) / 7 + 1;
        if (w < 1) w = 1;
        day.sedmNum = static_cast<int>(w);
        day.period = std::wstring(word) + Ordinal(static_cast<int>(w)) +
                     L" по Пятидесятнице";
        if (day.jdn == day.allSaints) {
            day.period += L" (Всех святых)";
        } else if (day.jdn == day.allSaints + 7) {
            day.period += L" (Всех святых, в земле Русской просиявших)";
        }
        if (day.isSunday) {
            day.matinsGospel = static_cast<int>(((w - 1) % 11) + 1);
        }
    } else {
        const long wk = day.isSunday ? (day.jdn - p) / 7 + 1
                                     : (ownerMonday - (p + 1)) / 7 + 1;
        day.sedmNum = static_cast<int>(wk);
        std::wstring label = std::wstring(word) + Ordinal(static_cast<int>(wk)) +
                             L" по Пасхе";
        if (day.isSunday) {
            static const wchar_t* names[] = {
                L"",
                L"",
                L" — Антипасха (апостола Фомы)",
                L" — святых жён-мироносиц",
                L" — о расслабленном",
                L" — о самарянке",
                L" — о слепом",
                L" — святых отцев I Вселенского Собора"};
            static const int kPentGospels[] = {0, 0, 1, 3, 4, 7, 8, 10};
            const int idx = wk >= 0 && wk <= 7 ? static_cast<int>(wk) : 0;
            if (names[idx][0] != L'\0') {
                label += names[idx];
            }
            day.matinsGospel = kPentGospels[idx];
        }
        day.period = std::move(label);
    }
}

DayInfo BuildDayInfo(long targetJdn) {
    DayInfo day;
    if (targetJdn <= 0) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        targetJdn = JdnGreg(static_cast<int>(st.wYear),
                            static_cast<int>(st.wMonth),
                            static_cast<int>(st.wDay));
    }
    day.jdn = targetJdn;
    day.g = FromJdnGreg(day.jdn);
    day.j = FromJdnJul(day.jdn);
    day.dow = DowFromJdn(day.jdn);
    day.isSunday = (day.dow == 0);
    day.isSaturday = (day.dow == 6);

    const long litPascha = PaschaJdn(day.g.y);
    day.paschaYear = (day.jdn < litPascha - 70) ? day.g.y - 1 : day.g.y;
    day.pascha = PaschaJdn(day.paschaYear);
    ComputeLiturgicalFields(day);

    const int julY = day.j.y;
    for (int y = julY - 1; y <= julY + 1; ++y) {
        for (int i = 0; i < MenaionCount(); ++i) {
            const MenaionRow& r = *(MenaionData() + i);
            if (JdnJul(y, r.m, r.d) == day.jdn) {
                SaintRow sr;
                sr.name = r.name;
                sr.rank = r.rank;
                day.saints.push_back(std::move(sr));
            }
        }
    }
    std::sort(day.saints.begin(), day.saints.end(),
              [](const SaintRow& a, const SaintRow& b) { return a.rank > b.rank; });
    day.polyeleos =
        day.isSunday || (!day.saints.empty() && day.saints.front().rank >= 2);
    day.fast = ComputeFast(day);
    ComputeVestmentColor(day);
    return day;
}

FastInfo ComputeFast(const DayInfo& day) {
    FastInfo f;
    const long j = day.jdn;
    const int year = day.j.y;
    const auto JJ = [year](int mm, int dd) { return JdnJul(year, mm, dd); };
    const bool wedFri = (day.dow == 3 || day.dow == 5);

    // Сплошные седмицы: Светлая, Троицкая, о мытаре и фарисее, Святки
    if ((j >= day.pascha && j < day.pascha + 7) ||
        (j >= day.pentecost && j < day.allSaints) ||
        (j >= day.pascha - 69 && j <= day.pascha - 64)) {
        f.continuous = true;
        f.note = L"Сплошная седмица — пост в среду и пятницу отменён";
        return f;
    }

    // Сыропустная седмица (Масленица)
    if (j >= day.pascha - 55 && j <= day.pascha - 49) {
        f.continuous = true;
        f.level = 0;
        f.note = L"Сыропустная седмица (Масленица): вкушаются молочные продукты, яйца и рыба, мясо не вкушается";
        return f;
    }

    // Святки (от Рождества 25 дек. до 4 янв. по ст. ст.)
    if ((day.j.m == 12 && day.j.d >= 25) || (day.j.m == 1 && day.j.d <= 4)) {
        f.continuous = true;
        f.note = L"Святки (от Рождества Христова до навечерия Богоявления) — сплошная седмица";
        return f;
    }
    if (day.j.m == 1 && day.j.d == 5) {
        f.isFast = true;
        f.level = 3;
        f.note = L"Крещенский сочельник (Навечерие Богоявления) — строгий пост";
        return f;
    }
    // Строгие постные праздники: Воздвижение Креста (14.09 ст. ст.) и Усекновение (29.08 ст. ст.)
    if ((day.j.m == 9 && day.j.d == 14) || (day.j.m == 8 && day.j.d == 29)) {
        f.isFast = true;
        f.level = 1;
        f.note = L"Строгий постный праздник (растительная пища с маслом, без рыбы)";
        return f;
    }

    // Великий пост и Страстная седмица
    if (j >= day.lentStart && j < day.pascha) {
        f.isFast = true;
        if (j >= day.pascha - 6) {
            f.note = L"Страстная седмица — особо строгий пост";
            f.level = 3;
            if (day.dow == 5) {
                f.level = 4;
                f.note = L"Великая Пятница: полное воздержание от пищи до выноса Плащаницы";
            }
            return f;
        }
        if (day.dow == 1 || day.dow == 3 || day.dow == 5) {
            f.level = 3;
            f.note = L"Великий пост: сухоядение (по монастырскому уставу)";
        } else if (day.dow == 2 || day.dow == 4) {
            f.level = 2;
            f.note = L"Великий пост: горячая растительная пища без масла";
        } else {
            f.level = 1;
            f.note = L"Великий пост (суббота/воскресенье): горячая пища с растительным маслом";
        }
        const int py = FromJdnJul(day.pascha).y;
        if (j == JdnJul(py, 3, 25)) {
            f.level = 1;
            f.note = L"Благовещение Пресвятой Богородицы — разрешается рыба и растительное масло";
        } else if (j == day.palmSun) {
            f.level = 1;
            f.note = L"Вербное воскресенье (Вход Господень в Иерусалим) — разрешается рыба";
        } else if (j == day.pascha - 8) {
            f.level = 1;
            f.note = L"Лазарева суббота — разрешается рыбная икра и растительное масло";
        }
        return f;
    }

    // Петров (Апостольский) пост
    const long petStart = day.allSaints + 1;
    const long petEnd = JJ(6, 28);
    if (j >= petStart && j <= petEnd) {
        f.isFast = true;
        if (wedFri) {
            f.level = 2;
            f.note = L"Петров пост (среда/пятница — без рыбы)";
        } else {
            f.level = 1;
            f.note = L"Петров пост (разрешается рыба и растительное масло)";
        }
        return f;
    }

    // Успенский пост (1–14 августа ст. ст.)
    if (day.j.m == 8 && day.j.d >= 1 && day.j.d <= 14) {
        f.isFast = true;
        if (day.j.d == 6) {
            f.level = 1;
            f.note = L"Преображение Господне — в праздник разрешается рыба";
        } else if (day.dow == 1 || day.dow == 3 || day.dow == 5) {
            f.level = 3;
            f.note = L"Успенский пост: сухоядение";
        } else if (day.dow == 2 || day.dow == 4) {
            f.level = 2;
            f.note = L"Успенский пост: горячая пища без масла";
        } else {
            f.level = 1;
            f.note = L"Успенский пост (суббота/воскресенье): пища с растительным маслом";
        }
        return f;
    }

    // Рождественский (Филиппов) пост (15 ноября – 24 декабря ст. ст.)
    if ((day.j.m == 11 && day.j.d >= 15) || (day.j.m == 12 && day.j.d <= 24)) {
        f.isFast = true;
        if (day.j.m == 12 && day.j.d == 24) {
            f.level = 3;
            f.note = L"Рождественский сочельник — строгий пост (сухоядение / сочиво)";
        } else if (day.j.m == 12 && day.j.d >= 20) {
            f.level = wedFri ? 3 : 2;
            f.note = L"Рождественский пост (предпразднство Рождества — строгие дни без рыбы)";
        } else {
            f.level = wedFri ? 2 : 1;
            f.note = wedFri ? L"Рождественский пост (среда/пятница — без рыбы)"
                            : L"Рождественский пост (разрешается рыба и масло)";
        }
        return f;
    }

    // Обычные среды и пятницы в течение года
    if (wedFri) {
        bool lighter = false;
        for (const SaintRow& sr : day.saints) {
            if (sr.rank >= 2) {
                lighter = true;
                break;
            }
        }
        f.isFast = true;
        if (lighter) {
            f.level = 1;
            f.note = L"Постный день (среда/пятница) с послаблением в честь праздника";
        } else {
            f.level = 2;
            f.note = L"Постный день (среда и пятница): воздержание от скоромной пищи";
        }
        return f;
    }

    f.level = 0;
    f.note = L"Поста нет";
    return f;
}

std::vector<FeastRow> UpcomingFeasts(const DayInfo& day, int maxRows) {
    std::vector<FeastRow> out;
    auto TryAdd = [&](const std::wstring& name, long jdn, int rank) {
        if (jdn < day.jdn) return;
        FeastRow row;
        row.name = name;
        row.jdn = jdn;
        row.rank = rank;
        out.push_back(row);
    };

    const int y0 = day.j.y;
    for (int y = y0; y <= y0 + 1; ++y) {
        for (int i = 0; i < MenaionCount(); ++i) {
            const MenaionRow& r = *(MenaionData() + i);
            if (r.rank < 2) continue;
            TryAdd(r.name, JdnJul(y, r.m, r.d), r.rank);
        }
    }
    for (int py = day.g.y; py <= day.g.y + 1; ++py) {
        const long p = PaschaJdn(py);
        TryAdd(L"Лазарева суббота", p - 8, 2);
        TryAdd(L"Вход Господень в Иерусалим (Вербное воскресенье)", p - 7, 3);
        TryAdd(L"Светлое Христово Воскресение (Пасха)", p, 3);
        TryAdd(L"Вознесение Господне", p + 39, 3);
        TryAdd(L"День Святой Троицы (Пятидесятница)", p + 49, 3);
    }

    std::sort(out.begin(), out.end(),
              [](const FeastRow& a, const FeastRow& b) { return a.jdn < b.jdn; });
    if (static_cast<int>(out.size()) > maxRows) {
        out.resize(static_cast<size_t>(maxRows));
    }
    return out;
}
