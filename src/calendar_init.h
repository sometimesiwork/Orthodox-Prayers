// Внутренняя служебная шапка модуля календаря.
#pragma once

#include <utility>
#include <vector>

#include "prayers.h"

// Обёртки для абзацев: обычный текст и рубрика (курсив).
std::pair<unsigned char, std::wstring> Txt(const wchar_t* s);
std::pair<unsigned char, std::wstring> Txt(std::wstring s);
std::pair<unsigned char, std::wstring> Rub(const wchar_t* s);
std::pair<unsigned char, std::wstring> Rub(std::wstring s);

// Дописать молитву (карточку) в раздел.
void AddPrayer(Section& s, const wchar_t* title,
               std::vector<std::pair<unsigned char, std::wstring>> paras);

// Подпись уровня трапезы по уровню из FastInfo::level (0–4).
const wchar_t* MealLevelText(int lv);
