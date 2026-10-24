// Православные молитвы — настольное приложение (Win32 + GDI+).
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "prayers.h"
#include "theme.h"
#include "calendar.h"
#include "pominalnik.h"
#include "online.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

namespace {

constexpr int IDC_SEARCH = 1001;
constexpr UINT_PTR TIMER_ANIM = 1;

constexpr int KIND_GROUP = 0;
constexpr int KIND_TITLE = 1;
constexpr int KIND_TEXT = 2;
constexpr int KIND_RUBRIC = 3;
constexpr int KIND_DIVIDER = 4;
constexpr int KIND_VERSE = 5;

struct ViewItem {
    int section = 0;
    int prayer = 0;
    std::wstring group;
};

struct Block {
    int kind = KIND_TEXT;
    int item = -1;
    int para = -1;
    std::wstring text;
    float y = 0;
    float h = 0;
    int firstLine = 0;
    int lineCount = 0;
};

struct Word {
    int block = 0;
    int start = 0;
    int len = 0;
    float x = 0;
    float w = 0;
};

struct Line {
    int block = 0;
    int firstWord = 0;
    int count = 0;
    float y = 0;
    float h = 0;
    float w = 0;
};

struct Card {
    int item = 0;
    float y = 0;
    float h = 0;
    float starX = 0;
    float starY = 0;
    int firstBlock = 0;
    int blockCount = 0;
};

// ---------- Строки и текст ----------

std::wstring Lower(std::wstring s) {
    if (s.empty()) {
        return s;
    }
    // towlower() в C-локали не понижает регистр кириллицы, поэтому используем
    // системную таблицу регистров Windows (важно для поиска «Псалом» = «псалом»).
    const int n = LCMapStringW(LOCALE_USER_DEFAULT, LCMAP_LOWERCASE, s.c_str(),
                               static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        for (wchar_t& c : s) {
            c = static_cast<wchar_t>(towlower(c));
        }
        return s;
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    LCMapStringW(LOCALE_USER_DEFAULT, LCMAP_LOWERCASE, s.c_str(),
                 static_cast<int>(s.size()), &out[0], n);
    return out;
}

bool ContainsNoCase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) {
        return false;
    }
    return Lower(haystack).find(Lower(needle)) != std::wstring::npos;
}

std::wstring Trim(const std::wstring& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && iswspace(s[b])) {
        ++b;
    }
    while (e > b && iswspace(s[e - 1])) {
        --e;
    }
    return s.substr(b, e - b);
}

std::wstring UpperLetters(const std::wstring& s) {
    std::wstring out = s;
    for (wchar_t& c : out) {
        if (c >= L'а' && c <= L'я') {
            c = static_cast<wchar_t>(c - 32);
        }
    }
    return out;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) {
        return std::string();
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len, nullptr,
                        nullptr);
    return out;
}

const wchar_t* kFontFamilies[] = {L"Cambria", L"Georgia", L"Constantia", L"Times New Roman",
                                   L"Palatino Linotype"};
constexpr int kFontFamilyCount = 5;

const wchar_t* kMonths[] = {L"января",   L"февраля",  L"марта",   L"апреля",  L"мая",     L"июня",
                             L"июля",     L"августа",  L"сентября", L"октября", L"ноября", L"декабря"};
const wchar_t* kWeekdays[] = {L"воскресенье", L"понедельник", L"вторник",    L"среда",
                              L"четверг",      L"пятница",     L"суббота"};

std::wstring DateString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return std::to_wstring(st.wDay) + L" " + kMonths[st.wMonth - 1] + L" " +
           std::to_wstring(st.wYear) + L", " + kWeekdays[st.wDayOfWeek];
}

std::wstring Greeting() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    const int hour = st.wHour;
    std::wstring word;
    if (hour >= 4 && hour < 11) {
        word = L"Доброе утро";
    } else if (hour >= 11 && hour < 17) {
        word = L"Добрый день";
    } else if (hour >= 17 && hour < 23) {
        word = L"Добрый вечер";
    } else {
        word = L"Доброй ночи";
    }
    return word;
}

}  // namespace

std::wstring Utf8ToWide(const char* s) {
    if (s == nullptr || *s == '\0') {
        return std::wstring();
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), len);
    out.resize(static_cast<size_t>(len - 1));
    return out;
}

std::wstring Utf8ToWide(const std::string& s) { return Utf8ToWide(s.c_str()); }

// Строгая проверка, являются ли байты корректной UTF-8-строкой.
bool TryDecodeUtf8(const std::string& in, std::wstring& out) {
    const int flags = MB_ERR_INVALID_CHARS;
    const int n = MultiByteToWideChar(CP_UTF8, flags, in.data(),
                                      static_cast<int>(in.size()), nullptr, 0);
    if (n <= 0) {
        return false;
    }
    out.resize(static_cast<size_t>(n));
    MultiByteToWideChar(CP_UTF8, flags, in.data(), static_cast<int>(in.size()),
                        out.data(), n);
    return true;
}

// Сигнатура двойного кодирования: «UTF-8-байты, прочитанные как CP1251» дают
// последовательности вида 'Р'/'С' + управляющий символ U+0080..U+00A0.
bool HasUtf8Mojibake(const std::wstring& t) {
    for (size_t i = 0; i + 1 < t.size(); ++i) {
        if ((t[i] == L'\u0420' || t[i] == L'\u0421') &&
            t[i + 1] >= L'\u0080' && t[i + 1] <= L'\u00A0') {
            return true;
        }
    }
    for (wchar_t c : t) {
        if (c == L'\u0098' || c == L'\u009D') {
            return true;
        }
    }
    return false;
}

// «Лечение»: широкую строку-мохибейк превращаем обратно в байты CP1251 и
// заново декодируем как UTF-8 (именно так текст был испорчен).
std::wstring RepairUtf8Mojibake(const std::wstring& t) {
    const UINT encs[] = {1251, 0};
    for (UINT enc : encs) {
        const int n = WideCharToMultiByte(enc, 0, t.data(), static_cast<int>(t.size()),
                                          nullptr, 0, nullptr, nullptr);
        if (n <= 0) {
            continue;
        }
        std::string bytes(static_cast<size_t>(n), '\0');
        WideCharToMultiByte(enc, 0, t.data(), static_cast<int>(t.size()), bytes.data(),
                            n, nullptr, nullptr);
        std::wstring fixed;
        if (TryDecodeUtf8(bytes, fixed)) {
            if (!HasUtf8Mojibake(fixed)) {
                return fixed;
            }
        }
    }
    return t;
}

// ------------------------------------------------------------- Popup-календарь

// Сообщение от popup-календаря в главное окно:
// wp = год (0 = «вернуться на сегодня»), lp = MAKELPARAM(месяц, день).
constexpr UINT WM_DATE_PICKED = WM_APP + 9;

constexpr wchar_t kDatePopupClass[] = L"OPDatePopup";

// Собственный мини-календарь на GDI+ (системный MonthCal рассинхронизирует
// карту попаданий после MCM_SETCURSEL, поэтому дату выбираем своей отрисовкой).
struct DatePopupState {
    HWND owner = nullptr;
    float scale = 1.0f;
    // метрика
    int padX = 0, titleH = 0, weekH = 0, cellW = 0, cellH = 0;
    int cellsY = 0, footerTop = 0, clientW = 0, clientH = 0;
    // отображаемый месяц
    int viewYear = 0, viewMonth = 0;
    // цвета (ARGB)
    DWORD bg = 0, border = 0, hover = 0, accent = 0, text = 0, textDim = 0;
    // интерактив
    int hoverCell = -1;  // 0..41
    bool hoverPrev = false;
    bool hoverNext = false;
    bool hoverToday = false;
};

struct DatePopupInit {
    HWND owner = nullptr;
    float scale = 1.0f;
    int year = 0, month = 0;  // открываем календарь на этом месяце
    DWORD bg = 0, border = 0, hover = 0, accent = 0, text = 0, textDim = 0;
};

constexpr int kPopupRows = 6;

const wchar_t* kPopupMonths[] = {L"Январь",   L"Февраль", L"Март",    L"Апрель",
                                 L"Май",      L"Июнь",    L"Июль",    L"Август",
                                 L"Сентябрь", L"Октябрь", L"Ноябрь",  L"Декабрь"};
