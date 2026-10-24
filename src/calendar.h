// Православный календарь, устав, Евангелие, поминальник, чётки и Азбука веры.
#pragma once

#include <string>
#include <vector>

#include "calendar_day.h"
#include "calendar_sections.h"
#include "prayers.h"

// Смещение выбранной даты в днях относительно сегодняшнего дня (0 = сегодня).
void SetCalendarOffsetDays(int offsetDays);
int GetCalendarOffsetDays();

// Полная информация о выбранном дне.
const DayInfo& GetCurrentDayInfo();

// Дата выбранного дня в формате YYYY-MM-DD (для запросов к azbyka.ru).
std::wstring GetCurrentDateKey();

// Добавляет динамические разделы в общий список разделов.
void AddCalendarSections(std::vector<Section>& out);

// Пересобирает все динамические разделы (при смене даты или получении онлайн-данных).
void RebuildAllDynamicSections(std::vector<Section>& sections);

// Пересобирает конкретный динамический раздел по его id.
bool RefreshDynamicSection(std::vector<Section>& sections, const wchar_t* id);
