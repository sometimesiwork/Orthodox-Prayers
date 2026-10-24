// Поминальник и сорокоусты: списки о живых и усопших, даты поминовения.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <vector>

#include "calendar_dates.h"
#include "calendar_day.h"
#include "prayers.h"

namespace pom {

struct DeadRow {
    std::wstring name;
    long jdn = 0;
};

// Ссылки на списки (изменяемые).
std::vector<std::wstring>& Live();
std::vector<DeadRow>& Dead();

// Разбор/сохранение настроек (settings.ini):
//   pom_live=имя;имя2;...
//   pom_dead=Имя (03.05.2001);Имя2;...
void FromSettings(const std::wstring& live, const std::wstring& dead);
std::wstring SettingsLive();
std::wstring SettingsDead();

// Число записей.
int LiveCount();
int DeadCount();

// Ближайшие дни поминовения: 9-й день, 40-й день, годовщины.
struct MemoryDay {
    long jdn = 0;
    std::wstring label;   // «9-й день — Имя»
};
std::vector<MemoryDay> UpcomingMemoryDays(const struct DayInfo& day, int maxRows);

Section BuildPominalnikSection(const struct DayInfo& day);

// Модальное редактирование списков. true — данные изменены.
bool EditDialog(HWND parent);

}  // namespace pom
