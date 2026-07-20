#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

struct PromptPreset {
    std::wstring name;
    std::wstring prompt;
};

struct HistoryEntry {
    std::wstring presetName;
    std::wstring text;
};

static constexpr int ID_TAB_PROMPT = 1000;
static constexpr int ID_TAB_SETTINGS = 1001;
static constexpr int ID_PRESET = 1002;
static constexpr int ID_INPUT = 1003;
static constexpr int ID_PASTE = 1004;
static constexpr int ID_PASTE_GO = 1005;
static constexpr int ID_OUTPUT = 1006;
static constexpr int ID_CLEAR = 1007;
static constexpr int ID_EDIT_PROMPTS = 1008;
static constexpr int ID_HISTORY_LIST = 1009;
static constexpr int ID_DELETE_ALL_HISTORY = 1010;
static constexpr int ID_HISTORY_ENABLED = 1011;
static constexpr int ID_CONFIRM_DELETE = 1012;
static constexpr int ID_CONFIRM_CANCEL = 1013;
static constexpr int ID_PRESET_OPTION_BASE = 3000;

static constexpr int ID_EDITOR_LIST = 2001;
static constexpr int ID_EDITOR_NAME = 2002;
static constexpr int ID_EDITOR_PROMPT = 2003;
static constexpr int ID_EDITOR_NEW = 2004;
static constexpr int ID_EDITOR_DELETE = 2005;
static constexpr int ID_EDITOR_SAVE = 2006;
static constexpr int ID_EDITOR_CANCEL = 2007;
static constexpr int ID_EDITOR_UP = 2008;
static constexpr int ID_EDITOR_DOWN = 2009;
static constexpr int MAX_PRESET_NAME_LENGTH = 30;
static constexpr int EDITOR_PRESET_ROW_HEIGHT = 24;

static const COLORREF COLOR_BG = RGB(25, 25, 25);
static const COLORREF COLOR_PANEL = RGB(35, 35, 35);
static const COLORREF COLOR_TEXT = RGB(245, 245, 245);
static const COLORREF COLOR_MUTED = RGB(180, 180, 180);
static const COLORREF COLOR_ACCENT = RGB(159, 0, 255);
static const COLORREF COLOR_BLUE = RGB(82, 139, 255);
static const COLORREF COLOR_GREEN = RGB(76, 175, 80);
static const COLORREF COLOR_RED = RGB(244, 67, 54);
static const COLORREF COLOR_YELLOW = RGB(255, 214, 64);
static const COLORREF COLOR_BORDER = RGB(56, 56, 56);

static HINSTANCE g_instance = nullptr;
static HWND g_main = nullptr;
static HWND g_promptTab = nullptr;
static HWND g_settingsTab = nullptr;
static HWND g_presetButton = nullptr;
static HWND g_presetPopup = nullptr;
static HWND g_input = nullptr;
static HWND g_historyList = nullptr;
static HWND g_historyEnabled = nullptr;
static HWND g_promptEditor = nullptr;
static HFONT g_font = nullptr;
static HFONT g_buttonFont = nullptr;
static HBRUSH g_bgBrush = nullptr;
static HBRUSH g_panelBrush = nullptr;
static HIMAGELIST g_historyRowImages = nullptr;
static std::vector<HWND> g_promptControls;
static std::vector<HWND> g_settingsControls;
static std::vector<PromptPreset> g_presets;
static std::vector<HistoryEntry> g_history;
static int g_selectedPreset = 0;
static bool g_saveHistory = true;
static int g_activeTab = 0;
static int g_presetPopupOffset = 0;
static std::vector<RECT> g_mainEditFrames;
static std::vector<RECT> g_settingsFrames;

static void ApplyDarkTitleBar(HWND hwnd);
static void TogglePresetList();
static void RefreshPresets();
static void SaveSettings();
static std::wstring CurrentPresetName();
static void RenderPresetPopupOptions();

static std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    std::wstring result(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), needed);
    return result;
}

static std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return "";
    int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string result(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), needed, nullptr, nullptr);
    return result;
}

static std::string Base64Encode(const std::string& input) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0;
    int valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string Base64Decode(const std::string& input) {
    static const std::string table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> map(256, -1);
    for (int i = 0; i < 64; ++i) map[(unsigned char)table[i]] = i;

    std::string out;
    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        if (map[c] == -1) continue;
        val = (val << 6) + map[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static std::wstring ExecutableDirectory() {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(g_instance, modulePath, MAX_PATH);
    return std::filesystem::path(modulePath).parent_path().wstring();
}

static std::wstring DataDirectory() {
    return ExecutableDirectory();
}

static std::wstring PresetsPath() { return DataDirectory() + L"\\presets.dat"; }
static std::wstring HistoryPath() { return DataDirectory() + L"\\history.dat"; }
static std::wstring SettingsPath() { return DataDirectory() + L"\\settings.dat"; }

static std::wstring DefaultPrompt() {
    return L"Make the text more readable. Only output the revised text. Do not use a dash when a comma works instead. These instructions apply to every message I send in this temporary chat. Treat each new message as completely independent, and do not carry over or add any content from previous messages.";
}

static std::wstring Trim(std::wstring text) {
    while (!text.empty() && iswspace(text.back())) text.pop_back();
    size_t first = 0;
    while (first < text.size() && iswspace(text[first])) ++first;
    return text.substr(first);
}

static std::wstring LimitPresetName(std::wstring name) {
    if (name.size() > MAX_PRESET_NAME_LENGTH) name.resize(MAX_PRESET_NAME_LENGTH);
    return name;
}

static std::wstring NormalizeForEdit(const std::wstring& text) {
    std::wstring normalized;
    normalized.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            normalized += L"\r\n";
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
        } else if (text[i] == L'\n') {
            normalized += L"\r\n";
        } else {
            normalized.push_back(text[i]);
        }
    }
    return normalized;
}

static std::wstring GetText(HWND hwnd) {
    int length = GetWindowTextLengthW(hwnd);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(length);
    return text;
}

static void SetText(HWND hwnd, const std::wstring& text) {
    SetWindowTextW(hwnd, NormalizeForEdit(text).c_str());
}

static void SetAppFont(HWND hwnd) {
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font, TRUE);
}

