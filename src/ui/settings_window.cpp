#include "ui/settings_window.hpp"
#include "config/config.hpp"
#include "core/hook.hpp"
#include "core/layout_engine.hpp"
#include "platform/tray.hpp"
#include "platform/win32_utils.hpp"
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

namespace UI {

constexpr int CLIENT_WIDTH = 515;
constexpr int CLIENT_HEIGHT = 668;

constexpr COLORREF COL_BG = RGB(32, 32, 32);
constexpr COLORREF COL_CARD = RGB(43, 43, 43);
constexpr COLORREF COL_BORDER = RGB(58, 58, 58);
constexpr COLORREF COL_TEXT_PRI = RGB(255, 255, 255);
constexpr COLORREF COL_TEXT_SEC = RGB(160, 160, 160);
constexpr COLORREF COL_ACCENT = RGB(0, 120, 215);
constexpr COLORREF COL_RU = RGB(216, 59, 1);
constexpr COLORREF COL_TOGGLE_OFF = RGB(68, 68, 72);
constexpr COLORREF COL_TAB_ACTIVE = RGB(58, 58, 64);
constexpr COLORREF COL_TAB_INACTIVE = RGB(36, 36, 38);
constexpr COLORREF COL_SUCCESS = RGB(16, 185, 129);
constexpr COLORREF COL_AMBER = RGB(245, 158, 11);

constexpr UINT ID_LIST_EXCL = 3001;
constexpr UINT ID_EDIT_EXCL = 3002;
constexpr UINT ID_BTN_ADD = 3003;
constexpr UINT ID_BTN_REMOVE = 3004;
constexpr UINT ID_BTN_SAVE = 3005;
constexpr UINT ID_BTN_CLOSE = 3006;

constexpr UINT ID_LIST_APP_LAYOUTS = 3010;
constexpr UINT ID_EDIT_APP_PROC = 3011;
constexpr UINT ID_BTN_PICK_APP = 3012;
constexpr UINT ID_BTN_APP_ADD = 3013;
constexpr UINT ID_BTN_APP_TOGGLE_LANG = 3014;
constexpr UINT ID_BTN_APP_REMOVE = 3015;
constexpr UINT ID_BTN_PICK_EXCL = 3016;

constexpr UINT ID_PICKER_PROC_BASE = 4000;
constexpr UINT ID_PICKER_BROWSE = 4999;

SettingsWindow& SettingsWindow::Instance() {
    static SettingsWindow s_instance;
    return s_instance;
}

SettingsWindow::SettingsWindow() {
    hBrBackground_ = CreateSolidBrush(COL_BG);
    hBrCard_ = CreateSolidBrush(COL_CARD);
    hBrBorder_ = CreateSolidBrush(COL_BORDER);
}

SettingsWindow::~SettingsWindow() {
    if (hBrBackground_) DeleteObject(hBrBackground_);
    if (hBrCard_) DeleteObject(hBrCard_);
    if (hBrBorder_) DeleteObject(hBrBorder_);
    if (hFontTitle_) DeleteObject(hFontTitle_);
    if (hFontSub_) DeleteObject(hFontSub_);
    if (hFontRegular_) DeleteObject(hFontRegular_);
    if (hFontBold_) DeleteObject(hFontBold_);
}

bool SettingsWindow::Create() {
    if (hWnd_) return true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"CapsSwitchSettingsWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = hBrBackground_;
    wc.hIcon = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_SHARED);
    wc.hIconSm = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_SHARED);

    RegisterClassExW(&wc);

    // Compute outer window size from desired client dimensions
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD exStyle = WS_EX_DLGMODALFRAME;
    RECT rcWin = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT };
    AdjustWindowRectEx(&rcWin, style, FALSE, exStyle);
    int winW = rcWin.right - rcWin.left;
    int winH = rcWin.bottom - rcWin.top;

    // Center window on primary monitor
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (scrW - winW) / 2;
    int posY = (scrH - winH) / 2;

    hWnd_ = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        L"CapsSwitch - Настройки",
        style,
        posX, posY, winW, winH,
        nullptr, nullptr, wc.hInstance, this
    );

    if (!hWnd_) return false;

    // Enable Windows 11 Dark Mode on Title Bar
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hWnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    // Fonts setup
    hFontTitle_ = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    hFontSub_ = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    hFontRegular_ = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    hFontBold_ = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // Tab 0: Per-app Layout Rules Controls
    hEditAppProc_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        34, 411, 184, 26,
        hWnd_, (HMENU)ID_EDIT_APP_PROC, wc.hInstance, nullptr
    );
    SendMessageW(hEditAppProc_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);
    SendMessageW(hEditAppProc_, EM_SETCUEBANNER, TRUE, (LPARAM)L"telegram.exe");

    hBtnPickApp_ = CreateWindowExW(
        0, L"BUTTON", L"Окна ▾",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        224, 411, 68, 26,
        hWnd_, (HMENU)ID_BTN_PICK_APP, wc.hInstance, nullptr
    );
    SendMessageW(hBtnPickApp_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hBtnAppAdd_ = CreateWindowExW(
        0, L"BUTTON", L"Добавить",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        392, 411, 89, 26,
        hWnd_, (HMENU)ID_BTN_APP_ADD, wc.hInstance, nullptr
    );
    SendMessageW(hBtnAppAdd_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hListAppLayouts_ = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        34, 447, 350, 141,
        hWnd_, (HMENU)ID_LIST_APP_LAYOUTS, wc.hInstance, nullptr
    );
    SendMessageW(hListAppLayouts_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hBtnAppToggleLang_ = CreateWindowExW(
        0, L"BUTTON", L"EN ⇄ RU",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        392, 447, 89, 26,
        hWnd_, (HMENU)ID_BTN_APP_TOGGLE_LANG, wc.hInstance, nullptr
    );
    SendMessageW(hBtnAppToggleLang_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hBtnAppRemove_ = CreateWindowExW(
        0, L"BUTTON", L"Удалить",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        392, 481, 89, 26,
        hWnd_, (HMENU)ID_BTN_APP_REMOVE, wc.hInstance, nullptr
    );
    SendMessageW(hBtnAppRemove_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    // Tab 1: Exclusions Controls
    hEditExcl_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        34, 411, 276, 26,
        hWnd_, (HMENU)ID_EDIT_EXCL, wc.hInstance, nullptr
    );
    SendMessageW(hEditExcl_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);
    SendMessageW(hEditExcl_, EM_SETCUEBANNER, TRUE, (LPARAM)L"cs2.exe");

    hBtnPickExcl_ = CreateWindowExW(
        0, L"BUTTON", L"Окна ▾",
        WS_CHILD | BS_PUSHBUTTON,
        316, 411, 68, 26,
        hWnd_, (HMENU)ID_BTN_PICK_EXCL, wc.hInstance, nullptr
    );
    SendMessageW(hBtnPickExcl_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hBtnAdd_ = CreateWindowExW(
        0, L"BUTTON", L"Добавить",
        WS_CHILD | BS_PUSHBUTTON,
        392, 411, 89, 26,
        hWnd_, (HMENU)ID_BTN_ADD, wc.hInstance, nullptr
    );
    SendMessageW(hBtnAdd_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hListBox_ = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        34, 447, 350, 141,
        hWnd_, (HMENU)ID_LIST_EXCL, wc.hInstance, nullptr
    );
    SendMessageW(hListBox_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hBtnRemove_ = CreateWindowExW(
        0, L"BUTTON", L"Удалить",
        WS_CHILD | BS_PUSHBUTTON,
        392, 447, 89, 26,
        hWnd_, (HMENU)ID_BTN_REMOVE, wc.hInstance, nullptr
    );
    SendMessageW(hBtnRemove_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    // Footer Buttons
    hBtnSave_ = CreateWindowExW(
        0, L"BUTTON", L"Сохранить",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        280, 620, 105, 32,
        hWnd_, (HMENU)ID_BTN_SAVE, wc.hInstance, nullptr
    );
    SendMessageW(hBtnSave_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    hBtnClose_ = CreateWindowExW(
        0, L"BUTTON", L"Закрыть",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        395, 620, 90, 32,
        hWnd_, (HMENU)ID_BTN_CLOSE, wc.hInstance, nullptr
    );
    SendMessageW(hBtnClose_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    UpdateControls();
    UpdateTabVisibility();
    return true;
}

void SettingsWindow::UpdateControls() {
    if (!hListBox_ || !hListAppLayouts_) return;

    const auto snapshot = Config::ConfigManager::Instance().GetSnapshot();

    SendMessageW(hListAppLayouts_, LB_RESETCONTENT, 0, 0);
    displayedAppRules_ = snapshot.appLayouts;
    for (const auto& rule : displayedAppRules_) {
        std::wstring label = (rule.layout == Config::AppLayout::Russian) ? L"[RU]  " : L"[EN]  ";
        label += rule.processName;
        SendMessageW(hListAppLayouts_, LB_ADDSTRING, 0, (LPARAM)label.c_str());
    }

    SendMessageW(hListBox_, LB_RESETCONTENT, 0, 0);
    for (const auto& excl : snapshot.exclusions) {
        SendMessageW(hListBox_, LB_ADDSTRING, 0, (LPARAM)excl.c_str());
    }
}

void SettingsWindow::UpdateTabVisibility() {
    if (!hWnd_) return;
    int showApp = (activeTab_ == ProgramsTab::AppLayouts) ? SW_SHOW : SW_HIDE;
    int showExcl = (activeTab_ == ProgramsTab::Exclusions) ? SW_SHOW : SW_HIDE;

    ShowWindow(hEditAppProc_, showApp);
    ShowWindow(hBtnPickApp_, showApp);
    ShowWindow(hBtnAppAdd_, showApp);
    ShowWindow(hListAppLayouts_, showApp);
    ShowWindow(hBtnAppToggleLang_, showApp);
    ShowWindow(hBtnAppRemove_, showApp);

    ShowWindow(hEditExcl_, showExcl);
    ShowWindow(hBtnPickExcl_, showExcl);
    ShowWindow(hBtnAdd_, showExcl);
    ShowWindow(hListBox_, showExcl);
    ShowWindow(hBtnRemove_, showExcl);

    RedrawWindow(hWnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void SettingsWindow::ShowAppPickerMenu(HWND anchorBtn, HWND targetEdit) {
    if (!anchorBtn || !targetEdit) return;

    std::vector<std::wstring> procs = Platform::GetRunningWindowProcesses();
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    if (procs.empty()) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(Нет активных окон)");
    } else {
        size_t limit = (procs.size() < 50) ? procs.size() : 50;
        for (size_t i = 0; i < limit; ++i) {
            AppendMenuW(hMenu, MF_STRING, ID_PICKER_PROC_BASE + static_cast<UINT>(i), procs[i].c_str());
        }
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_PICKER_BROWSE, L"Выбрать файл .exe...");

    RECT rcBtn = {};
    GetWindowRect(anchorBtn, &rcBtn);
    UINT cmd = TrackPopupMenuEx(
        hMenu,
        TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NONOTIFY,
        rcBtn.left,
        rcBtn.bottom,
        hWnd_,
        nullptr
    );
    DestroyMenu(hMenu);

    if (cmd == ID_PICKER_BROWSE) {
        wchar_t fileBuf[MAX_PATH] = { 0 };
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hWnd_;
        ofn.lpstrFilter = L"Программы (*.exe)\0*.exe\0Все файлы (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&ofn)) {
            std::wstring fullPath(fileBuf);
            size_t lastSlash = fullPath.find_last_of(L"\\/");
            std::wstring fileName = (lastSlash != std::wstring::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
            SetWindowTextW(targetEdit, Platform::ToLower(fileName).c_str());
            SetFocus(targetEdit);
        }
    } else if (cmd >= ID_PICKER_PROC_BASE && cmd < ID_PICKER_PROC_BASE + procs.size()) {
        SetWindowTextW(targetEdit, procs[cmd - ID_PICKER_PROC_BASE].c_str());
        SetFocus(targetEdit);
    }
}

void SettingsWindow::ToggleSelectedAppLayout() {
    if (!hListAppLayouts_) return;
    int sel = (int)SendMessageW(hListAppLayouts_, LB_GETCURSEL, 0, 0);
    if (sel < 0 || static_cast<size_t>(sel) >= displayedAppRules_.size()) return;

    const auto& rule = displayedAppRules_[sel];
    std::wstring proc = rule.processName;
    Config::AppLayout nextLayout = (rule.layout == Config::AppLayout::Russian)
        ? Config::AppLayout::English
        : Config::AppLayout::Russian;

    Config::ConfigManager::Instance().Update([&proc, nextLayout](Config::AppConfig& cfg) {
        cfg.SetAppLayout(proc, nextLayout);
    });
    Config::ConfigManager::Instance().Save();
    UpdateControls();
    SendMessageW(hListAppLayouts_, LB_SETCURSEL, sel, 0);
    InvalidateRect(hWnd_, nullptr, FALSE);
}

void SettingsWindow::Show() {
    if (!hWnd_) Create();

    // Center on the monitor containing the cursor using actual window dimensions
    RECT rcActual = {};
    GetWindowRect(hWnd_, &rcActual);
    int winW = rcActual.right - rcActual.left;
    int winH = rcActual.bottom - rcActual.top;

    POINT cursorPt = {};
    GetCursorPos(&cursorPt);
    HMONITOR hMon = MonitorFromPoint(cursorPt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfoW(hMon, &mi)) {
        RECT wr = mi.rcWork;
        int posX = wr.left + ((wr.right - wr.left) - winW) / 2;
        int posY = wr.top + ((wr.bottom - wr.top) - winH) / 2;
        SetWindowPos(hWnd_, nullptr, posX, posY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    UpdateControls();
    UpdateTabVisibility();
    ShowWindow(hWnd_, SW_SHOW);
    SetForegroundWindow(hWnd_);
}

void SettingsWindow::Hide() {
    if (hWnd_) {
        ShowWindow(hWnd_, SW_HIDE);
    }
}

bool SettingsWindow::IsVisible() const {
    return (hWnd_ && IsWindowVisible(hWnd_));
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SettingsWindow::DrawToggle(HDC hdc, int x, int y, bool state, const wchar_t* title, const wchar_t* subtitle, RECT& hitRect) {
    hitRect = { x, y, x + 460, y + 42 };

    // Draw Title & Subtitle
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_TEXT_PRI);
    SelectObject(hdc, hFontBold_);
    RECT rcTitle = { x, y, x + 380, y + 20 };
    DrawTextW(hdc, title, -1, &rcTitle, DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, COL_TEXT_SEC);
    SelectObject(hdc, hFontSub_);
    RECT rcSub = { x, y + 20, x + 380, y + 38 };
    DrawTextW(hdc, subtitle, -1, &rcSub, DT_LEFT | DT_SINGLELINE);

    // Draw Toggle Pill (Windows 11 Switch shape)
    int pillW = 44;
    int pillH = 22;
    int pillX = x + 410;
    int pillY = y + 8;

    HBRUSH hPillBrush = CreateSolidBrush(state ? COL_ACCENT : COL_TOGGLE_OFF);
    HPEN hPillPen = CreatePen(PS_SOLID, 1, state ? COL_ACCENT : COL_BORDER);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hPillBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPillPen);

    RoundRect(hdc, pillX, pillY, pillX + pillW, pillY + pillH, 22, 22);

    // Draw Toggle Knob (White circle)
    int knobSize = 16;
    int knobX = state ? (pillX + pillW - knobSize - 3) : (pillX + 3);
    int knobY = pillY + 3;

    HBRUSH hKnobBrush = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hdc, hKnobBrush);
    Ellipse(hdc, knobX, knobY, knobX + knobSize, knobY + knobSize);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPillBrush);
    DeleteObject(hPillPen);
    DeleteObject(hKnobBrush);
}

void SettingsWindow::DrawHeader(HDC hdc) {
    // Title
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_TEXT_PRI);
    SelectObject(hdc, hFontTitle_);
    RECT rcTitle = { 24, 18, 300, 48 };
    DrawTextW(hdc, L"CapsSwitch", -1, &rcTitle, DT_LEFT | DT_SINGLELINE);

    // Subtitle
    SetTextColor(hdc, COL_TEXT_SEC);
    SelectObject(hdc, hFontSub_);
    RECT rcSub = { 24, 46, 320, 64 };
    DrawTextW(hdc, L"Сверхлегкий портативный переключатель раскладки", -1, &rcSub, DT_LEFT | DT_SINGLELINE);

    // Status Badge
    bool paused = Core::HookManager::Instance().IsPaused();
    COLORREF statusCol = paused ? COL_AMBER : COL_SUCCESS;
    const wchar_t* statusText = paused ? L"⏸ На паузе" : L"● Активен";

    int badgeW = 95;
    int badgeH = 26;
    int badgeX = 390;
    int badgeY = 24;
    rcStatusBadge_ = { badgeX, badgeY, badgeX + badgeW, badgeY + badgeH };

    HBRUSH hBadgeBrush = CreateSolidBrush(RGB(50, 50, 50));
    HPEN hBadgePen = CreatePen(PS_SOLID, 1, statusCol);
    HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hBadgeBrush);
    HPEN hOldP = (HPEN)SelectObject(hdc, hBadgePen);

    RoundRect(hdc, badgeX, badgeY, badgeX + badgeW, badgeY + badgeH, 14, 14);

    SelectObject(hdc, hFontBold_);
    SetTextColor(hdc, statusCol);
    RECT rcBadge = { badgeX, badgeY, badgeX + badgeW, badgeY + badgeH };
    DrawTextW(hdc, statusText, -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldB);
    SelectObject(hdc, hOldP);
    DeleteObject(hBadgeBrush);
    DeleteObject(hBadgePen);
}

void SettingsWindow::DrawSettingsCard(HDC hdc) {
    // Rounded Card Background
    int cardX = 20;
    int cardY = 75;
    int cardW = 475;
    int cardH = 255;

    HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hBrCard_);
    HPEN hPen = CreatePen(PS_SOLID, 1, COL_BORDER);
    HPEN hOldP = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, cardX, cardY, cardX + cardW, cardY + cardH, 12, 12);

    SelectObject(hdc, hOldB);
    SelectObject(hdc, hOldP);
    DeleteObject(hPen);

    const auto cfg = Config::ConfigManager::Instance().GetSnapshot();

    // 5 Toggle Switches inside card
    int startY = 86;
    int stepY = 47;

    DrawToggle(hdc, 34, startY, cfg.autoSwitch,
        L"Автопереключение раскладки",
        L"Автоматически исправлять набранное слово при ошибке языка",
        rcToggleAuto_);

    DrawToggle(hdc, 34, startY + stepY, cfg.capsLockSwitch,
        L"Переключение по Caps Lock",
        L"Циклическое переключение RU <-> EN и подавление фиксатора регистра",
        rcToggleCaps_);

    DrawToggle(hdc, 34, startY + stepY * 2, cfg.doubleCapsConvert,
        L"Перевод по 2x Caps Lock",
        L"Быстрый перевод выделенного текста или последнего слова",
        rcToggleDoubleCaps_);

    DrawToggle(hdc, 34, startY + stepY * 3, cfg.passwordGuard,
        L"Защита полей паролей",
        L"Авто-EN и отключение буферизации в скрытых полях ввода (UIA & Win32)",
        rcTogglePwd_);

    DrawToggle(hdc, 34, startY + stepY * 4, cfg.autostart,
        L"Автозапуск с Windows",
        L"Запуск при входе в систему с привязкой к текущему пути .exe",
        rcToggleAutoStart_);
}

void SettingsWindow::DrawProgramsCard(HDC hdc) {
    int cardX = 20;
    int cardY = 344;
    int cardW = 475;
    int cardH = 260;

    HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hBrCard_);
    HPEN hPen = CreatePen(PS_SOLID, 1, COL_BORDER);
    HPEN hOldP = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, cardX, cardY, cardX + cardW, cardY + cardH, 12, 12);

    SelectObject(hdc, hOldB);
    SelectObject(hdc, hOldP);
    DeleteObject(hPen);

    const auto cfg = Config::ConfigManager::Instance().GetSnapshot();

    // Segmented tab buttons
    rcTabAppLayouts_ = { 34, 354, 260, 382 };
    rcTabExclusions_ = { 268, 354, 481, 382 };

    auto drawTab = [&](const RECT& rc, const std::wstring& label, bool active) {
        HBRUSH hTabBrush = CreateSolidBrush(active ? COL_TAB_ACTIVE : COL_TAB_INACTIVE);
        HPEN hTabPen = CreatePen(PS_SOLID, 1, active ? COL_ACCENT : COL_BORDER);
        HBRUSH hOB = (HBRUSH)SelectObject(hdc, hTabBrush);
        HPEN hOP = (HPEN)SelectObject(hdc, hTabPen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, hOB);
        SelectObject(hdc, hOP);
        DeleteObject(hTabBrush);
        DeleteObject(hTabPen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, active ? COL_TEXT_PRI : COL_TEXT_SEC);
        SelectObject(hdc, active ? hFontBold_ : hFontRegular_);
        RECT rcText = rc;
        DrawTextW(hdc, label.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    std::wstring tab0Title = L"Раскладка программ (" + std::to_wstring(cfg.appLayouts.size()) + L")";
    std::wstring tab1Title = L"Исключения (" + std::to_wstring(cfg.exclusions.size()) + L")";
    drawTab(rcTabAppLayouts_, tab0Title, activeTab_ == ProgramsTab::AppLayouts);
    drawTab(rcTabExclusions_, tab1Title, activeTab_ == ProgramsTab::Exclusions);

    // Tab subtitle
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_TEXT_SEC);
    SelectObject(hdc, hFontSub_);
    RECT rcSub = { 34, 389, 481, 405 };
    const wchar_t* subText = (activeTab_ == ProgramsTab::AppLayouts)
        ? L"Автоматическое переключение раскладки при переходе в выбранную программу"
        : L"Приложения, в которых автоматическое исправление отключено (игры, терминалы)";
    DrawTextW(hdc, subText, -1, &rcSub, DT_LEFT | DT_SINGLELINE);

    // Language selector pills on AppLayouts tab
    if (activeTab_ == ProgramsTab::AppLayouts) {
        rcLangEn_ = { 298, 411, 341, 437 };
        rcLangRu_ = { 341, 411, 384, 437 };

        auto drawLangPill = [&](const RECT& rc, const wchar_t* text, bool active, COLORREF activeCol) {
            HBRUSH hPillB = CreateSolidBrush(active ? activeCol : COL_TOGGLE_OFF);
            HPEN hPillP = CreatePen(PS_SOLID, 1, active ? activeCol : COL_BORDER);
            HBRUSH hOB = (HBRUSH)SelectObject(hdc, hPillB);
            HPEN hOP = (HPEN)SelectObject(hdc, hPillP);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, hOB);
            SelectObject(hdc, hOP);
            DeleteObject(hPillB);
            DeleteObject(hPillP);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, active ? COL_TEXT_PRI : COL_TEXT_SEC);
            SelectObject(hdc, hFontBold_);
            RECT rcText = rc;
            DrawTextW(hdc, text, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        };

        drawLangPill(rcLangEn_, L"EN", selectedAppLayout_ == Config::AppLayout::English, COL_ACCENT);
        drawLangPill(rcLangRu_, L"RU", selectedAppLayout_ == Config::AppLayout::Russian, COL_RU);
    } else {
        rcLangEn_ = {};
        rcLangRu_ = {};
    }
}

void SettingsWindow::Paint(HDC hdc) {
    RECT rcClient;
    GetClientRect(hWnd_, &rcClient);

    // Double buffer memory DC
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    // Fill background
    FillRect(hdcMem, &rcClient, hBrBackground_);

    DrawHeader(hdcMem);
    DrawSettingsCard(hdcMem);
    DrawProgramsCard(hdcMem);

    BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

void SettingsWindow::SaveAndApply() {
    Config::ConfigManager::Instance().Save();
    Hide();
}

LRESULT SettingsWindow::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Paint(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        if (PtInRect(&rcStatusBadge_, pt)) {
            bool next = !Core::HookManager::Instance().IsPaused();
            Core::HookManager::Instance().SetPaused(next);
            HWND fg = GetForegroundWindow();
            WORD lang = Core::LayoutEngine::Instance().GetLayout(fg);
            Platform::TrayIcon::Instance().UpdateLanguage(lang, next);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        if (PtInRect(&rcTabAppLayouts_, pt)) {
            if (activeTab_ != ProgramsTab::AppLayouts) {
                activeTab_ = ProgramsTab::AppLayouts;
                UpdateTabVisibility();
            }
            return 0;
        }

        if (PtInRect(&rcTabExclusions_, pt)) {
            if (activeTab_ != ProgramsTab::Exclusions) {
                activeTab_ = ProgramsTab::Exclusions;
                UpdateTabVisibility();
            }
            return 0;
        }

        if (activeTab_ == ProgramsTab::AppLayouts) {
            if (PtInRect(&rcLangEn_, pt)) {
                selectedAppLayout_ = Config::AppLayout::English;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (PtInRect(&rcLangRu_, pt)) {
                selectedAppLayout_ = Config::AppLayout::Russian;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }

        bool changed = false;
        Config::ConfigManager::Instance().Update([&](Config::AppConfig& cfg) {
            if (PtInRect(&rcToggleAuto_, pt)) {
                cfg.autoSwitch = !cfg.autoSwitch;
                changed = true;
            } else if (PtInRect(&rcToggleCaps_, pt)) {
                cfg.capsLockSwitch = !cfg.capsLockSwitch;
                changed = true;
            } else if (PtInRect(&rcToggleDoubleCaps_, pt)) {
                cfg.doubleCapsConvert = !cfg.doubleCapsConvert;
                changed = true;
            } else if (PtInRect(&rcTogglePwd_, pt)) {
                cfg.passwordGuard = !cfg.passwordGuard;
                changed = true;
            } else if (PtInRect(&rcToggleAutoStart_, pt)) {
                cfg.autostart = !cfg.autostart;
                changed = true;
            }
        });

        if (changed) {
            Config::ConfigManager::Instance().Save();
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        WORD notif = HIWORD(wParam);

        if (id == ID_BTN_PICK_APP) {
            ShowAppPickerMenu(hBtnPickApp_, hEditAppProc_);
        } else if (id == ID_BTN_PICK_EXCL) {
            ShowAppPickerMenu(hBtnPickExcl_, hEditExcl_);
        } else if (id == ID_BTN_APP_ADD) {
            wchar_t buffer[256] = { 0 };
            GetWindowTextW(hEditAppProc_, buffer, 256);
            std::wstring text(buffer);
            if (!text.empty()) {
                Config::AppLayout chosen = selectedAppLayout_;
                Config::ConfigManager::Instance().Update([&text, chosen](Config::AppConfig& cfg) {
                    cfg.SetAppLayout(text, chosen);
                });
                Config::ConfigManager::Instance().Save();
                UpdateControls();
                SetWindowTextW(hEditAppProc_, L"");
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else if (id == ID_BTN_APP_TOGGLE_LANG || (id == ID_LIST_APP_LAYOUTS && notif == LBN_DBLCLK)) {
            ToggleSelectedAppLayout();
        } else if (id == ID_BTN_APP_REMOVE) {
            int sel = (int)SendMessageW(hListAppLayouts_, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && static_cast<size_t>(sel) < displayedAppRules_.size()) {
                std::wstring proc = displayedAppRules_[sel].processName;
                Config::ConfigManager::Instance().Update([&proc](Config::AppConfig& cfg) {
                    cfg.RemoveAppLayout(proc);
                });
                Config::ConfigManager::Instance().Save();
                UpdateControls();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else if (id == ID_BTN_ADD) {
            wchar_t buffer[256] = { 0 };
            GetWindowTextW(hEditExcl_, buffer, 256);
            std::wstring text(buffer);
            if (!text.empty()) {
                Config::ConfigManager::Instance().Update([&text](Config::AppConfig& cfg) {
                    cfg.AddExclusion(text);
                });
                Config::ConfigManager::Instance().Save();
                UpdateControls();
                SetWindowTextW(hEditExcl_, L"");
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else if (id == ID_BTN_REMOVE) {
            int sel = (int)SendMessageW(hListBox_, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                wchar_t buffer[256] = { 0 };
                SendMessageW(hListBox_, LB_GETTEXT, sel, (LPARAM)buffer);
                Config::ConfigManager::Instance().Update([&buffer](Config::AppConfig& cfg) {
                    cfg.RemoveExclusion(buffer);
                });
                Config::ConfigManager::Instance().Save();
                UpdateControls();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else if (id == ID_BTN_SAVE) {
            SaveAndApply();
        } else if (id == ID_BTN_CLOSE) {
            Hide();
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC hdcCtl = (HDC)wParam;
        SetTextColor(hdcCtl, COL_TEXT_PRI);
        SetBkColor(hdcCtl, COL_CARD);
        return (LRESULT)hBrCard_;
    }
    case WM_CLOSE: {
        // Minimize/hide to system tray instead of closing application
        Hide();
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace UI