const wchar_t* kPopupWeekdays[] = {L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс"};

Gdiplus::Color ColArgb(DWORD v) {
    Gdiplus::Color c;
    c.SetValue(v);
    return c;
}

// Тот же цвет с другой прозрачностью.
Gdiplus::Color WithAlpha(const Gdiplus::Color& c, BYTE a) {
    return Gdiplus::Color(a, c.GetR(), c.GetG(), c.GetB());
}

RectF DatePopupCellRect(const DatePopupState& st, int index) {
    const int r = index / 7;
    const int c = index % 7;
    return RectF(static_cast<float>(st.padX + c * st.cellW),
                 static_cast<float>(st.cellsY + r * st.cellH), static_cast<float>(st.cellW),
                 static_cast<float>(st.cellH));
}

RectF DatePopupNavRect(const DatePopupState& st, bool left) {
    const int w = static_cast<int>(30.0f * st.scale);
    if (left) {
        return RectF(static_cast<float>(st.padX), 0.0f, static_cast<float>(w),
                     static_cast<float>(st.titleH));
    }
    return RectF(static_cast<float>(st.clientW - st.padX - w), 0.0f, static_cast<float>(w),
                 static_cast<float>(st.titleH));
}

RectF DatePopupFooterRect(const DatePopupState& st) {
    const int h = static_cast<int>(28.0f * st.scale);
    return RectF(static_cast<float>(st.padX), static_cast<float>(st.footerTop + 3),
                 static_cast<float>(st.clientW - st.padX * 2), static_cast<float>(h));
}

// Точка → индекс ячейки (0..41) или -1.
int DatePopupCellAt(const DatePopupState& st, POINT pt) {
    const int gx = pt.x - st.padX;
    const int gy = pt.y - st.cellsY;
    if (gx < 0 || gy < 0 || gx >= 7 * st.cellW || gy >= kPopupRows * st.cellH) {
        return -1;
    }
    const int c = gx / st.cellW;
    const int r = gy / st.cellH;
    return r * 7 + c;
}

void DatePopupShiftMonth(DatePopupState& st, int delta);  // определена ниже

void DatePopupShiftMonth(DatePopupState& st, int delta) {
    int m = st.viewMonth + delta;
    int y = st.viewYear;
    if (m < 1) {
        m = 12;
        --y;
    } else if (m > 12) {
        m = 1;
        ++y;
    }
    st.viewMonth = m;
    st.viewYear = y;
    st.hoverCell = -1;
}

void PaintDatePopup(Graphics& g, const DatePopupState& st) {
    const float s = st.scale;

    // фон и рамка
    const RectF full(0.0f, 0.0f, static_cast<float>(st.clientW),
                     static_cast<float>(st.clientH));
    SolidBrush bg(ColArgb(st.bg));
    g.FillRectangle(&bg, full);
    Pen border(ColArgb(st.border), 1.0f);
    g.DrawRectangle(&border, RectF(full.X + 0.5f, full.Y + 0.5f, full.Width - 1.0f,
                                   full.Height - 1.0f));

    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font fTitle(&ff, 10.5f * s, Gdiplus::FontStyleBold);
    Gdiplus::Font fWeek(&ff, 8.0f * s, Gdiplus::FontStyleRegular);
    Gdiplus::Font fDay(&ff, 9.0f * s, Gdiplus::FontStyleRegular);
    Gdiplus::Font fFoot(&ff, 8.5f * s, Gdiplus::FontStyleRegular);

    StringFormat center;
    center.SetAlignment(StringAlignmentCenter);
    center.SetLineAlignment(StringAlignmentCenter);
    center.SetFormatFlags(StringFormatFlagsNoWrap);

    // стрелки-треугольники в заголовке
    auto drawTri = [&](const RectF& r, bool left, bool hoverOn) {
        const float cx = r.X + r.Width * 0.5f;
        const float cy = r.Y + r.Height * 0.5f;
        const float d = 4.5f * s;
        PointF pts[3];
        if (left) {
            pts[0] = PointF(cx + d * 0.6f, cy - d);
            pts[1] = PointF(cx + d * 0.6f, cy + d);
            pts[2] = PointF(cx - d * 0.8f, cy);
        } else {
            pts[0] = PointF(cx - d * 0.6f, cy - d);
            pts[1] = PointF(cx - d * 0.6f, cy + d);
            pts[2] = PointF(cx + d * 0.8f, cy);
        }
        SolidBrush tri(WithAlpha(ColArgb(st.accent), static_cast<BYTE>(hoverOn ? 255 : 200)));
        g.FillPolygon(&tri, pts, 3);
    };
    drawTri(DatePopupNavRect(st, true), true, st.hoverPrev);
    drawTri(DatePopupNavRect(st, false), false, st.hoverNext);

    // заголовок месяца и года
    const std::wstring title =
        std::wstring(kPopupMonths[std::clamp(st.viewMonth - 1, 0, 11)]) + L" " +
        std::to_wstring(st.viewYear);
    RectF titleBox(static_cast<float>(st.padX + 30), 0.0f,
                   static_cast<float>(st.clientW - (st.padX + 30) * 2),
                   static_cast<float>(st.titleH));
    ui::Text(g, title.c_str(), -1, &fTitle, titleBox, &center, ColArgb(st.accent));

    // заголовки дней недели (Пн…Вс)
    for (int c = 0; c < 7; ++c) {
        const RectF wd(static_cast<float>(st.padX + c * st.cellW),
                       static_cast<float>(st.cellsY - st.weekH),
                       static_cast<float>(st.cellW), static_cast<float>(st.weekH));
        ui::Text(g, kPopupWeekdays[c], -1, &fWeek, wd, &center, ColArgb(st.textDim));
    }

    // сетка дней
    const long firstJdn = JdnGreg(st.viewYear, st.viewMonth, 1);
    const int firstCol = (DowFromJdn(firstJdn) + 6) % 7;  // Пн = 0
    const long gridStart = firstJdn - firstCol;

    SYSTEMTIME now = {};
    GetLocalTime(&now);
    const long todayJ = JdnGreg(static_cast<int>(now.wYear), static_cast<int>(now.wMonth),
                                static_cast<int>(now.wDay));
    const long viewJ = GetCurrentDayInfo().jdn;

    for (int i = 0; i < 42; ++i) {
        const long j = gridStart + i;
        const YMD t = FromJdnGreg(j);
        const RectF cell = DatePopupCellRect(st, i);

        if (st.hoverCell == i) {
            GraphicsPath hp;
            ui::AddRoundRect(hp, RectF(cell.X + 2.0f, cell.Y + 2.0f, cell.Width - 4.0f,
                                       cell.Height - 4.0f),
                             7.0f * s);
            SolidBrush hov(ColArgb(st.hover));
            g.FillPath(&hov, &hp);
        }
        // окружность «сегодня»
        if (j == todayJ) {
            Pen todayPen(ColArgb(st.accent), 1.4f * s);
            g.DrawEllipse(&todayPen, cell.X + cell.Width * 0.5f - 11.0f * s,
                          cell.Y + cell.Height * 0.5f - 11.0f * s, 22.0f * s, 22.0f * s);
        }
        // выделение просматриваемой даты
        Color dayCol = ColArgb(st.text);
        if (t.m != st.viewMonth) {
            dayCol = WithAlpha(ColArgb(st.textDim), 120);
        }
        if (j == viewJ) {
            SolidBrush acc(ColArgb(st.accent));
            g.FillEllipse(&acc, cell.X + cell.Width * 0.5f - 11.0f * s,
                          cell.Y + cell.Height * 0.5f - 11.0f * s, 22.0f * s, 22.0f * s);
            if (j == todayJ) {
                Pen todayPen(ColArgb(st.border), 1.4f * s);
                g.DrawEllipse(&todayPen, cell.X + cell.Width * 0.5f - 13.0f * s,
                              cell.Y + cell.Height * 0.5f - 13.0f * s, 26.0f * s,
                              26.0f * s);
            }
            dayCol = ColArgb(st.bg);
        }
        wchar_t num[8] = {};
        _itow_s(t.d, num, 10);
        ui::Text(g, num, -1, &fDay, cell, &center, dayCol);
    }

    // разделитель и кнопка «Сегодня»
    Pen line(ColArgb(st.border), 1.0f);
    g.DrawLine(&line, static_cast<float>(st.padX), static_cast<float>(st.footerTop),
               static_cast<float>(st.clientW - st.padX), static_cast<float>(st.footerTop));
    const RectF foot = DatePopupFooterRect(st);
    if (st.hoverToday) {
        GraphicsPath fp;
        ui::AddRoundRect(fp, foot, 7.0f * s);
        SolidBrush hov(ColArgb(st.hover));
        g.FillPath(&hov, &fp);
    }
    const YMD todayYmd{static_cast<int>(now.wYear), static_cast<int>(now.wMonth),
                       static_cast<int>(now.wDay)};
    const std::wstring footText = L"Сегодня: " + DateRu(todayYmd);
    ui::Text(g, footText.c_str(), -1, &fFoot, foot, &center,
             WithAlpha(ColArgb(st.textDim), static_cast<BYTE>(st.hoverToday ? 255 : 190)));
}

// Процедура окна-попапа мини-календаря.
LRESULT CALLBACK DatePopupProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<DatePopupState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lp);
            const auto* init = static_cast<const DatePopupInit*>(cs->lpCreateParams);
            auto* state = new DatePopupState();
            state->owner = init->owner;
            state->scale = init->scale;
            state->viewYear = init->year;
            state->viewMonth = init->month;
            state->bg = init->bg;
            state->border = init->border;
            state->hover = init->hover;
            state->accent = init->accent;
            state->text = init->text;
            state->textDim = init->textDim;
            const float s = init->scale;
            state->padX = static_cast<int>(14.0f * s);
            state->titleH = static_cast<int>(34.0f * s);
            state->weekH = static_cast<int>(22.0f * s);
            state->cellW = static_cast<int>(33.0f * s);
            state->cellH = static_cast<int>(30.0f * s);
            state->cellsY = state->titleH + state->weekH;
            state->footerTop = state->cellsY + kPopupRows * state->cellH;
            state->clientW = state->padX * 2 + 7 * state->cellW;
            state->clientH = state->footerTop + static_cast<int>(31.0f * s);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            return 0;
        }
        case WM_SHOWWINDOW:
            if (wp == TRUE) {
                SetCapture(h);  // ловим клики вне попапа, пока курсор снаружи
            }
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;  // попап не «активируется», не мешает окну-владельцу
        case WM_ERASEBKGND:
            return 1;  // фон рисуется в WM_PAINT (без мерцания)
        case WM_PAINT: {
            PAINTSTRUCT ps = {};
            HDC dc = BeginPaint(h, &ps);
            RECT rc = {};
            GetClientRect(h, &rc);
            const int w = rc.right - rc.left;
            const int hgt = rc.bottom - rc.top;
            Gdiplus::Bitmap bmp(w, hgt);  // двойная буферизация
            {
                Graphics mem(&bmp);
                mem.SetSmoothingMode(SmoothingModeAntiAlias);
                mem.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
                PaintDatePopup(mem, *st);
            }
            {
                Graphics out(dc);
                out.DrawImage(&bmp, 0, 0, w, hgt);
            }
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            POINT cur = {};
            GetCursorPos(&cur);
            RECT wr = {};
            GetWindowRect(h, &wr);
            if (GetCapture() == h) {
                if (PtInRect(&wr, cur)) {
                    ReleaseCapture();  // курсор вернулся в попап — отпускаем захват
                } else {
                    return 0;
                }
            }
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (!PtInRect(&wr, cur)) {
                ScreenToClient(h, &pt);
            }
            const int cell = DatePopupCellAt(*st, pt);
            const bool prev = DatePopupNavRect(*st, true).Contains(
                static_cast<float>(pt.x), static_cast<float>(pt.y));
            const bool next = DatePopupNavRect(*st, false).Contains(
                static_cast<float>(pt.x), static_cast<float>(pt.y));
            const bool today = DatePopupFooterRect(*st).Contains(
                static_cast<float>(pt.x), static_cast<float>(pt.y));
            if (cell != st->hoverCell || prev != st->hoverPrev || next != st->hoverNext ||
                today != st->hoverToday) {
                st->hoverCell = cell;
                st->hoverPrev = prev;
                st->hoverNext = next;
                st->hoverToday = today;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT cur = {};
            GetCursorPos(&cur);
            RECT wr = {};
            GetWindowRect(h, &wr);
            if (!PtInRect(&wr, cur)) {
                DestroyWindow(h);  // клик мимо попапа — закрыть
                return 0;
            }
            if (GetCapture() == h) {
                ReleaseCapture();
                // продолжаем обработку: клик пришёлся внутрь попапа
            }
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            auto shifted = [&](int delta) {
                DatePopupShiftMonth(*st, delta);
                InvalidateRect(h, nullptr, FALSE);
            };
            if (DatePopupNavRect(*st, true).Contains(static_cast<float>(pt.x),
                                                     static_cast<float>(pt.y))) {
                shifted(-1);
                return 0;
            }
            if (DatePopupNavRect(*st, false).Contains(static_cast<float>(pt.x),
                                                      static_cast<float>(pt.y))) {
                shifted(1);
                return 0;
            }
            if (DatePopupFooterRect(*st).Contains(static_cast<float>(pt.x),
                                                  static_cast<float>(pt.y))) {
                const HWND owner = st->owner;
                DestroyWindow(h);  // «Сегодня» — вернуться к текущей дате
                if (owner != nullptr) {
                    SendMessageW(owner, WM_DATE_PICKED, 0, 0);
                }
                return 0;
            }
            const int cell = DatePopupCellAt(*st, pt);
            if (cell >= 0) {
                const long firstJdn = JdnGreg(st->viewYear, st->viewMonth, 1);
                const long j = firstJdn - ((DowFromJdn(firstJdn) + 6) % 7) + cell;
                const YMD t = FromJdnGreg(j);
                const HWND owner = st->owner;
                DestroyWindow(h);
                if (owner != nullptr) {
                    SendMessageW(owner, WM_DATE_PICKED, static_cast<WPARAM>(t.y),
                                 static_cast<LPARAM>(MAKELPARAM(t.m, t.d)));
                }
            }
            return 0;
        }
        case WM_NCDESTROY: {
            auto* state = reinterpret_cast<DatePopupState*>(
                GetWindowLongPtrW(h, GWLP_USERDATA));
            delete state;
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ============================================================================

class App {
public:
    int Run(HINSTANCE inst, int showCmd);

private:
    bool CreateMainWindow(HINSTANCE inst, int showCmd);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void OnPaint(HWND hwnd);
    void OnSize(HWND hwnd);
    void OnMouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void OnKey(HWND hwnd, WPARAM wp);
    void OnChar(HWND hwnd, WPARAM wp);
    void OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    void SyncSearch(HWND hwnd);
    HICON MakeAppIcon(int size);

    void LoadSettings();
    void SaveSettings();
    void BuildView();
    void ApplySection(int index);
    const Prayer* PrayerOf(const ViewItem& item) const;
    std::wstring FavoriteKey(const ViewItem& item) const;
    bool IsFavorite(const ViewItem& item) const;
    void ToggleFavorite(const ViewItem& item);
    void SectionById(const std::wstring& id, int& index) const;
    void StepCalendarDate(int deltaDays, bool resetToToday = false);
    void ApplyCalendarOffset(int offsetDays, bool announceToday);  // абсолютная установка даты
    void ToggleDatePopup();           // миникалендарь выбора даты
    void CloseDatePopup();            // закрыть миникалендарь (если открыт)
    void RepositionDatePopup();       // вернуть попап к плитке даты (при перемещении окна)
    void ApplyWindowTheme();          // тёмный/светлый заголовок окна под палитру

    void Paint(Graphics& g, const RectF& client);
    void PaintSidebar(Graphics& g, const RectF& area);
    void PaintHeader(Graphics& g, const RectF& area);
    void PaintContent(Graphics& g, const RectF& area);
    void PaintScrollbar(Graphics& g, const RectF& area);
    bool ScrollbarHit(int x, int y) const;
    void ScrollbarDrag(int x, int y);
    int HeaderButtonAt(float x, float y) const;
    void DrawTooltip(Graphics& g);
    void PaintSidebarToggle(Graphics& g, const RectF& area);
    void PaintSidebarScrollbar(Graphics& g, const RectF& area);
    void BuildSidebarItems();
    RectF SidebarListRect() const;      // видимая область списка разделов
    float SidebarContentHeight() const;  // полная высота списка разделов
    float SidebarMaxScroll() const;      // насколько можно прокрутить
    void ClampSidebarScroll();
    void BuildSearchIndex();
    void UpdateWindowTitle();
    void UpdateHover(HWND hwnd, int sx, int sy);
    void PaintSearchBox(Graphics& g, const RectF& box);
    void PaintIconButton(Graphics& g, const RectF& r, int icon, bool active);
    void PaintContentsPanel(Graphics& g);
    void BuildContentsRows(const RectF& panel);
    void PaintSettingsPanel(Graphics& g);
    void PaintToast(Graphics& g);
    void PaintPanelFrame(Graphics& g, const RectF& r, const std::wstring& title);
    void BuildPanelItems();
    RectF PanelRect() const;
    void ScrollToCard(int cardIndex, bool smooth);
    void ShowToast(const std::wstring& text);
    void PaintHelpPanel(Graphics& g);
    void PaintProgress(Graphics& g, const RectF& area);
    void PaintFocusChrome(Graphics& g, const RectF& client);
    void SetFocusMode(bool on);
    void OpenPalette();
    void ClosePalette();
    void BuildPaletteItems();
    void RefreshPalette();
    void PaintPalette(Graphics& g, const RectF& client);
    void ActivatePaletteItem(int filteredIndex);
    static LRESULT CALLBACK EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void RandomPrayer();
    bool WordMatches(const std::wstring& word) const;
    void ShowCardMenu(int cardIndex, int screenX, int screenY);
    void CopyPrayer(int cardIndex, bool withoutAccents);
    void ExportPrayer(int cardIndex);
    void PrintPrayer(int cardIndex);
    std::wstring PrayerText(const Prayer& prayer, bool withoutAccents) const;
    int CardAtPoint(float localX, float localY) const;
    void DrawSectionIcon(Graphics& g, const RectF& r, const std::wstring& id, Color c);
    void DrawStar(Graphics& g, const RectF& r, Color c, bool filled);
    void DrawButtonGlyph(Graphics& g, const RectF& r, int icon, Color c);

    void Relayout(Graphics& g, float contentWidth);
    void LayoutBlock(Graphics& g, size_t blockIndex, float width, bool justify);
    float MeasureText(Graphics& g, const std::wstring& text, const Font& font, int role);
    void MakeFonts();

    // Месячная сетка в разделе «Календарь»
    struct CalGridDay {        long jdn = 0;
        int day = 0;        // нов. стиль
        int oldDay = 0;     // ст. стиль
        int month = 0;      // нов. стиль
        int vestment = 0;
        bool fast = false;
        bool sunday = false;
    };
    void EnsureCalendarGrid();
    void PaintCalendarGrid(Graphics& g);
    bool CalendarGridClick(float x, float y);
    int CalendarGridCellAt(float x, float y) const;
    RectF CalGridPrevRect() const;
    RectF CalGridNextRect() const;
    RectF CalGridTodayRect() const;
    RectF CalGridCellRect(int index) const;
    bool InCalendarSection() const;

    // Кольцо прогресса в первой карточке чёток
    void PaintChetkiProgress(Graphics& g);

    void ScrollBy(float delta);
    void ScrollTo(float value);
    bool HitTest(float x, float y, int& block, int& offset) const;
    bool IsSelected(int block, int start, int end) const;
    void CopySelection();
    void SetClipboardText(const std::wstring& text);
    std::vector<std::wstring> WrapForPrint(HDC dc, const std::wstring& text, int width) const;
    std::wstring SelectionText() const;
    void SelectAll();

    // --- окно ---
    HINSTANCE inst_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND edit_ = nullptr;
    HFONT editFont_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    std::wstring cueBanner_;

    // --- данные ---
    std::vector<Section> sections_;
    std::vector<ViewItem> view_;
    std::set<std::wstring> favorites_;

    // --- состояние ---
    int section_ = 0;
    std::wstring lastSectionId_;
    int fontSize_ = 16;
    bool dark_ = true;
    int themeIndex_ = 0;
    int lang_ = 0;
    std::wstring search_;
    std::wstring searchLower_;

    float dpi_ = 96.0f;
    float scale_ = 1.0f;
    float clientWidth_ = 0;
    float clientHeight_ = 0;
    float sidebarW_ = 272.0f;
    float headerH_ = 132.0f;
    float sidebarListTop_ = 112.0f;

    // --- разметка ---
    std::vector<Block> blocks_;
    std::vector<Word> words_;
    std::vector<Line> lines_;
    std::vector<Card> cards_;
    float contentHeight_ = 0;
    float colX_ = 0;
    float colW_ = 0;
    float textX_ = 0;
    float cardPadX_ = 30.0f;
    bool layoutDirty_ = true;
    bool fontsDirty_ = true;

    // --- прокрутка ---
    float scroll_ = 0;
    float scrollTarget_ = 0;
    float sidebarScroll_ = 0;
    RectF scrollTrack_;      // дорожка полосы прокрутки (клиентские координаты)
    RectF scrollThumb_;      // ползунок
    bool scrollHover_ = false;
    bool scrollDrag_ = false;
    float scrollGrab_ = 0.0f;  // смещение захвата внутри ползунка
    RectF sideTrack_;
    RectF sideThumb_;
    bool sideHover_ = false;
    bool sideDrag_ = false;
    float sideGrab_ = 0.0f;

    // --- клавиатура ---
    bool ctrlDown_ = false;
    bool shiftDown_ = false;

    // --- выделение ---
    bool hasSelection_ = false;
    int selBlockA_ = -1, selOffsetA_ = 0;
    int selBlockB_ = -1, selOffsetB_ = 0;
    bool dragging_ = false;

    // --- шрифты ---
    std::unique_ptr<Font> fBody_, fBodyBold_, fTitle_, fUi_, fUiBold_, fGroup_, fRubric_, fSide_,
        fTabSmall_;
    std::unordered_map<std::wstring, float> widths_[4];
    theme::Palette pal_;

    // --- настройки чтения ---
    int fontFamily_ = 0;
    float spacing_ = 1.0f;
    float columnScale_ = 1.0f;
    std::map<std::wstring, float> scrollMem_;

    // --- боковое меню ---
    struct SidebarItem {
        int section = -1;
        float y = 0;
        float h = 0;
        bool header = false;
        std::wstring label;
        std::wstring group;
    };
    std::vector<SidebarItem> sidebarItems_;
    float sidebarContentHeight_ = 0;

    // --- индекс поиска (в нижнем регистре) ---
    struct SearchEntry {
        int section = 0;
        int prayer = 0;
        std::wstring title;
        std::wstring text;
    };
    std::vector<SearchEntry> searchIndex_;
    int panel_ = 0;            // 0 — закрыто, 1 — содержание, 2 — настройки
    int panelHover_ = -1;
    float panelScroll_ = 0;
    struct PanelItem {
        RectF rect;
        int action = 0;
        int index = 0;
    };
    std::vector<PanelItem> panelItems_;
    struct TocRow {
        std::wstring text;
        float y = 0;
        float h = 0;
        int index = -1;
        bool header = false;
    };
    std::vector<TocRow> tocRows_;

    // --- кнопки шапки ---
    RectF btnTheme_, btnSettings_, btnContents_, btnSizePlus_, btnSizeMinus_, btnLang_, searchRect_;
    RectF searchClearRect_;
    bool searchClearHover_ = false;

    // --- командная палитра (Ctrl+K) ---
    struct PaletteItem {
        std::wstring label;
        std::wstring hint;
        std::wstring id;
        int section = -1;
        int action = 0;
    };
    std::vector<PaletteItem> paletteItems_;
    std::vector<int> paletteFiltered_;
    std::vector<RectF> paletteRows_;
    std::wstring paletteQuery_;
    int paletteSel_ = 0;
    int paletteHover_ = -1;
    int paletteScroll_ = 0;
    bool paletteOpen_ = false;
    ULONGLONG paletteOpenedAt_ = 0;
    RectF paletteRect_;
    WNDPROC editProc_ = nullptr;
    RectF btnPrevDay_, btnToday_, btnNextDay_;
    HWND datePopup_ = nullptr;   // popup-миникалендарь выбора даты
    struct QuickTab {
        RectF rect;
        std::wstring id;
    };
    std::vector<QuickTab> quickTabs_;
    struct CardBtn {
        RectF rect;  // в локальных координатах контента (colX_, card.y)
        int action = 0;
        int cardItem = -1;
        std::wstring url;
    };
    std::vector<CardBtn> cardBtns_;

    // --- месячная сетка календаря ---
    RectF calGridRect_;
    int calGridYear_ = 0;
    int calGridMonth_ = 0;
    bool calGridInit_ = false;   // нужно синхронизировать с просматриваемой датой
    bool calGridDirty_ = true;   // нужно пересчитать дни
    int calGridHover_ = -1;      // индекс дня под курсором
    int calGridHoverBtn_ = -1;   // 0 — назад, 1 — вперёд, 2 — «Сегодня»
    std::vector<CalGridDay> calGridDays_;  // 42 дня (6 недель)

    // --- кольцо прогресса чёток ---
    RectF chetkiRingRect_;

    // --- прочее ---
    std::wstring toast_;
    double toastSince_ = 0.0;
    double toastUntil_ = 0.0;
    RECT toastInvRect_ = {0, 0, 0, 0};
    int hoveredCard_ = -1;
    float lastMouseX_ = -1;
    float lastMouseY_ = -1;
    bool sidebarCollapsed_ = false;
    RectF sidebarToggle_;
    RectF topButton_;

    // --- режим чтения (F11) ---
    bool focusMode_ = false;
    LONG_PTR savedStyle_ = 0;
    LONG_PTR savedExStyle_ = 0;
    WINDOWPLACEMENT savedPlacement_ = {sizeof(WINDOWPLACEMENT)};
    RectF focusExitRect_;
    bool focusExitHover_ = false;
    std::wstring tooltipText_;
    int tooltipButton_ = -1;
    bool tooltipVisible_ = false;
    ULONGLONG tooltipSince_ = 0;

    // --- анимация ---
    double flame_ = 0.0;
    ULONGLONG lastSave_ = 0;
    RECT candleRect_ = {0, 0, 0, 0};

    // --- доп: напоминания и поминальник ---
    bool reminders_ = true;
    DWORD morningStamp_ = 0;
    DWORD eveningStamp_ = 0;
};

// ---------------------------------------------------------------- Запуск

int App::Run(HINSTANCE inst, int showCmd) {
    inst_ = inst;

    sections_ = LoadSections();
    online::LoadCacheForDate(GetCurrentDateKey());
    LoadSettings();                  // читает настройки + поминальник
    AddCalendarSections(sections_);  // динамические разделы (с именами)
    int saved = 0;
    SectionById(lastSectionId_.empty() ? sections_[0].id : lastSectionId_, saved);
    section_ = saved;
    BuildView();  // пересборка под финальный список разделов
    RebuildAllDynamicSections(sections_);
    BuildSearchIndex();

    if (!CreateMainWindow(inst, showCmd)) {
        return 1;
    }

    online::StartFetch(hwnd_, GetCurrentDateKey());  // фоновая загрузка дня

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    SaveSettings();
    // Сначала уничтожаем поле поиска, затем освобождаем его кисть и шрифт:
    // нельзя удалять GDI-объекты, пока они могут быть выбраны в контрол.
    if (edit_ != nullptr && IsWindow(edit_)) {
        DestroyWindow(edit_);
        edit_ = nullptr;
    }
    if (editBrush_ != nullptr) {
        DeleteObject(editBrush_);
        editBrush_ = nullptr;
    }
    if (editFont_ != nullptr) {
        DeleteObject(editFont_);
        editFont_ = nullptr;
    }
    return 0;
}

bool App::CreateMainWindow(HINSTANCE inst, int showCmd) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &App::WndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"OrthodoxPrayersWindow";
    if (!RegisterClassExW(&wc)) {
        return false;
    }

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"Православные молитвы",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                             1240, 860, nullptr, nullptr, inst, this);
    if (hwnd_ == nullptr) {
        return false;
    }

    edit_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, 0, 0,
                             10, 10, hwnd_,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SEARCH)), inst, nullptr);
    if (edit_ != nullptr) {
        editProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&App::EditSubclass)));
        SetWindowLongPtrW(edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));

    ApplyWindowTheme();

    ShowWindow(hwnd_, showCmd);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self != nullptr) {
        return self->Handle(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            SetTimer(hwnd, TIMER_ANIM, 33, nullptr);
            HICON iconBig = MakeAppIcon(48);
            HICON iconSmall = MakeAppIcon(16);
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(iconBig));
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(iconSmall));
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            OnPaint(hwnd);
            return 0;
        case WM_SIZE:
            OnSize(hwnd);
            RepositionDatePopup();  // попап следует за плиткой даты
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) {
                CloseDatePopup();  // ушли в другое приложение — попап не должен висеть
            }
            return 0;
        case WM_MOVE:
            RepositionDatePopup();  // окно перетащили — попап едет вместе с плиткой
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = 1120;
            mmi->ptMinTrackSize.y = 560;
            return 0;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
            OnMouse(hwnd, msg, wp, lp);
            return 0;
        case WM_CONTEXTMENU: {
            const int mx = GET_X_LPARAM(lp);
            const int my = GET_Y_LPARAM(lp);
            POINT p = {mx, my};
            ScreenToClient(hwnd, &p);
            const int card = CardAtPoint(static_cast<float>(p.x) - sidebarW_,
                                         static_cast<float>(p.y) - headerH_ + scroll_);
            if (card >= 0) {
                ShowCardMenu(card, mx, my);
            }
            return 0;
        }
        case WM_SYSKEYDOWN:
            if (wp == VK_LEFT) {
                StepCalendarDate(-1);
                return 0;
            } else if (wp == VK_RIGHT) {
                StepCalendarDate(1);
                return 0;
            } else if (wp == VK_UP) {
                StepCalendarDate(-7);
                return 0;
            } else if (wp == VK_DOWN) {
                StepCalendarDate(7);
                return 0;
            } else if (wp == VK_HOME) {
                StepCalendarDate(0, true);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wp == VK_CONTROL || wp == VK_LCONTROL || wp == VK_RCONTROL) {
                ctrlDown_ = true;
            } else if (wp == VK_SHIFT || wp == VK_LSHIFT || wp == VK_RSHIFT) {
                shiftDown_ = true;
            }
            OnKey(hwnd, wp);
            return 0;
        case WM_KEYUP:
            if (wp == VK_CONTROL || wp == VK_LCONTROL || wp == VK_RCONTROL) {
                ctrlDown_ = false;
            } else if (wp == VK_SHIFT || wp == VK_LSHIFT || wp == VK_RSHIFT) {
                shiftDown_ = false;
            }
            return 0;
        case WM_KILLFOCUS:
            ctrlDown_ = false;
            shiftDown_ = false;
            return 0;
        case WM_CHAR:
            OnChar(hwnd, wp);
            return 0;
        case WM_COMMAND:
            OnCommand(hwnd, wp, lp);
            return 0;
        case WM_SETFOCUS:
            hasSelection_ = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_APP + 7: {  // online::kOnlineDoneMsg
            RebuildAllDynamicSections(sections_);
            BuildSearchIndex();
            if (search_.empty()) {
                BuildView();
                layoutDirty_ = true;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_DATE_PICKED: {  // выбор даты в миникалендаре (wp=0 — «сегодня»)
            if (wp == 0) {
                if (GetCalendarOffsetDays() != 0) {
                    StepCalendarDate(0, true);
                }
                return 0;
            }
            const int year = static_cast<int>(wp);
            const int month = static_cast<int>(LOWORD(lp));
            const int day = static_cast<int>(HIWORD(lp));
            if (year >= 1800 && year <= 2400 && month >= 1 && month <= 12 && day >= 1 &&
                day <= 31) {
                SYSTEMTIME now = {};
                GetLocalTime(&now);
                const long jdn = JdnGreg(year, month, day);
                const long today = JdnGreg(static_cast<int>(now.wYear),
                                           static_cast<int>(now.wMonth),
                                           static_cast<int>(now.wDay));
                // АБСОЛЮТНАЯ установка: смещение от сегодня, а не приращение к текущему
                // (иначе при выборе даты в другом месяце смещение считалось бы дважды).
                ApplyCalendarOffset(static_cast<int>(jdn - today), false);
            }
            return 0;
        }
        case WM_TIMER:
            if (wp == TIMER_ANIM) {
                flame_ += 0.11;
                if (edit_ != nullptr) {
                    SyncSearch(hwnd);
                }
                // появление подсказки
                if (tooltipButton_ >= 0) {
                    const bool visible = (GetTickCount64() - tooltipSince_) >= 550;
                    if (visible != tooltipVisible_) {
                        tooltipVisible_ = visible;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                if (paletteOpen_) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                // автосохранение настроек раз в 10 секунд
                const ULONGLONG now = GetTickCount64();
                if (now - lastSave_ > 10000) {
                    lastSave_ = now;
                    SaveSettings();
                }
                // напоминания о молитвенном правиле (раз в день)
                if (reminders_) {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    const DWORD today = st.wYear * 400 + st.wMonth * 40 + st.wDay;
                    const int t = st.wHour * 60 + st.wMinute;
                    if (t >= 7 * 60 && morningStamp_ != today) {
                        morningStamp_ = today;
                        ShowToast(L"Время утреннего правила — раздел «Утренние молитвы»");
                    }
                    if (t >= 20 * 60 && eveningStamp_ != today) {
                        eveningStamp_ = today;
                        ShowToast(L"Время вечернего правила — раздел «Вечерние молитвы»");
                    }
                }
                bool animating = false;
                const float d = scrollTarget_ - scroll_;
                if (std::fabs(d) > 0.4f) {
                    scroll_ += d * 0.3f;
                    animating = true;
                } else {
                    scroll_ = scrollTarget_;
                }
                if (animating) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (candleRect_.right > candleRect_.left) {
                    InvalidateRect(hwnd, &candleRect_, FALSE);
                }
                // анимация тоста и его своевременное исчезновение
                if (!toast_.empty()) {
                    const double nowS = GetTickCount64() / 1000.0;
                    if (toastInvRect_.right > toastInvRect_.left) {
                        InvalidateRect(hwnd, &toastInvRect_, FALSE);
                    }
                    if (nowS > toastUntil_ + 0.06) {
                        toast_.clear();
                        if (toastInvRect_.right > toastInvRect_.left) {
                            InvalidateRect(hwnd, &toastInvRect_, FALSE);
                        }
                    }
                }
            }
            return 0;
        case WM_CTLCOLOREDIT: {
            auto* dc = reinterpret_cast<HDC>(wp);
            SetTextColor(dc, pal_.inputText);
            SetBkColor(dc, pal_.inputBg);
            if (editBrush_ != nullptr) {
                DeleteObject(editBrush_);
            }
            editBrush_ = CreateSolidBrush(pal_.inputBg);
            return reinterpret_cast<LRESULT>(editBrush_);
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- Отрисовка

void App::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    Bitmap bitmap(w, h, PixelFormat32bppARGB);
    {
        Graphics g(&bitmap);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        Paint(g, RectF(0, 0, static_cast<float>(w), static_cast<float>(h)));
    }
    {
        Graphics screen(dc);
        screen.DrawImage(&bitmap, 0, 0, w, h);
    }
    EndPaint(hwnd, &ps);
}

void App::Paint(Graphics& g, const RectF& client) {
    pal_ = theme::MakeByIndex(themeIndex_);
    dark_ = pal_.dark;
    if (fontsDirty_) {
        MakeFonts();
    }

    const float sideW = focusMode_ ? 0.0f : sidebarW_;
    const float headH = focusMode_ ? 0.0f : headerH_;
    const RectF side(0, 0, sideW, client.Height);
    const RectF header(sideW, 0, client.Width - sideW, headH);
    const RectF content(sideW, headH, client.Width - sideW, client.Height - headH);

    SolidBrush bg(theme::Gc(pal_.bg));
    g.FillRectangle(&bg, client);

    if (layoutDirty_) {
        Relayout(g, content.Width);
        layoutDirty_ = false;
    }

    if (!focusMode_) {
        PaintSidebar(g, side);
        PaintHeader(g, header);
    }
    PaintContent(g, content);

    if (!focusMode_) {
        // мягкая тень под шапкой — отделяет «хром» от полотна чтения
        {
            const float sh = 9.0f * scale_;
            LinearGradientBrush hb(PointF(0.0f, headerH_), PointF(0.0f, headerH_ + sh),
                                   theme::Gc(pal_.shadow, dark_ ? 90 : 26),
                                   theme::Gc(pal_.shadow, 0));
            g.FillRectangle(&hb, RectF(sidebarW_, headerH_, client.Width - sidebarW_, sh));
        }

        if (panel_ == 1) {
            PaintContentsPanel(g);
        } else if (panel_ == 2) {
            PaintSettingsPanel(g);
        } else if (panel_ == 3) {
            PaintHelpPanel(g);
        }
        DrawTooltip(g);
    } else {
        PaintFocusChrome(g, client);
    }
    if (paletteOpen_) {
        PaintPalette(g, client);
    }
    PaintToast(g);
}

void App::BuildSidebarItems() {
    sidebarItems_.clear();
    float y = sidebarListTop_;
    std::wstring lastGroup;
    for (size_t i = 0; i < sections_.size(); ++i) {
        const Section& s = sections_[i];
        if (s.id == L"favorites" && favorites_.empty()) {
            continue;
        }
        if (!s.group.empty() && s.group != lastGroup) {
            lastGroup = s.group;
            SidebarItem head;
            head.header = true;
            head.label = s.group;
            head.y = y;
            head.h = 24.0f * scale_;
            sidebarItems_.push_back(head);
            y += head.h;
        }
        SidebarItem item;
        item.section = static_cast<int>(i);
        item.label = s.shortTitle.empty() ? s.title : s.shortTitle;
        item.group = s.group;
        item.y = y;
        item.h = 42.0f * scale_;
        sidebarItems_.push_back(item);
        y += item.h;
    }
    sidebarContentHeight_ = y - sidebarListTop_;
}

RectF App::SidebarListRect() const {
    if (sidebarCollapsed_) {
        const float top = 64.0f * scale_;
        return RectF(0.0f, top, sidebarW_,
                     std::max(0.0f, clientHeight_ - top - 20.0f * scale_));
    }
    const float top = sidebarListTop_;
    return RectF(0.0f, top, sidebarW_,
                 std::max(0.0f, clientHeight_ - top - 158.0f * scale_));
}

float App::SidebarContentHeight() const {
    if (sidebarCollapsed_) {
        int n = 0;
        for (const Section& s : sections_) {
            if (s.id == L"favorites" && favorites_.empty()) {
                continue;
            }
            ++n;
        }
        return n * 46.0f * scale_;
    }
    return sidebarContentHeight_;
}

float App::SidebarMaxScroll() const {
    return std::max(0.0f, SidebarContentHeight() - SidebarListRect().Height);
}

void App::ClampSidebarScroll() {
    sidebarScroll_ = std::clamp(sidebarScroll_, 0.0f, SidebarMaxScroll());
}

void App::BuildSearchIndex() {
    searchIndex_.clear();
    for (size_t si = 0; si < sections_.size(); ++si) {
        const Section& sec = sections_[si];
        for (size_t pi = 0; pi < sec.prayers.size(); ++pi) {
            const Prayer& p = sec.prayers[pi];
            SearchEntry entry;
            entry.section = static_cast<int>(si);
            entry.prayer = static_cast<int>(pi);
            entry.title = Lower(p.title);
            for (const PrayerPara& para : p.paras) {
                entry.text += Lower(para.text);
                entry.text += L'\n';
            }
            searchIndex_.push_back(std::move(entry));
        }
    }
}

void App::UpdateWindowTitle() {
    if (hwnd_ == nullptr) {
        return;
    }
    std::wstring title = L"Православные молитвы";
    if (!search_.empty()) {
        title = L"Поиск — Православные молитвы";
    } else if (section_ >= 0 && section_ < static_cast<int>(sections_.size())) {
        title = sections_[static_cast<size_t>(section_)].title + L" — Православные молитвы";
    }
    SetWindowTextW(hwnd_, title.c_str());
}

void App::PaintSidebarToggle(Graphics& g, const RectF& area) {
    const float size = 28.0f * scale_;
    const float x = sidebarCollapsed_ ? (area.Width - size) * 0.5f : area.Width - size - 12.0f * scale_;
    const float y = 22.0f * scale_;
    sidebarToggle_ = RectF(x, y, size, size);
    const bool hover = sidebarToggle_.Contains(lastMouseX_, lastMouseY_);
    if (hover) {
        SolidBrush bg(theme::Gc(pal_.hover));
        GraphicsPath path;
        const float rad = 7.0f * scale_;
        path.AddArc(sidebarToggle_.X, sidebarToggle_.Y, rad, rad, 180, 90);
        path.AddArc(sidebarToggle_.GetRight() - rad, sidebarToggle_.Y, rad, rad, 270, 90);
        path.AddArc(sidebarToggle_.GetRight() - rad, sidebarToggle_.GetBottom() - rad, rad, rad, 0, 90);
        path.AddArc(sidebarToggle_.X, sidebarToggle_.GetBottom() - rad, rad, rad, 90, 90);
        path.CloseFigure();
        g.FillPath(&bg, &path);
    }
    Pen pen(theme::Gc(hover ? pal_.accent : pal_.textDim), 1.6f * scale_);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    const float cx = sidebarToggle_.X + size * 0.5f;
    const float cy = sidebarToggle_.Y + size * 0.5f;
    if (sidebarCollapsed_) {
        g.DrawLine(&pen, cx - 4.0f * scale_, cy - 5.0f * scale_, cx + 3.0f * scale_, cy);
        g.DrawLine(&pen, cx + 3.0f * scale_, cy, cx - 4.0f * scale_, cy + 5.0f * scale_);
    } else {
        g.DrawLine(&pen, cx + 3.0f * scale_, cy - 5.0f * scale_, cx - 4.0f * scale_, cy);
        g.DrawLine(&pen, cx - 4.0f * scale_, cy, cx + 3.0f * scale_, cy + 5.0f * scale_);
    }
}

void App::PaintSidebarScrollbar(Graphics& g, const RectF& area) {
    sideTrack_ = RectF();
    sideThumb_ = RectF();
    const RectF list = SidebarListRect();
    const float listH = list.Height;
    const float contentH = SidebarContentHeight();
    if (listH <= 0.0f || contentH <= listH) {
        return;
    }
    const float w = 6.0f * scale_;
    const float x = area.GetRight() - w - 2.0f * scale_;
    const float thumbH = std::max(30.0f * scale_, listH * (listH / contentH));
    const float t = std::clamp(sidebarScroll_ / std::max(1.0f, contentH - listH), 0.0f, 1.0f);
    const float y = list.Y + (listH - thumbH) * t;
    sideTrack_ = RectF(x, list.Y, w, listH);
    sideThumb_ = RectF(x, y, w, thumbH);
    SolidBrush track(theme::Gc(pal_.divider, 40));
    g.FillRectangle(&track, RectF(x + 2.0f * scale_, list.Y, 2.0f * scale_, listH));
    SolidBrush brush(theme::Gc(pal_.textDim, sideDrag_ ? 210 : (sideHover_ ? 160 : 90)));
    GraphicsPath path;
    const float rad = w * 0.5f;
    path.AddArc(sideThumb_.X, sideThumb_.Y, rad, rad, 180, 90);
    path.AddArc(sideThumb_.GetRight() - rad, sideThumb_.Y, rad, rad, 270, 90);
    path.AddArc(sideThumb_.GetRight() - rad, sideThumb_.GetBottom() - rad, rad, rad, 0, 90);
    path.AddArc(sideThumb_.X, sideThumb_.GetBottom() - rad, rad, rad, 90, 90);
    path.CloseFigure();
    g.FillPath(&brush, &path);
}

void App::PaintSidebar(Graphics& g, const RectF& area) {
    LinearGradientBrush grad(PointF(area.X, area.Y), PointF(area.X, area.GetBottom()),
                             theme::Gc(pal_.sidebarTop), theme::Gc(pal_.sidebarBottom));
    g.FillRectangle(&grad, area);

    const float pad = 26.0f * scale_;
    float y = 22.0f * scale_;

    PaintSidebarToggle(g, area);

    if (sidebarCollapsed_) {
        // --- свёрнутое меню: только значки ---
        const float itemH = 46.0f * scale_;
        const RectF list = SidebarListRect();
        const float listTop = list.Y;
        g.SetClip(RectF(area.X, listTop, area.Width, list.Height));
        float iy = listTop - sidebarScroll_;
        for (size_t i = 0; i < sections_.size(); ++i) {
            const Section& s = sections_[i];
            if (s.id == L"favorites" && favorites_.empty()) {
                continue;
            }
            const RectF item(area.X + 8.0f * scale_, iy, area.Width - 16.0f * scale_,
                             itemH - 6.0f * scale_);
            const bool selected = (static_cast<int>(i) == section_) && search_.empty();
            if (selected) {
                SolidBrush hl(theme::Gc(pal_.hover));
                GraphicsPath path;
                const float rad = 9.0f * scale_;
                path.AddArc(item.X, item.Y, rad, rad, 180, 90);
                path.AddArc(item.GetRight() - rad, item.Y, rad, rad, 270, 90);
                path.AddArc(item.GetRight() - rad, item.GetBottom() - rad, rad, rad, 0, 90);
                path.AddArc(item.X, item.GetBottom() - rad, rad, rad, 90, 90);
                path.CloseFigure();
                g.FillPath(&hl, &path);
                // киноварная полоска активного пункта
                SolidBrush bar(theme::Gc(pal_.accent));
                GraphicsPath barPath;
                barPath.AddRectangle(RectF(item.X, item.Y + 6.0f * scale_, 3.0f * scale_,
                                           item.Height - 12.0f * scale_));
                g.FillPath(&bar, &barPath);
            }
            const RectF iconRect(item.X + (item.Width - 22.0f * scale_) * 0.5f,
                                 item.Y + (item.Height - 22.0f * scale_) * 0.5f, 22.0f * scale_,
                                 22.0f * scale_);
            DrawSectionIcon(g, iconRect, s.id, theme::Gc(selected ? pal_.accent : pal_.textDim));
            iy += itemH;
        }
        g.ResetClip();
        PaintSidebarScrollbar(g, area);
        return;
    }

    // заголовок приложения
    {
        RectF cross(area.X + pad, y, 26.0f * scale_, 26.0f * scale_);
        icons::Cross(g, cross, theme::Gc(pal_.accent), 2.0f * scale_);
        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentNear);
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
        RectF titleBox(area.X + pad, y + 34.0f * scale_, area.Width - pad * 2.0f, 44.0f * scale_);
        ui::Text(g, L"Православные молитвы", -1, fSide_.get(), titleBox, &fmt, theme::Gc(pal_.text));
        RectF sub(area.X + pad, y + 58.0f * scale_, area.Width - pad * 2.0f, 18.0f * scale_);
        ui::Text(g, L"утренние и вечерние", -1, fUi_.get(), sub, &fmt, theme::Gc(pal_.textDim));
    }

    y = sidebarListTop_ - 14.0f * scale_;
    {
        Pen pen(theme::Gc(pal_.sidebarLine, 110), 1.0f);
        g.DrawLine(&pen, pad, y, area.Width - pad, y);
    }

    // список разделов (с группировкой)
    const float itemH = 42.0f * scale_;
    const RectF list = SidebarListRect();
    const float listTop = list.Y;
    const float listH = list.Height;
    BuildSidebarItems();
    ClampSidebarScroll();

    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentNear);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);

    g.SetClip(RectF(area.X, listTop, area.Width, std::max(0.0f, listH)));
    for (const SidebarItem& item : sidebarItems_) {
        // item.y уже отсчитан от начала списка — второй раз отступ не добавляем
        const float iy = item.y - sidebarScroll_;
        if (item.header) {
            RectF box(area.X + 20.0f * scale_, iy, area.Width - 40.0f * scale_, item.h);
            ui::Text(g, UpperLetters(item.label).c_str(), -1, fGroup_.get(), box, &fmt, theme::Gc(pal_.groupLabel));
            continue;
        }
        const Section& sec = sections_[static_cast<size_t>(item.section)];
        const RectF box(area.X + 12.0f * scale_, iy, area.Width - 24.0f * scale_, item.h - 4.0f * scale_);
        const bool selected = (item.section == section_) && search_.empty();
        if (selected) {
            SolidBrush hl(theme::Gc(pal_.hover));
            GraphicsPath path;
            const float rad = 9.0f * scale_;
            path.AddArc(box.X, box.Y, rad, rad, 180, 90);
            path.AddArc(box.GetRight() - rad, box.Y, rad, rad, 270, 90);
            path.AddArc(box.GetRight() - rad, box.GetBottom() - rad, rad, rad, 0, 90);
            path.AddArc(box.X, box.GetBottom() - rad, rad, rad, 90, 90);
            path.CloseFigure();
            g.FillPath(&hl, &path);
            SolidBrush bar(theme::Gc(pal_.accent));
            GraphicsPath barPath;
            barPath.AddRectangle(RectF(box.X, box.Y + 7.0f * scale_, 3.0f * scale_,
                                       box.Height - 14.0f * scale_));
            g.FillPath(&bar, &barPath);
        }
        const RectF iconRect(box.X + 15.0f * scale_, box.Y + (box.Height - 22.0f * scale_) * 0.5f,
                             22.0f * scale_, 22.0f * scale_);
        DrawSectionIcon(g, iconRect, sec.id, theme::Gc(selected ? pal_.accent : pal_.textDim));

        RectF textBox(iconRect.GetRight() + 14.0f * scale_, box.Y,
                      box.Width - (iconRect.GetRight() - box.X) - 46.0f * scale_, box.Height);
        ui::Text(g, item.label.c_str(), -1, selected ? fUiBold_.get() : fUi_.get(), textBox, &fmt,
                 theme::Gc(selected ? pal_.text : pal_.textDim));

        // число молитв
        StringFormat nf;
        nf.SetAlignment(StringAlignmentFar);
        nf.SetLineAlignment(StringAlignmentCenter);
        RectF numBox(box.X, box.Y, box.Width - 16.0f * scale_, box.Height);
        const std::wstring count = std::to_wstring(sec.prayers.size());
        ui::Text(g, count.c_str(), -1, fUi_.get(), numBox, &nf, theme::Gc(pal_.textDim, 120));
    }
    g.ResetClip();

    // индикатор прокрутки меню
    PaintSidebarScrollbar(g, area);

    // свеча
    {
        const RectF candle(area.Width * 0.5f - 26.0f * scale_, area.Height - 126.0f * scale_,
                           52.0f * scale_, 76.0f * scale_);
        icons::Candle(g, candle, theme::Gc(pal_.textDim), theme::Gc(pal_.accent), flame_);
        StringFormat cfmt;
        cfmt.SetAlignment(StringAlignmentCenter);
        cfmt.SetTrimming(StringTrimmingEllipsisCharacter);
        RectF verse(area.X + 14.0f * scale_, candle.GetBottom() + 2.0f * scale_,
                    area.Width - 28.0f * scale_, 40.0f * scale_);
        ui::Text(g, L"«Да будет воля Твоя,\nяко на небеси и на земли»", -1, fRubric_.get(),
                     verse, &cfmt, theme::Gc(pal_.accentDim));
        candleRect_.left = static_cast<LONG>(candle.X - 20.0f * scale_);
        candleRect_.top = static_cast<LONG>(candle.Y - 26.0f * scale_);
        candleRect_.right = static_cast<LONG>(candle.GetRight() + 20.0f * scale_);
        candleRect_.bottom = static_cast<LONG>(area.Height);
    }
}

void App::DrawSectionIcon(Graphics& g, const RectF& r, const std::wstring& id, Color c) {
    const float w = 1.7f * scale_;
    if (id == L"morning") {
        icons::Sun(g, r, c, w);
    } else if (id == L"evening") {
        icons::Moon(g, r, c, w);
    } else if (id == L"psaltir") {
        icons::Book(g, r, c, w);
    } else if (id == L"chin12") {
        icons::Harp(g, r, c, w);
    } else if (id == L"troparia") {
        icons::Crown(g, r, c, w);
    } else if (id == L"akafist" || id == L"akafisty" || id == L"kanon" ||
               id == L"kanon_bogorodice") {
        icons::Scroll(g, r, c, w);
    } else if (id == L"kanon_angel") {
        icons::Wings(g, r, c, w);
    } else if (id == L"pasxa") {
        icons::SunriseCross(g, r, c, w);
    } else if (id == L"prichastie") {
        icons::Chalice(g, r, c, w);
    } else if (id == L"bogorodica") {
        icons::Flower(g, r, c, w);
    } else if (id == L"daily") {
        icons::Censer(g, r, c, w);
    } else if (id == L"kratkie") {
        icons::Leaf(g, r, c, w);
    } else if (id == L"kalendar") {
        icons::CalendarIcon(g, r, c, w);
    } else if (id == L"sluzhby") {
        icons::Church(g, r, c, w);
    } else if (id == L"azbyka") {
        icons::Portal(g, r, c, w);
    } else if (id == L"bible") {
        icons::GospelBook(g, r, c, w);
    } else if (id == L"pominalnik") {
        icons::Memorial(g, r, c, w);
    } else if (id == L"chetki") {
        icons::Rosary(g, r, c, w);
    } else if (id == L"chasoslov") {
        icons::Bell(g, r, c, w);
    } else if (id == L"pomyannik") {
        icons::Heart(g, r, c, w);
    } else if (id == L"basics") {
        icons::Cross(g, r, c, w);
    } else {
        icons::Star(g, r, c, w, true);
    }
}

void App::PaintHeader(Graphics& g, const RectF& area) {
    SolidBrush bg(theme::Gc(pal_.bg));
    g.FillRectangle(&bg, area);
    Pen pen(theme::Gc(pal_.sidebarLine, 100), 1.0f);
    g.DrawLine(&pen, area.X, area.GetBottom(), area.GetRight(), area.GetBottom());

    const float pad = 34.0f * scale_;
    const float gap = 8.0f * scale_;

    // --- верхний ряд: инструменты справа ---
    const float bh = 32.0f * scale_;
    const float by = area.Y + 8.0f * scale_;

    float x = area.GetRight() - 20.0f * scale_;
    x -= 36.0f * scale_;
    btnTheme_ = RectF(x, by, 36.0f * scale_, bh);
    x -= gap;
    x -= 36.0f * scale_;
    btnSettings_ = RectF(x, by, 36.0f * scale_, bh);
    x -= gap;
    x -= 36.0f * scale_;
    btnContents_ = RectF(x, by, 36.0f * scale_, bh);
    x -= gap;
    x -= 36.0f * scale_;
    btnSizePlus_ = RectF(x, by, 36.0f * scale_, bh);
    x -= gap;
    x -= 36.0f * scale_;
    btnSizeMinus_ = RectF(x, by, 36.0f * scale_, bh);
    x -= gap;
    x -= 84.0f * scale_;
    btnLang_ = RectF(x, by, 84.0f * scale_, bh);
    x -= gap;

    const float toolsW = area.GetRight() - x;
    const float searchW = std::clamp(area.Width - toolsW - 540.0f * scale_, 136.0f * scale_,
                                      210.0f * scale_);
    x -= searchW;
    searchRect_ = RectF(x, by, searchW, bh);

    // --- быстрые вкладки разделов «Сегодня и Устав» в верхнем ряду ---
    quickTabs_.clear();
    StringFormat cf;
    cf.SetAlignment(StringAlignmentCenter);
    cf.SetLineAlignment(StringAlignmentCenter);
    cf.SetTrimming(StringTrimmingEllipsisCharacter);
    cf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);

    struct TabDef {
        const wchar_t* id;
        const wchar_t* label;
    };
    static const TabDef kQuickTabs[] = {
        {L"kalendar",   L"Календарь"},
        {L"sluzhby",    L"Устав"},
        {L"azbyka",     L"Азбука"},
        {L"bible",      L"Библия"},
        {L"pominalnik", L"Память"},
        {L"chetki",     L"Чётки"},
    };
    const std::wstring curSecId =
        (section_ >= 0 && section_ < static_cast<int>(sections_.size()))
            ? sections_[static_cast<size_t>(section_)].id
            : L"";
    float tx = area.X + 16.0f * scale_;
    const float maxTabRight = x - 8.0f * scale_;

    // Точная ширина строки (MeasureText кэширует по роли шрифта и не подходит
    // для подбора размера вкладок).
    auto measureLabel = [&](const wchar_t* label, const Font& font) {
        StringFormat mf(StringFormat::GenericTypographic());
        mf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                          StringFormatFlagsNoClip);
        RectF b;
        g.MeasureString(label, -1, &font, PointF(0.0f, 0.0f), &mf, &b);
        return b.Width;
    };

    // Подбираем компактность вкладок, чтобы все шесть помещались даже при
    // минимальной ширине окна (иначе «Чётки» обрезались совсем).
    struct TabStyle {
        float pad;
        float gap;
        int font;  // 0 — fUi_, 1 — fUiBold_, 2 — fGroup_, 3 — fTabSmall_
    };
    static const TabStyle kTabStyles[] = {
        {16.0f, 5.0f, 1}, {12.0f, 4.0f, 1}, {10.0f, 4.0f, 1}, {9.0f, 3.0f, 0},
        {8.0f, 3.0f, 2},  {7.0f, 2.0f, 2},  {6.0f, 2.0f, 3},  {4.0f, 1.0f, 3},
    };
    auto tabFontFor = [&](int k) -> const Font& {
        switch (k) {
            case 1: return *fUiBold_;
            case 2: return *fGroup_;
            case 3: return *fTabSmall_;
            default: return *fUi_;
        }
    };
    float tabWidths[6] = {};
    TabStyle style = kTabStyles[0];
    for (const TabStyle& st : kTabStyles) {
        const Font& f = tabFontFor(st.font);
        float total = st.gap * 5.0f * scale_;
        for (int i = 0; i < 6; ++i) {
            tabWidths[i] = measureLabel(kQuickTabs[i].label, f) + st.pad * 2.0f * scale_;
            total += tabWidths[i];
        }
        style = st;
        if (tx + total <= maxTabRight) {
            break;
        }
    }
    for (int ti = 0; ti < 6; ++ti) {
        const TabDef& td = kQuickTabs[ti];
        const Font* tabFont = &tabFontFor(style.font);
        const float tw = tabWidths[ti];
        if (tx + tw > maxTabRight + 0.5f) {
            break;
        }
        const RectF tr(tx, by + 2.0f * scale_, tw, bh - 4.0f * scale_);
        const bool active = search_.empty() && (curSecId == td.id);
        const bool hov = tr.Contains(lastMouseX_, lastMouseY_);
        GraphicsPath tpath;
        ui::AddRoundRect(tpath, tr, 7.0f * scale_);
        SolidBrush tbg(theme::Gc(active ? pal_.accent : (hov ? pal_.hover : pal_.inputBg),
                                 active ? 46 : 230));
        g.FillPath(&tbg, &tpath);
        Pen tborder(theme::Gc(active ? pal_.accent : pal_.cardBorder, active ? 180 : 190), 1.0f);
        g.DrawPath(&tborder, &tpath);
        const Font* drawFont = tabFont;
        ui::Text(g, td.label, -1, drawFont, tr, &cf,
                 theme::Gc(active ? pal_.accent : (hov ? pal_.text : pal_.textDim)));
        quickTabs_.push_back({tr, td.id});
        tx += tw + style.gap * scale_;
    }

    // --- переключатель даты календаря и бейдж гласа/облачения справа во 2-м ряду ---
    static const wchar_t* kShortMonths[] = {L"янв", L"фев", L"мар", L"апр", L"мая", L"июн",
                                            L"июл", L"авг", L"сен", L"окт", L"ноя", L"дек"};
    const DayInfo& dinfo = GetCurrentDayInfo();
    const float navY = area.Y + 52.0f * scale_;
    const float navH = 28.0f * scale_;
    float rx = area.GetRight() - 20.0f * scale_;

    auto drawNavPill = [&](const RectF& nr, const std::wstring& txt, bool highlight, int dotVestment = -1) {
        const bool hov = nr.Contains(lastMouseX_, lastMouseY_);
        GraphicsPath np;
        ui::AddRoundRect(np, nr, 7.0f * scale_);
        SolidBrush nbg(theme::Gc(highlight ? pal_.accent : (hov ? pal_.hover : pal_.inputBg),
                                 highlight ? 36 : 235));
        g.FillPath(&nbg, &np);
        Pen nb(theme::Gc(highlight || hov ? pal_.accent : pal_.cardBorder,
                         highlight || hov ? 160 : 200),
               1.0f);
        g.DrawPath(&nb, &np);
        RectF textRect = nr;
        if (dotVestment >= 0) {
            const float dotR = 4.5f * scale_;
            const float dx = nr.X + 10.0f * scale_;
            const float dy = nr.Y + nr.Height * 0.5f - dotR;
            SolidBrush vbr(theme::Gc(theme::VestmentColorRef(dotVestment, dark_)));
            g.FillEllipse(&vbr, dx, dy, dotR * 2.0f, dotR * 2.0f);
            Pen vpen(theme::Gc(pal_.cardBorder, 180), 1.0f);
            g.DrawEllipse(&vpen, dx, dy, dotR * 2.0f, dotR * 2.0f);
            textRect.X += 14.0f * scale_;
            textRect.Width -= 14.0f * scale_;
        }
        ui::Text(g, txt.c_str(), -1, highlight ? fUiBold_.get() : fUi_.get(), textRect, &cf,
                 theme::Gc(highlight ? pal_.accent : pal_.text));
    };

    // Кнопка «Следующий день ▶»
    rx -= 28.0f * scale_;
    btnNextDay_ = RectF(rx, navY, 28.0f * scale_, navH);
    drawNavPill(btnNextDay_, L"\u25B6", false);
    rx -= 5.0f * scale_;

    // Плитка даты (клик открывает миникалендарь выбора даты)
    std::wstring datePill =
        std::to_wstring(dinfo.g.d) + L" " + kShortMonths[std::clamp(dinfo.g.m - 1, 0, 11)] +
        L" / " + std::to_wstring(dinfo.j.d) + L" " +
        kShortMonths[std::clamp(dinfo.j.m - 1, 0, 11)] + L" ст.";
    if (GetCalendarOffsetDays() != 0) {
        datePill += L" \u21BA";
    }
    const float dpW = MeasureText(g, datePill, *fUiBold_, 2) + 30.0f * scale_;
    rx -= dpW;
    btnToday_ = RectF(rx, navY, dpW, navH);
    drawNavPill(btnToday_, datePill, GetCalendarOffsetDays() != 0, dinfo.vestmentColor);
    rx -= 5.0f * scale_;

    // Кнопка «◀ Предыдущий день»
    rx -= 28.0f * scale_;
    btnPrevDay_ = RectF(rx, navY, 28.0f * scale_, navH);
    drawNavPill(btnPrevDay_, L"\u25C0", false);
    rx -= 8.0f * scale_;

    // Бейдж гласа и поста
    std::wstring litBadge =
        (dinfo.glas > 0 ? (L"Глас " + std::to_wstring(dinfo.glas)) : L"Праздник") +
        L" · " + (dinfo.fast.isFast ? L"Пост" : L"Поста нет");
    const float lbW = MeasureText(g, litBadge, *fUi_, 2) + 18.0f * scale_;
    if (rx - lbW > area.X + pad + 340.0f * scale_) {
        rx -= lbW;
        const RectF badgeRect(rx, navY, lbW, navH);
        GraphicsPath bp;
        ui::AddRoundRect(bp, badgeRect, 7.0f * scale_);
        SolidBrush bbg(theme::Gc(pal_.card));
        g.FillPath(&bbg, &bp);
        Pen bpen(theme::Gc(pal_.divider), 1.0f);
        g.DrawPath(&bpen, &bp);
        ui::Text(g, litBadge.c_str(), -1, fUi_.get(), badgeRect, &cf,
                 theme::Gc(dinfo.fast.isFast ? pal_.rubric : pal_.textDim));
        rx -= 10.0f * scale_;
    }

    // --- заголовок раздела (слева во 2-м ряду) ---
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentNear);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    StringFormat titleFmt(StringFormat::GenericTypographic());
    titleFmt.SetTrimming(StringTrimmingEllipsisCharacter);

    const std::wstring title = search_.empty() ? sections_[static_cast<size_t>(section_)].title
                                                : L"Поиск молитв";
    // Заголовок и подзаголовок выравниваем по колонке чтения: на широких окнах
    // текст и карточки образуют единый блок, а не «висят» по разным краям.
    const float textLeft = area.X + std::max(pad, colX_);
    const float titleW =
        std::min(colW_, std::max(140.0f * scale_, rx - textLeft - 8.0f * scale_));
    RectF tbox(textLeft, area.Y + 46.0f * scale_, titleW, 40.0f * scale_);

    std::unique_ptr<Font> titleFont;
    const Font* tf = fTitle_.get();
    {
        const float px = static_cast<float>(fontSize_) * (dpi_ / 72.0f) * 1.44f;
        StringFormat mf(StringFormat::GenericTypographic());
        mf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                          StringFormatFlagsNoClip);
        auto measureTitle = [&](const Font& f) {
            RectF b;
            g.MeasureString(title.c_str(), -1, &f, PointF(0.0f, 0.0f), &mf, &b);
            return b.Width;
        };
        float size = px;
        float measured = measureTitle(*tf);
        for (int i = 0; i < 6 && measured > titleW - 2.0f * scale_ && size > 13.0f * scale_; ++i) {
            size = std::max(13.0f * scale_, size * (titleW / measured) * 0.97f);
            titleFont = std::make_unique<Font>(
                kFontFamilies[std::clamp(fontFamily_, 0, kFontFamilyCount - 1)], size,
                FontStyleBold, UnitPixel);
            tf = titleFont.get();
            measured = measureTitle(*tf);
        }
    }
    ui::Text(g, title.c_str(), -1, tf, tbox, &titleFmt, theme::Gc(pal_.title));

    // --- подзаголовок: церковный период, описание и счётчик ---
    std::wstring subtitle;
    if (!search_.empty()) {
        subtitle = L"Найдено молитв: " + std::to_wstring(view_.size());
    } else {
        subtitle = dinfo.period + L"  ·  " + sections_[static_cast<size_t>(section_)].desc;
        if (!cards_.empty()) {
            subtitle += L"  ·  карточек: " + std::to_wstring(view_.size());
        }
    }
    RectF sbox(textLeft, area.Y + 92.0f * scale_,
               std::max(200.0f * scale_, area.GetRight() - 20.0f * scale_ - textLeft),
               22.0f * scale_);
    if (search_.empty()) {
        StringFormat mf(StringFormat::GenericTypographic());
        mf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                          StringFormatFlagsNoClip);
        RectF b;
        g.MeasureString(subtitle.c_str(), -1, fUi_.get(), PointF(0.0f, 0.0f), &mf, &b);
        if (b.Width > sbox.Width) {
            // На узких окнах описание раздела не помещается — оставляем главное.
            subtitle = dinfo.period;
            if (!cards_.empty()) {
                subtitle += L"  ·  карточек: " + std::to_wstring(view_.size());
            }
        }
    }
    ui::Text(g, subtitle.c_str(), -1, fUi_.get(), sbox, &fmt, theme::Gc(pal_.textDim));

    // --- рисуем инструменты ---
    PaintIconButton(g, btnTheme_, 3, !dark_);
    PaintIconButton(g, btnSettings_, 6, panel_ == 2);
    PaintIconButton(g, btnContents_, 5, panel_ == 1);
    PaintIconButton(g, btnSizePlus_, 4, false);
    PaintIconButton(g, btnSizeMinus_, 2, false);

    {
        const RectF& r = btnLang_;
        const float rad = 8.0f * scale_;
        GraphicsPath path;
        path.AddArc(r.X, r.Y, rad, rad, 180, 90);
        path.AddArc(r.GetRight() - rad, r.Y, rad, rad, 270, 90);
        path.AddArc(r.GetRight() - rad, r.GetBottom() - rad, rad, rad, 0, 90);
        path.AddArc(r.X, r.GetBottom() - rad, rad, rad, 90, 90);
        path.CloseFigure();
        SolidBrush bgBrush(theme::Gc(pal_.inputBg));
        g.FillPath(&bgBrush, &path);
        if (lang_ == 1) {
            SolidBrush sel(theme::Gc(pal_.accent, 46));
            GraphicsPath selPath;
            selPath.AddArc(r.X + r.Width * 0.5f, r.Y, rad, rad, 180, 90);
            selPath.AddArc(r.GetRight() - rad, r.Y, rad, rad, 270, 90);
            selPath.AddArc(r.GetRight() - rad, r.GetBottom() - rad, rad, rad, 0, 90);
            selPath.AddArc(r.X + r.Width * 0.5f, r.GetBottom() - rad, rad, rad, 90, 90);
            selPath.CloseFigure();
            g.FillPath(&sel, &selPath);
        }
        StringFormat cf;
        cf.SetAlignment(StringAlignmentCenter);
        cf.SetLineAlignment(StringAlignmentCenter);
        ui::Text(g, L"РУ", -1, fUiBold_.get(), RectF(r.X, r.Y, r.Width * 0.5f, r.Height), &cf,
                 theme::Gc(lang_ == 0 ? pal_.accent : pal_.textDim));
        ui::Text(g, L"ЦС", -1, fUiBold_.get(),
                 RectF(r.X + r.Width * 0.5f, r.Y, r.Width * 0.5f, r.Height), &cf,
                 theme::Gc(lang_ == 1 ? pal_.accent : pal_.textDim));
    }

    PaintSearchBox(g, searchRect_);

    if (edit_ != nullptr) {
        const int left = static_cast<int>(30.0f * scale_);
        const int reserve = searchClearRect_.Width > 0.0f ? static_cast<int>(28.0f * scale_) : 0;
        SetWindowPos(edit_, nullptr, static_cast<int>(searchRect_.X + left),
                     static_cast<int>(searchRect_.Y + 2.0f * scale_),
                     std::max(10, static_cast<int>(searchRect_.Width) - left - reserve),
                     static_cast<int>(searchRect_.Height - 4.0f * scale_),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void App::PaintSearchBox(Graphics& g, const RectF& box) {
    const float rad = 9.0f * scale_;
    GraphicsPath path;
    path.AddArc(box.X, box.Y, rad, rad, 180, 90);
    path.AddArc(box.GetRight() - rad, box.Y, rad, rad, 270, 90);
    path.AddArc(box.GetRight() - rad, box.GetBottom() - rad, rad, rad, 0, 90);
    path.AddArc(box.X, box.GetBottom() - rad, rad, rad, 90, 90);
    path.CloseFigure();
    SolidBrush bg(theme::Gc(pal_.inputBg));
    g.FillPath(&bg, &path);
    if (GetFocus() == edit_) {
        Pen pen(theme::Gc(pal_.accent, 170), 1.4f);
        g.DrawPath(&pen, &path);
    }
    const RectF icon(box.X + 9.0f * scale_, box.Y + (box.Height - 16.0f * scale_) * 0.5f,
                     16.0f * scale_, 16.0f * scale_);
    icons::Magnifier(g, icon, theme::Gc(pal_.textDim), 1.4f * scale_);

    // кнопка очистки поиска
    searchClearRect_ = RectF();
    if (!search_.empty()) {
        const float d = 18.0f * scale_;
        searchClearRect_ = RectF(box.GetRight() - d - 7.0f * scale_,
                                 box.Y + (box.Height - d) * 0.5f, d, d);
        const bool hov = searchClearRect_.Contains(lastMouseX_, lastMouseY_);
        if (hov) {
            SolidBrush hb(theme::Gc(pal_.hover));
            g.FillEllipse(&hb, searchClearRect_);
        }
        Pen xp(theme::Gc(hov ? pal_.accent : pal_.textDim, hov ? 230 : 175), 1.5f * scale_);
        xp.SetStartCap(LineCapRound);
        xp.SetEndCap(LineCapRound);
        const float cx = searchClearRect_.X + d * 0.5f;
        const float cy = searchClearRect_.Y + d * 0.5f;
        const float r = 3.4f * scale_;
        g.DrawLine(&xp, cx - r, cy - r, cx + r, cy + r);
        g.DrawLine(&xp, cx + r, cy - r, cx - r, cy + r);
    }
    // Подсказку рисует сам EDIT (cue banner): родительское окно обрезано
    // по дочернему контролу (WS_CLIPCHILDREN), поэтому текст из PaintSearchBox
    // под полем ввода был не виден.
    if (edit_ != nullptr) {
        const wchar_t* placeholder = box.Width < 168.0f * scale_ ? L"Поиск…" : L"Поиск молитв…";
        if (cueBanner_ != placeholder) {
            cueBanner_ = placeholder;
            SendMessageW(edit_, 0x1501 /* EM_SETCUEBANNER */, TRUE,
                         reinterpret_cast<LPARAM>(placeholder));
        }
    }
}

void App::PaintIconButton(Graphics& g, const RectF& r, int icon, bool active) {
    if (active) {
        const float rad = 8.0f * scale_;
        GraphicsPath path;
        path.AddArc(r.X, r.Y, rad, rad, 180, 90);
        path.AddArc(r.GetRight() - rad, r.Y, rad, rad, 270, 90);
        path.AddArc(r.GetRight() - rad, r.GetBottom() - rad, rad, rad, 0, 90);
        path.AddArc(r.X, r.GetBottom() - rad, rad, rad, 90, 90);
        path.CloseFigure();
        SolidBrush bg(theme::Gc(pal_.hover));
        g.FillPath(&bg, &path);
    }
    const Color c = theme::Gc(active ? pal_.accent : pal_.textDim);
    if (icon == 3) {
        const RectF box(r.X + 8.0f * scale_, r.Y + 8.0f * scale_, r.Width - 16.0f * scale_,
                         r.Height - 16.0f * scale_);
        if (dark_) {
            icons::Sun(g, box, c, 1.6f * scale_);
        } else {
            icons::Moon(g, box, c, 1.6f * scale_);
        }
    } else {
        DrawButtonGlyph(g, r, icon, c);
    }
}

void App::DrawButtonGlyph(Graphics& g, const RectF& r, int icon, Color c) {
    StringFormat cf;
    cf.SetAlignment(StringAlignmentCenter);
    cf.SetLineAlignment(StringAlignmentCenter);
    if (icon == 2) {
        Font f(L"Cambria", 11.0f * scale_, FontStyleRegular, UnitPixel);
        ui::Text(g, L"A", -1, &f, r, &cf, c);
    } else if (icon == 4) {
        Font f(L"Cambria", 16.0f * scale_, FontStyleBold, UnitPixel);
        ui::Text(g, L"A", -1, &f, r, &cf, c);
    } else if (icon == 5) {
        // список (содержание)
        Pen pen(c, 1.6f * scale_);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        const float x0 = r.X + 9.0f * scale_;
        const float x1 = r.GetRight() - 9.0f * scale_;
        for (int i = 0; i < 3; ++i) {
            const float y = r.Y + 11.0f * scale_ + i * 6.0f * scale_;
            g.DrawLine(&pen, x0, y, x0 + 3.0f * scale_, y);
            g.DrawLine(&pen, x0 + 7.0f * scale_, y, x1, y);
        }
    } else if (icon == 6) {
        // ползунки (настройки)
        Pen pen(c, 1.6f * scale_);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        const float x0 = r.X + 9.0f * scale_;
        const float x1 = r.GetRight() - 9.0f * scale_;
        for (int i = 0; i < 3; ++i) {
            const float y = r.Y + 11.0f * scale_ + i * 6.0f * scale_;
            g.DrawLine(&pen, x0, y, x1, y);
            SolidBrush knob(c);
            const float kx = x0 + (x1 - x0) * (0.3f + 0.2f * static_cast<float>(i));
            g.FillEllipse(&knob, kx - 2.0f * scale_, y - 2.0f * scale_, 4.0f * scale_,
                          4.0f * scale_);
        }
    }
}

void App::DrawStar(Graphics& g, const RectF& r, Color c, bool filled) {
    icons::Star(g, r, c, 1.3f * scale_, filled);
}

RectF App::PanelRect() const {
    const RectF& anchor = (panel_ == 1) ? btnContents_ : btnSettings_;
    float w = (panel_ == 1) ? 392.0f : 384.0f;
    float h = (panel_ == 1) ? 430.0f : 560.0f;
    w *= scale_;
    h *= scale_;
    float x = anchor.X + anchor.Width * 0.5f - w * 0.5f;
    x = std::clamp(x, 10.0f, std::max(10.0f, clientWidth_ - w - 10.0f));
    const float y = anchor.GetBottom() + 10.0f * scale_;
    h = std::min(h, std::max(120.0f * scale_, clientHeight_ - y - 14.0f * scale_));
    return RectF(x, y, w, h);
}

void App::PaintPanelFrame(Graphics& g, const RectF& r, const std::wstring& title) {
    const float rad = 14.0f * scale_;
    // тень
    {
        GraphicsPath shadow;
        shadow.AddArc(r.X, r.Y + 5.0f * scale_, rad, rad, 180, 90);
        shadow.AddArc(r.GetRight() - rad, r.Y + 5.0f * scale_, rad, rad, 270, 90);
        shadow.AddArc(r.GetRight() - rad, r.GetBottom() + 5.0f * scale_ - rad, rad, rad, 0, 90);
        shadow.AddArc(r.X, r.GetBottom() + 5.0f * scale_ - rad, rad, rad, 90, 90);
        shadow.CloseFigure();
        SolidBrush sb(theme::Gc(pal_.shadow, dark_ ? 90 : 34));
        g.FillPath(&sb, &shadow);
    }
    GraphicsPath path;
    path.AddArc(r.X, r.Y, rad, rad, 180, 90);
    path.AddArc(r.GetRight() - rad, r.Y, rad, rad, 270, 90);
    path.AddArc(r.GetRight() - rad, r.GetBottom() - rad, rad, rad, 0, 90);
    path.AddArc(r.X, r.GetBottom() - rad, rad, rad, 90, 90);
    path.CloseFigure();
    LinearGradientBrush gb(PointF(r.X, r.Y), PointF(r.X, r.GetBottom()), theme::Gc(pal_.cardTop),
                           theme::Gc(pal_.card));
    g.FillPath(&gb, &path);
    Pen border(theme::Gc(pal_.cardBorder), 1.0f);
    g.DrawPath(&border, &path);

    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentNear);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    RectF box(r.X + 18.0f * scale_, r.Y + 12.0f * scale_, r.Width - 36.0f * scale_, 24.0f * scale_);
    ui::Text(g, title.c_str(), -1, fUiBold_.get(), box, &fmt, theme::Gc(pal_.title));

    Pen line(theme::Gc(pal_.divider, 150), 1.0f);
    g.DrawLine(&line, r.X + 14.0f * scale_, r.Y + 42.0f * scale_, r.GetRight() - 14.0f * scale_,
               r.Y + 42.0f * scale_);
}

void App::BuildContentsRows(const RectF& panel) {
    tocRows_.clear();
    panelItems_.clear();
    float y = panel.Y + 52.0f * scale_;
    std::wstring lastGroup;
    for (size_t i = 0; i < view_.size(); ++i) {
        const ViewItem& item = view_[i];
        if (!item.group.empty() && item.group != lastGroup) {
            lastGroup = item.group;
            TocRow row;
            row.text = item.group;
            row.y = y;
            row.h = 22.0f * scale_;
            row.header = true;
            tocRows_.push_back(row);
            y += row.h;
        }
        const Prayer* p = PrayerOf(item);
        std::wstring text;
        if (p != nullptr) {
            text = p->title;
            if (text.empty() && !p->paras.empty()) {
                text = p->paras.front().text;
                if (text.size() > 70) {
                    text = text.substr(0, 68) + L"…";
                }
            }
        }
        TocRow row;
        row.text = text;
        row.y = y;
        row.h = 26.0f * scale_;
        row.index = static_cast<int>(i);
        tocRows_.push_back(row);
        y += row.h;
    }
}

void App::PaintContentsPanel(Graphics& g) {
    const RectF panel = PanelRect();
    PaintPanelFrame(g, panel, L"Содержание");

    BuildContentsRows(panel);

    // текущая позиция
    int current = -1;
    for (const Card& card : cards_) {
        if (card.y + card.h > scroll_ + 4.0f * scale_) {
            current = card.item;
            break;
        }
    }

    const float footerH = 46.0f * scale_;
    const RectF listArea(panel.X, panel.Y, panel.Width, panel.Height - footerH);
    g.SetClip(listArea);
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentNear);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);

    for (const TocRow& row : tocRows_) {
        const float y = row.y - panelScroll_;
        if (y + row.h < panel.Y || y > panel.GetBottom()) {
            continue;
        }
        if (row.header) {
            RectF box(panel.X + 16.0f * scale_, y, panel.Width - 32.0f * scale_, row.h);
            ui::Text(g, UpperLetters(row.text).c_str(), -1, fGroup_.get(), box, &fmt,
                     theme::Gc(pal_.groupLabel));
            continue;
        }
        if (row.index == current) {
            SolidBrush hl(theme::Gc(pal_.accent, 40));
            GraphicsPath path;
            const float rad = 7.0f * scale_;
            path.AddArc(panel.X + 10.0f * scale_, y + 1.0f * scale_, rad, rad, 180, 90);
            path.AddArc(panel.GetRight() - 10.0f * scale_ - rad, y + 1.0f * scale_, rad, rad, 270, 90);
            path.AddArc(panel.GetRight() - 10.0f * scale_ - rad, y + row.h - 1.0f * scale_ - rad, rad,
                        rad, 0, 90);
            path.AddArc(panel.X + 10.0f * scale_, y + row.h - 1.0f * scale_ - rad, rad, rad, 90, 90);
            path.CloseFigure();
            g.FillPath(&hl, &path);
        }
        RectF box(panel.X + 18.0f * scale_, y, panel.Width - 34.0f * scale_, row.h);
        ui::Text(g, row.text.c_str(), -1, row.index == current ? fUiBold_.get() : fUi_.get(), box,
                 &fmt, theme::Gc(row.index == current ? pal_.accent : pal_.text));
    }
    g.ResetClip();

    // кнопка «случайная молитва» — отдельной строкой внизу панели
    {
        Pen sep(theme::Gc(pal_.divider, 150), 1.0f);
        g.DrawLine(&sep, panel.X + 14.0f * scale_, panel.GetBottom() - footerH,
                   panel.GetRight() - 14.0f * scale_, panel.GetBottom() - footerH);
        const RectF b(panel.X + 14.0f * scale_, panel.GetBottom() - footerH + 9.0f * scale_,
                      panel.Width - 28.0f * scale_, 28.0f * scale_);
        SolidBrush bg(theme::Gc(pal_.inputBg));
        GraphicsPath path;
        const float rad = 7.0f * scale_;
        path.AddArc(b.X, b.Y, rad, rad, 180, 90);
        path.AddArc(b.GetRight() - rad, b.Y, rad, rad, 270, 90);
        path.AddArc(b.GetRight() - rad, b.GetBottom() - rad, rad, rad, 0, 90);
        path.AddArc(b.X, b.GetBottom() - rad, rad, rad, 90, 90);
        path.CloseFigure();
        g.FillPath(&bg, &path);
        Pen border(theme::Gc(pal_.cardBorder), 1.0f);
        g.DrawPath(&border, &path);
        StringFormat cf;
        cf.SetAlignment(StringAlignmentCenter);
        cf.SetLineAlignment(StringAlignmentCenter);
        ui::Text(g, L"Случайная молитва", -1, fUi_.get(), b, &cf, theme::Gc(pal_.textDim));
        PanelItem item;
        item.rect = b;
        item.action = 20;
        panelItems_.push_back(item);
    }

    // полоса прокрутки списка
    const float contentH = tocRows_.empty() ? 0.0f : tocRows_.back().y + tocRows_.back().h;
    if (contentH > listArea.Height) {
        const float maxScroll = contentH - listArea.Height + 20.0f * scale_;
        const float t = std::clamp(panelScroll_ / std::max(1.0f, maxScroll), 0.0f, 1.0f);
        const float thumbH = std::max(30.0f * scale_, listArea.Height * (listArea.Height / contentH));
        const float ty = listArea.Y + (listArea.Height - thumbH) * t;
        SolidBrush brush(theme::Gc(pal_.textDim, 90));
        GraphicsPath path;
        const float rad = 4.0f * scale_;
        path.AddArc(panel.GetRight() - 12.0f * scale_, ty, rad, rad, 180, 90);
        path.AddArc(panel.GetRight() - 12.0f * scale_ + 6.0f * scale_, ty, rad, rad, 270, 90);
        path.AddArc(panel.GetRight() - 12.0f * scale_ + 6.0f * scale_, ty + thumbH - rad, rad, rad, 0,
                    90);
        path.AddArc(panel.GetRight() - 12.0f * scale_, ty + thumbH - rad, rad, rad, 90, 90);
        path.CloseFigure();
        g.FillPath(&brush, &path);
    }
}

