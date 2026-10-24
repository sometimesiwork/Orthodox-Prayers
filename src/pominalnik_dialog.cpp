// Модальный редактор поминальника (Win32): списки живых и усопших.

#include "pominalnik.h"

#include <commctrl.h>
#include <windowsx.h>

#include <cwchar>

#include "calendar_dates.h"

namespace pom {

namespace {

constexpr int IDC_LIVE = 101;
constexpr int IDC_DEAD = 102;
constexpr int IDC_SAVE = 2001;
constexpr int IDC_CANCEL = 2002;
constexpr UINT WM_EDITOR_DONE = WM_APP + 5;

constexpr wchar_t kWndClass[] = L"OPPominalnikEditor";

LRESULT CALLBACK FreeProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = static_cast<int>(LOWORD(wp));
            if (id == IDC_SAVE || id == IDC_CANCEL) {
                PostMessageW(h, WM_EDITOR_DONE, 0,
                             id == IDC_SAVE ? IDOK : IDCANCEL);
                return 0;
            }
            break;
        }
        case WM_SYSCOMMAND:
            if (wp == SC_CLOSE) {
                PostMessageW(h, WM_EDITOR_DONE, 0, IDCANCEL);
                return 0;
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

bool EditDialog(HWND parent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &FreeProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kWndClass;
        if (RegisterClassExW(&wc) == 0) {
            return false;
        }
        registered = true;
    }

    HFONT uiFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               FF_DONTCARE, L"Segoe UI");

    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW, kWndClass,
                             L"Поминальник",
                             WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT, 520, 390, parent,
                             nullptr, GetModuleHandleW(nullptr), nullptr);
    if (h == nullptr) {
        return false;
    }

    auto AddLabel = [&](const wchar_t* text, int x, int y, int w) {
        HWND c = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, 20,
                                 h, nullptr, nullptr, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);
        return c;
    };
    auto AddEdit = [&](int id, const std::wstring& text, int x, int y, int w, int hh) {
        const DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
                         ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL;
        HWND c = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(), st, x, y, w,
                                 hh, h, (HMENU)(INT_PTR)(INT64)id, nullptr, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);
        return c;
    };
    auto AddButton = [&](int id, const wchar_t* text, int x, int y) {
        HWND c = CreateWindowExW(0, L"BUTTON", text,
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                 x, y, 110, 30, h, (HMENU)(INT_PTR)(INT64)id, nullptr,
                                 nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);
        return c;
    };

    AddLabel(L"О живых (имена через точку с запятой):", 16, 14, 400);
    std::wstring liveText;
    for (size_t i = 0; i < Live().size(); ++i) {
        if (i) liveText += L"; ";
        liveText += Live()[i];
    }
    AddEdit(IDC_LIVE, liveText, 16, 36, 470, 110);

    AddLabel(L"Об усопших: «Имя (дд.мм.гггг)», записи через точку с запятой:",
             16, 156, 460);
    AddEdit(IDC_DEAD, SettingsDead(), 16, 178, 470, 110);

    AddButton(IDC_SAVE, L"Сохранить", 286, 300);
    AddButton(IDC_CANCEL, L"Отмена", 402, 300);

    EnableWindow(parent, FALSE);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    SetFocus(GetDlgItem(h, IDC_LIVE));

    int result = IDCANCEL;
    bool running = true;
    MSG msg;
    while (running) {
        const BOOL ok = GetMessageW(&msg, nullptr, 0, 0);
        if (ok <= 0) {
            break;
        }
        if (msg.message == WM_EDITOR_DONE) {
            result = static_cast<int>(msg.lParam);
            running = false;
            continue;
        }
        if (msg.message == WM_QUIT) {
            PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            result = IDCANCEL;
            running = false;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (result == IDOK) {
        auto GetText = [&](int id) {
            const int len = GetWindowTextLengthW(GetDlgItem(h, id)) + 1;
            std::wstring buf(static_cast<size_t>(len), L'\0');
            GetWindowTextW(GetDlgItem(h, id), buf.data(), len);
            return buf;
        };
        FromSettings(GetText(IDC_LIVE), GetText(IDC_DEAD));
    }

    EnableWindow(parent, TRUE);
    DestroyWindow(h);
    if (uiFont != nullptr) {
        DeleteObject(uiFont);
    }
    SetForegroundWindow(parent);
    return result == IDOK;
}

}  // namespace pom
