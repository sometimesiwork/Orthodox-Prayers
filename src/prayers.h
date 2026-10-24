// Структуры данных молитв.
#pragma once

#include <string>
#include <vector>

enum class ParaLang : unsigned char {
    Cs = 0,    // церковнославянский
    Ru = 1,    // русский перевод
    Both = 2,  // смешанный
};

// Стиль абзаца: 0 — обычный текст, 1 — пояснение (курсив), 2 — стихотворная строка.
constexpr unsigned char kStyleText = 0;
constexpr unsigned char kStyleRubric = 1;
constexpr unsigned char kStyleVerse = 2;

struct PrayerPara {
    std::wstring text;
    ParaLang lang = ParaLang::Ru;
    unsigned char style = kStyleText;
};

struct Prayer {
    std::wstring group;
    std::wstring title;
    std::vector<PrayerPara> paras;
    std::wstring url;  // карточка-ссылка (открывается в браузере) — опционально
};

struct Section {
    std::wstring id;
    std::wstring title;
    std::wstring shortTitle;  // название для бокового меню
    std::wstring group;       // группа в боковом меню
    std::wstring desc;
    std::vector<Prayer> prayers;
};

const std::vector<Section>& LoadSections();

std::wstring Utf8ToWide(const char* s);
std::wstring Utf8ToWide(const std::string& s);
