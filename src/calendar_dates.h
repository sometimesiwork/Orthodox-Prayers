// Календарные утилиты: юлианские дни (JDN), пасхалия, месяцеслов.
#pragma once

#include <string>

// Gregorian/Julian date holder.
struct YMD {
    int y = 0, m = 0, d = 0;
};

// Julian calendar <-> JDN.
long JdnJul(int y, int m, int d);

// Gregorian calendar <-> JDN.
long JdnGreg(int y, int m, int d);

YMD FromJdnGreg(long jdn);
YMD FromJdnJul(long jdn);

// Julian (Orthodox) paschal computation: JDN of Holy Pascha for a given year.
long PaschaJdn(int year);

// 0 = Sunday, 1 = Monday, ..., 6 = Saturday.
int DowFromJdn(long jdn);

// Date in words: "10 октября 2026".
std::wstring DateRu(const YMD& d);

extern const wchar_t* kMonthNames[];
extern const wchar_t* kWeekdayNames[];

// —— Месяцеслов (неподвижный круг, старый стиль) ——
// rank: 3 — великий/двунадесятый; 2 — полиелейный; 1 — славословный;
// 0 — общеизвестная память.
struct MenaionRow {
    int m, d;
    const wchar_t* name;
    unsigned char rank;
};

int MenaionCount();
const MenaionRow* MenaionData();
