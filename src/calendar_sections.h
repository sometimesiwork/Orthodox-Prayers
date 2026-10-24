// Динамические разделы календаря, устава, Евангелия, чёток, поминальника и Азбуки.
#pragma once

#include <vector>

#include "calendar_day.h"
#include "prayers.h"

Section BuildKalendarSection(const DayInfo& day);
Section BuildSluzhbySection(const DayInfo& day);
Section BuildBibleSection(const DayInfo& day);
Section BuildChetkiSection();
Section BuildAzbykaSection();

namespace chetki {
int Count();
int Target();
void Step(int delta);
void Reset();
void CycleTarget();
void Load(int count, int target);   // восстановление счёта из настроек
}  // namespace chetki