static void SetButtonFont(HWND hwnd) {
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_buttonFont, TRUE);
}

static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_KEYDOWN && wp == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        SendMessageW(hwnd, EM_SETSEL, 0, -1);
        return 0;
    }

    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, EditSubclassProc, 1);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void StyleEdit(HWND hwnd) {
    SetAppFont(hwnd);
    SendMessageW(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
    SetWindowSubclass(hwnd, EditSubclassProc, 1, 0);
}

static RECT MakeRect(int x, int y, int w, int h) {
    return RECT{x, y, x + w, y + h};
}

static void DrawEditFrame(HDC dc, const RECT& rect) {
    HBRUSH brush = CreateSolidBrush(COLOR_PANEL);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static HWND MakeFramedEdit(HWND parent, int id, int x, int y, int w, int h, DWORD style, std::vector<RECT>& frames) {
    frames.push_back(MakeRect(x, y, w, h));
    const int verticalInset = h <= 40 ? 5 : 8;
    HWND edit = CreateWindowExW(0, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | style, x + 8, y + verticalInset, w - 16, h - (verticalInset * 2), parent, (HMENU)(INT_PTR)id, g_instance, nullptr);
    StyleEdit(edit);
    return edit;
}

static LRESULT CALLBACK EditorListSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL) {
        int count = (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
        RECT client{};
        GetClientRect(hwnd, &client);
        int visibleRows = std::max(1, (int)(client.bottom - client.top) / EDITOR_PRESET_ROW_HEIGHT);
        int maxTop = std::max(0, count - visibleRows);
        int topIndex = (int)SendMessageW(hwnd, LB_GETTOPINDEX, 0, 0);
        int step = GET_WHEEL_DELTA_WPARAM(wp) < 0 ? 3 : -3;
        topIndex = std::clamp(topIndex + step, 0, maxTop);
        SendMessageW(hwnd, LB_SETTOPINDEX, topIndex, 0);
        InvalidateRect(GetParent(hwnd), nullptr, TRUE);
        return 0;
    }

    LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
    if (msg == WM_KEYDOWN || msg == WM_LBUTTONUP || msg == WM_VSCROLL) {
        InvalidateRect(GetParent(hwnd), nullptr, TRUE);
    } else if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, EditorListSubclassProc, 4);
    }
    return result;
}

static void DrawModernScrollIndicator(HDC dc, HWND list, const RECT& frame) {
    int count = (int)SendMessageW(list, LB_GETCOUNT, 0, 0);
    RECT client{};
    GetClientRect(list, &client);
    int visibleRows = std::max(1, (int)(client.bottom - client.top) / EDITOR_PRESET_ROW_HEIGHT);
    if (count <= visibleRows) return;

    RECT track{frame.right - 12, frame.top + 10, frame.right - 6, frame.bottom - 10};
    int trackHeight = std::max(1, (int)(track.bottom - track.top));
    int thumbHeight = std::max(24, (trackHeight * visibleRows) / count);
    int topIndex = (int)SendMessageW(list, LB_GETTOPINDEX, 0, 0);
    int maxTop = std::max(1, count - visibleRows);
    int thumbTop = track.top + ((trackHeight - thumbHeight) * topIndex) / maxTop;
    RECT thumb{track.left, thumbTop, track.right, thumbTop + thumbHeight};

    HBRUSH trackBrush = CreateSolidBrush(RGB(42, 42, 42));
    HBRUSH thumbBrush = CreateSolidBrush(COLOR_ACCENT);
    HPEN nullPen = CreatePen(PS_NULL, 0, COLOR_BG);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    HGDIOBJ oldBrush = SelectObject(dc, trackBrush);
    RoundRect(dc, track.left, track.top, track.right, track.bottom, 6, 6);
    SelectObject(dc, thumbBrush);
    RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, 6, 6);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(trackBrush);
    DeleteObject(thumbBrush);
    DeleteObject(nullPen);
}

static LRESULT CALLBACK PresetButtonSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_LBUTTONUP) {
        TogglePresetList();
        return 0;
    }

    if (msg == WM_KEYDOWN && (wp == VK_SPACE || wp == VK_RETURN)) {
        TogglePresetList();
        return 0;
    }

    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, PresetButtonSubclassProc, 2);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK PresetOptionSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL) {
        SendMessageW(GetParent(hwnd), WM_MOUSEWHEEL, wp, lp);
        return 0;
    }

    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, PresetOptionSubclassProc, 3);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void SavePresets() {
    std::ofstream file(std::filesystem::path(PresetsPath()), std::ios::binary | std::ios::trunc);
    for (const auto& preset : g_presets) {
        file << Base64Encode(WideToUtf8(LimitPresetName(preset.name))) << '\t'
             << Base64Encode(WideToUtf8(preset.prompt)) << '\n';
    }
}

static void LoadPresets() {
    g_presets.clear();
    std::ifstream file(std::filesystem::path(PresetsPath()), std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;

        PromptPreset preset;
        preset.name = LimitPresetName(Utf8ToWide(Base64Decode(line.substr(0, tab))));
        preset.prompt = Utf8ToWide(Base64Decode(line.substr(tab + 1)));
        if (!preset.name.empty()) g_presets.push_back(preset);
    }

    if (g_presets.empty()) {
        g_presets.push_back({L"Grammar Fix", DefaultPrompt()});
        g_presets.push_back({L"Summarize", L"Summarize the text clearly and briefly. Only output the summary."});
    }
}

static void SaveHistory() {
    std::ofstream file(std::filesystem::path(HistoryPath()), std::ios::binary | std::ios::trunc);
    size_t start = g_history.size() > 200 ? g_history.size() - 200 : 0;
    for (size_t i = start; i < g_history.size(); ++i) {
        file << Base64Encode(WideToUtf8(g_history[i].presetName)) << '\t'
             << Base64Encode(WideToUtf8(g_history[i].text)) << '\n';
    }
}

