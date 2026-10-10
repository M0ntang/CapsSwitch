#include "ui/settings_window.hpp"
#include "config/config.hpp"
#include "core/hook.hpp"
#include "platform/win32_utils.hpp"
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace UI {

constexpr int WINDOW_WIDTH = 520;
constexpr int WINDOW_HEIGHT = 680;

constexpr COLORREF COL_BG = RGB(32, 32, 32);
constexpr COLORREF COL_CARD = RGB(43, 43, 43);
constexpr COLORREF COL_BORDER = RGB(58, 58, 58);
constexpr COLORREF COL_TEXT_PRI = RGB(255, 255, 255);
constexpr COLORREF COL_TEXT_SEC = RGB(160, 160, 160);
constexpr COLORREF COL_ACCENT = RGB(0, 120, 215);
constexpr COLORREF COL_TOGGLE_OFF = RGB(68, 68, 72);
constexpr COLORREF COL_SUCCESS = RGB(16, 185, 129);
constexpr COLORREF COL_AMBER = RGB(245, 158, 11);

constexpr UINT ID_LIST_EXCL = 3001;
constexpr UINT ID_EDIT_EXCL = 3002;
constexpr UINT ID_BTN_ADD = 3003;
constexpr UINT ID_BTN_REMOVE = 3004;
constexpr UINT ID_BTN_SAVE = 3005;
constexpr UINT ID_BTN_CLOSE = 3006;

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

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"CapsSwitchSettingsWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = hBrBackground_;
    wc.hIcon = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_SHARED);
    wc.hIconSm = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_SHARED);

    RegisterClassExW(&wc);

    // Center window on primary monitor
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (scrW - WINDOW_WIDTH) / 2;
    int posY = (scrH - WINDOW_HEIGHT) / 2;

    hWnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        wc.lpszClassName,
        L"CapsSwitch - Настройки",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
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

    // Exclusions Controls
    hListBox_ = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        34, 430, 340, 105,
        hWnd_, (HMENU)ID_LIST_EXCL, wc.hInstance, nullptr
    );
    SendMessageW(hListBox_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hEditExcl_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        34, 390, 340, 26,
        hWnd_, (HMENU)ID_EDIT_EXCL, wc.hInstance, nullptr
    );
    SendMessageW(hEditExcl_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hBtnAdd_ = CreateWindowExW(
        0, L"BUTTON", L"Добавить",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        384, 390, 100, 26,
        hWnd_, (HMENU)ID_BTN_ADD, wc.hInstance, nullptr
    );
    SendMessageW(hBtnAdd_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    hBtnRemove_ = CreateWindowExW(
        0, L"BUTTON", L"Удалить",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        384, 430, 100, 26,
        hWnd_, (HMENU)ID_BTN_REMOVE, wc.hInstance, nullptr
    );
    SendMessageW(hBtnRemove_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    // Footer Buttons
    hBtnSave_ = CreateWindowExW(
        0, L"BUTTON", L"Сохранить",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        280, 595, 105, 32,
        hWnd_, (HMENU)ID_BTN_SAVE, wc.hInstance, nullptr
    );
    SendMessageW(hBtnSave_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    hBtnClose_ = CreateWindowExW(
        0, L"BUTTON", L"Закрыть",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        395, 595, 90, 32,
        hWnd_, (HMENU)ID_BTN_CLOSE, wc.hInstance, nullptr
    );
    SendMessageW(hBtnClose_, WM_SETFONT, (WPARAM)hFontRegular_, TRUE);

    UpdateControls();
    return true;
}

void SettingsWindow::UpdateControls() {
    if (!hListBox_) return;

    SendMessageW(hListBox_, LB_RESETCONTENT, 0, 0);
    const auto& exclusions = Config::ConfigManager::Instance().Get().exclusions;
    for (const auto& excl : exclusions) {
        SendMessageW(hListBox_, LB_ADDSTRING, 0, (LPARAM)excl.c_str());
    }
}

void SettingsWindow::Show() {
    if (!hWnd_) Create();
    UpdateControls();
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

    const auto& cfg = Config::ConfigManager::Instance().Get();

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

void SettingsWindow::DrawExclusionsCard(HDC hdc) {
    int cardX = 20;
    int cardY = 345;
    int cardW = 475;
    int cardH = 230;

    HBRUSH hOldB = (HBRUSH)SelectObject(hdc, hBrCard_);
    HPEN hPen = CreatePen(PS_SOLID, 1, COL_BORDER);
    HPEN hOldP = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, cardX, cardY, cardX + cardW, cardY + cardH, 12, 12);

    SelectObject(hdc, hOldB);
    SelectObject(hdc, hOldP);
    DeleteObject(hPen);

    // Section title
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_TEXT_PRI);
    SelectObject(hdc, hFontBold_);
    RECT rcT = { 34, 355, 400, 373 };
    DrawTextW(hdc, L"Процессы-исключения", -1, &rcT, DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, COL_TEXT_SEC);
    SelectObject(hdc, hFontSub_);
    RECT rcS = { 34, 373, 480, 389 };
    DrawTextW(hdc, L"Приложения, в которых автоматическое исправление отключено (игры, терминалы)", -1, &rcS, DT_LEFT | DT_SINGLELINE);
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
    DrawExclusionsCard(hdcMem);

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
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        auto& cfg = Config::ConfigManager::Instance().GetMutable();
        bool changed = false;

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

        if (changed) {
            Config::ConfigManager::Instance().Save();
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == ID_BTN_ADD) {
            wchar_t buffer[256] = { 0 };
            GetWindowTextW(hEditExcl_, buffer, 256);
            std::wstring text(buffer);
            if (!text.empty()) {
                if (text.find(L".exe") == std::wstring::npos) {
                    text += L".exe";
                }
                Config::ConfigManager::Instance().GetMutable().AddExclusion(text);
                Config::ConfigManager::Instance().Save();
                UpdateControls();
                SetWindowTextW(hEditExcl_, L"");
            }
        } else if (id == ID_BTN_REMOVE) {
            int sel = (int)SendMessageW(hListBox_, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                wchar_t buffer[256] = { 0 };
                SendMessageW(hListBox_, LB_GETTEXT, sel, (LPARAM)buffer);
                Config::ConfigManager::Instance().GetMutable().RemoveExclusion(buffer);
                Config::ConfigManager::Instance().Save();
                UpdateControls();
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
