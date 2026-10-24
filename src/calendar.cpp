// Точка входа модуля календаря: управление выбранной датой и сборка
// динамических разделов («Православный календарь», «Последование богослужения»,
// «Азбука веры и порталы», «Евангелие и Библия», «Поминальник и сорокоусты»,
// «Иисусова молитва и чётки»).

#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <utility>

#include "calendar.h"
#include "calendar_dates.h"
#include "calendar_day.h"
#include "calendar_init.h"
#include "calendar_sections.h"
#include "online.h"
#include "pominalnik.h"

namespace {

int g_offsetDays = 0;
DayInfo g_curDay;
bool g_dayReady = false;

long TodayJdn() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return JdnGreg(static_cast<int>(st.wYear),
                   static_cast<int>(st.wMonth),
                   static_cast<int>(st.wDay));
}

void EnsureDayInfo() {
    g_curDay = BuildDayInfo(TodayJdn() + g_offsetDays);
    g_dayReady = true;
}

}  // namespace

std::pair<unsigned char, std::wstring> Txt(const wchar_t* s) {
    return std::make_pair(kStyleText, std::wstring(s));
}

std::pair<unsigned char, std::wstring> Txt(std::wstring s) {
    return std::make_pair(kStyleText, std::move(s));
}

std::pair<unsigned char, std::wstring> Rub(const wchar_t* s) {
    return std::make_pair(kStyleRubric, std::wstring(s));
}

std::pair<unsigned char, std::wstring> Rub(std::wstring s) {
    return std::make_pair(kStyleRubric, std::move(s));
}

void AddPrayer(Section& s, const wchar_t* title,
               std::vector<std::pair<unsigned char, std::wstring>> paras) {
    Prayer p;
    p.group.clear();
    p.title = title;
    for (auto& item : paras) {
        PrayerPara para;
        para.text = std::move(item.second);
        para.lang = ParaLang::Ru;
        para.style = item.first;
        p.paras.push_back(std::move(para));
    }
    s.prayers.push_back(std::move(p));
}

void SetCalendarOffsetDays(int offsetDays) {
    g_offsetDays = std::clamp(offsetDays, -36500, 36500);
    EnsureDayInfo();
}

int GetCalendarOffsetDays() {
    return g_offsetDays;
}

const DayInfo& GetCurrentDayInfo() {
    if (!g_dayReady) {
        EnsureDayInfo();
    }
    return g_curDay;
}

std::wstring GetCurrentDateKey() {
    const DayInfo& d = GetCurrentDayInfo();
    wchar_t buf[16];
    wsprintfW(buf, L"%04d-%02d-%02d", d.g.y, d.g.m, d.g.d);
    return buf;
}

void AddCalendarSections(std::vector<Section>& out) {
    EnsureDayInfo();

    out.push_back(BuildKalendarSection(g_curDay));
    out.push_back(BuildSluzhbySection(g_curDay));
    out.push_back(BuildAzbykaSection());
    out.push_back(BuildBibleSection(g_curDay));
    out.push_back(pom::BuildPominalnikSection(g_curDay));
    out.push_back(BuildChetkiSection());
}

void RebuildAllDynamicSections(std::vector<Section>& sections) {
    EnsureDayInfo();
    RefreshDynamicSection(sections, L"kalendar");
    RefreshDynamicSection(sections, L"sluzhby");
    RefreshDynamicSection(sections, L"azbyka");
    RefreshDynamicSection(sections, L"bible");
    RefreshDynamicSection(sections, L"pominalnik");
    RefreshDynamicSection(sections, L"chetki");
}

bool RefreshDynamicSection(std::vector<Section>& sections, const wchar_t* id) {
    EnsureDayInfo();
    for (size_t i = 0; i < sections.size(); ++i) {
        if (sections[i].id == id) {
            if (wcscmp(id, L"kalendar") == 0) {
                sections[i] = BuildKalendarSection(g_curDay);
            } else if (wcscmp(id, L"sluzhby") == 0) {
                sections[i] = BuildSluzhbySection(g_curDay);
            } else if (wcscmp(id, L"azbyka") == 0) {
                sections[i] = BuildAzbykaSection();
            } else if (wcscmp(id, L"bible") == 0) {
                sections[i] = BuildBibleSection(g_curDay);
            } else if (wcscmp(id, L"pominalnik") == 0) {
                sections[i] = pom::BuildPominalnikSection(g_curDay);
            } else if (wcscmp(id, L"chetki") == 0) {
                sections[i] = BuildChetkiSection();
            } else {
                return false;
            }
            return true;
        }
    }
    return false;
}