static void LoadHistory() {
    g_history.clear();
    std::ifstream file(std::filesystem::path(HistoryPath()), std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            std::wstring item = Utf8ToWide(Base64Decode(line));
            if (!item.empty()) g_history.push_back({L"Unknown", item});
            continue;
        }

        HistoryEntry entry;
        entry.presetName = Utf8ToWide(Base64Decode(line.substr(0, tab)));
        entry.text = Utf8ToWide(Base64Decode(line.substr(tab + 1)));
        if (entry.presetName.empty()) entry.presetName = L"Unknown";
        if (!entry.text.empty()) g_history.push_back(entry);
    }
}

static void SaveSettings() {
    std::ofstream file(std::filesystem::path(SettingsPath()), std::ios::binary | std::ios::trunc);
    std::wstring selectedName;
    if (g_selectedPreset >= 0 && g_selectedPreset < (int)g_presets.size()) selectedName = g_presets[g_selectedPreset].name;
    file << "selectedName=" << Base64Encode(WideToUtf8(LimitPresetName(selectedName))) << '\n';
    file << "history=" << (g_saveHistory ? "1" : "0") << '\n';
}

static void LoadSettings() {
    g_selectedPreset = 0;
    g_saveHistory = true;

    std::ifstream file(std::filesystem::path(SettingsPath()), std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("selectedName=", 0) == 0) {
            std::wstring name = LimitPresetName(Utf8ToWide(Base64Decode(line.substr(13))));
            for (int i = 0; i < (int)g_presets.size(); ++i) {
                if (g_presets[i].name == name) {
                    g_selectedPreset = i;
                    break;
                }
            }
        } else if (line.rfind("history=", 0) == 0) {
            g_saveHistory = line.substr(8) != "0";
        }
    }
}

static std::string UrlEncode(const std::wstring& text) {
    std::string input = WideToUtf8(text);
    const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : input) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

static HWND MakeLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, bool settings = false) {
    HWND hwnd = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_CENTER, x, y, w, h, parent, nullptr, g_instance, nullptr);
    SetAppFont(hwnd);
    (settings ? g_settingsControls : g_promptControls).push_back(hwnd);
    return hwnd;
}

static HWND MakeButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, COLORREF color, bool settings = false) {
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, y, w, h, parent, (HMENU)(INT_PTR)id, g_instance, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(color));
    SetButtonFont(hwnd);
    (settings ? g_settingsControls : g_promptControls).push_back(hwnd);
    return hwnd;
}

static HWND MakeUntrackedButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, COLORREF color) {
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, y, w, h, parent, (HMENU)(INT_PTR)id, g_instance, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(color));
    SetButtonFont(hwnd);
    return hwnd;
}

