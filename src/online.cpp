// Онлайн-блок «Азбуки»: загрузка дня из календаря azbyka.ru, разбор всех
// рубрик (святцы, чтения, тропари дня, притча, цитата, основы православия,
// практический совет, пролог, день в истории) и кэш в %APPDATA%.

#include "online.h"

#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>

#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace online {

namespace {

std::wstring g_dateKey;
std::wstring g_targetKey;
Data g_data;

std::wstring AppDataDir() {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return std::wstring();
    }
    const std::wstring dir = std::wstring(path) + L"\\OrthodoxPrayers";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring WideFromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                      static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) {
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                            out.data(), n);
    }
    return out;
}

std::wstring TodayKey() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[16];
    wsprintfW(buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring TrimStr(const std::wstring& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && iswspace(s[b])) ++b;
    while (e > b && iswspace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

std::wstring CollapseSpaces(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    bool inSpace = false;
    for (wchar_t c : s) {
        if (iswspace(c)) {
            if (!inSpace && !out.empty()) {
                out += L' ';
            }
            inSpace = true;
        } else {
            out += c;
            inSpace = false;
        }
    }
    return TrimStr(out);
}

void DecodeEntities(std::wstring& s) {
    auto Replace = [&s](const wchar_t* what, const wchar_t* to) {
        const size_t wlen = wcslen(what);
        const size_t tlen = wcslen(to);
        size_t pos = 0;
        while ((pos = s.find(what, pos)) != std::wstring::npos) {
            s.replace(pos, wlen, to);
            pos += tlen;
        }
    };
    Replace(L"&ndash;", L"\u2013");
    Replace(L"&mdash;", L"\u2014");
    Replace(L"&laquo;", L"\u00AB");
    Replace(L"&raquo;", L"\u00BB");
    Replace(L"&bdquo;", L"\u201E");
    Replace(L"&ldquo;", L"\u201C");
    Replace(L"&rdquo;", L"\u201D");
    Replace(L"&hellip;", L"\u2026");
    Replace(L"&nbsp;", L" ");
    Replace(L"&amp;", L"&");
    Replace(L"&quot;", L"\"");
    Replace(L"&#039;", L"'");
    Replace(L"&apos;", L"'");
    Replace(L"&gt;", L">");
    Replace(L"&lt;", L"<");
}

// Удаление HTML-тегов. Если preserveParagraphs == true, теги <br>, </p>, </li>
// превращаются в перенос строки, а внутри абзацев пробелы сжимаются.
std::wstring CleanHtml(std::wstring s, bool preserveParagraphs = true) {
    // Вырезаем <span class="bg_data_tooltip">...</span> и <script>...</script>
    auto EraseBlock = [&s](const wchar_t* openTag, const wchar_t* closeTag) {
        size_t p = 0;
        while ((p = s.find(openTag, p)) != std::wstring::npos) {
            const size_t e = s.find(closeTag, p);
            if (e == std::wstring::npos) {
                s.erase(p);
                break;
            }
            s.erase(p, e + wcslen(closeTag) - p);
        }
    };
    EraseBlock(L"<script", L"</script>");
    EraseBlock(L"<style", L"</style>");

    size_t pos = 0;
    while ((pos = s.find(L'<', pos)) != std::wstring::npos) {
        const size_t end = s.find(L'>', pos);
        if (end == std::wstring::npos) {
            s.resize(pos);
            break;
        }
        std::wstring tag = s.substr(pos, end - pos + 1);
        for (wchar_t& c : tag) c = static_cast<wchar_t>(towlower(c));
        bool nl = false;
        if (preserveParagraphs) {
            if (tag.find(L"<br") == 0 || tag.find(L"</p") == 0 ||
                tag.find(L"</li") == 0 || tag.find(L"</div") == 0 ||
                tag.find(L"</h2") == 0 || tag.find(L"</h3") == 0) {
                nl = true;
            }
        }
        s.erase(pos, end - pos + 1);
        if (nl) {
            s.insert(pos, L"\n");
            pos += 1;
        }
    }

    DecodeEntities(s);

    if (!preserveParagraphs) {
        return CollapseSpaces(s);
    }

    std::wstring out;
    size_t start = 0;
    while (start < s.size()) {
        size_t e = s.find(L'\n', start);
        if (e == std::wstring::npos) e = s.size();
        std::wstring line = CollapseSpaces(s.substr(start, e - start));
        if (!line.empty()) {
            if (!out.empty()) out += L"\n";
            out += line;
        }
        start = e + 1;
    }
    return out;
}

std::wstring SliceBetween(const std::wstring& html, const wchar_t* startMarker,
                          const wchar_t* endMarker, size_t fromPos = 0) {
    const size_t a = html.find(startMarker, fromPos);
    if (a == std::wstring::npos) return std::wstring();
    const size_t contentStart = a + wcslen(startMarker);
    const size_t b = html.find(endMarker, contentStart);
    const size_t finish = (b == std::wstring::npos) ? html.size() : b;
    return html.substr(contentStart, finish - contentStart);
}

bool ParseAzbykaHtml(const std::wstring& raw, Data& out) {
    out.saints.clear();
    out.readings.clear();
    out.quote.clear();
    out.parable.clear();
    out.osnovy.clear();
    out.sovet.clear();
    out.prologue.clear();
    out.history.clear();
    out.tropari.clear();

    // 1. Святцы дня (список святых в верхнем блоке)
    {
        const std::wstring dayText =
            SliceBetween(raw, L"<div class=\"text day__text\">", L"<div id=\"chteniya\"");
        if (!dayText.empty()) {
            size_t ulPos = dayText.find(L"<ul");
            if (ulPos != std::wstring::npos) {
                std::wstring listHtml = dayText.substr(ulPos);
                std::wstring saintsList;
                size_t li = 0;
                while ((li = listHtml.find(L"<li", li)) != std::wstring::npos) {
                    const size_t gt = listHtml.find(L'>', li);
                    const size_t endLi = listHtml.find(L"</li>", li);
                    if (gt == std::wstring::npos || endLi == std::wstring::npos) break;
                    std::wstring item =
                        CleanHtml(listHtml.substr(gt + 1, endLi - gt - 1), false);
                    if (!item.empty()) {
                        if (!saintsList.empty()) saintsList += L"\n";
                        saintsList += L"\u2022 " + item;
                    }
                    li = endLi + 5;
                }
                out.saints = std::move(saintsList);
            }
        }
    }

    // 2. Чтения Священного Писания
    {
        std::wstring rblock =
            SliceBetween(raw, L"<div class=\"readings-text\">", L"<div class=\"scripture-sub");
        if (rblock.empty()) {
            rblock = SliceBetween(raw, L"<div id=\"chteniya\"", L"<div class=\"scripture-sub");
        }
        if (!rblock.empty()) {
            out.readings = CleanHtml(rblock, false);
        }
    }

    // 3. Притча дня
    {
        const std::wstring pblock =
            SliceBetween(raw, L"<div id=\"pritcha\"", L"<div id=\"propoved\"");
        if (!pblock.empty()) {
            const std::wstring brif = SliceBetween(pblock, L"<div class=\"brif\">", L"</div>");
            out.parable = CleanHtml(brif.empty() ? pblock : brif, true);
        }
    }

    // 4. Основы православия
    {
        const std::wstring oblock =
            SliceBetween(raw, L"<div id=\"osnovy\"", L"<div id=\"sovet\"");
        if (!oblock.empty()) {
            const std::wstring body = SliceBetween(oblock, L"</h2>", L"</div>");
            out.osnovy = CleanHtml(body.empty() ? oblock : body, true);
        }
    }

    // 5. Практический совет
    {
        const std::wstring sblock =
            SliceBetween(raw, L"<div id=\"sovet\"", L"<div class=\"block boxes taks_block random_test");
        if (!sblock.empty()) {
            const std::wstring body = SliceBetween(sblock, L"</h2>", L"</div>");
            out.sovet = CleanHtml(body.empty() ? sblock : body, true);
        }
    }

    // 6. Цитата дня
    {
        const std::wstring qblock =
            SliceBetween(raw, L"<!--day-widget:quote-->", L"<!--/day-widget:quote-->");
        if (!qblock.empty()) {
            const std::wstring box = SliceBetween(qblock, L"<div class=\"box\">", L"</div>");
            out.quote = CleanHtml(box.empty() ? qblock : box, true);
        }
    }

    // 7. Пролог в поучениях
    {
        const std::wstring prblock =
            SliceBetween(raw, L"<!--day-widget:prologue-->", L"<!--/day-widget:prologue-->");
        if (!prblock.empty()) {
            const std::wstring box = SliceBetween(prblock, L"<div class=\"box\">", L"</div>");
            out.prologue = CleanHtml(box.empty() ? prblock : box, true);
        }
    }

    // 8. Этот день в истории
    {
        const std::wstring hblock =
            SliceBetween(raw, L"<!--day-widget:history-->", L"<!--/day-widget:history-->");
        if (!hblock.empty()) {
            const std::wstring box = SliceBetween(hblock, L"<div class=\"box\">", L"</div>");
            out.history = CleanHtml(box.empty() ? hblock : box, true);
        }
    }

    // 9. Тропари, кондаки, молитвы и величания дня
    {
        const std::wstring tblock =
            SliceBetween(raw, L"<div id=\"tropari\"", L"<div id=\"pritcha\"");
        size_t pos = 0;
        while ((pos = tblock.find(L"<div class=\"tropary-item\"", pos)) != std::wstring::npos) {
            const size_t nextItem = tblock.find(L"<div class=\"tropary-item\"", pos + 25);
            const std::wstring itemHtml =
                tblock.substr(pos, (nextItem == std::wstring::npos ? tblock.size() : nextItem) - pos);
            const std::wstring groupName =
                CleanHtml(SliceBetween(itemHtml, L"<h2 class=\"block_title\">", L"</h2>"), false);

            size_t fpos = 0;
            while ((fpos = itemHtml.find(L"<div class=\"inner taks_content\">", fpos)) !=
                   std::wstring::npos) {
                const size_t nextFrame =
                    itemHtml.find(L"<div class=\"inner taks_content\">", fpos + 30);
                const std::wstring frameHtml = itemHtml.substr(
                    fpos, (nextFrame == std::wstring::npos ? itemHtml.size() : nextFrame) - fpos);

                std::wstring head = CleanHtml(SliceBetween(frameHtml, L"<h3", L"</h3>"), false);
                // Срезаем остаток открывающего тега <h3 ...>
                const size_t gt = frameHtml.find(L'>', frameHtml.find(L"<h3"));
                const size_t endH3 = frameHtml.find(L"</h3>");
                if (gt != std::wstring::npos && endH3 != std::wstring::npos && endH3 > gt) {
                    head = CleanHtml(frameHtml.substr(gt + 1, endH3 - gt - 1), false);
                }

                std::wstring bodyPart;
                if (endH3 != std::wstring::npos) {
                    bodyPart = frameHtml.substr(endH3 + 5);
                }
                std::wstring ruText;
                const size_t explPos = bodyPart.find(L"<p class=\"taks-explanation\"");
                std::wstring csPart =
                    (explPos != std::wstring::npos) ? bodyPart.substr(0, explPos) : bodyPart;
                if (explPos != std::wstring::npos) {
                    const std::wstring info =
                        SliceBetween(bodyPart, L"<div class=\"taks-explanation-info\"", L"</div>", explPos);
                    const size_t infoGt = info.find(L'>');
                    if (infoGt != std::wstring::npos) {
                        ruText = CleanHtml(info.substr(infoGt + 1), false);
                    }
                }
                std::wstring csText = CleanHtml(csPart, false);
                if (!csText.empty()) {
                    TroparBlock tb;
                    tb.group = groupName;
                    tb.title = head.empty() ? groupName : (groupName + L" — " + head);
                    tb.cs = std::move(csText);
                    tb.ru = std::move(ruText);
                    out.tropari.push_back(std::move(tb));
                }
                fpos = (nextFrame == std::wstring::npos) ? itemHtml.size() : nextFrame;
            }
            pos = (nextItem == std::wstring::npos) ? tblock.size() : nextItem;
        }
    }

    return !out.readings.empty() || !out.saints.empty() || !out.history.empty() ||
           !out.quote.empty() || !out.tropari.empty();
}

std::string ReadBytesFile(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::string();
    std::string content;
    char buf[16384];
    DWORD read = 0;
    while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0) {
        content.append(buf, static_cast<size_t>(read));
    }
    CloseHandle(h);
    return content;
}

void WriteBytesFile(const std::wstring& path, const std::string& bytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(h);
}

std::wstring CacheFileForDate(const std::wstring& key) {
    const std::wstring dir = AppDataDir();
    if (dir.empty()) return std::wstring();
    return dir + L"\\day_" + key + L".html";
}

}  // namespace