void App::PaintSettingsPanel(Graphics& g) {
    const RectF panel = PanelRect();
    PaintPanelFrame(g, panel, L"Настройки чтения");
    panelItems_.clear();

    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    StringFormat lfmt;
    lfmt.SetAlignment(StringAlignmentNear);
    lfmt.SetLineAlignment(StringAlignmentCenter);
    lfmt.SetTrimming(StringTrimmingEllipsisCharacter);

    float y = panel.Y + 54.0f * scale_;
    const float left = panel.X + 18.0f * scale_;
    const float right = panel.GetRight() - 18.0f * scale_;
    const float chipH = 28.0f * scale_;

    auto drawChip = [&](const RectF& r, const std::wstring& label, int action, int index, bool active) {
        const float rad = 7.0f * scale_;
        GraphicsPath path;
        path.AddArc(r.X, r.Y, rad, rad, 180, 90);
        path.AddArc(r.GetRight() - rad, r.Y, rad, rad, 270, 90);
        path.AddArc(r.GetRight() - rad, r.GetBottom() - rad, rad, rad, 0, 90);
        path.AddArc(r.X, r.GetBottom() - rad, rad, rad, 90, 90);
        path.CloseFigure();
        SolidBrush bg(theme::Gc(active ? pal_.accent : pal_.inputBg, active ? 48 : 255));
        g.FillPath(&bg, &path);
        Pen border(theme::Gc(active ? pal_.accent : pal_.cardBorder, active ? 150 : 255), 1.0f);
        g.DrawPath(&border, &path);
        ui::Text(g, label.c_str(), -1, active ? fUiBold_.get() : fUi_.get(), r, &fmt,
                 theme::Gc(active ? pal_.accent : pal_.textDim));
        PanelItem item;
        item.rect = r;
        item.action = action;
        item.index = index;
        panelItems_.push_back(item);
    };

    auto label = [&](const std::wstring& text) {
        RectF box(left, y, right - left, 20.0f * scale_);
        ui::Text(g, text.c_str(), -1, fGroup_.get(), box, &lfmt, theme::Gc(pal_.groupLabel));
        y += 22.0f * scale_;
    };

    // --- шрифт ---
    label(L"Шрифт");
    {
        const wchar_t* names[] = {L"Cambria", L"Georgia", L"Constantia", L"Times", L"Palatino"};
        float x = left;
        for (int i = 0; i < 5; ++i) {
            const float w = MeasureText(g, names[i], *fUi_, 2) + 20.0f * scale_;
            if (x + w > right) {
                x = left;
                y += chipH + 6.0f * scale_;
            }
            drawChip(RectF(x, y, w, chipH), names[i], 10, i, fontFamily_ == i);
            x += w + 6.0f * scale_;
        }
        y += chipH + 14.0f * scale_;
    }

    // --- межстрочный интервал ---
    label(L"Межстрочный интервал");
    {
        const wchar_t* names[] = {L"Плотно", L"Обычно", L"Свободно"};
        const float values[] = {0.85f, 1.0f, 1.2f};
        const float gap = 6.0f * scale_;
        const float w = (right - left - gap * 2.0f) / 3.0f;
        for (int i = 0; i < 3; ++i) {
            drawChip(RectF(left + i * (w + gap), y, w, chipH), names[i], 11, i,
                     std::fabs(spacing_ - values[i]) < 0.05f);
        }
        y += chipH + 14.0f * scale_;
    }

    // --- ширина колонки ---
    label(L"Ширина колонки");
    {
        const wchar_t* names[] = {L"Узкая", L"Обычная", L"Широкая"};
        const float values[] = {0.78f, 1.0f, 1.25f};
        const float gap = 6.0f * scale_;
        const float w = (right - left - gap * 2.0f) / 3.0f;
        for (int i = 0; i < 3; ++i) {
            drawChip(RectF(left + i * (w + gap), y, w, chipH), names[i], 12, i,
                     std::fabs(columnScale_ - values[i]) < 0.05f);
        }
        y += chipH + 14.0f * scale_;
    }

    // --- оформление ---
    label(L"Оформление");
    {
        const wchar_t* names[] = {L"Тёмная", L"Светлая"};
        const float gap = 6.0f * scale_;
        const float w = (right - left - gap) / 2.0f;
        for (int i = 0; i < 2; ++i) {
            drawChip(RectF(left + i * (w + gap), y, w, chipH), names[i], 13, i, (dark_ ? 0 : 1) == i);
        }
        y += chipH + 14.0f * scale_;
    }

    // --- палитра темы ---
    label(L"Цветовая палитра");
    {
        const wchar_t* shortNames[] = {L"Лампада", L"Пергамент", L"Камень", L"Лазурь"};
        const float gap = 6.0f * scale_;
        const float w = (right - left - gap) / 2.0f;
        for (int i = 0; i < 4; ++i) {
            const int row = i / 2;
            const int col = i % 2;
            drawChip(RectF(left + col * (w + gap), y + row * (chipH + gap), w, chipH),
                     shortNames[i], 14, i, themeIndex_ == i);
        }
        y += chipH * 2.0f + gap + 14.0f * scale_;
    }

    // --- устав и поминальник ---
    label(L"Правило и поминальник");
    {
        const float gap = 6.0f * scale_;
        const float w = (right - left - gap) / 2.0f;
        drawChip(RectF(left, y, w, chipH),
                 reminders_ ? L"Напоминания: Вкл" : L"Напоминания: Выкл", 15, 0,
                 reminders_);
        drawChip(RectF(left + w + gap, y, w, chipH), L"Поминальник…", 16, 0, false);
    }

}

