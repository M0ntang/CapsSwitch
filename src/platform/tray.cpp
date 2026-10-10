#include "platform/tray.hpp"
#include "core/layout_engine.hpp"
#include "core/hook.hpp"

namespace Platform {

TrayIcon& TrayIcon::Instance() {
    static TrayIcon s_instance;
    return s_instance;
}

TrayIcon::TrayIcon() {}

TrayIcon::~TrayIcon() {
    Destroy();
}

HICON TrayIcon::CreateBadgeIcon(const wchar_t* text, COLORREF bgCol, COLORREF textCol) {
    int iconSize = GetSystemMetrics(SM_CXSMICON);
    if (iconSize <= 0) iconSize = 16;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HDC hdcMask = CreateCompatibleDC(hdcScreen);

    // Color bitmap
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = iconSize;
    bi.bmiHeader.biHeight = iconSize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hbmMask = CreateBitmap(iconSize, iconSize, 1, 1, nullptr);

    HBITMAP hOldColor = (HBITMAP)SelectObject(hdcMem, hbmColor);
    HBITMAP hOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);

    // Fill background with black for mask
    RECT rc = { 0, 0, iconSize, iconSize };
    HBRUSH hMaskBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdcMask, &rc, hMaskBrush);
    DeleteObject(hMaskBrush);

    // Rounded rectangle background in color bitmap
    HBRUSH hBgBrush = CreateSolidBrush(bgCol);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMem, hBgBrush);
    HPEN hPen = CreatePen(PS_SOLID, 1, bgCol);
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);

    RoundRect(hdcMem, 0, 0, iconSize, iconSize, 4, 4);

    // Also draw clear mask rectangle
    HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
    SelectObject(hdcMask, hBlackBrush);
    RoundRect(hdcMask, 0, 0, iconSize, iconSize, 4, 4);
    DeleteObject(hBlackBrush);

    // Draw text centered
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, textCol);

    int fontHeight = -MulDiv(iconSize >= 24 ? 9 : 8, GetDeviceCaps(hdcMem, LOGPIXELSY), 72);
    HFONT hFont = CreateFontW(
        fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

    DrawTextW(hdcMem, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hOldFont);
    SelectObject(hdcMem, hOldBrush);
    SelectObject(hdcMem, hOldPen);
    SelectObject(hdcMem, hOldColor);
    SelectObject(hdcMask, hOldMask);

    DeleteObject(hFont);
    DeleteObject(hBgBrush);
    DeleteObject(hPen);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    DeleteDC(hdcMem);
    DeleteDC(hdcMask);
    ReleaseDC(nullptr, hdcScreen);

    return hIcon;
}

bool TrayIcon::Create(HWND hWndOwner) {
    if (isVisible_) return true;

    hWndOwner_ = hWndOwner;

    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hWndOwner_;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;

    hCurrentIcon_ = CreateBadgeIcon(L"EN", RGB(0, 120, 215), RGB(255, 255, 255));
    nid_.hIcon = hCurrentIcon_;
    wcscpy_s(nid_.szTip, L"CapsSwitch (Активен - EN)");

    isVisible_ = Shell_NotifyIconW(NIM_ADD, &nid_);
    return isVisible_;
}

void TrayIcon::Destroy() {
    if (isVisible_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        isVisible_ = false;
    }
    if (hCurrentIcon_) {
        DestroyIcon(hCurrentIcon_);
        hCurrentIcon_ = nullptr;
    }
}

void TrayIcon::UpdateLanguage(WORD langId, bool isPaused) {
    if (!isVisible_) return;

    if (hCurrentIcon_) {
        DestroyIcon(hCurrentIcon_);
        hCurrentIcon_ = nullptr;
    }

    if (isPaused) {
        hCurrentIcon_ = CreateBadgeIcon(L"||", RGB(118, 118, 118), RGB(255, 255, 255));
        wcscpy_s(nid_.szTip, L"CapsSwitch (Приостановлен)");
    } else if (langId == Core::LANG_ID_RU) {
        hCurrentIcon_ = CreateBadgeIcon(L"RU", RGB(216, 59, 1), RGB(255, 255, 255));
        wcscpy_s(nid_.szTip, L"CapsSwitch (Активен - RU)");
    } else {
        hCurrentIcon_ = CreateBadgeIcon(L"EN", RGB(0, 120, 215), RGB(255, 255, 255));
        wcscpy_s(nid_.szTip, L"CapsSwitch (Активен - EN)");
    }

    nid_.hIcon = hCurrentIcon_;
    nid_.uFlags = NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::ShowContextMenu(POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    bool paused = Core::HookManager::Instance().IsPaused();

    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, L"Настройки");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_PAUSE, paused ? L"Возобновить работу" : L"Приостановить работу");
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Выход");

    SetForegroundWindow(hWndOwner_);
    UINT cmd = TrackPopupMenuEx(
        hMenu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        pt.x,
        pt.y,
        hWndOwner_,
        nullptr
    );
    PostMessageW(hWndOwner_, WM_NULL, 0, 0);
    DestroyMenu(hMenu);

    if (cmd == ID_TRAY_SETTINGS && onSettings_) {
        onSettings_();
    } else if (cmd == ID_TRAY_PAUSE && onPause_) {
        onPause_();
    } else if (cmd == ID_TRAY_EXIT && onExit_) {
        onExit_();
    }
}

} // namespace Platform