const std::wstring& DateKey() { return g_dateKey; }

const Data& Get() { return g_data; }

void LoadCacheForDate(const std::wstring& dateKey) {
    const std::wstring key = dateKey.empty() ? TodayKey() : dateKey;
    g_dateKey = key;
    g_data = Data{};
    g_data.dateKey = key;

    std::string html = ReadBytesFile(CacheFileForDate(key));
    if (html.empty() && key == L"2026-10-23") {
        html = ReadBytesFile(L"data\\azbyka_sample.html");
        if (html.empty()) {
            html = ReadBytesFile(L"..\\data\\azbyka_sample.html");
        }
    }
    if (!html.empty()) {
        const std::wstring raw = WideFromUtf8(html);
        if (ParseAzbykaHtml(raw, g_data)) {
            g_data.loaded = true;
            g_data.status = L"Загружено для даты " + key + L" (кэш azbyka.ru)";
            return;
        }
    }
    g_data.status = L"Загрузка с azbyka.ru/days/" + key + L"…";
}

namespace {

DWORD WINAPI FetchThread(LPVOID param) {
    const HWND hwnd = static_cast<HWND>(param);
    const std::wstring key = g_targetKey.empty() ? TodayKey() : g_targetKey;

    std::string html;
    HINTERNET ses = WinHttpOpen(L"OrthodoxPrayers/2.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (ses != nullptr) {
        HINTERNET con =
            WinHttpConnect(ses, L"azbyka.ru", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (con != nullptr) {
            const std::wstring path = L"/days/" + key;
            HINTERNET req = WinHttpOpenRequest(con, L"GET", path.c_str(), nullptr,
                                               WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               WINHTTP_FLAG_SECURE);
            if (req != nullptr) {
                if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(req, nullptr)) {
                    char buf[65536];
                    DWORD read = 0;
                    while (WinHttpReadData(req, buf, sizeof(buf), &read) && read > 0) {
                        html.append(buf, static_cast<size_t>(read));
                    }
                }
                WinHttpCloseHandle(req);
            }
            WinHttpCloseHandle(con);
        }
        WinHttpCloseHandle(ses);
    }

    if (!html.empty()) {
        const std::wstring raw = WideFromUtf8(html);
        Data parsed;
        parsed.dateKey = key;
        if (ParseAzbykaHtml(raw, parsed)) {
            const std::wstring cachePath = CacheFileForDate(key);
            if (!cachePath.empty()) {
                WriteBytesFile(cachePath, html);
            }
            parsed.loaded = true;
            parsed.failed = false;
            parsed.busy = false;
            parsed.status = L"Обновлено с портала azbyka.ru/days/" + key;
            g_data = std::move(parsed);
            g_dateKey = key;
        } else {
            g_data.status = L"Ответ получен, но разметка страницы не распознана";
            g_data.failed = true;
            g_data.busy = false;
        }
    } else {
        if (!g_data.loaded) {
            g_data.status = L"Нет соединения с интернетом (показаны офлайн-данные)";
            g_data.failed = true;
        }
        g_data.busy = false;
    }

    if (hwnd != nullptr) {
        PostMessageW(hwnd, kOnlineDoneMsg, 0, 0);
    }
    return 0;
}

}  // namespace

void StartFetch(HWND hwnd, const std::wstring& dateKey, bool force) {
    const std::wstring key = dateKey.empty() ? TodayKey() : dateKey;
    if (g_data.busy) return;
    if (!force && g_data.loaded && g_data.dateKey == key) {
        return;
    }
    g_targetKey = key;
    g_data.busy = true;
    if (!g_data.loaded) {
        g_data.status = L"Загрузка с azbyka.ru/days/" + key + L"…";
    }
    HANDLE h = CreateThread(nullptr, 0, &FetchThread, hwnd, 0, nullptr);
    if (h != nullptr) {
        CloseHandle(h);
    }
}

}  // namespace online