void App::PaintHelpPanel(Graphics& g) {
    const float w = 620.0f * scale_;
    const float h = 656.0f * scale_;
    const RectF r((clientWidth_ - w) * 0.5f, (clientHeight_ - h) * 0.5f, w, h);
    PaintPanelFrame(g, r, L"Горячие клавиши");

    struct Row {
        const wchar_t* key;
        const wchar_t* desc;
    };
    struct Group {
        const wchar_t* title;
        const Row* rows;
        int count;
    };
    static const Row readRows[] = {
        {L"Ctrl + K", L"командная палитра"},
        {L"Ctrl + F", L"перейти к поиску"},
        {L"Esc", L"очистить поиск / закрыть панель"},
        {L"Ctrl + C", L"копировать выделенный текст"},
        {L"Ctrl + Shift + C", L"копировать молитву целиком"},
        {L"Ctrl + A", L"выделить всё"},
        {L"Ctrl + + / Ctrl + −", L"размер шрифта"},
        {L"Ctrl + PgUp / PgDn", L"предыдущая / следующая молитва"},
        {L"Пробел", L"пролистнуть вниз"},
        {L"Shift + F10", L"меню молитвы"},
    };
    static const Row calRows[] = {
        {L"Alt + ← / →", L"предыдущий / следующий день"},
        {L"Alt + ↑ / ↓", L"предыдущая / следующая седмица"},
        {L"Alt + Home", L"сегодняшняя дата"},
        {L"Клик по дате", L"мини-календарь на месяц"},
    };
    static const Row navRows[] = {
        {L"Ctrl + 1…9", L"переход к разделу молитв"},
        {L"Ctrl + B", L"свернуть / развернуть меню"},
        {L"F11", L"режим чтения — только текст"},
        {L"Enter", L"+1 молитва (в разделе «Чётки»)"},
    };
    static const Group groups[] = {
        {L"ЧТЕНИЕ И ПОИСК", readRows, 10},
        {L"КАЛЕНДАРЬ", calRows, 4},
        {L"РАЗДЕЛЫ И ЧЁТКИ", navRows, 4},
    };

    StringFormat kf;
    kf.SetAlignment(StringAlignmentNear);
    kf.SetLineAlignment(StringAlignmentCenter);
    StringFormat df;
    df.SetAlignment(StringAlignmentNear);
    df.SetLineAlignment(StringAlignmentCenter);
    df.SetTrimming(StringTrimmingEllipsisCharacter);
    df.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    const float keyW = 170.0f * scale_;
    float y = r.Y + 52.0f * scale_;
    for (const Group& grp : groups) {
        ui::Text(g, grp.title, -1, fGroup_.get(),
                 RectF(r.X + 22.0f * scale_, y, r.Width - 44.0f * scale_, 20.0f * scale_), &df,
                 theme::Gc(pal_.groupLabel));
        y += 23.0f * scale_;
        for (int i = 0; i < grp.count; ++i) {
            const Row& row = grp.rows[i];
            const RectF kb(r.X + 22.0f * scale_, y, keyW, 21.0f * scale_);
            SolidBrush bg(theme::Gc(pal_.inputBg));
            GraphicsPath path;
            const float rad = 6.0f * scale_;
            path.AddArc(kb.X, kb.Y, rad, rad, 180, 90);
            path.AddArc(kb.GetRight() - rad, kb.Y, rad, rad, 270, 90);
            path.AddArc(kb.GetRight() - rad, kb.GetBottom() - rad, rad, rad, 0, 90);
            path.AddArc(kb.X, kb.GetBottom() - rad, rad, rad, 90, 90);
            path.CloseFigure();
            g.FillPath(&bg, &path);
            Pen bp(theme::Gc(pal_.cardBorder, 150), 1.0f);
            g.DrawPath(&bp, &path);
            ui::Text(g, row.key, -1, fUiBold_.get(), kb, &kf, theme::Gc(pal_.accent));
            RectF db(kb.GetRight() + 12.0f * scale_, y,
                     r.GetRight() - kb.GetRight() - 34.0f * scale_, 21.0f * scale_);
            ui::Text(g, row.desc, -1, fUi_.get(), db, &df, theme::Gc(pal_.textDim));
            y += 26.0f * scale_;
        }
        y += 6.0f * scale_;
    }

    Pen line(theme::Gc(pal_.divider, 140), 1.0f);
    g.DrawLine(&line, r.X + 22.0f * scale_, r.GetBottom() - 36.0f * scale_,
               r.GetRight() - 22.0f * scale_, r.GetBottom() - 36.0f * scale_);
    ui::Text(g, L"Esc, F1 или клик вне панели — закрыть", -1, fUi_.get(),
             RectF(r.X + 22.0f * scale_, r.GetBottom() - 30.0f * scale_,
                   r.Width - 44.0f * scale_, 20.0f * scale_),
             &df, theme::Gc(pal_.textDim, 160));
}

void App::RandomPrayer() {
    if (view_.empty()) {
        return;
    }
    const int pick = static_cast<int>(rand() % static_cast<int>(view_.size()));
    ScrollToCard(pick, true);
    ShowToast(L"Случайная молитва");
}

void App::DrawTooltip(Graphics& g) {
    if (panel_ != 0) {
        return;  // панель перекрывает кнопки — подсказки не нужны
    }
    if (tooltipText_.empty() || tooltipButton_ < 0) {
        return;
    }
    if (!tooltipVisible_) {
        return;
    }
    const float h = 26.0f * scale_;
    const float w = MeasureText(g, tooltipText_, *fUi_, 2) + 22.0f * scale_;
    RectF anchor;
    switch (tooltipButton_) {
        case 0: anchor = btnTheme_; break;
        case 1: anchor = btnSettings_; break;
        case 2: anchor = btnContents_; break;
        case 3: anchor = btnSizePlus_; break;
        case 4: anchor = btnSizeMinus_; break;
        case 5: anchor = btnLang_; break;
        default: anchor = searchRect_; break;
    }
    float x = anchor.X + anchor.Width * 0.5f - w * 0.5f;
    x = std::clamp(x, 6.0f, std::max(6.0f, clientWidth_ - w - 6.0f));
    const RectF box(x, anchor.GetBottom() + 8.0f * scale_, w, h);

    GraphicsPath path;
    const float rad = 8.0f * scale_;
    path.AddArc(box.X, box.Y, rad, rad, 180, 90);
    path.AddArc(box.GetRight() - rad, box.Y, rad, rad, 270, 90);
    path.AddArc(box.GetRight() - rad, box.GetBottom() - rad, rad, rad, 0, 90);
    path.AddArc(box.X, box.GetBottom() - rad, rad, rad, 90, 90);
    path.CloseFigure();
    SolidBrush bg(theme::Gc(pal_.card, 250));
    g.FillPath(&bg, &path);
    Pen border(theme::Gc(pal_.cardBorder), 1.0f);
    g.DrawPath(&border, &path);
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    ui::Text(g, tooltipText_.c_str(), -1, fUi_.get(), box, &fmt, theme::Gc(pal_.text));
}

void App::ShowToast(const std::wstring& text) {
    toast_ = text;
    toastSince_ = GetTickCount64() / 1000.0;
    toastUntil_ = toastSince_ + 2.7;
    if (hwnd_ != nullptr) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::PaintToast(Graphics& g) {
    const double now = GetTickCount64() / 1000.0;
    if (toast_.empty() || now > toastUntil_ + 0.05) {
        return;
    }
    const double t = now - toastSince_;
    const double left = toastUntil_ - now;
    // мягкое появление (180 мс) и угасание (420 мс)
    const double inP = std::clamp(t / 0.18, 0.0, 1.0);
    const double inEase = 1.0 - std::pow(1.0 - inP, 3.0);
    const double outP = std::clamp(left / 0.42, 0.0, 1.0);
    const int alpha = static_cast<int>(255 * std::min(inEase, outP));
    const float dy = static_cast<float>((1.0 - inEase) * 10.0 * scale_);

    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);

    float textW = 0.0f;
    {
        StringFormat mf(StringFormat::GenericTypographic());
        mf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                          StringFormatFlagsNoClip);
        RectF b;
        g.MeasureString(toast_.c_str(), -1, fUi_.get(), PointF(0.0f, 0.0f), &mf, &b);
        textW = b.Width;
    }
    const float w = std::clamp(textW + 40.0f * scale_, 220.0f * scale_,
                               std::min(560.0f * scale_, clientWidth_ * 0.6f));
    const float h = 42.0f * scale_;
    const RectF r(clientWidth_ - w - 28.0f * scale_, clientHeight_ - h - 26.0f * scale_ + dy, w, h);

    GraphicsPath path;
    ui::AddRoundRect(path, r, 12.0f * scale_);
    {
        GraphicsPath sh;
        ui::AddRoundRect(sh, RectF(r.X, r.Y + 3.0f * scale_, r.Width, r.Height), 12.0f * scale_);
        SolidBrush shb(theme::Gc(pal_.shadow, alpha / 3));
        g.FillPath(&shb, &sh);
    }
    SolidBrush sb(theme::Gc(pal_.card, alpha));
    g.FillPath(&sb, &path);
    Pen border(theme::Gc(pal_.accent, alpha * 2 / 3), 1.0f);
    g.DrawPath(&border, &path);
    // акцентная полоска слева
    SolidBrush bar(theme::Gc(pal_.accent, alpha));
    g.FillRectangle(&bar, RectF(r.X + 10.0f * scale_, r.Y + 12.0f * scale_, 3.0f * scale_,
                                r.Height - 24.0f * scale_));
    ui::Text(g, toast_.c_str(), -1, fUi_.get(), r, &fmt, theme::Gc(pal_.text, alpha));

    toastInvRect_.left = static_cast<LONG>(std::floor(r.X)) - 6;
    toastInvRect_.top = static_cast<LONG>(std::floor(r.Y)) - 8;
    toastInvRect_.right = static_cast<LONG>(std::ceil(r.GetRight())) + 6;
    toastInvRect_.bottom = static_cast<LONG>(std::ceil(r.GetBottom())) + 12;
}

void App::ScrollToCard(int viewIndex, bool smooth) {
    for (const Card& card : cards_) {
        if (card.item == viewIndex) {
            const float target = std::max(0.0f, card.y - 24.0f * scale_);
            if (smooth) {
                scrollTarget_ = target;
            } else {
                scroll_ = scrollTarget_ = target;
            }
            return;
        }
    }
}

void App::PaintContent(Graphics& g, const RectF& area) {
    SolidBrush bg(theme::Gc(pal_.bg));
    g.FillRectangle(&bg, area);

    if (view_.empty()) {
        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
        const float cx = area.X + area.Width * 0.5f;
        const float cy = area.Y + area.Height * 0.34f;

        if (search_.empty()) {
            icons::Book(g, RectF(cx - 26.0f * scale_, cy - 64.0f * scale_, 52.0f * scale_,
                                52.0f * scale_),
                        theme::Gc(pal_.textDim, 90), 1.6f * scale_);
        } else {
            icons::Magnifier(g, RectF(cx - 22.0f * scale_, cy - 64.0f * scale_, 44.0f * scale_,
                                      44.0f * scale_),
                             theme::Gc(pal_.textDim, 120), 1.8f * scale_);
        }

        const std::wstring head = search_.empty() ? L"Здесь пока пусто" : L"Ничего не найдено";
        const std::wstring hint =
            search_.empty()
                ? L"Отметьте молитвы звёздочкой — они появятся в разделе «Избранное»"
                : L"Попробуйте другое слово или переключите язык: РУ / ЦС";

        RectF box(area.X + 40.0f * scale_, cy, area.Width - 80.0f * scale_, 32.0f * scale_);
        ui::Text(g, head.c_str(), -1, fTitle_.get(), box, &fmt, theme::Gc(pal_.title));
        RectF box2(area.X + 40.0f * scale_, cy + 34.0f * scale_, area.Width - 80.0f * scale_,
                   24.0f * scale_);
        ui::Text(g, hint.c_str(), -1, fUi_.get(), box2, &fmt, theme::Gc(pal_.textDim));
        return;
    }

    GraphicsState state = g.Save();
    g.SetClip(area);
    g.TranslateTransform(area.X, area.Y - scroll_);

    cardBtns_.clear();
    // месячная сетка календаря — над карточками раздела
    PaintCalendarGrid(g);
    const float cardPadY = 24.0f * scale_;
    const float mouseLocalX = lastMouseX_ - area.X;
    const float mouseLocalY = lastMouseY_ - area.Y + scroll_;

    for (const Card& card : cards_) {
        const RectF r(colX_, card.y, colW_, card.h);
        if (r.GetBottom() < scroll_ - 60.0f * scale_ || r.Y > scroll_ + area.Height + 60.0f * scale_) {
            continue;
        }
        // тень
        {
            const float rad = 12.0f * scale_;
            GraphicsPath shadow;
            shadow.AddArc(r.X, r.Y + 4.0f * scale_, rad, rad, 180, 90);
            shadow.AddArc(r.GetRight() - rad, r.Y + 4.0f * scale_, rad, rad, 270, 90);
            shadow.AddArc(r.GetRight() - rad, r.GetBottom() + 4.0f * scale_ - rad, rad, rad, 0, 90);
            shadow.AddArc(r.X, r.GetBottom() + 4.0f * scale_ - rad, rad, rad, 90, 90);
            shadow.CloseFigure();
            SolidBrush sb(theme::Gc(pal_.shadow, dark_ ? 76 : 26));
            g.FillPath(&sb, &shadow);
        }
        // карточка
        {
            const float rad = 12.0f * scale_;
            GraphicsPath path;
            path.AddArc(r.X, r.Y, rad, rad, 180, 90);
            path.AddArc(r.GetRight() - rad, r.Y, rad, rad, 270, 90);
            path.AddArc(r.GetRight() - rad, r.GetBottom() - rad, rad, rad, 0, 90);
            path.AddArc(r.X, r.GetBottom() - rad, rad, rad, 90, 90);
            path.CloseFigure();
            LinearGradientBrush gb(PointF(r.X, r.Y), PointF(r.X, r.GetBottom()), theme::Gc(pal_.cardTop),
                                   theme::Gc(pal_.card));
            g.FillPath(&gb, &path);
            Pen border(theme::Gc(card.item == hoveredCard_ ? pal_.accent : pal_.cardBorder,
                                 card.item == hoveredCard_ ? 165 : 255),
                       card.item == hoveredCard_ ? 1.5f : 1.0f);
            g.DrawPath(&border, &path);
        }

        const int first = card.firstBlock;
        const int count = card.blockCount;
        bool drewDropCap = false;
        for (int bi = first; bi < first + count && bi < static_cast<int>(blocks_.size()); ++bi) {
            const Block& b = blocks_[static_cast<size_t>(bi)];
            const float x = textX_;
            const float w = colW_ - cardPadX_ * 2.0f;
            StringFormat fmt;
            fmt.SetAlignment(StringAlignmentNear);
            fmt.SetLineAlignment(StringAlignmentNear);
            switch (b.kind) {
                case KIND_GROUP: {
                    RectF box(x, b.y, w, b.h);
                    ui::Text(g, UpperLetters(b.text).c_str(), -1, fGroup_.get(), box, &fmt,
                                 theme::Gc(pal_.groupLabel));
                    break;
                }
                case KIND_TITLE: {
                    const ViewItem& citem = view_[static_cast<size_t>(card.item)];
                    const Prayer* cprayer = PrayerOf(citem);
                    const bool hasSameRowBtn =
                        (bi == first) &&
                        ((cprayer != nullptr && !cprayer->url.empty()) ||
                         sections_[static_cast<size_t>(citem.section)].id == L"pomyannik");
                    const float rightReserve =
                        (hasSameRowBtn ? 205.0f : 44.0f) * scale_;
                    const float availTitleW = std::max(120.0f * scale_, w - rightReserve);
                    RectF box(x, b.y, availTitleW, b.h);
                    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
                    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
                    std::unique_ptr<Font> shrunkTitle;
                    const Font* ctf = fTitle_.get();
                    const float mW = MeasureText(g, b.text, *fTitle_, 1);
                    if (mW > availTitleW && mW > 0.0f) {
                        const float px = static_cast<float>(fontSize_) * (dpi_ / 72.0f) * 1.44f;
                        const float sz =
                            std::max(13.0f * scale_, px * (availTitleW / mW) * 0.94f);
                        shrunkTitle = std::make_unique<Font>(
                            kFontFamilies[std::clamp(fontFamily_, 0, kFontFamilyCount - 1)],
                            sz, FontStyleBold, UnitPixel);
                        ctf = shrunkTitle.get();
                    }
                    ui::Text(g, b.text.c_str(), -1, ctf, box, &fmt, theme::Gc(pal_.title));
                    break;
                }
                case KIND_DIVIDER: {
                    ui::DrawOrnamentDivider(g, x, b.y, w, scale_, theme::Gc(pal_.divider),
                                            theme::Gc(pal_.accentDim));
                    break;
                }
                default:
                    break;
            }
            if (b.kind != KIND_TEXT && b.kind != KIND_RUBRIC) {
                continue;
            }
            const Font& font = (b.kind == KIND_RUBRIC) ? *fRubric_ : *fBody_;
            const Color color = (b.kind == KIND_RUBRIC) ? theme::Gc(pal_.rubric) : theme::Gc(pal_.text);
            StringFormat wf(StringFormat::GenericTypographic());
            wf.SetAlignment(StringAlignmentNear);
            wf.SetLineAlignment(StringAlignmentNear);
            wf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                               StringFormatFlagsNoClip);
            for (int li = b.firstLine; li < b.firstLine + b.lineCount; ++li) {
                const Line& line = lines_[static_cast<size_t>(li)];
                for (int wi = 0; wi < line.count; ++wi) {
                    const Word& w0 = words_[static_cast<size_t>(line.firstWord + wi)];
                    if (!searchLower_.empty()) {
                        const std::wstring word(b.text.c_str() + w0.start,
                                                static_cast<size_t>(w0.len));
                        if (WordMatches(word)) {
                            SolidBrush hl(theme::Gc(pal_.accent, 55));
                            g.FillRectangle(&hl, RectF(x + w0.x, line.y + 1.0f * scale_, w0.w,
                                                       line.h - 2.0f * scale_));
                        }
                    }
                    RectF wr(x + w0.x, line.y - 2.0f * scale_, w0.w + 6.0f, line.h + 4.0f);
                    ui::Text(g, b.text.c_str() + w0.start, w0.len, &font, wr, &wf, color);

                    // Уставная киноварная буквица для первого текстового слова карточки
                    if (!drewDropCap && b.kind == KIND_TEXT && li == b.firstLine && wi == 0 &&
                        w0.len > 0) {
                        drewDropCap = true;
                        int capIdx = w0.start;
                        if ((b.text[static_cast<size_t>(capIdx)] == L'\u00AB' ||
                             b.text[static_cast<size_t>(capIdx)] == L'"') &&
                            w0.len > 1) {
                            ++capIdx;
                        }
                        if (iswalpha(b.text[static_cast<size_t>(capIdx)])) {
                            int capLen = (capIdx - w0.start) + 1;
                            if (capLen < w0.len &&
                                b.text[static_cast<size_t>(w0.start + capLen)] == L'\u0301') {
                                ++capLen;
                            }
                            ui::Text(g, b.text.c_str() + w0.start, capLen, &font, wr, &wf,
                                     theme::Gc(pal_.dropCap));
                        }
                    }
                }
            }
        }

        // избранное и интерактивные кнопки действий на карточке
        const ViewItem& item = view_[static_cast<size_t>(card.item)];
        const bool fav = IsFavorite(item);
        DrawStar(g, RectF(card.starX, card.starY, 20.0f * scale_, 20.0f * scale_),
                 theme::Gc(fav ? pal_.accent : pal_.textDim), fav);

        const Section& sec = sections_[static_cast<size_t>(item.section)];
        const Prayer* prayer = PrayerOf(item);
        float bx = card.starX - 10.0f * scale_;
        const float by = card.starY - 3.0f * scale_;
        const float bh = 26.0f * scale_;

        StringFormat bf;
        bf.SetAlignment(StringAlignmentCenter);
        bf.SetLineAlignment(StringAlignmentCenter);
        bf.SetTrimming(StringTrimmingEllipsisCharacter);
        bf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);

        auto addCardButton = [&](const std::wstring& label, int action, const std::wstring& url = L"",
                                 bool primary = false) {
            const float bw = MeasureText(g, label, *fUiBold_, 2) + 22.0f * scale_;
            bx -= bw;
            const RectF br(bx, by, bw, bh);
            const bool hov = (panel_ == 0) && br.Contains(mouseLocalX, mouseLocalY);
            GraphicsPath bp;
            ui::AddRoundRect(bp, br, 7.0f * scale_);
            SolidBrush bbg(theme::Gc(primary || hov ? pal_.accent : pal_.inputBg,
                                     primary ? (hov ? 72 : 44) : (hov ? 44 : 240)));
            g.FillPath(&bbg, &bp);
            Pen bpen(theme::Gc(primary || hov ? pal_.accent : pal_.cardBorder,
                               primary || hov ? 185 : 210),
                     1.0f);
            g.DrawPath(&bpen, &bp);
            ui::Text(g, label.c_str(), -1, primary || hov ? fUiBold_.get() : fUi_.get(), br, &bf,
                     theme::Gc(primary || hov ? pal_.accent : pal_.text));
            cardBtns_.push_back({br, action, card.item, url});
            bx -= 6.0f * scale_;
        };

        if (prayer != nullptr && !prayer->url.empty()) {
            addCardButton(L"Открыть на сайте \u2197", 100, prayer->url, false);
        }
        if ((sec.id == L"pominalnik" && item.prayer == 0) || sec.id == L"pomyannik") {
            addCardButton(L"\u270E Изменить списки", 101, L"", true);
        }
        if (sec.id == L"azbyka" && item.prayer == 0) {
            addCardButton(L"\u21BB Обновить", 102, L"", false);
        }
        if (sec.id == L"chetki" && item.prayer == 0) {
            addCardButton(L"Сброс", 105, L"", false);
            addCardButton(L"Круг: " + std::to_wstring(chetki::Target()), 104, L"", false);
            addCardButton(L"+1 Молитва", 103, L"", true);
            // кольцо прогресса и клик по нему
            if (chetkiRingRect_.Height > 0.0f) {
                PaintChetkiProgress(g);
                const float cx = chetkiRingRect_.X + 68.0f * scale_;
                const float cy = chetkiRingRect_.Y + chetkiRingRect_.Height * 0.5f;
                const float hr = 54.0f * scale_;
                cardBtns_.push_back({RectF(cx - hr, cy - hr, hr * 2.0f, hr * 2.0f), 103,
                                     card.item, L""});
            }
        }
    }

    // выделение текста
    if (hasSelection_ && !blocks_.empty()) {
        for (size_t li = 0; li < lines_.size(); ++li) {
            const Line& line = lines_[li];
            for (int wi = 0; wi < line.count; ++wi) {
                const Word& w0 = words_[static_cast<size_t>(line.firstWord + wi)];
                if (!IsSelected(line.block, w0.start, w0.start + w0.len)) {
                    continue;
                }
                SolidBrush sb(theme::Gc(pal_.selection, 120));
                g.FillRectangle(&sb, RectF(textX_ + w0.x, line.y + 1.0f, w0.w + 1.0f, line.h - 2.0f));
            }
        }
        for (size_t li = 0; li < lines_.size(); ++li) {
            const Line& line = lines_[li];
            const Block& b = blocks_[static_cast<size_t>(line.block)];
            if (b.kind != KIND_TEXT && b.kind != KIND_RUBRIC) {
                continue;
            }
            const Font& font = (b.kind == KIND_RUBRIC) ? *fRubric_ : *fBody_;
            const Color color = (b.kind == KIND_RUBRIC) ? theme::Gc(pal_.rubric) : theme::Gc(pal_.text);
            StringFormat wf(StringFormat::GenericTypographic());
            wf.SetAlignment(StringAlignmentNear);
            wf.SetLineAlignment(StringAlignmentNear);
            wf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                               StringFormatFlagsNoClip);
            for (int wi = 0; wi < line.count; ++wi) {
                const Word& w0 = words_[static_cast<size_t>(line.firstWord + wi)];
                if (!IsSelected(line.block, w0.start, w0.start + w0.len)) {
                    continue;
                }
                RectF wr(textX_ + w0.x, line.y - 2.0f * scale_, w0.w + 6.0f, line.h + 4.0f);
                ui::Text(g, b.text.c_str() + w0.start, w0.len, &font, wr, &wf, color);
            }
        }
    }

    g.Restore(state);
    PaintProgress(g, area);
    PaintScrollbar(g, area);

    // кнопка «вверх» появляется после прокрутки
    topButton_ = RectF();
    if (scroll_ > 320.0f * scale_ && panel_ == 0) {
        const float d = 40.0f * scale_;
        const RectF r(area.GetRight() - d - 22.0f * scale_, area.GetBottom() - d - 22.0f * scale_, d, d);
        topButton_ = r;
        SolidBrush bg(theme::Gc(pal_.card, 235));
        GraphicsPath path;
        path.AddEllipse(r.X, r.Y, r.Width, r.Height);
        g.FillPath(&bg, &path);
        Pen border(theme::Gc(pal_.cardBorder), 1.0f);
        g.DrawPath(&border, &path);
        Pen arrow(theme::Gc(pal_.accent), 1.8f * scale_);
        arrow.SetStartCap(LineCapRound);
        arrow.SetEndCap(LineCapRound);
        const float cx = r.X + r.Width * 0.5f;
        const float cy = r.Y + r.Height * 0.5f;
        g.DrawLine(&arrow, cx - 7.0f * scale_, cy + 3.0f * scale_, cx, cy - 4.0f * scale_);
        g.DrawLine(&arrow, cx, cy - 4.0f * scale_, cx + 7.0f * scale_, cy + 3.0f * scale_);
    }
}

