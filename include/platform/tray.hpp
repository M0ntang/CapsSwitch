#pragma once

#include <windows.h>
#include <shellapi.h>
#include <mutex>
#include <string>
#include <functional>

namespace Platform {

constexpr UINT WM_TRAYICON = WM_APP + 100;
constexpr UINT ID_TRAY_SETTINGS = 2001;
constexpr UINT ID_TRAY_PAUSE = 2002;
constexpr UINT ID_TRAY_EXIT = 2003;

constexpr UINT IDI_TRAY_EN = 101;
constexpr UINT IDI_TRAY_RU = 102;
constexpr UINT IDI_TRAY_PAUSED = 103;

class TrayIcon {
public:
    static TrayIcon& Instance();

    bool Create(HWND hWndOwner);
    bool Readd();
    void Destroy();

    void UpdateLanguage(WORD langId, bool isPaused);
    void RefreshTheme();
    void ShowContextMenu(POINT pt);

    static HICON CreateFluentBadgeIcon(int sizePx, bool isRu, bool isPaused, bool darkTaskbar);
    static HICON CreateFluentBadgeIcon(int sizePx, bool isRu, bool isPaused);
    static HICON CreateFluentAppIcon(int sizePx);

    void SetOnSettingsClicked(std::function<void()> cb) { onSettings_ = cb; }
    void SetOnPauseToggled(std::function<void()> cb) { onPause_ = cb; }
    void SetOnExitClicked(std::function<void()> cb) { onExit_ = cb; }

private:
    TrayIcon();
    ~TrayIcon();

    void EnsureIconsLocked();
    void DestroyIconsLocked();
    void ApplyStateLocked();

    mutable std::mutex mutex_;
    HWND hWndOwner_ = nullptr;
    NOTIFYICONDATAW nid_ = {};
    HICON hIconEn_ = nullptr;
    HICON hIconRu_ = nullptr;
    HICON hIconPaused_ = nullptr;
    int iconSize_ = 0;
    bool darkTaskbar_ = true;
    bool isVisible_ = false;
    bool hasState_ = false;
    bool currentIsRu_ = false;
    bool currentPaused_ = false;

    std::function<void()> onSettings_;
    std::function<void()> onPause_;
    std::function<void()> onExit_;
};

} // namespace Platform
