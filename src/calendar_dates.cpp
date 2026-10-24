#include "calendar_dates.h"

#include <windows.h>

long JdnJul(int y, int m, int d) {
    const long a = (14 - m) / 12;
    const long y1 = y + 4800 - a;
    const long m1 = m + 12 * a - 3;
    return d + (153 * m1 + 2) / 5 + 365 * y1 + y1 / 4 - 32083;
}

long JdnGreg(int y, int m, int d) {
    const long a = (14 - m) / 12;
    const long y1 = y + 4800 - a;
    const long m1 = m + 12 * a - 3;
    return d + (153 * m1 + 2) / 5 + 365 * y1 + y1 / 4 - y1 / 100 + y1 / 400 - 32045;
}

YMD FromJdnGreg(long jdn) {
    const long a = jdn + 32044;
    const long b = (4 * a + 3) / 146097;
    const long c = a - (146097 * b) / 4;
    const long d = (4 * c + 3) / 1461;
    const long e = c - (1461 * d) / 4;
    const long m = (5 * e + 2) / 153;
    YMD r;
    r.d = static_cast<int>(e - (153 * m + 2) / 5 + 1);
    r.m = static_cast<int>(m + 3 - 12 * (m / 10));
    r.y = static_cast<int>(100 * b + d - 4800 + m / 10);
    return r;
}

YMD FromJdnJul(long jdn) {
    const long t = jdn + 32082;          // дней от 1 марта года -4800 (юн.)
    const long y1 = (4 * t + 3) / 1461;  // «мартовских» лет
    const long e = t - (1461 * y1) / 4;  // день внутри марта-года (0 = 1 март)
    const long m = (5 * e + 2) / 153;    // 0 = март, 1 = апрель, ... 11 = февраль
    YMD r;
    r.d = static_cast<int>(e - (153 * m + 2) / 5 + 1);
    r.m = static_cast<int>(m + 3 - 12 * (m / 10));
    r.y = static_cast<int>(y1 - 4800 + m / 10);
    return r;
}

long PaschaJdn(int year) {
    const long a = year % 4;
    const long b = year % 7;
    const long c = year % 19;
    const long d = (19 * c + 15) % 30;
    const long e = (2 * a + 4 * b - d + 34) % 7;
    const long m = (d + e + 114) / 31;  // 3 = март, 4 = апрель (юл.)
    const long day = ((d + e + 114) % 31) + 1;
    return JdnJul(year, static_cast<int>(m), static_cast<int>(day));
}

// 0 = воскресенье, 1 = понедельник, ..., 6 = суббота
// (JDN кратно 7 выпадает на понедельник)
int DowFromJdn(long jdn) {
    return static_cast<int>((jdn + 1) % 7);
}

const wchar_t* kMonthNames[] = {
    L"января",   L"февраля",  L"марта",    L"апреля",
    L"мая",      L"июня",     L"июля",     L"августа",
    L"сентября", L"октября",  L"ноября",   L"декабря"};

const wchar_t* kWeekdayNames[] = {
    L"воскресенье", L"понедельник", L"вторник", L"среда",
    L"четверг",     L"пятница",     L"суббота"};

std::wstring DateRu(const YMD& d) {
    return std::to_wstring(d.d) + L" " + kMonthNames[d.m - 1] + L" " +
           std::to_wstring(d.y);
}