void App::PaintProgress(Graphics& g, const RectF& area) {
    if (contentHeight_ <= area.Height) {
        return;
    }
    const float h = 3.0f * scale_;
    const float t = std::clamp(scroll_ / std::max(1.0f, contentHeight_ - area.Height), 0.0f, 1.0f);
    SolidBrush track(theme::Gc(pal_.divider, 120));
    g.FillRectangle(&track, RectF(area.X, area.Y, area.Width, h));
    SolidBrush fill(theme::Gc(pal_.accent, 190));
    g.FillRectangle(&fill, RectF(area.X, area.Y, area.Width * t, h));
}

void App::PaintFocusChrome(Graphics& g, const RectF& client) {
    // Плавающая кнопка выхода из режима чтения. На широком окне — с подписью,
    // на узком — компактная пилюля «F11», чтобы не перекрывать колонку текста.
    const bool wide = client.Width >= 1400.0f * scale_;
    const float w = (wide ? 194.0f : 78.0f) * scale_;
    const float h = 30.0f * scale_;
    focusExitRect_ = RectF(client.GetRight() - w - 18.0f * scale_, 14.0f * scale_, w, h);
    const bool hov = focusExitRect_.Contains(lastMouseX_, lastMouseY_);

    GraphicsPath path;
    ui::AddRoundRect(path, focusExitRect_, h * 0.5f);
    SolidBrush bg(theme::Gc(pal_.card, hov ? 240 : 200));
    g.FillPath(&bg, &path);
    Pen border(theme::Gc(hov ? pal_.accent : pal_.cardBorder, hov ? 200 : 145), 1.0f);
    g.DrawPath(&border, &path);

    // значок «развернуть обратно»
    const float ix = focusExitRect_.X + (wide ? 16.0f : 13.0f) * scale_;
    const float iy = focusExitRect_.Y + h * 0.5f;
    Pen icon(theme::Gc(hov ? pal_.accent : pal_.textDim), 1.4f * scale_);
    icon.SetStartCap(LineCapRound);
    icon.SetEndCap(LineCapRound);
    g.DrawRectangle(&icon, ix - 5.0f * scale_, iy - 5.0f * scale_, 10.0f * scale_,
                    10.0f * scale_);
    g.DrawLine(&icon, ix + 1.0f * scale_, iy + 1.0f * scale_, ix + 7.0f * scale_,
               iy - 5.0f * scale_);
    g.DrawLine(&icon, ix + 7.0f * scale_, iy - 5.0f * scale_, ix + 3.0f * scale_,
               iy - 5.0f * scale_);
    g.DrawLine(&icon, ix + 7.0f * scale_, iy - 5.0f * scale_, ix + 7.0f * scale_,
               iy - 1.0f * scale_);

    StringFormat fmt;
    fmt.SetAlignment(wide ? StringAlignmentNear : StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    ui::Text(g, wide ? L"F11 · обычный вид" : L"F11", -1,
             wide ? fUi_.get() : fUiBold_.get(),
             RectF(focusExitRect_.X + (wide ? 30.0f : 24.0f) * scale_, focusExitRect_.Y,
                   focusExitRect_.Width - (wide ? 36.0f : 28.0f) * scale_,
                   focusExitRect_.Height),
             &fmt, theme::Gc(hov ? pal_.text : pal_.textDim));
}

void App::SetFocusMode(bool on) {
    if (hwnd_ == nullptr || on == focusMode_) {
        return;
    }
    if (on) {
        savedPlacement_.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd_, &savedPlacement_);
        savedStyle_ = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        savedExStyle_ = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);

        CloseDatePopup();
        panel_ = 0;
        if (GetFocus() == edit_) {
            SetFocus(hwnd_);
        }
        if (edit_ != nullptr) {
            ShowWindow(edit_, SW_HIDE);
        }

        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(MONITORINFO)};
        focusMode_ = true;
        if (GetMonitorInfoW(monitor, &mi)) {
            SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, savedExStyle_ | WS_EX_APPWINDOW);
            SetWindowPos(hwnd_, HWND_TOP, mi.rcWork.left, mi.rcWork.top,
                         mi.rcWork.right - mi.rcWork.left,
                         mi.rcWork.bottom - mi.rcWork.top,
                         SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
        sidebarW_ = 0.0f;
        headerH_ = 0.0f;
        layoutDirty_ = true;
        ShowToast(L"Режим чтения · F11 или Esc — выйти");
        InvalidateRect(hwnd_, nullptr, FALSE);
    } else {
        focusMode_ = false;
        focusExitRect_ = RectF();
        focusExitHover_ = false;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, savedStyle_);
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, savedExStyle_);
        SetWindowPlacement(hwnd_, &savedPlacement_);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        if (edit_ != nullptr) {
            ShowWindow(edit_, SW_SHOW);
        }
        OnSize(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

// ---------------------------------------------------------------- командная палитра

void App::OpenPalette() {
    if (paletteOpen_) {
        return;
    }
    CloseDatePopup();
    panel_ = 0;
    paletteOpen_ = true;
    paletteOpenedAt_ = GetTickCount64();
    paletteQuery_.clear();
    paletteSel_ = 0;
    paletteHover_ = -1;
    paletteScroll_ = 0;
    BuildPaletteItems();
    RefreshPalette();
    if (GetFocus() == edit_) {
        SetFocus(hwnd_);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::ClosePalette() {
    if (!paletteOpen_) {
        return;
    }
    paletteOpen_ = false;
    paletteHover_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::BuildPaletteItems() {
    paletteItems_.clear();
    for (size_t i = 0; i < sections_.size(); ++i) {
        const Section& s = sections_[i];
        if (s.id == L"favorites" && favorites_.empty()) {
            continue;
        }
        PaletteItem it;
        it.label = s.title;
        it.hint = s.group;
        it.id = s.id;
        it.section = static_cast<int>(i);
        paletteItems_.push_back(std::move(it));
    }
    auto add = [&](const wchar_t* label, const wchar_t* hint, int action) {
        PaletteItem it;
        it.label = label;
        it.hint = hint;
        it.action = action;
        paletteItems_.push_back(std::move(it));
    };
    add(L"Настройки чтения", L"Оформление и шрифт", 2);
    add(L"Содержание раздела", L"Оглавление", 3);
    add(L"Горячие клавиши", L"Справка · F1", 4);
    add(dark_ ? L"Светлая тема" : L"Тёмная тема", L"Сменить оформление", 1);
    add(sidebarCollapsed_ ? L"Развернуть меню" : L"Свернуть меню", L"Ctrl + B", 5);
    add(focusMode_ ? L"Обычный вид" : L"Режим чтения", L"F11", 6);
    add(L"Случайная молитва", L"Перейти к случайной", 7);
    add(L"Сегодня", L"Текущая дата", 8);
    add(L"Поиск по молитвам", L"Ctrl + F", 9);
    if (!paletteQuery_.empty()) {
        PaletteItem it;
        it.label = L"Искать «" + paletteQuery_ + L"»";
        it.hint = L"Во всех молитвах";
        it.action = 10;
        paletteItems_.push_back(std::move(it));
    }
}

void App::RefreshPalette() {
    paletteFiltered_.clear();
    const std::wstring q = Lower(paletteQuery_);
    for (size_t i = 0; i < paletteItems_.size(); ++i) {
        if (q.empty()) {
            paletteFiltered_.push_back(static_cast<int>(i));
            continue;
        }
        const PaletteItem& it = paletteItems_[i];
        if (Lower(it.label).find(q) != std::wstring::npos ||
            Lower(it.hint).find(q) != std::wstring::npos) {
            paletteFiltered_.push_back(static_cast<int>(i));
        }
    }
    if (paletteFiltered_.empty()) {
        paletteSel_ = 0;
    } else if (paletteSel_ >= static_cast<int>(paletteFiltered_.size())) {
        paletteSel_ = static_cast<int>(paletteFiltered_.size()) - 1;
    }
    if (paletteSel_ < 0) {
        paletteSel_ = 0;
    }
    paletteHover_ = -1;
}

void App::ActivatePaletteItem(int filteredIndex) {
    if (filteredIndex < 0 || filteredIndex >= static_cast<int>(paletteFiltered_.size())) {
        return;
    }
    const PaletteItem item = paletteItems_[static_cast<size_t>(paletteFiltered_[filteredIndex])];
    ClosePalette();
    if (item.section >= 0) {
        ApplySection(item.section);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    switch (item.action) {
        case 1:
            dark_ = !dark_;
            themeIndex_ = dark_ ? 0 : 1;
            ApplyWindowTheme();
            SaveSettings();
            break;
        case 2:
            if (focusMode_) {
                SetFocusMode(false);
            }
            panel_ = 2;
            panelScroll_ = 0;
            break;
        case 3:
            if (focusMode_) {
                SetFocusMode(false);
            }
            panel_ = 1;
            panelScroll_ = 0;
            break;
        case 4:
            if (focusMode_) {
                SetFocusMode(false);
            }
            panel_ = 3;
            break;
        case 5:
            sidebarCollapsed_ = !sidebarCollapsed_;
            sidebarW_ = focusMode_ ? 0.0f : (sidebarCollapsed_ ? 70.0f : 272.0f) * scale_;
            layoutDirty_ = true;
            SaveSettings();
            break;
        case 6:
            SetFocusMode(!focusMode_);
            break;
        case 7:
            RandomPrayer();
            break;
        case 8:
            ApplyCalendarOffset(0, true);
            break;
        case 9:
            if (focusMode_) {
                SetFocusMode(false);
            }
            if (edit_ != nullptr) {
                SetFocus(edit_);
            }
            break;
        case 10:
            if (focusMode_) {
                SetFocusMode(false);
            }
            if (edit_ != nullptr) {
                SetWindowTextW(edit_, paletteQuery_.c_str());
                SetFocus(edit_);
            }
            break;
        default:
            break;
    }
    if (edit_ != nullptr) {
        InvalidateRect(edit_, nullptr, TRUE);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::PaintPalette(Graphics& g, const RectF& client) {
    // затемнение фона
    SolidBrush dim(theme::Gc(RGB(0, 0, 0), dark_ ? 150 : 78));
    g.FillRectangle(&dim, client);

    const float w = std::min(660.0f * scale_, client.Width - 40.0f * scale_);
    const float h = std::min(520.0f * scale_, client.Height - 40.0f * scale_);
    const RectF panel((client.Width - w) * 0.5f, (client.Height - h) * 0.5f, w, h);
    paletteRect_ = panel;
    const float rad = 18.0f * scale_;

    // тень
    GraphicsPath shadow;
    ui::AddRoundRect(shadow, RectF(panel.X, panel.Y + 8.0f * scale_, panel.Width, panel.Height), rad);
    SolidBrush sb(theme::Gc(pal_.shadow, dark_ ? 120 : 44));
    g.FillPath(&sb, &shadow);

    GraphicsPath path;
    ui::AddRoundRect(path, panel, rad);
    LinearGradientBrush pg(PointF(panel.X, panel.Y), PointF(panel.X, panel.GetBottom()),
                           theme::Gc(pal_.cardTop), theme::Gc(pal_.card));
    g.FillPath(&pg, &path);
    Pen border(theme::Gc(pal_.cardBorder, 220), 1.0f);
    g.DrawPath(&border, &path);

    const float headerH = 64.0f * scale_;
    const float footerH = 48.0f * scale_;
    const float queryX = panel.X + 56.0f * scale_;
    const float queryY = panel.Y + 15.0f * scale_;
    const float queryW = panel.Width - 80.0f * scale_;

    // значок поиска
    const RectF searchIcon(panel.X + 24.0f * scale_, panel.Y + 22.0f * scale_,
                           20.0f * scale_, 20.0f * scale_);
    icons::Magnifier(g, searchIcon,
                     theme::Gc(paletteQuery_.empty() ? pal_.textDim : pal_.accent), 1.5f * scale_);

    StringFormat qf;
    qf.SetAlignment(StringAlignmentNear);
    qf.SetLineAlignment(StringAlignmentCenter);
    qf.SetTrimming(StringTrimmingEllipsisCharacter);
    qf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);

    float caretX = queryX;
    if (paletteQuery_.empty()) {
        ui::Text(g, L"Раздел, действие или молитва…", -1, fBody_.get(),
                 RectF(queryX, queryY, queryW, 32.0f * scale_), &qf,
                 theme::Gc(pal_.textDim, 170));
    } else {
        StringFormat mf(StringFormat::GenericTypographic());
        mf.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                          StringFormatFlagsNoClip);
        RectF mb;
        g.MeasureString(paletteQuery_.c_str(), -1, fBodyBold_.get(), PointF(0.0f, 0.0f), &mf, &mb);
        caretX = queryX + std::min(mb.Width, queryW - 8.0f * scale_) + 2.0f * scale_;
        ui::Text(g, paletteQuery_.c_str(), -1, fBodyBold_.get(),
                 RectF(queryX, queryY, queryW, 32.0f * scale_), &qf, theme::Gc(pal_.text));
    }
    if ((GetTickCount64() / 530) % 2 == 0) {
        SolidBrush caret(theme::Gc(pal_.accent));
        g.FillRectangle(&caret, RectF(caretX, panel.Y + 20.0f * scale_, 1.6f * scale_,
                                      24.0f * scale_));
    }

    Pen divider(theme::Gc(pal_.divider, 150), 1.0f);
    g.DrawLine(&divider, panel.X + 20.0f * scale_, panel.Y + headerH,
               panel.GetRight() - 20.0f * scale_, panel.Y + headerH);

    const float listTop = panel.Y + headerH + 6.0f * scale_;
    const float listBottom = panel.GetBottom() - footerH;
    const float rowH = 38.0f * scale_;
    const int maxVisible = std::max(1, static_cast<int>((listBottom - listTop) / rowH));
    const int total = static_cast<int>(paletteFiltered_.size());
    if (paletteSel_ < paletteScroll_) {
        paletteScroll_ = paletteSel_;
    }
    if (paletteSel_ >= paletteScroll_ + maxVisible) {
        paletteScroll_ = paletteSel_ - maxVisible + 1;
    }
    paletteScroll_ = std::clamp(paletteScroll_, 0, std::max(0, total - maxVisible));

    paletteRows_.assign(paletteFiltered_.size(), RectF());

    StringFormat lf;
    lf.SetAlignment(StringAlignmentNear);
    lf.SetLineAlignment(StringAlignmentCenter);
    lf.SetTrimming(StringTrimmingEllipsisCharacter);
    lf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    StringFormat hf;
    hf.SetAlignment(StringAlignmentFar);
    hf.SetLineAlignment(StringAlignmentCenter);
    hf.SetTrimming(StringTrimmingEllipsisCharacter);
    hf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    StringFormat cf;
    cf.SetAlignment(StringAlignmentCenter);
    cf.SetLineAlignment(StringAlignmentCenter);

    if (total == 0) {
        ui::Text(g, L"Ничего не найдено", -1, fUiBold_.get(),
                 RectF(panel.X, listTop + 110.0f * scale_, panel.Width, 30.0f * scale_), &cf,
                 theme::Gc(pal_.text));
        ui::Text(g, L"Попробуйте другое слово или действие", -1, fUi_.get(),
                 RectF(panel.X, listTop + 142.0f * scale_, panel.Width, 26.0f * scale_), &cf,
                 theme::Gc(pal_.textDim, 180));
    } else {
        const int last = std::min(total, paletteScroll_ + maxVisible);
        for (int i = paletteScroll_; i < last; ++i) {
            const PaletteItem& item = paletteItems_[static_cast<size_t>(paletteFiltered_[i])];
            const float ry = listTop + static_cast<float>(i - paletteScroll_) * rowH;
            const RectF rr(panel.X + 10.0f * scale_, ry, panel.Width - 20.0f * scale_,
                           rowH - 2.0f * scale_);
            paletteRows_[static_cast<size_t>(i)] = rr;

            const bool selected = (i == paletteSel_);
            const bool hover = (i == paletteHover_);
            if (selected || hover) {
                GraphicsPath rp;
                ui::AddRoundRect(rp, rr, 9.0f * scale_);
                SolidBrush rb(theme::Gc(selected ? pal_.accent : pal_.hover,
                                        selected ? 30 : 150));
                g.FillPath(&rb, &rp);
                if (selected) {
                    SolidBrush bar(theme::Gc(pal_.accent));
                    g.FillRectangle(&bar, RectF(rr.X + 3.0f * scale_, rr.Y + 9.0f * scale_,
                                                3.0f * scale_, rr.Height - 18.0f * scale_));
                }
            }

            const RectF iconRect(rr.X + 18.0f * scale_, rr.Y + (rr.Height - 20.0f * scale_) * 0.5f,
                                 20.0f * scale_, 20.0f * scale_);
            const Color iconColor = theme::Gc(selected ? pal_.accent : pal_.textDim, 220);
            if (!item.id.empty()) {
                DrawSectionIcon(g, iconRect, item.id, iconColor);
            } else {
                Pen ip(iconColor, 1.5f * scale_);
                ip.SetStartCap(LineCapRound);
                ip.SetEndCap(LineCapRound);
                const float cx = iconRect.X + 10.0f * scale_;
                const float cy = iconRect.Y + 10.0f * scale_;
                g.DrawLine(&ip, cx, cy - 5.0f * scale_, cx + 5.0f * scale_, cy);
                g.DrawLine(&ip, cx + 5.0f * scale_, cy, cx, cy + 5.0f * scale_);
                g.DrawLine(&ip, cx, cy + 5.0f * scale_, cx - 5.0f * scale_, cy);
                g.DrawLine(&ip, cx - 5.0f * scale_, cy, cx, cy - 5.0f * scale_);
            }

            const RectF labelRect(rr.X + 50.0f * scale_, rr.Y,
                                  rr.Width - 50.0f * scale_ - 224.0f * scale_, rr.Height);
            ui::Text(g, item.label.c_str(), -1, fUiBold_.get(), labelRect, &lf,
                     theme::Gc(pal_.text, 235));
            const RectF hintRect(rr.GetRight() - 214.0f * scale_, rr.Y, 200.0f * scale_,
                                 rr.Height);
            ui::Text(g, item.hint.c_str(), -1, fUi_.get(), hintRect, &hf,
                     theme::Gc(selected ? pal_.accent : pal_.textDim, selected ? 210 : 165));
        }
    }

    // тонкий индикатор прокрутки, если элементов больше, чем помещается
    if (total > maxVisible) {
        const float trackX = panel.GetRight() - 9.0f * scale_;
        const float trackW = 3.0f * scale_;
        const float trackY = listTop + 4.0f * scale_;
        const float trackH = (listBottom - listTop) - 8.0f * scale_;
        SolidBrush st(theme::Gc(pal_.divider, 90));
        g.FillRectangle(&st, RectF(trackX, trackY, trackW, trackH));
        const float frac = static_cast<float>(maxVisible) / static_cast<float>(total);
        const float thumbH = std::max(24.0f * scale_, trackH * frac);
        const float maxScroll = static_cast<float>(total - maxVisible);
        const float t = maxScroll > 0.0f ? static_cast<float>(paletteScroll_) / maxScroll : 0.0f;
        const float thumbY = trackY + (trackH - thumbH) * t;
        SolidBrush th(theme::Gc(pal_.accent, 170));
        g.FillRectangle(&th, RectF(trackX, thumbY, trackW, thumbH));
    }

    // нижняя подсказка
    g.DrawLine(&divider, panel.X + 20.0f * scale_, panel.GetBottom() - footerH,
               panel.GetRight() - 20.0f * scale_, panel.GetBottom() - footerH);
    ui::Text(g, L"↑ ↓ — выбор     Enter — открыть     Esc — закрыть", -1, fUi_.get(),
             RectF(panel.X + 22.0f * scale_, panel.GetBottom() - footerH + 12.0f * scale_,
                   panel.Width - 140.0f * scale_, 26.0f * scale_),
             &lf, theme::Gc(pal_.textDim, 190));

    const float kw = 74.0f * scale_;
    const float kh = 22.0f * scale_;
    const RectF kr(panel.GetRight() - 22.0f * scale_ - kw,
                   panel.GetBottom() - footerH + 14.0f * scale_, kw, kh);
    GraphicsPath kp;
    ui::AddRoundRect(kp, kr, 6.0f * scale_);
    SolidBrush kb(theme::Gc(pal_.inputBg));
    g.FillPath(&kb, &kp);
    Pen kbp(theme::Gc(pal_.cardBorder, 160), 1.0f);
    g.DrawPath(&kbp, &kp);
    ui::Text(g, L"Ctrl + K", -1, fUiBold_.get(), kr, &cf, theme::Gc(pal_.accent));
}

LRESULT CALLBACK App::EditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr && msg == WM_KEYDOWN) {
        if (wp == VK_CONTROL || wp == VK_LCONTROL || wp == VK_RCONTROL) {
            self->ctrlDown_ = true;
        }
        const bool ctrl = self->ctrlDown_ || (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if ((wp == 'K' && ctrl) || wp == VK_F11 || wp == VK_ESCAPE) {
            self->OnKey(self->hwnd_, wp);
            return 0;
        }
    }
    if (self != nullptr && msg == WM_KEYUP) {
        if (wp == VK_CONTROL || wp == VK_LCONTROL || wp == VK_RCONTROL) {
            self->ctrlDown_ = false;
        }
    }
    if (self != nullptr && msg == WM_MOUSEMOVE) {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ClientToScreen(hwnd, &pt);
        ScreenToClient(self->hwnd_, &pt);
        self->UpdateHover(self->hwnd_, pt.x, pt.y);
    }
    if (self != nullptr && msg == WM_CHAR && self->paletteOpen_ &&
        GetTickCount64() - self->paletteOpenedAt_ < 120) {
        return 0;
    }
    WNDPROC proc = (self != nullptr) ? self->editProc_ : DefWindowProcW;
    return CallWindowProcW(proc, hwnd, msg, wp, lp);
}

void App::PaintScrollbar(Graphics& g, const RectF& area) {
    scrollTrack_ = RectF();
    scrollThumb_ = RectF();
    if (contentHeight_ <= area.Height + 1.0f) {
        return;
    }
    const float trackW = 14.0f * scale_;
    const float x = area.GetRight() - trackW;
    const float top = area.Y;
    const float height = area.Height;
    const float thumbH = std::max(48.0f * scale_, height * (height / contentHeight_));
    const float maxScroll = contentHeight_ - area.Height;
    const float t = maxScroll > 0 ? std::clamp(scroll_ / maxScroll, 0.0f, 1.0f) : 0.0f;
    const float y = top + (height - thumbH) * t;

    scrollTrack_ = RectF(x, top, trackW, height);
    scrollThumb_ = RectF(x + 3.0f * scale_, y, trackW - 6.0f * scale_, thumbH);

    // дорожка
    SolidBrush track(theme::Gc(pal_.divider, 40));
    g.FillRectangle(&track, RectF(x + 5.0f * scale_, top, 4.0f * scale_, height));

    // ползунок
    const int alpha = scrollDrag_ ? 220 : (scrollHover_ ? 170 : 95);
    SolidBrush brush(theme::Gc(pal_.textDim, alpha));
    const float rad = scrollThumb_.Width * 0.5f;
    GraphicsPath path;
    path.AddArc(scrollThumb_.X, scrollThumb_.Y, rad, rad, 180, 90);
    path.AddArc(scrollThumb_.GetRight() - rad, scrollThumb_.Y, rad, rad, 270, 90);
    path.AddArc(scrollThumb_.GetRight() - rad, scrollThumb_.GetBottom() - rad, rad, rad, 0, 90);
    path.AddArc(scrollThumb_.X, scrollThumb_.GetBottom() - rad, rad, rad, 90, 90);
    path.CloseFigure();
    g.FillPath(&brush, &path);
}

bool App::ScrollbarHit(int x, int y) const {
    if (scrollTrack_.Width <= 0.0f) {
        return false;
    }
    return x >= static_cast<int>(scrollTrack_.X) && y >= static_cast<int>(scrollTrack_.Y) &&
           y <= static_cast<int>(scrollTrack_.GetBottom());
}

void App::ScrollbarDrag(int x, int y) {
    if (scrollTrack_.Height <= 0.0f) {
        return;
    }
    const float thumbH = scrollThumb_.Height;
    const float travel = std::max(1.0f, scrollTrack_.Height - thumbH);
    const float maxScroll = std::max(0.0f, contentHeight_ - (clientHeight_ - headerH_));
    float t = (static_cast<float>(y) - scrollGrab_ - scrollTrack_.Y) / travel;
    t = std::clamp(t, 0.0f, 1.0f);
    ScrollTo(t * maxScroll);
}

bool App::WordMatches(const std::wstring& word) const {
    if (searchLower_.empty()) {
        return false;
    }
    return Lower(word).find(searchLower_) != std::wstring::npos;
}

int App::HeaderButtonAt(float x, float y) const {
    if (searchClearRect_.Width > 0.0f && searchClearRect_.Contains(x, y)) {
        return 7;
    }
    struct Hit {
        const RectF* rect;
        int id;
    };
    const Hit hits[] = {
        {&btnTheme_, 0},      {&btnSettings_, 1}, {&btnContents_, 2}, {&btnSizePlus_, 3},
        {&btnSizeMinus_, 4},  {&btnLang_, 5},    {&searchRect_, 6},
    };
    for (const Hit& hit : hits) {
        if (hit.rect->Width > 0.0f && hit.rect->Contains(x, y)) {
            return hit.id;
        }
    }
    return -1;
}

// ---------------------------------------------------------------- Разметка

void App::MakeFonts() {
    const float px = static_cast<float>(fontSize_) * (dpi_ / 72.0f);
    const WCHAR* family = kFontFamilies[std::clamp(fontFamily_, 0, kFontFamilyCount - 1)];
    fBody_ = std::make_unique<Font>(family, px, FontStyleRegular, UnitPixel);
    fBodyBold_ = std::make_unique<Font>(family, px, FontStyleBold, UnitPixel);
    fTitle_ = std::make_unique<Font>(family, px * 1.44f, FontStyleBold, UnitPixel);
    fUi_ = std::make_unique<Font>(L"Segoe UI", px * 0.76f, FontStyleRegular, UnitPixel);
    fUiBold_ = std::make_unique<Font>(L"Segoe UI", px * 0.76f, FontStyleBold, UnitPixel);
    fGroup_ = std::make_unique<Font>(L"Segoe UI", px * 0.64f, FontStyleBold, UnitPixel);
    fTabSmall_ = std::make_unique<Font>(L"Segoe UI", px * 0.55f, FontStyleBold, UnitPixel);
    fRubric_ = std::make_unique<Font>(family, px * 0.86f, FontStyleItalic, UnitPixel);
    fSide_ = std::make_unique<Font>(family, 17.0f * scale_, FontStyleBold, UnitPixel);
    for (auto& cache : widths_) {
        cache.clear();
    }
    fontsDirty_ = false;
    // Поле поиска — обычный EDIT: ему нужен настоящий HFONT, а не GDI+ Font.
    if (editFont_ != nullptr) {
        DeleteObject(editFont_);
        editFont_ = nullptr;
    }
    const int editPx = static_cast<int>(std::lround(px * 0.76f));
    editFont_ = CreateFontW(-std::max(11, editPx), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (edit_ != nullptr && editFont_ != nullptr) {
        SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(editFont_), TRUE);
        InvalidateRect(edit_, nullptr, TRUE);
    }
}

float App::MeasureText(Graphics& g, const std::wstring& text, const Font& font, int role) {
    if (text.empty()) {
        return 0.0f;
    }
    auto& cache = widths_[role & 3];
    auto it = cache.find(text);
    if (it != cache.end()) {
        return it->second;
    }
    StringFormat fmt(StringFormat::GenericTypographic());
    fmt.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap |
                        StringFormatFlagsNoClip);
    fmt.SetTrimming(StringTrimmingNone);
    RectF bounds;
    g.MeasureString(text.c_str(), -1, &font, PointF(0, 0), &fmt, &bounds);
    const float w = bounds.Width;
    cache[text] = w;
    return w;
}

void App::LayoutBlock(Graphics& g, size_t blockIndex, float width, bool justify) {
    Block& block = blocks_[blockIndex];
    const Font& font = (block.kind == KIND_RUBRIC) ? *fRubric_ : *fBody_;
    const int role = (block.kind == KIND_RUBRIC) ? 3 : 0;
    const std::wstring& text = block.text;
    const float lineH = font.GetHeight(&g) * spacing_;
    const float spaceW = MeasureText(g, L" ", font, role);

    struct Tok {
        int start;
        int len;
        float w;
        bool brk;  // после этого слова — жёсткий перенос строки
    };
    std::vector<Tok> tokens;
    const int n = static_cast<int>(text.size());
    int i = 0;
    bool pendingBreak = false;
    while (i < n) {
        while (i < n && (text[static_cast<size_t>(i)] == L' ' || text[static_cast<size_t>(i)] == L'\n')) {
            if (text[static_cast<size_t>(i)] == L'\n') {
                pendingBreak = true;
            }
            ++i;
        }
        if (i >= n) {
            break;
        }
        int start = i;
        while (i < n && text[static_cast<size_t>(i)] != L' ' && text[static_cast<size_t>(i)] != L'\n') {
            ++i;
        }
        const int len = i - start;
        const std::wstring word = text.substr(static_cast<size_t>(start), static_cast<size_t>(len));
        float w = MeasureText(g, word, font, role);
        if (w > width && len > 1) {
            // слишком длинное слово — разрываем по символам
            int cut = start;
            std::wstring part;
            bool first = true;
            while (cut < i) {
                part += text[static_cast<size_t>(cut)];
                ++cut;
                if (MeasureText(g, part, font, role) > width && part.size() > 1) {
                    part.pop_back();
                    tokens.push_back({start, static_cast<int>(part.size()),
                                      MeasureText(g, part, font, role), first && pendingBreak});
                    part.clear();
                    start = cut;
                    first = false;
                }
            }
            if (!part.empty()) {
                tokens.push_back({start, static_cast<int>(part.size()),
                                  MeasureText(g, part, font, role), first && pendingBreak});
            }
            pendingBreak = false;
            continue;
        }
        tokens.push_back({start, len, w, pendingBreak});
        pendingBreak = false;
    }

    struct Ln {
        int first;
        int count;
        float natural;
        bool hard;  // перенос задан явно — строку не выравниваем
    };
    std::vector<Ln> lineSpecs;
    int tok = 0;
    while (tok < static_cast<int>(tokens.size())) {
        const int first = tok;
        float sum = 0;
        bool hard = false;
        while (tok < static_cast<int>(tokens.size())) {
            const Tok& t = tokens[static_cast<size_t>(tok)];
            // жёсткий перенос: этот слово начинает новую строку
            if (tok > first && t.brk) {
                hard = true;
                break;
            }
            const float add = (tok == first) ? t.w : spaceW + t.w;
            if (tok > first && sum + add > width) {
                break;
            }
            sum += add;
            ++tok;
        }
        lineSpecs.push_back({first, tok - first, sum, hard});
    }

    block.firstLine = static_cast<int>(lines_.size());
    float y = block.y;
    for (size_t li = 0; li < lineSpecs.size(); ++li) {
        const Ln& spec = lineSpecs[li];
        const bool last = (li + 1 == lineSpecs.size());
        float extra = 0.0f;
        if (justify && !last && !spec.hard && spec.count > 1) {
            const float natural = spec.natural + spaceW * (spec.count - 1);
            extra = std::max(0.0f, (width - natural) / static_cast<float>(spec.count - 1));
        }
        Line line;
        line.block = static_cast<int>(blockIndex);
        line.firstWord = static_cast<int>(words_.size());
        line.count = spec.count;
        line.y = y;
        line.h = lineH;
        float x = 0;
        for (int k = 0; k < spec.count; ++k) {
            const Tok& t = tokens[static_cast<size_t>(spec.first + k)];
            Word word;
            word.block = static_cast<int>(blockIndex);
            word.start = t.start;
            word.len = t.len;
            word.x = x;
            word.w = t.w;
            words_.push_back(word);
            x += t.w + spaceW + extra;
        }
        line.w = x;
        lines_.push_back(line);
        y += lineH;
    }
    block.lineCount = static_cast<int>(lineSpecs.size());
    block.h = lineSpecs.empty() ? lineH : y - block.y;
}

bool App::InCalendarSection() const {
    return search_.empty() && section_ >= 0 && section_ < static_cast<int>(sections_.size()) &&
           sections_[static_cast<size_t>(section_)].id == L"kalendar";
}

void App::EnsureCalendarGrid() {
    if (!calGridInit_) {
        const DayInfo& d = GetCurrentDayInfo();
        calGridYear_ = d.g.y;
        calGridMonth_ = d.g.m;
        calGridInit_ = true;
        calGridDirty_ = true;
    }
    if (!calGridDirty_) {
        return;
    }
    calGridDays_.clear();
    calGridDays_.reserve(42);
    const long firstJdn = JdnGreg(calGridYear_, calGridMonth_, 1);
    const int firstCol = (DowFromJdn(firstJdn) + 6) % 7;  // понедельник = 0
    const long start = firstJdn - firstCol;
    for (int i = 0; i < 42; ++i) {
        const long jdn = start + i;
        const DayInfo di = BuildDayInfo(jdn);
        CalGridDay d;
        d.jdn = jdn;
        d.day = di.g.d;
        d.oldDay = di.j.d;
        d.month = di.g.m;
        d.vestment = di.vestmentColor;
        d.fast = di.fast.isFast;
        d.sunday = di.isSunday;
        calGridDays_.push_back(d);
    }
    calGridDirty_ = false;
}

RectF App::CalGridCellRect(int index) const {
    const float px = 18.0f * scale_;
    const float top = calGridRect_.Y + 62.0f * scale_ + 24.0f * scale_;
    const float cw = (calGridRect_.Width - 2.0f * px) / 7.0f;
    const float ch = 34.0f * scale_;
    const int row = index / 7;
    const int col = index % 7;
    return RectF(calGridRect_.X + px + col * cw, top + row * ch, cw, ch);
}

RectF App::CalGridNextRect() const {
    const float side = 28.0f * scale_;
    return RectF(calGridRect_.GetRight() - 18.0f * scale_ - side, calGridRect_.Y + 16.0f * scale_,
                 side, side);
}

RectF App::CalGridPrevRect() const {
    const RectF n = CalGridNextRect();
    return RectF(n.X - 8.0f * scale_ - n.Width, n.Y, n.Width, n.Height);
}

RectF App::CalGridTodayRect() const {
    const RectF p = CalGridPrevRect();
    const float w = 92.0f * scale_;
    return RectF(p.X - 10.0f * scale_ - w, calGridRect_.Y + 16.0f * scale_, w, 28.0f * scale_);
}

int App::CalendarGridCellAt(float x, float y) const {
    if (calGridRect_.Height <= 0.0f) {
        return -1;
    }
    const float px = 18.0f * scale_;
    const float top = calGridRect_.Y + 62.0f * scale_ + 24.0f * scale_;
    const float cw = (calGridRect_.Width - 2.0f * px) / 7.0f;
    const float ch = 34.0f * scale_;
    const float gx = x - (calGridRect_.X + px);
    const float gy = y - top;
    if (gx < 0.0f || gy < 0.0f) {
        return -1;
    }
    const int col = static_cast<int>(gx / cw);
    const int row = static_cast<int>(gy / ch);
    if (col < 0 || col > 6 || row < 0 || row > 5) {
        return -1;
    }
    return row * 7 + col;
}

bool App::CalendarGridClick(float x, float y) {
    if (!InCalendarSection() || calGridRect_.Height <= 0.0f) {
        return false;
    }
    if (!calGridRect_.Contains(x, y)) {
        return false;
    }
    if (calGridDays_.empty()) {
        EnsureCalendarGrid();
    }
    auto shift = [&](int delta) {
        int m = calGridMonth_ + delta;
        int yr = calGridYear_;
        while (m < 1) {
            m += 12;
            --yr;
        }
        while (m > 12) {
            m -= 12;
            ++yr;
        }
        calGridYear_ = yr;
        calGridMonth_ = m;
        calGridDirty_ = true;
        calGridHover_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
    };
    if (CalGridPrevRect().Contains(x, y)) {
        shift(-1);
        return true;
    }
    if (CalGridNextRect().Contains(x, y)) {
        shift(1);
        return true;
    }
    if (CalGridTodayRect().Contains(x, y)) {
        ApplyCalendarOffset(0, true);
        calGridInit_ = false;
        return true;
    }
    const int index = CalendarGridCellAt(x, y);
    if (index >= 0 && index < static_cast<int>(calGridDays_.size())) {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        const long today = JdnGreg(static_cast<int>(now.wYear), static_cast<int>(now.wMonth),
                                   static_cast<int>(now.wDay));
        ApplyCalendarOffset(static_cast<int>(calGridDays_[static_cast<size_t>(index)].jdn - today),
                            false);
        calGridInit_ = false;  // после выбора показываем месяц выбранной даты
    }
    return true;
}

void App::PaintCalendarGrid(Graphics& g) {
    if (!InCalendarSection() || calGridRect_.Height <= 0.0f) {
        return;
    }
    EnsureCalendarGrid();

    const RectF r = calGridRect_;
    const float s = scale_;
    const float rad = 12.0f * s;
    const float px = 18.0f * s;

    // тень и карточка
    {
        GraphicsPath sh;
        ui::AddRoundRect(sh, RectF(r.X, r.Y + 4.0f * s, r.Width, r.Height), rad);
        SolidBrush sb(theme::Gc(pal_.shadow, dark_ ? 80 : 28));
        g.FillPath(&sb, &sh);
    }
    {
        GraphicsPath cp;
        ui::AddRoundRect(cp, r, rad);
        LinearGradientBrush cg(PointF(r.X, r.Y), PointF(r.X, r.GetBottom()),
                               theme::Gc(pal_.cardTop), theme::Gc(pal_.card));
        g.FillPath(&cg, &cp);
        Pen b(theme::Gc(pal_.cardBorder), 1.0f);
        g.DrawPath(&b, &cp);
    }

    static const wchar_t* kMonths[] = {L"Январь",  L"Февраль", L"Март",   L"Апрель",
                                       L"Май",     L"Июнь",    L"Июль",   L"Август",
                                       L"Сентябрь", L"Октябрь", L"Ноябрь", L"Декабрь"};
    const int mi = std::clamp(calGridMonth_, 1, 12) - 1;
    const std::wstring monthTitle =
        std::wstring(kMonths[mi]) + L" " + std::to_wstring(calGridYear_);

    StringFormat lf;
    lf.SetAlignment(StringAlignmentNear);
    lf.SetLineAlignment(StringAlignmentCenter);
    lf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    ui::Text(g, monthTitle.c_str(), -1, fBodyBold_.get(),
             RectF(r.X + px, r.Y + 10.0f * s, 320.0f * s, 28.0f * s), &lf,
             theme::Gc(pal_.title));
    ui::Text(g, L"точка — цвет облачений дня", -1, fUi_.get(),
             RectF(r.X + px, r.Y + 37.0f * s, 320.0f * s, 18.0f * s), &lf,
             theme::Gc(pal_.textDim, 150));

    // «Сегодня»
    {
        const RectF cr = CalGridTodayRect();
        const bool hov = calGridHoverBtn_ == 2;
        GraphicsPath p;
        ui::AddRoundRect(p, cr, 7.0f * s);
        SolidBrush b(theme::Gc(hov ? pal_.accent : pal_.inputBg, hov ? 54 : 235));
        g.FillPath(&b, &p);
        Pen pen(theme::Gc(hov ? pal_.accent : pal_.cardBorder, 200), 1.0f);
        g.DrawPath(&pen, &p);
        StringFormat cf;
        cf.SetAlignment(StringAlignmentCenter);
        cf.SetLineAlignment(StringAlignmentCenter);
        cf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
        ui::Text(g, L"Сегодня", -1, fUiBold_.get(), cr, &cf,
                 theme::Gc(hov ? pal_.accent : pal_.textDim));
    }

    // стрелки месяцев
    auto arrow = [&](const RectF& br, bool right, bool hov) {
        GraphicsPath p;
        ui::AddRoundRect(p, br, 7.0f * s);
        SolidBrush b(theme::Gc(hov ? pal_.hover : pal_.inputBg, hov ? 255 : 235));
        g.FillPath(&b, &p);
        Pen pen(theme::Gc(hov ? pal_.accent : pal_.cardBorder, 200), 1.0f);
        g.DrawPath(&pen, &p);
        const float cx = br.X + br.Width * 0.5f;
        const float cy = br.Y + br.Height * 0.5f;
        const float w = 3.6f * s;
        const float h = 6.0f * s;
        PointF pts[3];
        if (right) {
            pts[0] = PointF(cx - w * 0.5f, cy - h);
            pts[1] = PointF(cx + w * 0.5f, cy);
            pts[2] = PointF(cx - w * 0.5f, cy + h);
        } else {
            pts[0] = PointF(cx + w * 0.5f, cy - h);
            pts[1] = PointF(cx - w * 0.5f, cy);
            pts[2] = PointF(cx + w * 0.5f, cy + h);
        }
        SolidBrush ab(theme::Gc(hov ? pal_.accent : pal_.textDim));
        g.FillPolygon(&ab, pts, 3);
    };
    arrow(CalGridPrevRect(), false, calGridHoverBtn_ == 0);
    arrow(CalGridNextRect(), true, calGridHoverBtn_ == 1);

    // дни недели
    static const wchar_t* kDow[] = {L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс"};
    StringFormat cf;
    cf.SetAlignment(StringAlignmentCenter);
    cf.SetLineAlignment(StringAlignmentCenter);
    cf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    for (int c = 0; c < 7; ++c) {
        const RectF cell = CalGridCellRect(c);
        ui::Text(g, kDow[c], -1, fGroup_.get(),
                 RectF(cell.X, r.Y + 62.0f * s, cell.Width, 24.0f * s), &cf,
                 theme::Gc(c == 6 ? pal_.rubric : pal_.textDim, c == 6 ? 230 : 170));
    }

    // дни месяца
    const DayInfo& cur = GetCurrentDayInfo();
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    const long todayJdn = JdnGreg(static_cast<int>(now.wYear), static_cast<int>(now.wMonth),
                                  static_cast<int>(now.wDay));
    Font smallFont(L"Segoe UI", 9.5f * s, FontStyleRegular, UnitPixel);
    for (int i = 0; i < static_cast<int>(calGridDays_.size()); ++i) {
        const CalGridDay& d = calGridDays_[static_cast<size_t>(i)];
        const RectF cell = CalGridCellRect(i);
        const RectF inner(cell.X + 3.0f * s, cell.Y + 1.0f * s, cell.Width - 6.0f * s,
                          cell.Height - 2.0f * s);
        const bool other = d.month != calGridMonth_;
        const bool selected = d.jdn == cur.jdn;
        const bool isToday = d.jdn == todayJdn;
        const bool hov = i == calGridHover_;
        const int baseAlpha = other ? 90 : 255;

        if (selected || hov) {
            GraphicsPath p;
            ui::AddRoundRect(p, inner, 8.0f * s);
            SolidBrush b(theme::Gc(pal_.accent, selected ? 52 : 26));
            g.FillPath(&b, &p);
            if (selected) {
                Pen pen(theme::Gc(pal_.accent, 210), 1.4f);
                g.DrawPath(&pen, &p);
            }
        } else if (isToday) {
            GraphicsPath p;
            ui::AddRoundRect(p, inner, 8.0f * s);
            Pen pen(theme::Gc(pal_.accent, 120), 1.2f);
            g.DrawPath(&pen, &p);
        }

        Color dayColor =
            theme::Gc(selected ? pal_.accent : (d.sunday ? pal_.rubric : pal_.text), baseAlpha);
        if (hov && !selected) {
            dayColor = theme::Gc(pal_.accent, baseAlpha);
        }
        ui::Text(g, std::to_wstring(d.day).c_str(), -1, fUiBold_.get(),
                 RectF(cell.X, cell.Y + 2.0f * s, cell.Width, 21.0f * s), &cf, dayColor);
        ui::Text(g, std::to_wstring(d.oldDay).c_str(), -1, &smallFont,
                 RectF(cell.X, cell.Y + 21.0f * s, cell.Width, 11.0f * s), &cf,
                 theme::Gc(selected ? pal_.accent : pal_.textDim,
                           other ? 70 : (selected ? 200 : 150)));

        if (!other) {
            SolidBrush vb(theme::Gc(theme::VestmentColorRef(d.vestment, dark_),
                                    selected ? 235 : 210));
            const float dot = 4.0f * s;
            g.FillEllipse(&vb, cell.X + cell.Width - 11.0f * s - dot * 0.5f, cell.Y + 7.0f * s,
                          dot, dot);
        }
    }
}

void App::PaintChetkiProgress(Graphics& g) {
    if (chetkiRingRect_.Width <= 0.0f) {
        return;
    }
    const RectF r = chetkiRingRect_;
    const float s = scale_;
    const float mx = lastMouseX_ - sidebarW_;
    const float my = lastMouseY_ - headerH_ + scroll_;
    const bool hov = r.Contains(mx, my);

    const int tgt = std::max(1, chetki::Target());
    const int cnt = chetki::Count();
    int inCircle = cnt % tgt;
    if (cnt > 0 && inCircle == 0) {
        inCircle = tgt;
    }
    const float frac = static_cast<float>(inCircle) / static_cast<float>(tgt);
    const int circleNo = std::max(1, (cnt + tgt - 1) / tgt);

    // панель-подложка
    {
        GraphicsPath p;
        ui::AddRoundRect(p, r, 10.0f * s);
        SolidBrush fill(theme::Gc(pal_.inputBg, hov ? 235 : 165));
        g.FillPath(&fill, &p);
        Pen b(theme::Gc(hov ? pal_.accent : pal_.divider, hov ? 170 : 130), 1.0f);
        g.DrawPath(&b, &p);
    }

    const float rad = 52.0f * s;
    const float cx = r.X + 68.0f * s;
    const float cy = r.Y + r.Height * 0.5f;
    const RectF ring(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
    const float th = 8.0f * s;

    Pen base(theme::Gc(pal_.divider, 150), th);
    base.SetStartCap(LineCapRound);
    base.SetEndCap(LineCapRound);
    g.DrawEllipse(&base, ring);

    if (frac > 0.01f) {
        Pen prog(theme::Gc(pal_.accent), th);
        prog.SetStartCap(LineCapRound);
        prog.SetEndCap(LineCapRound);
        g.DrawArc(&prog, ring, -90.0f, std::min(359.9f, 360.0f * frac));
    }

    StringFormat cf;
    cf.SetAlignment(StringAlignmentCenter);
    cf.SetLineAlignment(StringAlignmentCenter);
    cf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    ui::Text(g, std::to_wstring(inCircle).c_str(), -1, fTitle_.get(),
             RectF(cx - rad, cy - 27.0f * s, rad * 2.0f, 34.0f * s), &cf,
             theme::Gc(pal_.accent));
    ui::Text(g, (L"из " + std::to_wstring(tgt)).c_str(), -1, fUi_.get(),
             RectF(cx - rad, cy + 7.0f * s, rad * 2.0f, 18.0f * s), &cf,
             theme::Gc(pal_.textDim, 200));

    StringFormat lf;
    lf.SetAlignment(StringAlignmentNear);
    lf.SetLineAlignment(StringAlignmentCenter);
    lf.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    const float tx = cx + rad + 24.0f * s;
    const float tw = std::max(80.0f * s, r.GetRight() - tx - 16.0f * s);
    ui::Text(g, (L"Круг " + std::to_wstring(circleNo)).c_str(), -1, fBodyBold_.get(),
             RectF(tx, cy - 40.0f * s, tw, 28.0f * s), &lf, theme::Gc(pal_.title));
    ui::Text(g, (L"прочитано " + std::to_wstring(cnt) + L" молитв").c_str(), -1, fUi_.get(),
             RectF(tx, cy - 12.0f * s, tw, 20.0f * s), &lf, theme::Gc(pal_.text));
    ui::Text(g, L"Нажмите на кольцо или Enter", -1, fUi_.get(),
             RectF(tx, cy + 12.0f * s, tw, 18.0f * s), &lf, theme::Gc(pal_.textDim, 200));
    ui::Text(g, L"чтобы добавить молитву", -1, fUi_.get(),
             RectF(tx, cy + 30.0f * s, tw, 18.0f * s), &lf, theme::Gc(pal_.textDim, 150));

    // Справа — сколько осталось до завершения круга и полоса прогресса.
    if (r.Width > 620.0f * s) {
        const float rw = 200.0f * s;
        const float rxx = r.GetRight() - rw - 18.0f * s;
        const int remaining = std::max(0, tgt - inCircle);
        ui::Text(g, L"до завершения круга", -1, fUi_.get(),
                 RectF(rxx, cy - 38.0f * s, rw, 18.0f * s), &lf, theme::Gc(pal_.textDim, 190));
        ui::Text(g, remaining == 0 ? L"круг завершён"
                                  : (std::to_wstring(remaining) + L" молитв").c_str(),
                 -1, fBodyBold_.get(), RectF(rxx, cy - 16.0f * s, rw, 28.0f * s), &lf,
                 theme::Gc(remaining == 0 ? pal_.accent : pal_.text));
        const float bh = 5.0f * s;
        const RectF bar(rxx, cy + 20.0f * s, rw, bh);
        GraphicsPath bp;
        ui::AddRoundRect(bp, bar, bh * 0.5f);
        SolidBrush bb(theme::Gc(pal_.divider, 130));
        g.FillPath(&bb, &bp);
        if (frac > 0.005f) {
            GraphicsPath fp;
            ui::AddRoundRect(fp, RectF(bar.X, bar.Y, std::max(bh, rw * frac), bh), bh * 0.5f);
            SolidBrush fb(theme::Gc(pal_.accent));
            g.FillPath(&fb, &fp);
        }
        ui::Text(g, (L"из " + std::to_wstring(tgt) + L" в текущем круге").c_str(), -1, fUi_.get(),
                 RectF(rxx, cy + 32.0f * s, rw, 18.0f * s), &lf, theme::Gc(pal_.textDim, 150));
    }
}

void App::Relayout(Graphics& g, float contentWidth) {
    blocks_.clear();
    words_.clear();
    lines_.clear();
    cards_.clear();

    const float pad = 26.0f * scale_;
    cardPadX_ = 30.0f * scale_;
    const float cardPadY = 24.0f * scale_;
    const float gap = 16.0f * scale_;
    const float maxCol = 880.0f * scale_ * columnScale_;
    const float scrollSpace = 24.0f * scale_;

    const float avail = contentWidth - pad * 2.0f - scrollSpace;
    colW_ = std::max(260.0f * scale_, std::min(avail, maxCol));
    colX_ = pad + (avail - colW_) * 0.5f;
    textX_ = colX_ + cardPadX_;

    float y = 10.0f * scale_;
    std::wstring lastGroup;
    chetkiRingRect_ = RectF();

    // В разделе «Календарь» над карточками — интерактивная месячная сетка.
    calGridRect_ = RectF();
    if (InCalendarSection()) {
        const float gridH = 302.0f * scale_;
        calGridRect_ = RectF(colX_, y, colW_, gridH);
        y += gridH + gap;
    }

    for (size_t vi = 0; vi < view_.size(); ++vi) {
        const ViewItem& item = view_[vi];
        const Prayer* prayer = PrayerOf(item);
        if (prayer == nullptr) {
            continue;
        }
        Card card;
        card.item = static_cast<int>(vi);
        card.y = y;
        card.firstBlock = static_cast<int>(blocks_.size());

        const float inner = colW_ - cardPadX_ * 2.0f;

        // группа
        if (!item.group.empty() && item.group != lastGroup) {
            lastGroup = item.group;
            Block b;
            b.kind = KIND_GROUP;
            b.item = static_cast<int>(vi);
            b.text = item.group;
            b.y = y + cardPadY;
            b.h = 18.0f * scale_;
            blocks_.push_back(b);
            y += cardPadY + b.h + 6.0f * scale_;
        } else {
            lastGroup = item.group;
        }

        // заголовок
        if (!prayer->title.empty()) {
            Block b;
            b.kind = KIND_TITLE;
            b.item = static_cast<int>(vi);
            b.text = prayer->title;
            b.y = y + cardPadY;
            b.h = fTitle_->GetHeight(&g) * 1.18f;
            blocks_.push_back(b);
            y += cardPadY + b.h + 4.0f * scale_;

            Block d;
            d.kind = KIND_DIVIDER;
            d.item = static_cast<int>(vi);
            d.y = y;
            d.h = 1.0f;
            blocks_.push_back(d);
            y += 14.0f * scale_;
        } else {
            y += 6.0f * scale_;
        }

        // в первой карточке чёток — место под кольцо прогресса
        if (search_.empty() && sections_[static_cast<size_t>(section_)].id == L"chetki" &&
            vi == 0) {
            chetkiRingRect_ = RectF(textX_ + 4.0f * scale_, y + 2.0f * scale_,
                                    inner - 8.0f * scale_, 148.0f * scale_);
            y += 148.0f * scale_ + 14.0f * scale_;
        }

        // абзацы: если перевода нет, показываем церковнославянский текст
        bool hasTranslation = false;
        for (const PrayerPara& p : prayer->paras) {
            if (p.lang != ParaLang::Cs) {
                hasTranslation = true;
                break;
            }
        }
        for (size_t pi = 0; pi < prayer->paras.size(); ++pi) {
            const PrayerPara& para = prayer->paras[pi];
            if (hasTranslation) {
                if (para.lang == ParaLang::Cs && lang_ == 0) {
                    continue;
                }
                if (para.lang == ParaLang::Ru && lang_ == 1) {
                    continue;
                }
            }
            Block b;
            b.kind = (para.style == kStyleRubric) ? KIND_RUBRIC
                                                  : (para.style == kStyleVerse ? KIND_VERSE : KIND_TEXT);
            b.item = static_cast<int>(vi);
            b.para = static_cast<int>(pi);
            b.text = para.text;
            b.y = y;
            blocks_.push_back(b);
            LayoutBlock(g, blocks_.size() - 1, inner, b.kind == KIND_TEXT);
            const float bh = blocks_.back().h;
            y += bh + (b.kind == KIND_TEXT ? 13.0f * scale_ : 5.0f * scale_);
        }

        y += cardPadY * 0.6f;
        card.h = y - card.y;
        card.blockCount = static_cast<int>(blocks_.size()) - card.firstBlock;
        card.starX = colX_ + colW_ - cardPadX_ - 18.0f * scale_;
        card.starY = card.y + cardPadY + 2.0f * scale_;
        cards_.push_back(card);
        y += gap;
    }

    contentHeight_ = y + 16.0f * scale_;
}

// Иконка приложения: крест на тёмном щите.
HICON App::MakeAppIcon(int size) {
    Bitmap bmp(size, size, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));

    const float r = static_cast<float>(size);
    GraphicsPath plate;
    plate.AddArc(0.0f, 0.0f, r, r, 90, 180);
    plate.AddArc(0.0f, 0.0f, r, r, 270, 180);
    plate.CloseFigure();
    LinearGradientBrush grad(PointF(0, 0), PointF(r, r), Color(255, 32, 44, 72),
                             Color(255, 12, 18, 32));
    g.FillPath(&grad, &plate);
    Pen edge(Color(255, 227, 187, 117), std::max(1.0f, r * 0.05f));
    g.DrawPath(&edge, &plate);

    RectF cross(r * 0.42f, r * 0.14f, r * 0.16f, r * 0.72f);
    icons::Cross(g, cross, Color(255, 240, 214, 158), std::max(1.4f, r * 0.09f));

    Color background(0, 0, 0, 0);
    HBITMAP hbm = nullptr;
    bmp.GetHBITMAP(background, &hbm);
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hbm;
    ii.hbmMask = CreateBitmap(size, size, 1, 1, nullptr);
    HICON icon = CreateIconIndirect(&ii);
    if (ii.hbmMask != nullptr) {
        DeleteObject(ii.hbmMask);
    }
    return icon;
}

std::wstring App::PrayerText(const Prayer& prayer, bool withoutAccents) const {
    std::wstring out;
    if (!prayer.title.empty()) {
        out = prayer.title;
        out += L"\r\n\r\n";
    }
    for (const PrayerPara& para : prayer.paras) {
        if (para.lang == ParaLang::Cs && lang_ == 0) {
            continue;
        }
        if (para.lang == ParaLang::Ru && lang_ == 1) {
            continue;
        }
        std::wstring text = para.text;
        if (withoutAccents) {
            std::wstring clean;
            clean.reserve(text.size());
            for (wchar_t c : text) {
                if (c != 0x0301) {
                    clean += c;
                }
            }
            text = clean;
        }
        out += text;
        out += L"\r\n\r\n";
    }
    return out;
}

void App::SetClipboardText(const std::wstring& text) {
    if (text.empty() || !OpenClipboard(hwnd_)) {
        return;
    }
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem != nullptr) {
        void* ptr = GlobalLock(mem);
        if (ptr != nullptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
        }
    }
    CloseClipboard();
}

void App::CopyPrayer(int cardIndex, bool withoutAccents) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(view_.size())) {
        return;
    }
    const Prayer* prayer = PrayerOf(view_[static_cast<size_t>(cardIndex)]);
    if (prayer == nullptr) {
        return;
    }
    SetClipboardText(PrayerText(*prayer, withoutAccents));
    ShowToast(withoutAccents ? L"Молитва скопирована без знаков ударения"
                              : L"Молитва скопирована в буфер обмена");
}

void App::ExportPrayer(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(view_.size())) {
        return;
    }
    const Prayer* prayer = PrayerOf(view_[static_cast<size_t>(cardIndex)]);
    if (prayer == nullptr) {
        return;
    }
    wchar_t fileName[MAX_PATH] = L"молитва.txt";
    wchar_t filter[] = L"Текстовые файлы (*.txt)\0*.txt\0Все файлы (*.*)\0*.*\0";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Сохранить молитву";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) {
        return;
    }
    const std::wstring text = PrayerText(*prayer, false);
    HANDLE file = CreateFileW(fileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        ShowToast(L"Не удалось сохранить файл");
        return;
    }
    const std::string utf8 = WideToUtf8(text);
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    DWORD written = 0;
    WriteFile(file, bom, 3, &written, nullptr);
    WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
    ShowToast(L"Молитва сохранена");
}