static void DrawButton(const DRAWITEMSTRUCT* item) {
    COLORREF outline = static_cast<COLORREF>(GetWindowLongPtrW(item->hwndItem, GWLP_USERDATA));
    bool isTab = item->CtlID == ID_TAB_PROMPT || item->CtlID == ID_TAB_SETTINGS;
    bool selectedTab = (item->CtlID == ID_TAB_PROMPT && g_activeTab == 0) || (item->CtlID == ID_TAB_SETTINGS && g_activeTab == 1);
    if (item->CtlID == ID_HISTORY_ENABLED) outline = g_saveHistory ? COLOR_GREEN : COLOR_RED;
    if (isTab && !selectedTab) outline = COLOR_BORDER;
    if (item->itemState & ODS_SELECTED) {
        outline = RGB(std::min(255, GetRValue(outline) + 34), std::min(255, GetGValue(outline) + 34), std::min(255, GetBValue(outline) + 34));
    }

    FillRect(item->hDC, &item->rcItem, g_bgBrush);
    HBRUSH brush = CreateSolidBrush(isTab && selectedTab ? COLOR_PANEL : COLOR_BG);
    HPEN pen = CreatePen(PS_SOLID, selectedTab ? 2 : 1, outline);
    HGDIOBJ oldBrush = SelectObject(item->hDC, brush);
    HGDIOBJ oldPen = SelectObject(item->hDC, pen);
    RECT inset = item->rcItem;
    InflateRect(&inset, -2, -2);
    RoundRect(item->hDC, inset.left, inset.top, inset.right, inset.bottom, 12, 12);
    SelectObject(item->hDC, oldBrush);
    SelectObject(item->hDC, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    SetBkMode(item->hDC, TRANSPARENT);
    SelectObject(item->hDC, g_buttonFont);

    if (item->CtlID == ID_PRESET) {
        RECT prefixRect = item->rcItem;
        prefixRect.left += 18;
        prefixRect.right -= 18;
        SetTextColor(item->hDC, COLOR_ACCENT);
        DrawTextW(item->hDC, L"Preset:", -1, &prefixRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SIZE prefixSize{};
        GetTextExtentPoint32W(item->hDC, L"Preset: ", 8, &prefixSize);
        RECT nameRect = prefixRect;
        nameRect.left += prefixSize.cx;
        SetTextColor(item->hDC, COLOR_GREEN);
        std::wstring name = CurrentPresetName();
        DrawTextW(item->hDC, name.c_str(), -1, &nameRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        return;
    }

    if (item->CtlID == ID_HISTORY_ENABLED) {
        RECT prefixRect = item->rcItem;
        prefixRect.left += 18;
        prefixRect.right -= 18;
        SetTextColor(item->hDC, COLOR_ACCENT);
        DrawTextW(item->hDC, L"History Saving: ", -1, &prefixRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SIZE prefixSize{};
        GetTextExtentPoint32W(item->hDC, L"History Saving: ", 16, &prefixSize);
        RECT stateRect = prefixRect;
        stateRect.left += prefixSize.cx;
        SetTextColor(item->hDC, g_saveHistory ? COLOR_GREEN : COLOR_RED);
        DrawTextW(item->hDC, g_saveHistory ? L"On" : L"Off", -1, &stateRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }

    if (item->CtlID == ID_EDITOR_UP || item->CtlID == ID_EDITOR_DOWN) {
        POINT points[3]{};
        int centerX = (item->rcItem.left + item->rcItem.right) / 2;
        int centerY = (item->rcItem.top + item->rcItem.bottom) / 2;
        if (item->CtlID == ID_EDITOR_UP) {
            points[0] = POINT{centerX, centerY - 6};
            points[1] = POINT{centerX - 7, centerY + 5};
            points[2] = POINT{centerX + 7, centerY + 5};
        } else {
            points[0] = POINT{centerX - 7, centerY - 5};
            points[1] = POINT{centerX + 7, centerY - 5};
            points[2] = POINT{centerX, centerY + 6};
        }

        HBRUSH arrowBrush = CreateSolidBrush(COLOR_YELLOW);
        HPEN arrowPen = CreatePen(PS_SOLID, 1, COLOR_YELLOW);
        HGDIOBJ previousBrush = SelectObject(item->hDC, arrowBrush);
        HGDIOBJ previousPen = SelectObject(item->hDC, arrowPen);
        Polygon(item->hDC, points, 3);
        SelectObject(item->hDC, previousBrush);
        SelectObject(item->hDC, previousPen);
        DeleteObject(arrowBrush);
        DeleteObject(arrowPen);
        return;
    }

    wchar_t text[128]{};
    GetWindowTextW(item->hwndItem, text, 128);
    SetTextColor(item->hDC, isTab && !selectedTab ? COLOR_MUTED : (item->CtlID == ID_HISTORY_ENABLED && !g_saveHistory ? COLOR_MUTED : outline));
    RECT rect = item->rcItem;
    DrawTextW(item->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static std::wstring HistoryPreview(std::wstring text) {
    std::replace(text.begin(), text.end(), L'\r', L' ');
    std::replace(text.begin(), text.end(), L'\n', L' ');
    text = Trim(text);
    if (text.size() > 50) text = text.substr(0, 47) + L"...";
    return text;
}

static std::wstring CurrentPresetName() {
    if (g_selectedPreset >= 0 && g_selectedPreset < (int)g_presets.size()) return g_presets[g_selectedPreset].name;
    return L"Unknown";
}

static std::wstring PresetButtonText() {
    return L"Preset: " + CurrentPresetName();
}

static void HidePresetList() {
    if (IsWindow(g_presetPopup)) {
        DestroyWindow(g_presetPopup);
    }
    g_presetPopup = nullptr;
    g_presetPopupOffset = 0;
}

static void RenderPresetPopupOptions() {
    if (!IsWindow(g_presetPopup)) return;

    HWND child = GetWindow(g_presetPopup, GW_CHILD);
    while (child) {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }

    const int width = 374;
    const int height = 30;
    const int gap = 4;
    const int visibleCount = std::min((int)g_presets.size() - g_presetPopupOffset, 6);
    for (int i = 0; i < visibleCount; ++i) {
        int presetIndex = g_presetPopupOffset + i;
        COLORREF color = presetIndex == g_selectedPreset ? COLOR_GREEN : COLOR_ACCENT;
        HWND option = MakeUntrackedButton(g_presetPopup, g_presets[presetIndex].name.c_str(), ID_PRESET_OPTION_BASE + presetIndex, 0, i * (height + gap), width, height, color);
        SetWindowSubclass(option, PresetOptionSubclassProc, 3, 0);
    }
}

static LRESULT CALLBACK PresetPopupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= ID_PRESET_OPTION_BASE && id < ID_PRESET_OPTION_BASE + (int)g_presets.size()) {
            g_selectedPreset = id - ID_PRESET_OPTION_BASE;
            RefreshPresets();
            SaveSettings();
            HidePresetList();
        }
        return 0;
    }
    case WM_DRAWITEM:
        DrawButton((DRAWITEMSTRUCT*)lp);
        return TRUE;
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int maxOffset = std::max(0, (int)g_presets.size() - 6);
        if (delta < 0 && g_presetPopupOffset < maxOffset) {
            ++g_presetPopupOffset;
            RenderPresetPopupOptions();
        } else if (delta > 0 && g_presetPopupOffset > 0) {
            --g_presetPopupOffset;
            RenderPresetPopupOptions();
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_BG);
        return (LRESULT)g_bgBrush;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) HidePresetList();
        return 0;
    case WM_DESTROY:
        if (hwnd == g_presetPopup) g_presetPopup = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void TogglePresetList() {
    if (IsWindow(g_presetPopup)) {
        HidePresetList();
        return;
    }

    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PresetPopupProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = L"Pr0mtPresetPopup";
        wc.hbrBackground = g_bgBrush;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    RECT buttonRect{};
    GetWindowRect(g_presetButton, &buttonRect);
    const int width = 374;
    const int height = 30;
    const int gap = 4;
    const int visibleCount = std::min((int)g_presets.size(), 6);
    const int popupHeight = std::max(1, visibleCount) * (height + gap) - gap;
    g_presetPopupOffset = 0;

    g_presetPopup = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"Pr0mtPresetPopup",
        nullptr,
        WS_POPUP,
        buttonRect.left,
        buttonRect.bottom + 4,
        width,
        popupHeight,
        g_main,
        nullptr,
        g_instance,
        nullptr);

    RenderPresetPopupOptions();

    ShowWindow(g_presetPopup, SW_SHOW);
    SetWindowPos(g_presetPopup, HWND_TOPMOST, buttonRect.left, buttonRect.bottom + 4, width, popupHeight, SWP_SHOWWINDOW);
}

static void RefreshPresets() {
    if (g_selectedPreset < 0 || g_selectedPreset >= (int)g_presets.size()) g_selectedPreset = 0;
    SetWindowTextW(g_presetButton, PresetButtonText().c_str());
    InvalidateRect(g_presetButton, nullptr, TRUE);
}

static void RefreshHistory() {
    ListView_DeleteAllItems(g_historyList);
    for (int visual = 0; visual < (int)g_history.size(); ++visual) {
        int historyIndex = (int)g_history.size() - visual - 1;
        std::wstring preview = HistoryPreview(g_history[historyIndex].text);

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = visual;
        item.iSubItem = 0;
        item.pszText = preview.data();
        item.lParam = historyIndex;
        ListView_InsertItem(g_historyList, &item);
        ListView_SetItemText(g_historyList, visual, 1, g_history[historyIndex].presetName.data());
        ListView_SetItemText(g_historyList, visual, 2, const_cast<wchar_t*>(L"X"));
    }
}

static void SetActiveTab(int index) {
    g_activeTab = index;
    HidePresetList();
    for (HWND control : g_promptControls) ShowWindow(control, index == 0 ? SW_SHOW : SW_HIDE);
    for (HWND control : g_settingsControls) ShowWindow(control, index == 1 ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_promptTab, nullptr, TRUE);
    InvalidateRect(g_settingsTab, nullptr, TRUE);
    InvalidateRect(g_main, nullptr, TRUE);
}

static LRESULT CALLBACK ClipboardEmptyProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool* done = reinterpret_cast<bool*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        done = reinterpret_cast<bool*>(((CREATESTRUCTW*)lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)done);

        HWND label = CreateWindowExW(0, L"STATIC", L"Clipboard Empty", WS_CHILD | WS_VISIBLE, 20, 18, 280, 26, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(label);
        HWND detail = CreateWindowExW(0, L"STATIC", L"Clipboard does not contain any text.", WS_CHILD | WS_VISIBLE, 20, 46, 280, 24, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(detail);
        MakeUntrackedButton(hwnd, L"OK", ID_CONFIRM_CANCEL, 112, 82, 86, 34, COLOR_ACCENT);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_CONFIRM_CANCEL) {
            if (done) *done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_BG);
        return (LRESULT)g_bgBrush;
    }
    case WM_DRAWITEM:
        DrawButton((DRAWITEMSTRUCT*)lp);
        return TRUE;
    case WM_CLOSE:
        if (done) *done = true;
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowClipboardEmptyDialog() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ClipboardEmptyProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = L"Pr0mtXTextClipboardEmpty";
        wc.hbrBackground = g_bgBrush;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    bool done = false;
    EnableWindow(g_main, FALSE);

    RECT mainRect{};
    GetWindowRect(g_main, &mainRect);
    RECT dialogRect{0, 0, 310, 145};
    AdjustWindowRectEx(&dialogRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;
    int x = mainRect.left + ((mainRect.right - mainRect.left) - dialogWidth) / 2;
    int y = mainRect.top + ((mainRect.bottom - mainRect.top) - dialogHeight) / 2;

    HMONITOR monitor = MonitorFromWindow(g_main, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (GetMonitorInfoW(monitor, &monitorInfo)) {
        x = std::clamp(x, (int)monitorInfo.rcWork.left, (int)monitorInfo.rcWork.right - dialogWidth);
        y = std::clamp(y, (int)monitorInfo.rcWork.top, (int)monitorInfo.rcWork.bottom - dialogHeight);
    }

    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"Pr0mtXTextClipboardEmpty", L"Clipboard Empty", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, dialogWidth, dialogHeight, g_main, nullptr, g_instance, &done);
    ApplyDarkTitleBar(dialog);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);

    MSG msg{};
    while (!done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(g_main, TRUE);
    SetForegroundWindow(g_main);
}
static bool PasteClipboardText() {
    if (!OpenClipboard(g_main)) return false;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) {
        CloseClipboard();
        ShowClipboardEmptyDialog();
        return false;
    }

    const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(handle));
    bool pasted = false;
    if (text) {
        SetText(g_input, text);
        pasted = true;
        GlobalUnlock(handle);
    }
    CloseClipboard();
    return pasted;
}

static void RunOutput() {
    HidePresetList();
    if (g_selectedPreset < 0 || g_selectedPreset >= (int)g_presets.size()) g_selectedPreset = 0;
    SaveSettings();

    std::wstring input = GetText(g_input);
    if (Trim(input).empty()) {
        MessageBoxW(g_main, L"Please enter some text first.", L"Empty Input", MB_ICONWARNING);
        return;
    }

    std::wstring fullPrompt = g_presets[g_selectedPreset].prompt + L"\r\n\r\n" + input;
    std::wstring url = L"https://chat.openai.com/?prompt=" + Utf8ToWide(UrlEncode(fullPrompt)) + L"&temporary-chat=true";
    ShellExecuteW(g_main, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    if (g_saveHistory) {
        g_history.push_back({CurrentPresetName(), input});
        SaveHistory();
        RefreshHistory();
    }
    SetText(g_input, L"");
}

static void PasteAndGo() {
    if (PasteClipboardText()) RunOutput();
}

static void LoadHistoryEntry(int historyIndex) {
    if (historyIndex < 0 || historyIndex >= (int)g_history.size()) return;
    SetText(g_input, g_history[historyIndex].text);
    SetActiveTab(0);
    SetFocus(g_input);
}

static void DeleteHistoryEntry(int historyIndex) {
    if (historyIndex < 0 || historyIndex >= (int)g_history.size()) return;
    g_history.erase(g_history.begin() + historyIndex);
    SaveHistory();
    RefreshHistory();
}

struct ConfirmState {
    bool done = false;
    bool confirmed = false;
};

static LRESULT CALLBACK ConfirmProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ConfirmState* state = reinterpret_cast<ConfirmState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        state = reinterpret_cast<ConfirmState*>(((CREATESTRUCTW*)lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        HWND label = CreateWindowExW(0, L"STATIC", L"Delete all saved entered text history?", WS_CHILD | WS_VISIBLE, 22, 24, 330, 28, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(label);
        HWND detail = CreateWindowExW(0, L"STATIC", L"This cannot be undone.", WS_CHILD | WS_VISIBLE, 22, 54, 330, 24, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(detail);
        MakeUntrackedButton(hwnd, L"Delete All", ID_CONFIRM_DELETE, 96, 98, 112, 36, COLOR_RED);
        MakeUntrackedButton(hwnd, L"Cancel", ID_CONFIRM_CANCEL, 226, 98, 96, 36, COLOR_ACCENT);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_CONFIRM_DELETE || LOWORD(wp) == ID_CONFIRM_CANCEL) {
            state->confirmed = LOWORD(wp) == ID_CONFIRM_DELETE;
            state->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_BG);
        return (LRESULT)g_bgBrush;
    }
    case WM_DRAWITEM:
        DrawButton((DRAWITEMSTRUCT*)lp);
        return TRUE;
    case WM_CLOSE:
        state->confirmed = false;
        state->done = true;
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool ConfirmDeleteAllHistory() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ConfirmProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = L"Pr0mtXTextConfirm";
        wc.hbrBackground = g_bgBrush;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    ConfirmState state;
    EnableWindow(g_main, FALSE);

    RECT mainRect{};
    GetWindowRect(g_main, &mainRect);
    RECT dialogRect{0, 0, 390, 190};
    AdjustWindowRectEx(&dialogRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;
    int x = mainRect.left + ((mainRect.right - mainRect.left) - dialogWidth) / 2;
    int y = mainRect.top + ((mainRect.bottom - mainRect.top) - dialogHeight) / 2;

    HMONITOR monitor = MonitorFromWindow(g_main, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (GetMonitorInfoW(monitor, &monitorInfo)) {
        x = std::clamp(x, (int)monitorInfo.rcWork.left, (int)monitorInfo.rcWork.right - dialogWidth);
        y = std::clamp(y, (int)monitorInfo.rcWork.top, (int)monitorInfo.rcWork.bottom - dialogHeight);
    }

    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"Pr0mtXTextConfirm", L"Delete History", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, dialogWidth, dialogHeight, g_main, nullptr, g_instance, &state);
    ApplyDarkTitleBar(dialog);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);

    MSG msg{};
    while (!state.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(g_main, TRUE);
    SetForegroundWindow(g_main);
    return state.confirmed;
}

static void DeleteAllHistory() {
    if (!ConfirmDeleteAllHistory()) return;
    g_history.clear();
    SaveHistory();
    RefreshHistory();
}

struct EditorState {
    HWND list = nullptr;
    HWND name = nullptr;
    HWND prompt = nullptr;
    int selected = 0;
    std::vector<PromptPreset> presets;
    std::vector<RECT> editFrames;
    RECT presetListFrame{};
};

static void EditorRefreshList(EditorState* state) {
    SendMessageW(state->list, LB_RESETCONTENT, 0, 0);
    for (const auto& preset : state->presets) {
        SendMessageW(state->list, LB_ADDSTRING, 0, (LPARAM)preset.name.c_str());
    }
    if (state->selected < 0 || state->selected >= (int)state->presets.size()) state->selected = 0;
    SendMessageW(state->list, LB_SETCURSEL, state->selected, 0);
}

static void EditorSaveCurrent(EditorState* state) {
    if (state->selected < 0 || state->selected >= (int)state->presets.size()) return;

    std::wstring name = Trim(GetText(state->name));
    std::wstring prompt = Trim(GetText(state->prompt));
    if (name.empty()) name = L"Untitled Prompt";
    name = LimitPresetName(name);
    state->presets[state->selected] = {name, prompt};
}

static void EditorLoadCurrent(EditorState* state) {
    if (state->selected < 0 || state->selected >= (int)state->presets.size()) return;
    SetText(state->name, state->presets[state->selected].name);
    SetText(state->prompt, state->presets[state->selected].prompt);
    SendMessageW(state->list, LB_SETCURSEL, state->selected, 0);
}

static void EditorMoveSelected(EditorState* state, int direction) {
    if (state->selected < 0 || state->selected >= (int)state->presets.size()) return;

    int target = state->selected + direction;
    if (target < 0 || target >= (int)state->presets.size()) return;

    EditorSaveCurrent(state);
    std::swap(state->presets[state->selected], state->presets[target]);
    state->selected = target;
    EditorRefreshList(state);
    EditorLoadCurrent(state);
}

static LRESULT CALLBACK EditorProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    EditorState* state = reinterpret_cast<EditorState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        state = new EditorState();
        state->presets = g_presets;
        state->selected = g_selectedPreset;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        HWND label = CreateWindowExW(0, L"STATIC", L"Presets", WS_CHILD | WS_VISIBLE, 16, 16, 140, 24, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(label);
        state->presetListFrame = MakeRect(16, 44, 190, 198);
        state->editFrames.push_back(state->presetListFrame);
        state->list = CreateWindowExW(0, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | LBS_NOTIFY, 24, 52, 166, 182, hwnd, (HMENU)(INT_PTR)ID_EDITOR_LIST, g_instance, nullptr);
        SetAppFont(state->list);
        SendMessageW(state->list, LB_SETITEMHEIGHT, 0, EDITOR_PRESET_ROW_HEIGHT);
        SetWindowSubclass(state->list, EditorListSubclassProc, 4, 0);

        label = CreateWindowExW(0, L"STATIC", L"Name", WS_CHILD | WS_VISIBLE, 224, 16, 100, 24, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(label);
        state->name = MakeFramedEdit(hwnd, ID_EDITOR_NAME, 224, 44, 392, 30, ES_AUTOHSCROLL, state->editFrames);
        SendMessageW(state->name, EM_SETLIMITTEXT, MAX_PRESET_NAME_LENGTH, 0);

        label = CreateWindowExW(0, L"STATIC", L"Prompt", WS_CHILD | WS_VISIBLE, 224, 92, 100, 24, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(label);
        state->prompt = MakeFramedEdit(hwnd, ID_EDITOR_PROMPT, 224, 120, 392, 174, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, state->editFrames);

        label = CreateWindowExW(0, L"STATIC", L"Move Preset", WS_CHILD | WS_VISIBLE, 16, 258, 94, 24, hwnd, nullptr, g_instance, nullptr);
        SetAppFont(label);
        MakeUntrackedButton(hwnd, L"", ID_EDITOR_UP, 116, 252, 34, 34, COLOR_YELLOW);
        MakeUntrackedButton(hwnd, L"", ID_EDITOR_DOWN, 160, 252, 34, 34, COLOR_YELLOW);
        MakeUntrackedButton(hwnd, L"New", ID_EDITOR_NEW, 16, 312, 86, 34, COLOR_ACCENT);
        MakeUntrackedButton(hwnd, L"Delete", ID_EDITOR_DELETE, 120, 312, 86, 34, COLOR_RED);
        MakeUntrackedButton(hwnd, L"Save", ID_EDITOR_SAVE, 426, 312, 86, 34, COLOR_GREEN);
        MakeUntrackedButton(hwnd, L"Cancel", ID_EDITOR_CANCEL, 530, 312, 86, 34, RGB(255, 255, 255));

        EditorRefreshList(state);
        EditorLoadCurrent(state);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_EDITOR_LIST && HIWORD(wp) == LBN_SELCHANGE) {
            EditorSaveCurrent(state);
            int selected = (int)SendMessageW(state->list, LB_GETCURSEL, 0, 0);
            if (selected != LB_ERR) {
                state->selected = selected;
                EditorLoadCurrent(state);
            }
        } else if (id == ID_EDITOR_NEW) {
            EditorSaveCurrent(state);
            state->presets.push_back({L"New Prompt", L""});
            state->selected = (int)state->presets.size() - 1;
            EditorRefreshList(state);
            EditorLoadCurrent(state);
            SetFocus(state->name);
        } else if (id == ID_EDITOR_DELETE) {
            if (state->presets.size() <= 1) {
                MessageBoxW(hwnd, L"At least one prompt preset is required.", L"Cannot Delete", MB_ICONINFORMATION);
                return 0;
            }

            state->presets.erase(state->presets.begin() + state->selected);
            if (state->selected >= (int)state->presets.size()) state->selected = (int)state->presets.size() - 1;
            EditorRefreshList(state);
            EditorLoadCurrent(state);
        } else if (id == ID_EDITOR_UP) {
            EditorMoveSelected(state, -1);
        } else if (id == ID_EDITOR_DOWN) {
            EditorMoveSelected(state, 1);
        } else if (id == ID_EDITOR_SAVE) {
            EditorSaveCurrent(state);
            g_presets = state->presets;
            g_selectedPreset = state->selected;
            SavePresets();
            SaveSettings();
            HidePresetList();
            RefreshPresets();
            DestroyWindow(hwnd);
        } else if (id == ID_EDITOR_CANCEL) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_BG);
        return (LRESULT)g_bgBrush;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_PANEL);
        return (LRESULT)g_panelBrush;
    }
    case WM_DRAWITEM:
        DrawButton((DRAWITEMSTRUCT*)lp);
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            for (const RECT& rect : state->editFrames) DrawEditFrame(dc, rect);
            DrawModernScrollIndicator(dc, state->list, state->presetListFrame);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (hwnd == g_promptEditor) g_promptEditor = nullptr;
        delete state;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void OpenPromptEditor() {
    if (IsWindow(g_promptEditor)) {
        ShowWindow(g_promptEditor, SW_RESTORE);
        SetForegroundWindow(g_promptEditor);
        return;
    }

    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = EditorProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = L"CustomPromptEditor";
        wc.hbrBackground = g_bgBrush;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    RECT mainRect{};
    GetWindowRect(g_main, &mainRect);
    RECT editorRect{0, 0, 650, 400};
    AdjustWindowRectEx(&editorRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, FALSE, WS_EX_DLGMODALFRAME);
    int editorWidth = editorRect.right - editorRect.left;
    int editorHeight = editorRect.bottom - editorRect.top;
    int x = mainRect.left + ((mainRect.right - mainRect.left) - editorWidth) / 2;
    int y = mainRect.top + ((mainRect.bottom - mainRect.top) - editorHeight) / 2;

    HMONITOR monitor = MonitorFromWindow(g_main, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (GetMonitorInfoW(monitor, &monitorInfo)) {
        x = std::clamp(x, (int)monitorInfo.rcWork.left, (int)monitorInfo.rcWork.right - editorWidth);
        y = std::clamp(y, (int)monitorInfo.rcWork.top, (int)monitorInfo.rcWork.bottom - editorHeight);
    }

    g_promptEditor = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"CustomPromptEditor",
        L"Pr0mt_X Text Presets",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x,
        y,
        editorWidth,
        editorHeight,
        g_main,
        nullptr,
        g_instance,
        nullptr);

    ShowWindow(g_promptEditor, SW_SHOW);
    ApplyDarkTitleBar(g_promptEditor);
    SetForegroundWindow(g_promptEditor);
}

static void ApplyDarkTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    COLORREF caption = COLOR_BG;
    COLORREF text = COLOR_TEXT;
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
}

static HICON LoadAppIcon() {
    HICON icon = LoadIconW(g_instance, MAKEINTRESOURCEW(1));
    return icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
}

static void CreateMainUi(HWND hwnd) {
    g_mainEditFrames.clear();
    g_settingsFrames.clear();
    g_promptTab = MakeUntrackedButton(hwnd, L"Prompt", ID_TAB_PROMPT, 116, 14, 138, 34, COLOR_ACCENT);
    g_settingsTab = MakeUntrackedButton(hwnd, L"Settings", ID_TAB_SETTINGS, 274, 14, 138, 34, COLOR_ACCENT);

    MakeLabel(hwnd, L"Prompt preset", 27, 70, 474, 24);
    g_presetButton = MakeButton(hwnd, L"Preset", ID_PRESET, 77, 98, 374, 38, COLOR_ACCENT);
    SetWindowSubclass(g_presetButton, PresetButtonSubclassProc, 2, 0);

    MakeLabel(hwnd, L"Enter text", 27, 146, 474, 24);
    g_input = MakeFramedEdit(hwnd, ID_INPUT, 27, 174, 474, 174, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, g_mainEditFrames);
    g_promptControls.push_back(g_input);

    MakeButton(hwnd, L"Paste", ID_PASTE, 50, 364, 88, 38, COLOR_BLUE);
    MakeButton(hwnd, L"Send", ID_OUTPUT, 150, 364, 104, 38, COLOR_GREEN);
    MakeButton(hwnd, L"Paste&Go", ID_PASTE_GO, 266, 364, 112, 38, COLOR_ACCENT);
    MakeButton(hwnd, L"Clear", ID_CLEAR, 390, 364, 88, 38, COLOR_RED);

    MakeLabel(hwnd, L"Edit Custom Prompt Presets", 27, 70, 474, 24, true);
    MakeButton(hwnd, L"Edit Prompt Presets", ID_EDIT_PROMPTS, 160, 100, 208, 36, COLOR_ACCENT, true);

    MakeLabel(hwnd, L"Entered Text History", 27, 154, 474, 24, true);
    g_historyEnabled = MakeButton(hwnd, L"History Saving", ID_HISTORY_ENABLED, 64, 180, 190, 34, COLOR_ACCENT, true);

    MakeButton(hwnd, L"Delete All History", ID_DELETE_ALL_HISTORY, 290, 180, 174, 34, COLOR_RED, true);

    g_settingsFrames.push_back(MakeRect(27, 226, 474, 176));
    g_historyList = CreateWindowExW(0, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER | LVS_NOCOLUMNHEADER, 35, 234, 458, 160, hwnd, (HMENU)(INT_PTR)ID_HISTORY_LIST, g_instance, nullptr);
    SetAppFont(g_historyList);
    SetWindowTheme(g_historyList, L"", L"");
    ListView_SetExtendedListViewStyle(g_historyList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(g_historyList, COLOR_PANEL);
    ListView_SetTextBkColor(g_historyList, COLOR_PANEL);
    ListView_SetTextColor(g_historyList, COLOR_TEXT);
    g_historyRowImages = ImageList_Create(1, 24, ILC_COLOR32, 1, 1);
    HDC screenDc = GetDC(hwnd);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP rowBitmap = CreateCompatibleBitmap(screenDc, 1, 24);
    HGDIOBJ oldBitmap = SelectObject(memoryDc, rowBitmap);
    HBRUSH rowBrush = CreateSolidBrush(COLOR_PANEL);
    RECT rowRect{0, 0, 1, 24};
    FillRect(memoryDc, &rowRect, rowBrush);
    DeleteObject(rowBrush);
    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(hwnd, screenDc);
    ImageList_Add(g_historyRowImages, rowBitmap, nullptr);
    DeleteObject(rowBitmap);
    ListView_SetImageList(g_historyList, g_historyRowImages, LVSIL_SMALL);
    g_settingsControls.push_back(g_historyList);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 276;
    col.pszText = const_cast<wchar_t*>(L"Text");
    ListView_InsertColumn(g_historyList, 0, &col);
    col.fmt = LVCFMT_LEFT;
    col.cx = 128;
    col.pszText = const_cast<wchar_t*>(L"Preset");
    ListView_InsertColumn(g_historyList, 1, &col);
    col.fmt = LVCFMT_CENTER;
    col.cx = 26;
    col.pszText = const_cast<wchar_t*>(L"");
    ListView_InsertColumn(g_historyList, 2, &col);

    RefreshPresets();
    RefreshHistory();
    SetActiveTab(0);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_main = hwnd;
        LoadPresets();
        LoadSettings();
        LoadHistory();
        CreateMainUi(hwnd);
        ApplyDarkTitleBar(hwnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_PRESET) {
            return 0;
        } else if (id >= ID_PRESET_OPTION_BASE && id < ID_PRESET_OPTION_BASE + (int)g_presets.size()) {
            g_selectedPreset = id - ID_PRESET_OPTION_BASE;
            RefreshPresets();
            HidePresetList();
            SaveSettings();
        } else if (id == ID_TAB_PROMPT) {
            SetActiveTab(0);
        } else if (id == ID_TAB_SETTINGS) {
            SetActiveTab(1);
        } else if (id == ID_EDIT_PROMPTS) {
            OpenPromptEditor();
        } else if (id == ID_PASTE) {
            PasteClipboardText();
        } else if (id == ID_PASTE_GO) {
            PasteAndGo();
        } else if (id == ID_OUTPUT) {
            RunOutput();
        } else if (id == ID_CLEAR) {
            SetText(g_input, L"");
        } else if (id == ID_DELETE_ALL_HISTORY) {
            DeleteAllHistory();
        } else if (id == ID_HISTORY_ENABLED) {
            g_saveHistory = !g_saveHistory;
            InvalidateRect(g_historyEnabled, nullptr, TRUE);
            SaveSettings();
        }
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR* header = (NMHDR*)lp;
        if (header->hwndFrom == g_historyList && header->code == NM_CLICK) {
            DWORD pos = GetMessagePos();
            LVHITTESTINFO hit{};
            hit.pt.x = GET_X_LPARAM(pos);
            hit.pt.y = GET_Y_LPARAM(pos);
            ScreenToClient(g_historyList, &hit.pt);
            ListView_SubItemHitTest(g_historyList, &hit);
            if (hit.iItem >= 0) {
                LVITEMW listItem{};
                listItem.mask = LVIF_PARAM;
                listItem.iItem = hit.iItem;
                ListView_GetItem(g_historyList, &listItem);
                int historyIndex = (int)listItem.lParam;
                if (hit.iSubItem == 2) DeleteHistoryEntry(historyIndex);
                else LoadHistoryEntry(historyIndex);
            }
            return 0;
        }
        if (header->hwndFrom == g_historyList && header->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW* draw = (NMLVCUSTOMDRAW*)lp;
            if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) return CDRF_NOTIFYSUBITEMDRAW;
            if (draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                draw->clrTextBk = COLOR_PANEL;
                draw->clrText = draw->iSubItem == 2 ? COLOR_RED : COLOR_TEXT;
                if (draw->iSubItem == 2) {
                    SelectObject(draw->nmcd.hdc, g_buttonFont);
                    return CDRF_NEWFONT;
                }
                return CDRF_DODEFAULT;
            }
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_BG);
        return (LRESULT)g_bgBrush;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COLOR_TEXT);
        SetBkColor(dc, COLOR_PANEL);
        return (LRESULT)g_panelBrush;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* item = (DRAWITEMSTRUCT*)lp;
        DrawButton(item);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        const std::vector<RECT>& frames = g_activeTab == 0 ? g_mainEditFrames : g_settingsFrames;
        for (const RECT& rect : frames) DrawEditFrame(dc, rect);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        SaveSettings();
        if (g_historyRowImages) {
            ImageList_Destroy(g_historyRowImages);
            g_historyRowImages = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    g_instance = instance;

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);

    g_bgBrush = CreateSolidBrush(COLOR_BG);
    g_panelBrush = CreateSolidBrush(COLOR_PANEL);
    g_font = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_buttonFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HICON appIcon = LoadAppIcon();

    WNDCLASSW wc{};
    wc.lpfnWndProc = MainProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"Pr0mtXTextWindow";
    wc.hbrBackground = g_bgBrush;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = appIcon;
    RegisterClassW(&wc);

    RECT rect{0, 0, 528, 444};
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, FALSE);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        L"Pr0mtXTextWindow",
        L"Pr0mt_X Text",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        nullptr);

    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)appIcon);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)appIcon);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_font);
    DeleteObject(g_buttonFont);
    DeleteObject(g_bgBrush);
    DeleteObject(g_panelBrush);
    return 0;
}
