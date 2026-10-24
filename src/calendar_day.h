// Расчёт одного богослужебного дня: Пасхалия, седмицы, глас, пост, облачения.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <vector>

#include "calendar_dates.h"

struct FastInfo {
    // Уровень трапезы:
    //  0 — без ограничений;
    //  1 — без мяса и молочного;
    //  2 — растительная пища без масла;
    //  3 — сухоядение;
    //  4 — полное воздержание от пищи до литургии.
    int level = 0;
    std::wstring note;
    bool isFast = false;       // постный день вообще
    bool continuous = false;   // «сплошная седмица», пост отменён
};

struct SaintRow {
    std::wstring name;
    unsigned char rank = 0;
};

struct FeastRow {
    std::wstring name;
    long jdn = 0;
    int rank = 0;
};

struct DayInfo {
    YMD g;      // дата по новому стилю
    YMD j;      // дата по старому стилю
    long jdn = 0;
    int dow = 0;              // 0 = воскресенье ... 6 = суббота
    bool isSunday = false;
    bool isSaturday = false;

    long pascha = 0;          // JDN Пасхи активного литургического года
    int paschaYear = 0;
    long dPascha = 0;         // дней от этой Пасхи (может быть < 0)

    int glas = 0;             // 1..8; 0 — глас не воспевается
    int sedmNum = 0;
    int matinsGospel = 0;     // 1..11 — номер воскресного утреннего Евангелия
    std::wstring period;      // «Седмица 21-я по Пятидесятнице» и др.

    // Богослужебный цвет облачений:
    // 0 — золотой, 1 — голубой, 2 — красный, 3 — зелёный, 4 — фиолетовый, 5 — белый
    int vestmentColor = 0;
    std::wstring vestmentName;

    bool inBrightWeek = false;
    bool inPassionWeek = false;
    bool inGreatLent = false; // будничные дни Великого поста
    bool inTriodion = false;  // вся Триодь (от мытаря до Вербного)

    long palmSun = 0, ascen = 0, pentecost = 0, allSaints = 0;
    long triodionStart = 0, lentStart = 0;

    FastInfo fast;
    std::vector<SaintRow> saints;
    bool polyeleos = false;
};

// Полная сборка структуры DayInfo на произвольный JDN (или 0 = сегодня).
DayInfo BuildDayInfo(long targetJdn = 0);

// Заполняет всё, кроме дат: вызывать после заполнения jdn/g/j/dow и pascha.
void ComputeLiturgicalFields(DayInfo& day);

// Подвижные праздники активной Пасхи.
void SetSeasonDates(DayInfo& day);

// Пост/трапеза (нужны saints и season-даты).
FastInfo ComputeFast(const DayInfo& day);

// Ближайшие праздники вперёд (не более maxRows).
std::vector<FeastRow> UpcomingFeasts(const DayInfo& day, int maxRows);