std::vector<std::wstring> App::WrapForPrint(HDC dc, const std::wstring& text, int width) const {
    std::vector<std::wstring> lines;
    std::wstring paragraph;
    auto flush = [&]() {
        if (paragraph.empty()) {
            return;
        }
        std::wstring line;
        SIZE size = {};
        size_t pos = 0;
        while (pos < paragraph.size()) {
            size_t space = paragraph.find(L' ', pos);
            if (space == std::wstring::npos) {
                space = paragraph.size();
            }
            const std::wstring word = paragraph.substr(pos, space - pos);
            std::wstring candidate = line.empty() ? word : line + L" " + word;
            GetTextExtentPoint32W(dc, candidate.c_str(), static_cast<int>(candidate.size()), &size);
            if (size.cx > width && !line.empty()) {
                lines.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
            pos = space + 1;
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
        paragraph.clear();
    };
    for (wchar_t c : text) {
        if (c == L'\n') {
            flush();
        } else if (c != L'\r') {
            paragraph += c;
        }
    }
    flush();
    return lines;
}

void App::PrintPrayer(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(view_.size())) {
        return;
    }
    const Prayer* prayer = PrayerOf(view_[static_cast<size_t>(cardIndex)]);
    if (prayer == nullptr) {
        return;
    }
    PRINTDLGW pd = {};
    pd.Flags = PD_HIDEPRINTTOFILE;
    if (!PrintDlgW(&pd)) {
        return;
    }
    HDC dc = pd.hDC;
    if (dc == nullptr) {
        if (pd.hDC != nullptr) {
            DeleteDC(pd.hDC);
        }
        return;
    }

    const int hRes = GetDeviceCaps(dc, HORZRES);
    const int vRes = GetDeviceCaps(dc, VERTRES);
    const int logPixels = GetDeviceCaps(dc, LOGPIXELSY);
    const int margin = std::max(200, logPixels * 2 / 3);
    const int fontHeight = -MulDiv(fontSize_, logPixels, 72);

    HFONT font = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, kFontFamilies[fontFamily_]);
    HGDIOBJ oldFont = SelectObject(dc, font);
    TEXTMETRICW tm = {};
    GetTextMetricsW(dc, &tm);
    const int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    const std::vector<std::wstring> lines = WrapForPrint(dc, PrayerText(*prayer, false),
                                                          hRes - 2 * margin);
    DOCINFOW info = {};
    info.lpszDocName = L"Молитва";
    StartDocW(dc, &info);
    int y = margin;
    for (const std::wstring& line : lines) {
        if (y + lineHeight > vRes - margin) {
            EndPage(dc);
            StartPage(dc);
            y = margin;
        }
        TextOutW(dc, margin, y, line.c_str(), static_cast<int>(line.size()));
        y += lineHeight;
    }
    EndDoc(dc);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
    ShowToast(L"Отправлено на печать");
}

