// Онлайн-блок: загрузка полного календаря дня с azbyka.ru (WinHTTP + кэш).
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <vector>

#include "prayers.h"

namespace online {

struct TroparBlock {
    std::wstring group;       // кому (напр. «Преподобному Амвросию Оптинскому»)
    std::wstring title;       // «Тропарь, глас 5»
    std::wstring cs;          // церковнославянский текст
    std::wstring ru;          // русский перевод
};

struct Data {
    std::wstring dateKey;     // «2026-10-23»
    std::wstring saints;      // полный список святых дня с сайта
    std::wstring readings;    // чтения Священного Писания дня
    std::wstring quote;       // цитата дня
    std::wstring parable;     // притча дня
    std::wstring osnovy;      // основы православия (тема дня)
    std::wstring sovet;       // практический совет дня
    std::wstring prologue;    // пролог в поучениях
    std::wstring history;     // этот день в истории
    std::vector<TroparBlock> tropari;  // тропари, кондаки и молитвы дня с сайта

    std::wstring status;
    bool busy = false;
    bool loaded = false;
    bool failed = false;
};

const std::wstring& DateKey();
const Data& Get();

// Загрузка кэша для указанной даты YYYY-MM-DD (пусто = сегодня).
void LoadCacheForDate(const std::wstring& dateKey = L"");

// Запуск фонового потока загрузки для указанной даты YYYY-MM-DD (пусто = сегодня).
void StartFetch(HWND hwnd, const std::wstring& dateKey = L"", bool force = false);

// Сообщение-сигнал об окончании загрузки.
constexpr UINT kOnlineDoneMsg = WM_APP + 7;

}  // namespace online
