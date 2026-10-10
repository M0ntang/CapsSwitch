#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

namespace Platform {

constexpr UINT WM_TRAYICON = WM_APP + 100;
constexpr UINT ID_TRAY_SETTINGS = 2001;
constexpr UINT ID_TRAY_PAUSE = 2002;
constexpr UINT ID_TRAY_EXIT = 2003;

class TrayIcon {
public:
    static TrayIcon& Instance();

    bool Create(HWND hWndOwner);
    void Destroy();

    void UpdateLanguage(WORD langId, bool isPaused);
    void ShowContextMenu(POINT pt);

    void SetOnSettingsClicked(std::function<void()> cb) { onSettings_ = cb; }
    void SetOnPauseToggled(std::function<void()> cb) { onPause_ = cb; }
    void SetOnExitClicked(std::function<void()> cb) { onExit_ = cb; }

private:
    TrayIcon();
    ~TrayIcon();

    HICON CreateBadgeIcon(const wchar_t* text, COLORREF bgCol, COLORREF textCol);

    HWND hWndOwner_ = nullptr;
    NOTIFYICONDATAW nid_ = {};
    HICON hCurrentIcon_ = nullptr;
    bool isVisible_ = false;

    std::function<void()> onSettings_;
    std::function<void()> onPause_;
    std::function<void()> onExit_;
};

} // namespace Platform