void App::ShowCardMenu(int cardIndex, int screenX, int screenY) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(view_.size())) {
        return;
    }
    const bool fav = IsFavorite(view_[static_cast<size_t>(cardIndex)]);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Копировать молитву");
    AppendMenuW(menu, MF_STRING, 2, L"Копировать без знаков ударения");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"Сохранить в файл…");
    AppendMenuW(menu, MF_STRING, 4, L"Печать…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 5, fav ? L"Убрать из избранного" : L"Добавить в избранное");
    const Prayer* prayer = PrayerOf(view_[static_cast<size_t>(cardIndex)]);
    if (prayer != nullptr && !prayer->url.empty()) {
        AppendMenuW(menu, MF_STRING, 6, L"Открыть в браузере");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    if (sections_[static_cast<size_t>(view_[static_cast<size_t>(cardIndex)].section)].id ==
        L"pominalnik") {
        AppendMenuW(menu, MF_STRING, 7, L"Изменить поминальник…");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenX, screenY, 0, hwnd_,
                                    nullptr);
    DestroyMenu(menu);
    switch (cmd) {
        case 1:
            CopyPrayer(cardIndex, false);
            break;
        case 2:
            CopyPrayer(cardIndex, true);
            break;
        case 3:
            ExportPrayer(cardIndex);
            break;
        case 4:
            PrintPrayer(cardIndex);
            break;
        case 5:
            ToggleFavorite(view_[static_cast<size_t>(cardIndex)]);
            BuildView();
            layoutDirty_ = true;
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        case 6:
            if (prayer != nullptr) {
                ShellExecuteW(hwnd_, L"open", prayer->url.c_str(), nullptr, nullptr,
                              SW_SHOWNORMAL);
            }
            break;
        case 7:
            if (pom::EditDialog(hwnd_)) {
                SaveSettings();
                if (RefreshDynamicSection(sections_, L"pominalnik")) {
                    BuildSearchIndex();
                    const bool visible =
                        search_.empty() &&
                        sections_[static_cast<size_t>(section_)].id == L"pominalnik";
                    if (visible) {
                        BuildView();
                        layoutDirty_ = true;
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
            }
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------- Данные

void App::BuildView() {
    view_.clear();
    if (!search_.empty()) {
        const std::wstring needle = Lower(search_);
        for (const SearchEntry& entry : searchIndex_) {
            if (entry.title.find(needle) == std::wstring::npos &&
                entry.text.find(needle) == std::wstring::npos) {
                continue;
            }
            const Section& sec = sections_[static_cast<size_t>(entry.section)];
            if (sec.id == L"favorites") {
                continue;
            }
            ViewItem item;
            item.section = entry.section;
            item.prayer = entry.prayer;
            item.group = sec.title;
            view_.push_back(item);
        }
        return;
    }

    const Section& sec = sections_[static_cast<size_t>(section_)];
    if (sec.id == L"favorites") {
        for (size_t si = 0; si < sections_.size(); ++si) {
            const Section& s = sections_[si];
            if (s.id == L"favorites") {
                continue;
            }
            for (size_t pi = 0; pi < s.prayers.size(); ++pi) {
                ViewItem probe;
                probe.section = static_cast<int>(si);
                probe.prayer = static_cast<int>(pi);
                if (!IsFavorite(probe)) {
                    continue;
                }
                ViewItem item = probe;
                item.group = s.title;
                view_.push_back(item);
            }
        }
        return;
    }

    for (size_t pi = 0; pi < sec.prayers.size(); ++pi) {
        ViewItem item;
        item.section = section_;
        item.prayer = static_cast<int>(pi);
        item.group = sec.prayers[pi].group;
        view_.push_back(item);
    }
}

const Prayer* App::PrayerOf(const ViewItem& item) const {
    if (item.section < 0 || item.section >= static_cast<int>(sections_.size())) {
        return nullptr;
    }
    const Section& sec = sections_[static_cast<size_t>(item.section)];
    if (item.prayer < 0 || item.prayer >= static_cast<int>(sec.prayers.size())) {
        return nullptr;
    }
    return &sec.prayers[static_cast<size_t>(item.prayer)];
}

std::wstring App::FavoriteKey(const ViewItem& item) const {
    return sections_[static_cast<size_t>(item.section)].id + L"|" + std::to_wstring(item.prayer);
}

bool App::IsFavorite(const ViewItem& item) const {
    return favorites_.find(FavoriteKey(item)) != favorites_.end();
}

void App::ToggleFavorite(const ViewItem& item) {
    const std::wstring key = FavoriteKey(item);
    if (favorites_.find(key) != favorites_.end()) {
        favorites_.erase(key);
    } else {
        favorites_.insert(key);
    }
    SaveSettings();
}

void App::SectionById(const std::wstring& id, int& index) const {
    for (size_t i = 0; i < sections_.size(); ++i) {
        if (sections_[i].id == id) {
            index = static_cast<int>(i);
            return;
        }
    }
    index = 0;
}

void App::ApplySection(int index) {
    if (index < 0 || index >= static_cast<int>(sections_.size())) {
        return;
    }
    // запоминаем, где остановились
    if (search_.empty()) {
        scrollMem_[sections_[static_cast<size_t>(section_)].id] = scroll_;
    }
    section_ = index;
    search_.clear();
    if (edit_ != nullptr) {
        SetWindowTextW(edit_, L"");
    }
    const float saved = scrollMem_[sections_[static_cast<size_t>(section_)].id];
    scroll_ = saved;
    scrollTarget_ = saved;
    hasSelection_ = false;
    panel_ = 0;
    BuildView();
    layoutDirty_ = true;
    calGridInit_ = false;  // при входе в «Календарь» — месяц текущей даты
    calGridHover_ = -1;
    calGridHoverBtn_ = -1;
    UpdateWindowTitle();
    SaveSettings();
}

// ---------------------------------------------------------------- Прокрутка

void App::ScrollBy(float delta) {
    const float maxScroll = std::max(0.0f, contentHeight_ - clientHeight_ + headerH_);
    scrollTarget_ = std::clamp(scrollTarget_ + delta, 0.0f, maxScroll);
    if (std::fabs(scrollTarget_ - scroll_) < 0.5f) {
        scroll_ = scrollTarget_;
    }
}

void App::ScrollTo(float value) {
    const float maxScroll = std::max(0.0f, contentHeight_ - clientHeight_ + headerH_);
    scrollTarget_ = std::clamp(value, 0.0f, maxScroll);
    if (std::fabs(scrollTarget_ - scroll_) < 0.5f) {
        scroll_ = scrollTarget_;
    }
}

int App::CardAtPoint(float localX, float localY) const {
    for (const Card& card : cards_) {
        if (localY >= card.y && localY < card.y + card.h && localX >= colX_ - 10.0f * scale_ &&
            localX <= colX_ + colW_ + 10.0f * scale_) {
            return card.item;
        }
    }
    return -1;
}

// ---------------------------------------------------------------- Выделение

bool App::HitTest(float x, float y, int& block, int& offset) const {
    for (size_t li = 0; li < lines_.size(); ++li) {
        const Line& line = lines_[li];
        if (y < line.y || y > line.y + line.h) {
            continue;
        }
        for (int wi = 0; wi < line.count; ++wi) {
            const Word& w = words_[static_cast<size_t>(line.firstWord + wi)];
            if (x >= w.x && x <= w.x + w.w) {
                block = line.block;
                offset = w.start;
                return true;
            }
        }
        if (line.count > 0) {
            block = line.block;
            const Word& last = words_[static_cast<size_t>(line.firstWord + line.count - 1)];
            offset = last.start + last.len;
            return true;
        }
    }
    return false;
}

bool App::IsSelected(int block, int start, int end) const {
    if (!hasSelection_) {
        return false;
    }
    int b1 = selBlockA_;
    int o1 = selOffsetA_;
    int b2 = selBlockB_;
    int o2 = selOffsetB_;
    if (b1 > b2 || (b1 == b2 && o1 > o2)) {
        std::swap(b1, b2);
        std::swap(o1, o2);
    }
    if (block < b1 || block > b2) {
        return false;
    }
    if (block == b1 && end <= o1) {
        return false;
    }
    if (block == b2 && start >= o2) {
        return false;
    }
    return true;
}

std::wstring App::SelectionText() const {
    if (!hasSelection_ || blocks_.empty()) {
        return std::wstring();
    }
    int b1 = selBlockA_;
    int o1 = selOffsetA_;
    int b2 = selBlockB_;
    int o2 = selOffsetB_;
    if (b1 > b2 || (b1 == b2 && o1 > o2)) {
        std::swap(b1, b2);
        std::swap(o1, o2);
    }
    std::wstring out;
    for (int b = b1; b <= b2 && b < static_cast<int>(blocks_.size()); ++b) {
        const Block& blk = blocks_[static_cast<size_t>(b)];
        int from = (b == b1) ? o1 : 0;
        int to = (b == b2) ? o2 : static_cast<int>(blk.text.size());
        from = std::clamp(from, 0, static_cast<int>(blk.text.size()));
        to = std::clamp(to, 0, static_cast<int>(blk.text.size()));
        if (to > from) {
            out += blk.text.substr(static_cast<size_t>(from), static_cast<size_t>(to - from));
        }
        if (b != b2) {
            out += L"\r\n";
        }
    }
    return out;
}

void App::CopySelection() {
    const std::wstring text = SelectionText();
    if (text.empty()) {
        return;
    }
    if (!OpenClipboard(hwnd_)) {
        return;
    }
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem != nullptr) {
        void* ptr = GlobalLock(mem);
        if (ptr != nullptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
        }
    }
    CloseClipboard();
}

void App::SelectAll() {
    if (blocks_.empty()) {
        return;
    }
    hasSelection_ = true;
    selBlockA_ = 0;
    selOffsetA_ = 0;
    selBlockB_ = static_cast<int>(blocks_.size()) - 1;
    selOffsetB_ = static_cast<int>(blocks_.back().text.size());
}

// ---------------------------------------------------------------- Ввод

void App::OnSize(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    dpi_ = static_cast<float>(GetDpiForWindow(hwnd));
    if (dpi_ <= 0) {
        dpi_ = 96.0f;
    }
    scale_ = dpi_ / 96.0f;
    clientWidth_ = static_cast<float>(rc.right - rc.left);
    clientHeight_ = static_cast<float>(rc.bottom - rc.top);
    sidebarW_ = focusMode_ ? 0.0f : (sidebarCollapsed_ ? 70.0f : 272.0f) * scale_;
    headerH_ = focusMode_ ? 0.0f : 132.0f * scale_;
    sidebarListTop_ = 112.0f * scale_;
    ClampSidebarScroll();
    fontsDirty_ = true;
    layoutDirty_ = true;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void App::OnMouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    const int sx = GET_X_LPARAM(lp);
    const int sy = GET_Y_LPARAM(lp);

    if (paletteOpen_) {
        if (msg == WM_MOUSEWHEEL) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wp);
            paletteScroll_ += (delta > 0) ? -2 : 2;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (msg == WM_LBUTTONDOWN) {
            const float px = static_cast<float>(sx);
            const float py = static_cast<float>(sy);
            if (paletteRect_.Contains(px, py)) {
                for (size_t i = 0; i < paletteRows_.size(); ++i) {
                    if (paletteRows_[i].Width > 0.0f && paletteRows_[i].Contains(px, py)) {
                        paletteSel_ = static_cast<int>(i);
                        ActivatePaletteItem(static_cast<int>(i));
                        return;
                    }
                }
                return;
            }
            ClosePalette();
            return;
        }
        if (msg == WM_MOUSEMOVE) {
            int hover = -1;
            for (size_t i = 0; i < paletteRows_.size(); ++i) {
                if (paletteRows_[i].Width > 0.0f &&
                    paletteRows_[i].Contains(static_cast<float>(sx), static_cast<float>(sy))) {
                    hover = static_cast<int>(i);
                    break;
                }
            }
            if (hover != paletteHover_) {
                paletteHover_ = hover;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        }
        return;
    }

    if (msg == WM_MOUSEWHEEL) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        POINT pt = {sx, sy};
        ScreenToClient(hwnd, &pt);
        if (static_cast<float>(pt.x) < sidebarW_) {
            sidebarScroll_ = std::clamp(sidebarScroll_ - static_cast<float>(delta) * 0.5f, 0.0f,
                                        SidebarMaxScroll());
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (panel_ != 0) {
            const RectF panel = PanelRect();
            if (panel.Contains(static_cast<float>(pt.x), static_cast<float>(pt.y))) {
                panelScroll_ = std::max(0.0f, panelScroll_ - static_cast<float>(delta) * 0.6f);
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }
        ScrollBy(-static_cast<float>(delta) * 0.7f);
        return;
    }

    if (msg == WM_LBUTTONDOWN) {
        // popup-календарь открыт — любой клик в главном окне его закрывает
        if (datePopup_ != nullptr && IsWindow(datePopup_)) {
            DestroyWindow(datePopup_);
            datePopup_ = nullptr;
            if (static_cast<float>(sy) < headerH_ &&
                btnToday_.Contains(static_cast<float>(sx), static_cast<float>(sy))) {
                return;  // повторный клик по плитке — снять попап (toggle)
            }
        }
        if (focusMode_ && focusExitRect_.Width > 0.0f &&
            focusExitRect_.Contains(static_cast<float>(sx), static_cast<float>(sy))) {
            SetFocusMode(false);
            return;
        }
        if (static_cast<float>(sx) < sidebarW_) {
            if (sideTrack_.Width > 0.0f && sideThumb_.Contains(static_cast<float>(sx),
                                                              static_cast<float>(sy))) {
                sideDrag_ = true;
                sideGrab_ = static_cast<float>(sy) - sideThumb_.Y;
                SetCapture(hwnd);
                return;
            }
            if (sideTrack_.Width > 0.0f && static_cast<float>(sx) >= sideTrack_.X &&
                static_cast<float>(sy) >= sideTrack_.Y &&
                static_cast<float>(sy) <= sideTrack_.GetBottom()) {
                // щелчок по дорожке — прокрутка на страницу
                const float page = std::max(60.0f * scale_, SidebarListRect().Height * 0.9f);
                sidebarScroll_ = std::clamp(
                    sidebarScroll_ + (static_cast<float>(sy) < sideThumb_.Y ? -page : page), 0.0f,
                    SidebarMaxScroll());
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (sidebarToggle_.Contains(static_cast<float>(sx), static_cast<float>(sy))) {
                sidebarCollapsed_ = !sidebarCollapsed_;
                sidebarW_ = focusMode_ ? 0.0f : (sidebarCollapsed_ ? 70.0f : 272.0f) * scale_;
                ClampSidebarScroll();
                layoutDirty_ = true;
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (sidebarCollapsed_) {
                // свёрнутое меню: значки разделов
                const float itemH = 46.0f * scale_;
                const float listTop = SidebarListRect().Y;
                float iy = listTop - sidebarScroll_;
                for (size_t i = 0; i < sections_.size(); ++i) {
                    const Section& s = sections_[i];
                    if (s.id == L"favorites" && favorites_.empty()) {
                        continue;
                    }
                    if (static_cast<float>(sy) >= iy && static_cast<float>(sy) < iy + itemH - 6.0f * scale_) {
                        ApplySection(static_cast<int>(i));
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return;
                    }
                    iy += itemH;
                }
                return;
            }
            BuildSidebarItems();
            for (const SidebarItem& item : sidebarItems_) {
                if (item.header) {
                    continue;
                }
                const float iy = item.y - sidebarScroll_;
                if (static_cast<float>(sy) >= iy && static_cast<float>(sy) < iy + item.h) {
                    ApplySection(item.section);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return;
                }
            }
            return;
        }

        if (static_cast<float>(sy) < headerH_) {
            const float px = static_cast<float>(sx);
            const float py = static_cast<float>(sy);
            auto contains = [&](const RectF& r) {
                return r.Width > 0.0f && r.Height > 0.0f && r.Contains(px, py);
            };
            if (searchClearRect_.Width > 0.0f && contains(searchClearRect_)) {
                search_.clear();
                SetWindowTextW(edit_, L"");
                BuildView();
                layoutDirty_ = true;
                tooltipButton_ = -1;
                tooltipVisible_ = false;
                tooltipText_.clear();
                SetFocus(edit_);
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (contains(btnTheme_)) {
                dark_ = !dark_;
                themeIndex_ = dark_ ? 0 : 1;
                ApplyWindowTheme();
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
                if (edit_ != nullptr) {
                    InvalidateRect(edit_, nullptr, TRUE);
                }
                return;
            }
            if (contains(btnPrevDay_)) {
                StepCalendarDate(-1, false);
                return;
            }
            if (contains(btnNextDay_)) {
                StepCalendarDate(1, false);
                return;
            }
            if (contains(btnToday_)) {
                ToggleDatePopup();
                return;
            }
            for (const QuickTab& qt : quickTabs_) {
                if (contains(qt.rect)) {
                    int idx = 0;
                    SectionById(qt.id, idx);
                    ApplySection(idx);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return;
                }
            }
            if (contains(btnSettings_)) {
                panel_ = (panel_ == 2) ? 0 : 2;
                panelScroll_ = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (contains(btnContents_)) {
                panel_ = (panel_ == 1) ? 0 : 1;
                panelScroll_ = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (contains(btnSizePlus_)) {
                fontSize_ = std::min(26, fontSize_ + 1);
                fontsDirty_ = true;
                layoutDirty_ = true;
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (contains(btnSizeMinus_)) {
                fontSize_ = std::max(13, fontSize_ - 1);
                fontsDirty_ = true;
                layoutDirty_ = true;
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (contains(btnLang_)) {
                lang_ = (px - btnLang_.X < btnLang_.Width * 0.5f) ? 0 : 1;
                BuildView();
                layoutDirty_ = true;
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            return;
        }

        // содержимое
        SetFocus(hwnd);
        const float localX = static_cast<float>(sx) - sidebarW_;

        // кнопка «вверх» (рисуется в клиентских координатах)
        if (topButton_.Width > 0.0f &&
            topButton_.Contains(static_cast<float>(sx), static_cast<float>(sy))) {
            ScrollTo(0);
            return;
        }

        // полоса прокрутки: ползунок или дорожка
        if (ScrollbarHit(sx, sy)) {
            if (scrollThumb_.Contains(static_cast<float>(sx), static_cast<float>(sy))) {
                scrollDrag_ = true;
                scrollGrab_ = static_cast<float>(sy) - scrollThumb_.Y;
                SetCapture(hwnd);
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            } else {
                const float page = clientHeight_ - headerH_;
                ScrollBy(static_cast<float>(sy) < scrollThumb_.Y ? -page : page);
            }
            return;
        }

        if (panel_ != 0) {
            // панели рисуются в клиентских координатах
            const RectF panel = PanelRect();
            const float px = static_cast<float>(sx);
            const float py = static_cast<float>(sy);
            if (panel.Contains(px, py)) {

                if (panel_ == 1) {
                    for (const PanelItem& item : panelItems_) {
                        if (item.action == 20 && item.rect.Contains(px, py)) {
                            RandomPrayer();
                            panel_ = 0;
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return;
                        }
                    }
                    for (const TocRow& row : tocRows_) {
                        if (py >= row.y - panelScroll_ && py < row.y - panelScroll_ + row.h &&
                            !row.header) {
                            ScrollToCard(row.index, true);
                            panel_ = 0;
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return;
                        }
                    }
                } else {
                    for (const PanelItem& item : panelItems_) {
                        if (item.rect.Contains(px, py)) {
                            switch (item.action) {
                                case 10:
                                    fontFamily_ = item.index;
                                    fontsDirty_ = true;
                                    layoutDirty_ = true;
                                    break;
                                case 11: {
                                    static const float values[] = {0.85f, 1.0f, 1.2f};
                                    spacing_ = values[item.index];
                                    layoutDirty_ = true;
                                    break;
                                }
                                case 12: {
                                    static const float values[] = {0.78f, 1.0f, 1.25f};
                                    columnScale_ = values[item.index];
                                    layoutDirty_ = true;
                                    break;
                                }
                                case 13:
                                    dark_ = (item.index == 0);
                                    themeIndex_ = dark_ ? 0 : 1;
                                    ApplyWindowTheme();
                                    if (edit_ != nullptr) {
                                        InvalidateRect(edit_, nullptr, TRUE);
                                    }
                                    break;
                                case 14:
                                    themeIndex_ = std::clamp(item.index, 0, 3);
                                    dark_ = (themeIndex_ == 0 || themeIndex_ == 2);
                                    ApplyWindowTheme();
                                    if (edit_ != nullptr) {
                                        InvalidateRect(edit_, nullptr, TRUE);
                                    }
                                    break;
                                case 15:
                                    reminders_ = !reminders_;
                                    ShowToast(reminders_ ? L"Напоминания о правиле включены"
                                                         : L"Напоминания о правиле выключены");
                                    break;
                                case 16:
                                    panel_ = 0;
                                    if (pom::EditDialog(hwnd)) {
                                        SaveSettings();
                                        RebuildAllDynamicSections(sections_);
                                        BuildSearchIndex();
                                        BuildView();
                                        layoutDirty_ = true;
                                        ShowToast(L"Поминальные списки сохранены");
                                    }
                                    InvalidateRect(hwnd, nullptr, FALSE);
                                    return;
                                default:
                                    break;
                            }
                            SaveSettings();
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return;
                        }
                    }
                }
                return;
            }
            panel_ = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        const float y = static_cast<float>(sy) - headerH_ + scroll_;

        // клики по месячной сетке календаря (дни, месяцы, «Сегодня»)
        if (CalendarGridClick(localX, y)) {
            return;
        }

        for (const CardBtn& cb : cardBtns_) {
            if (cb.rect.Contains(localX, y)) {
                if (cb.action == 100 && !cb.url.empty()) {
                    ShellExecuteW(hwnd, L"open", cb.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    ShowToast(L"Открытие ссылки в браузере…");
                } else if (cb.action == 101) {
                    if (pom::EditDialog(hwnd)) {
                        SaveSettings();
                        RebuildAllDynamicSections(sections_);
                        BuildSearchIndex();
                        BuildView();
                        layoutDirty_ = true;
                        ShowToast(L"Поминальные списки обновлены");
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                } else if (cb.action == 102) {
                    online::StartFetch(hwnd, GetCurrentDateKey(), true);
                    RefreshDynamicSection(sections_, L"azbyka");
                    BuildView();
                    layoutDirty_ = true;
                    ShowToast(L"Обновление данных с azbyka.ru…");
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (cb.action == 103) {
                    chetki::Step(1);
                    RefreshDynamicSection(sections_, L"chetki");
                    BuildView();
                    layoutDirty_ = true;
                    if (chetki::Count() % chetki::Target() == 0) {
                        ShowToast(L"Круг чёток завершён (" + std::to_wstring(chetki::Count()) +
                                  L" молитв) — земной поклон");
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (cb.action == 104) {
                    chetki::CycleTarget();
                    RefreshDynamicSection(sections_, L"chetki");
                    BuildView();
                    layoutDirty_ = true;
                    ShowToast(L"Чётки: круг на " + std::to_wstring(chetki::Target()) + L" молитв");
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (cb.action == 105) {
                    chetki::Reset();
                    RefreshDynamicSection(sections_, L"chetki");
                    BuildView();
                    layoutDirty_ = true;
                    ShowToast(L"Счётчик чёток сброшен");
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return;
            }
        }

        for (const Card& card : cards_) {
            const RectF star(card.starX - 14.0f * scale_, card.starY - 14.0f * scale_,
                             48.0f * scale_, 48.0f * scale_);
            if (star.Contains(localX, y)) {
                ToggleFavorite(view_[static_cast<size_t>(card.item)]);
                BuildView();
                layoutDirty_ = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }

        int block = -1;
        int offset = 0;
        if (HitTest(localX - textX_, y, block, offset)) {
            hasSelection_ = true;
            selBlockA_ = block;
            selOffsetA_ = offset;
            selBlockB_ = block;
            selOffsetB_ = offset;
            dragging_ = true;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else {
            hasSelection_ = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    if (msg == WM_MOUSEMOVE && scrollDrag_) {
        ScrollbarDrag(sx, sy);
        return;
    }

    if (msg == WM_MOUSEMOVE && sideDrag_) {
        const float thumbH = sideThumb_.Height;
        const float travel = std::max(1.0f, sideTrack_.Height - thumbH);
        const float maxScroll = SidebarMaxScroll();
        float t = (static_cast<float>(sy) - sideGrab_ - sideTrack_.Y) / travel;
        t = std::clamp(t, 0.0f, 1.0f);
        sidebarScroll_ = t * maxScroll;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (msg == WM_LBUTTONUP && sideDrag_) {
        sideDrag_ = false;
        ReleaseCapture();
        return;
    }

    if (msg == WM_LBUTTONUP && scrollDrag_) {
        scrollDrag_ = false;
        ReleaseCapture();
        return;
    }

    if (msg == WM_MOUSEMOVE) {
        const bool over = ScrollbarHit(sx, sy);
        if (over != scrollHover_) {
            scrollHover_ = over;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        const bool sideOver = sideTrack_.Width > 0.0f && sx >= static_cast<int>(sideTrack_.X);
        if (sideOver != sideHover_) {
            sideHover_ = sideOver;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        UpdateHover(hwnd, sx, sy);
    }

    if (msg == WM_MOUSEMOVE && dragging_) {
        const float localX = static_cast<float>(sx) - sidebarW_;
        const float y = static_cast<float>(sy) - headerH_ + scroll_;
        int block = -1;
        int offset = 0;
        if (HitTest(localX - textX_, y, block, offset)) {
            selBlockB_ = block;
            selOffsetB_ = offset;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    if (msg == WM_LBUTTONUP && dragging_) {
        dragging_ = false;
        ReleaseCapture();
        return;
    }


    if (msg == WM_LBUTTONDBLCLK) {
        const float localX = static_cast<float>(sx) - sidebarW_;
        const float y = static_cast<float>(sy) - headerH_ + scroll_;
        int block = -1;
        int offset = 0;
        if (HitTest(localX - textX_, y, block, offset)) {
            hasSelection_ = true;
            selBlockA_ = block;
            selOffsetA_ = 0;
            selBlockB_ = block;
            selOffsetB_ = static_cast<int>(blocks_[static_cast<size_t>(block)].text.size());
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }
}

void App::UpdateHover(HWND hwnd, int sx, int sy) {
    lastMouseX_ = static_cast<float>(sx);
    lastMouseY_ = static_cast<float>(sy);

    if (focusMode_) {
        const bool hov = focusExitRect_.Width > 0.0f &&
                         focusExitRect_.Contains(static_cast<float>(sx), static_cast<float>(sy));
        if (hov != focusExitHover_) {
            focusExitHover_ = hov;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        tooltipButton_ = -1;
        tooltipVisible_ = false;
    }

    const bool clearHover =
        !focusMode_ && searchClearRect_.Width > 0.0f &&
        searchClearRect_.Contains(static_cast<float>(sx), static_cast<float>(sy));
    if (clearHover != searchClearHover_) {
        searchClearHover_ = clearHover;
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    // подсказка для кнопок в шапке
    static const wchar_t* kHints[] = {L"Тёмное / светлое оформление",
                                      L"Настройки чтения",
                                      L"Содержание раздела",
                                      L"Увеличить шрифт",
                                      L"Уменьшить шрифт",
                                      L"Русский / церковнославянский",
                                      L"Поиск по всем молитвам",
                                      L"Очистить поиск"};
    const int button =
        focusMode_ ? -1 : HeaderButtonAt(static_cast<float>(sx), static_cast<float>(sy));
    if (button != tooltipButton_) {
        tooltipButton_ = button;
        tooltipText_ = (button >= 0 && button < 8) ? kHints[button] : L"";
        tooltipSince_ = GetTickCount64();
        tooltipVisible_ = false;
    }

    // подсветка карточки под курсором
    int card = -1;
    if (panel_ == 0 && sx >= static_cast<int>(sidebarW_) && sy >= static_cast<int>(headerH_)) {
        const float localX = static_cast<float>(sx) - sidebarW_;
        const float y = static_cast<float>(sy) - headerH_ + scroll_;
        card = CardAtPoint(localX, y);
    }
    if (card != hoveredCard_) {
        hoveredCard_ = card;
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    // подсветка дней и кнопок месячной сетки календаря
    int gridCell = -1;
    int gridBtn = -1;
    if (panel_ == 0 && InCalendarSection() && calGridRect_.Height > 0.0f) {
        const float localX = static_cast<float>(sx) - sidebarW_;
        const float y = static_cast<float>(sy) - headerH_ + scroll_;
        if (calGridRect_.Contains(localX, y)) {
            gridCell = CalendarGridCellAt(localX, y);
            if (CalGridPrevRect().Contains(localX, y)) {
                gridBtn = 0;
            } else if (CalGridNextRect().Contains(localX, y)) {
                gridBtn = 1;
            } else if (CalGridTodayRect().Contains(localX, y)) {
                gridBtn = 2;
            }
        }
    }
    if (gridCell != calGridHover_ || gridBtn != calGridHoverBtn_) {
        calGridHover_ = gridCell;
        calGridHoverBtn_ = gridBtn;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void App::OnKey(HWND hwnd, WPARAM wp) {
    const bool ctrl = ctrlDown_ || (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = shiftDown_ || (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (paletteOpen_) {
        if (wp == VK_ESCAPE) {
            ClosePalette();
            return;
        }
        if (wp == VK_RETURN) {
            ActivatePaletteItem(paletteSel_);
            return;
        }
        if (wp == 'K' && ctrl) {
            ClosePalette();
            return;
        }
        if (wp == VK_BACK) {
            if (!paletteQuery_.empty()) {
                paletteQuery_.pop_back();
                paletteSel_ = 0;
                paletteScroll_ = 0;
                BuildPaletteItems();
                RefreshPalette();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        }
        const int total = static_cast<int>(paletteFiltered_.size());
        if (total > 0) {
            if (wp == VK_UP) {
                paletteSel_ = std::max(0, paletteSel_ - 1);
            } else if (wp == VK_DOWN) {
                paletteSel_ = std::min(total - 1, paletteSel_ + 1);
            } else if (wp == VK_PRIOR) {
                paletteSel_ = std::max(0, paletteSel_ - 10);
            } else if (wp == VK_NEXT) {
                paletteSel_ = std::min(total - 1, paletteSel_ + 10);
            } else if (wp == VK_HOME) {
                paletteSel_ = 0;
            } else if (wp == VK_END) {
                paletteSel_ = total - 1;
            } else {
                return;
            }
            paletteHover_ = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }
    if (focusMode_ && (wp == VK_F11 || wp == VK_ESCAPE)) {
        SetFocusMode(false);
        return;
    }
    switch (wp) {
        case VK_RETURN:
            if (GetFocus() != edit_ && section_ >= 0 &&
                section_ < static_cast<int>(sections_.size()) &&
                sections_[static_cast<size_t>(section_)].id == L"chetki") {
                chetki::Step(1);
                RefreshDynamicSection(sections_, L"chetki");
                BuildView();
                layoutDirty_ = true;
                if (chetki::Count() % chetki::Target() == 0) {
                    ShowToast(L"Круг чёток завершён (" + std::to_wstring(chetki::Count()) +
                              L" молитв)");
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        case VK_ESCAPE:
            if (datePopup_ != nullptr && IsWindow(datePopup_)) {
                CloseDatePopup();  // Esc закрывает миникалендарь
                return;
            }
            if (panel_ != 0) {
                panel_ = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            if (GetFocus() == edit_) {
                search_.clear();
                SetWindowTextW(edit_, L"");
                BuildView();
                layoutDirty_ = true;
                SetFocus(hwnd);
            } else {
                hasSelection_ = false;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        case VK_HOME:
            ScrollTo(0);
            return;
        case VK_END:
            ScrollTo(1e9f);
            return;
        case VK_UP:
            ScrollBy(-60.0f * scale_);
            return;
        case VK_DOWN:
            ScrollBy(60.0f * scale_);
            return;
        case VK_OEM_PLUS:
        case VK_ADD:
            if (ctrl) {
                fontSize_ = std::min(26, fontSize_ + 1);
                fontsDirty_ = true;
                layoutDirty_ = true;
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        case VK_OEM_MINUS:
        case VK_SUBTRACT:
            if (ctrl) {
                fontSize_ = std::max(13, fontSize_ - 1);
                fontsDirty_ = true;
                layoutDirty_ = true;
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        case 'F':
            if (ctrl) {
                if (focusMode_) {
                    SetFocusMode(false);
                }
                SetFocus(edit_);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        case 'K':
            if (ctrl) {
                OpenPalette();
            }
            return;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (ctrl) {
                const int digit = static_cast<int>(wp) - '0';
                BuildSidebarItems();
                int index = 0;
                for (const SidebarItem& item : sidebarItems_) {
                    if (item.header) {
                        continue;
                    }
                    if (index == digit - 1) {
                        ApplySection(item.section);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return;
                    }
                    ++index;
                }
            }
            return;
        case 'B':
            if (ctrl) {
                sidebarCollapsed_ = !sidebarCollapsed_;
                sidebarW_ = focusMode_ ? 0.0f : (sidebarCollapsed_ ? 70.0f : 272.0f) * scale_;
                layoutDirty_ = true;
                SaveSettings();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        case VK_F1:
            panel_ = (panel_ == 3) ? 0 : 3;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        case VK_F11:
            SetFocusMode(!focusMode_);
            return;
        case VK_NEXT:
            if (ctrl) {
                for (size_t i = 0; i < cards_.size(); ++i) {
                    if (cards_[i].y > scroll_ + 4.0f * scale_) {
                        ScrollToCard(cards_[i].item, true);
                        return;
                    }
                }
            }
            ScrollBy(clientHeight_ * 0.8f);
            return;
        case VK_PRIOR:
            if (ctrl) {
                for (size_t i = cards_.size(); i > 0; --i) {
                    if (cards_[i - 1].y < scroll_ - 4.0f * scale_) {
                        ScrollToCard(cards_[i - 1].item, true);
                        return;
                    }
                }
            }
            ScrollBy(-clientHeight_ * 0.8f);
            return;
        case 'C':
            if (ctrl) {
                if (shift) {
                    const int card = CardAtPoint(colX_ + 40.0f * scale_, scroll_ + 40.0f * scale_);
                    if (card >= 0) {
                        CopyPrayer(card, false);
                    }
                } else {
                    CopySelection();
                }
            }
            return;
        case 'A':
            if (ctrl) {
                SelectAll();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return;
        default:
            return;
    }
}

void App::OnChar(HWND hwnd, WPARAM wp) {
    if (paletteOpen_) {
        if (GetTickCount64() - paletteOpenedAt_ < 120) {
            return;  // символ, порождённый самой комбинацией Ctrl+K
        }
        if (wp >= 32 && wp != 127) {
            paletteQuery_.push_back(static_cast<wchar_t>(wp));
            paletteSel_ = 0;
            paletteScroll_ = 0;
            BuildPaletteItems();
            RefreshPalette();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }
    if (GetFocus() == edit_) {
        return;
    }
    if (wp == VK_SPACE) {
        ScrollBy(clientHeight_ * 0.8f);
    }
}

void App::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    if ((LOWORD(wp) == IDC_SEARCH) && (HIWORD(wp) == EN_CHANGE)) {
        SyncSearch(hwnd);
    }
}

// Считывает текст из поля поиска и обновляет список молитв.
void App::SyncSearch(HWND hwnd) {
    if (edit_ == nullptr) {
        return;
    }
    wchar_t buf[256] = {};
    GetWindowTextW(edit_, buf, 256);
    const std::wstring value = Trim(buf);
    if (value == search_) {
        return;
    }
    search_ = value;
    searchLower_ = Lower(value);
    BuildView();
    UpdateWindowTitle();
    scroll_ = 0;
    scrollTarget_ = 0;
    layoutDirty_ = true;
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------- Настройки

void App::LoadSettings() {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        const std::wstring dir = std::wstring(path) + L"\\OrthodoxPrayers";
        CreateDirectoryW(dir.c_str(), nullptr);
        const std::wstring file = dir + L"\\settings.ini";
        HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            std::string content;
            char buf[512];
            DWORD read = 0;
            while (ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
                content.append(buf, read);
            }
            CloseHandle(h);
            std::wstring text;
            if (!TryDecodeUtf8(content, text)) {
                // Файл не является UTF-8 (например, отредактирован вручную в нужной
                // локали) — читаем как ANSI текущей системы.
                const int n = MultiByteToWideChar(CP_ACP, 0, content.data(),
                                                  static_cast<int>(content.size()), nullptr,
                                                  0);
                if (n > 0) {
                    text.resize(static_cast<size_t>(n));
                    MultiByteToWideChar(CP_ACP, 0, content.data(),
                                        static_cast<int>(content.size()), text.data(), n);
                }
            }
            if (HasUtf8Mojibake(text)) {  // лечим «Р�?…» — двойную кодировку
                text = RepairUtf8Mojibake(text);
            }
            if (!text.empty() && text.front() == L'\uFEFF') {
                text.erase(0, 1);  // убираем BOM, если файл писался с ним
            }
            size_t pos = 0;
            while (pos < text.size()) {
                size_t eol = text.find(L'\n', pos);
                if (eol == std::wstring::npos) {
                    eol = text.size();
                }
                const std::wstring line = Trim(text.substr(pos, eol - pos));
                pos = eol + 1;
                if (line.empty() || line[0] == L'[') {
                    continue;
                }
                const size_t eq = line.find(L'=');
                if (eq == std::wstring::npos) {
                    continue;
                }
                const std::wstring key = Trim(line.substr(0, eq));
                const std::wstring value = Trim(line.substr(eq + 1));
                if (key == L"fontSize") {
                    fontSize_ = std::clamp(_wtoi(value.c_str()), 13, 26);
                } else if (key == L"dark") {
                    dark_ = (value != L"0");
                    themeIndex_ = dark_ ? 0 : 1;
                } else if (key == L"theme") {
                    themeIndex_ = std::clamp(_wtoi(value.c_str()), 0, 3);
                    dark_ = (themeIndex_ == 0 || themeIndex_ == 2);
                } else if (key == L"lang") {
                    lang_ = (value == L"1") ? 1 : 0;
                } else if (key == L"section") {
                    lastSectionId_ = value;
                } else if (key == L"reminders") {
                    reminders_ = (value != L"0");
                } else if (key == L"chetki_count") {
                    chetki::Load(_wtoi(value.c_str()), chetki::Target());
                } else if (key == L"chetki_target") {
                    chetki::Load(chetki::Count(), _wtoi(value.c_str()));
                } else if (key == L"sidebar") {
                    sidebarCollapsed_ = (value != L"0");
                } else if (key == L"fontFamily") {
                    fontFamily_ = std::clamp(_wtoi(value.c_str()), 0, kFontFamilyCount - 1);
                } else if (key == L"spacing") {
                    spacing_ = std::clamp((float)_wtof(value.c_str()), 0.8f, 1.6f);
                } else if (key == L"column") {
                    columnScale_ = std::clamp((float)_wtof(value.c_str()), 0.7f, 1.3f);
                } else if (key == L"scroll") {
                    std::wstring token;
                    for (wchar_t c : value) {
                        if (c == L';') {
                            const size_t sep = token.find(L'=');
                            if (sep != std::wstring::npos) {
                                scrollMem_[token.substr(0, sep)] =
                                    (float)_wtof(token.c_str() + sep + 1);
                            }
                            token.clear();
                        } else {
                            token += c;
                        }
                    }
                } else if (key == L"favorites") {
                    std::wstring token;
                    for (wchar_t c : value) {
                        if (c == L',') {
                            if (!token.empty()) {
                                favorites_.insert(token);
                            }
                            token.clear();
                        } else {
                            token += c;
                        }
                    }
                    if (!token.empty()) {
                        favorites_.insert(token);
                    }
                } else if (key == L"pom_live") {
                    pom::FromSettings(value, L"");
                } else if (key == L"pom_dead") {
                    std::wstring prevLive;
                    for (const std::wstring& name : pom::Live()) {
                        if (!prevLive.empty()) prevLive += L";";
                        prevLive += name;
                    }
                    pom::FromSettings(prevLive, value);
                }
            }
        }
    }
    if (section_ < 0 || section_ >= static_cast<int>(sections_.size())) {
        section_ = 0;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    const int hour = st.wHour;
    std::wstring cur = lastSectionId_;
    if (cur.empty()) {
        if (hour >= 4 && hour < 11) {
            cur = L"morning";
        } else if (hour >= 17 && hour < 24) {
            cur = L"evening";
        } else {
            cur = L"morning";
        }
    }
    lastSectionId_ = cur;
    SectionById(cur, section_);
    BuildView();
}

// Миникалендарь: выбор любой даты во всплывающем окне под плиткой даты.
// Собственная отрисовка (GDI+) и хит-тест — системный MonthCal на этой системе
// теряет согласованность координат после MCM_SETCURSEL.
void App::ToggleDatePopup() {
    if (datePopup_ != nullptr) {
        const bool wasOpen = IsWindow(datePopup_) != FALSE;
        CloseDatePopup();
        if (wasOpen) {
            return;  // повторный клик по плитке закрывает попап
        }
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DatePopupProc;
        wc.hInstance = inst_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // фон рисуем сами (WM_ERASEBKGND → 1)
        wc.lpszClassName = kDatePopupClass;
        RegisterClassW(&wc);
        registered = true;
    }

    // метрика (должна совпадать с расчётом в WM_CREATE)
    const float s = scale_;
    const int padX = static_cast<int>(14.0f * s);
    const int titleH = static_cast<int>(34.0f * s);
    const int weekH = static_cast<int>(22.0f * s);
    const int cellW = static_cast<int>(33.0f * s);
    const int cellH = static_cast<int>(30.0f * s);
    const int clientW = padX * 2 + 7 * cellW;
    const int clientH = titleH + weekH + kPopupRows * cellH + static_cast<int>(31.0f * s);

    const DayInfo& d = GetCurrentDayInfo();
    DatePopupInit init{};
    init.owner = hwnd_;
    init.scale = s;
    init.year = d.g.y;
    init.month = d.g.m;  // календарь открывается на месяце просматриваемой даты
    init.bg = theme::Gc(pal_.card).GetValue();
    init.border = theme::Gc(pal_.cardBorder).GetValue();
    init.hover = theme::Gc(pal_.hover).GetValue();
    init.accent = theme::Gc(pal_.accent).GetValue();
    init.text = theme::Gc(pal_.text).GetValue();
    init.textDim = theme::Gc(pal_.textDim).GetValue();

    // размещаем под плиткой даты, не вылезая за экран
    POINT anchor{
        static_cast<LONG>(btnToday_.X + btnToday_.Width * 0.5f),
        static_cast<LONG>(btnToday_.GetBottom()),
    };
    ClientToScreen(hwnd_, &anchor);
    POINT pillTop{ static_cast<LONG>(btnToday_.X), static_cast<LONG>(btnToday_.Y) };
    ClientToScreen(hwnd_, &pillTop);

    int x = anchor.x - clientW / 2;
    int y = anchor.y + static_cast<int>(4.0f * s);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) {
        x = std::clamp(x, static_cast<int>(mi.rcWork.left) + 4,
                       static_cast<int>(mi.rcWork.right) - clientW - 4);
        if (y + clientH > mi.rcWork.bottom) {
            y = std::max(static_cast<LONG>(mi.rcWork.top), pillTop.y - clientH - 8);
        }
    }

    datePopup_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                 kDatePopupClass, nullptr, WS_POPUP | WS_CLIPSIBLINGS, x, y,
                                 clientW, clientH, hwnd_, nullptr, inst_, &init);
    if (datePopup_ == nullptr) {
        return;
    }
    ShowWindow(datePopup_, SW_SHOWNA);
}

void App::CloseDatePopup() {
    if (datePopup_ != nullptr) {
        if (IsWindow(datePopup_)) {
            DestroyWindow(datePopup_);
        }
        datePopup_ = nullptr;
    }
}

// Заголовок окна и рамка в тон палитры (Windows 11; на Windows 10 просто
// переключается тёмная шапка, если она поддерживается системой).
void App::ApplyWindowTheme() {
    if (hwnd_ == nullptr) {
        return;
    }
    const theme::Palette p = theme::MakeByIndex(themeIndex_);
    BOOL dark = p.dark ? TRUE : FALSE;
    if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
    }
    const COLORREF caption = p.sidebarTop;
    const COLORREF captionText = p.text;
    const COLORREF borderColor = p.sidebarLine;
    DwmSetWindowAttribute(hwnd_, 35, &caption, sizeof(caption));       // DWMWA_CAPTION_COLOR
    DwmSetWindowAttribute(hwnd_, 36, &captionText, sizeof(captionText));// DWMWA_TEXT_COLOR
    DwmSetWindowAttribute(hwnd_, 34, &borderColor, sizeof(borderColor));// DWMWA_BORDER_COLOR
}

void App::RepositionDatePopup() {
    if (datePopup_ == nullptr || !IsWindow(datePopup_)) {
        return;
    }
    RECT wr = {};
    GetWindowRect(datePopup_, &wr);
    const int w = wr.right - wr.left;
    const int hgt = wr.bottom - wr.top;
    if (w <= 0 || hgt <= 0) {
        return;
    }
    POINT anchor{
        static_cast<LONG>(btnToday_.X + btnToday_.Width * 0.5f),
        static_cast<LONG>(btnToday_.GetBottom()),
    };
    ClientToScreen(hwnd_, &anchor);
    POINT pillTop{static_cast<LONG>(btnToday_.X), static_cast<LONG>(btnToday_.Y)};
    ClientToScreen(hwnd_, &pillTop);
    int x = anchor.x - w / 2;
    int y = anchor.y + static_cast<int>(4.0f * scale_);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) {
        x = std::clamp(x, static_cast<int>(mi.rcWork.left) + 4,
                       static_cast<int>(mi.rcWork.right) - w - 4);
        if (y + hgt > mi.rcWork.bottom) {
            y = static_cast<int>(
                std::max(static_cast<LONG>(mi.rcWork.top), pillTop.y - hgt - 8));
        }
    }
    SetWindowPos(datePopup_, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void App::ApplyCalendarOffset(int offsetDays, bool announceToday) {
    CloseDatePopup();  // смена даты закрывает висящий миникалендарь
    SetCalendarOffsetDays(offsetDays);
    const std::wstring key = GetCurrentDateKey();
    online::LoadCacheForDate(key);
    RebuildAllDynamicSections(sections_);
    BuildSearchIndex();
    BuildView();
    layoutDirty_ = true;
    calGridInit_ = false;  // сетка показывает месяц выбранной даты
    calGridHover_ = -1;
    calGridDirty_ = true;
    const DayInfo& d = GetCurrentDayInfo();
    ShowToast(announceToday ? L"Календарь: возврат на сегодня (" + DateRu(d.g) + L")"
                            : L"Дата: " + DateRu(d.g) + L" (" + d.period + L")");
    if (hwnd_ != nullptr) {
        online::StartFetch(hwnd_, key);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::StepCalendarDate(int deltaDays, bool resetToToday) {
    // относительный сдвиг (стрелки, горячие клавиши)
    ApplyCalendarOffset(resetToToday ? 0 : (GetCalendarOffsetDays() + deltaDays), resetToToday);
}

void App::SaveSettings() {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return;
    }
    const std::wstring dir = std::wstring(path) + L"\\OrthodoxPrayers";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring file = dir + L"\\settings.ini";

    std::wstring out;
    out += L"[settings]\n";
    out += L"fontSize=" + std::to_wstring(fontSize_) + L"\n";
    out += L"dark=" + std::wstring(dark_ ? L"1" : L"0") + L"\n";
    out += L"theme=" + std::to_wstring(themeIndex_) + L"\n";
    out += L"lang=" + std::to_wstring(lang_) + L"\n";
    out += L"section=" + sections_[static_cast<size_t>(section_)].id + L"\n";
    out += L"fontFamily=" + std::to_wstring(fontFamily_) + L"\n";
    out += L"sidebar=" + std::wstring(sidebarCollapsed_ ? L"1" : L"0") + L"\n";
    out += L"spacing=" + std::to_wstring(spacing_) + L"\n";
    out += L"column=" + std::to_wstring(columnScale_) + L"\n";
    out += L"scroll=";
    {
        if (search_.empty()) {
            scrollMem_[sections_[static_cast<size_t>(section_)].id] = scroll_;
        }
        bool firstPos = true;
        for (const auto& entry : scrollMem_) {
            if (!firstPos) {
                out += L";";
            }
            out += entry.first + L"=" + std::to_wstring(entry.second);
            firstPos = false;
        }
    }
    out += L"\n";
    out += L"favorites=";
    bool first = true;
    for (const std::wstring& key : favorites_) {
        if (!first) {
            out += L",";
        }
        out += key;
        first = false;
    }
    out += L"\n";
    out += L"pom_live=" + pom::SettingsLive() + L"\n";
    out += L"pom_dead=" + pom::SettingsDead() + L"\n";
    out += L"reminders=" + std::wstring(reminders_ ? L"1" : L"0") + L"\n";
    out += L"chetki_count=" + std::to_wstring(chetki::Count()) + L"\n";
    out += L"chetki_target=" + std::to_wstring(chetki::Target()) + L"\n";

    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    const std::string utf8 = WideToUtf8(out);
    DWORD written = 0;
    WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(h);
}

// ============================================================================

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int showCmd) {
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES | ICC_DATE_CLASSES};
    InitCommonControlsEx(&icc);

    GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    if (GdiplusStartup(&gdiToken, &gdiInput, nullptr) != Ok) {
        return 1;
    }

    int code = 0;
    {
        // App хранит GDI+ шрифты и кисти: он должен быть уничтожен до
        // GdiplusShutdown, иначе деструкторы обращаются к выгруженному GDI+.
        App app;
        code = app.Run(inst, showCmd);
    }
    GdiplusShutdown(gdiToken);
    return code;
}
