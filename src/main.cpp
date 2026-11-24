#include <windows.h>
#include "config/config.hpp"
#include "core/hook.hpp"
#include "core/layout_engine.hpp"
#include "platform/tray.hpp"
#include "platform/win32_utils.hpp"
#include "ui/settings_window.hpp"

static const wchar_t* MSG_WINDOW_CLASS = L"CapsSwitchMessageReceiver";
static const wchar_t* MUTEX_NAME = L"Local\\CapsSwitch_SingleInstance_Mutex";
static HWND g_hMsgWnd = nullptr;
static UINT g_wmTaskbarCreated = 0;

constexpr UINT_PTR TIMER_TRIM_WORKING_SET = 1;
constexpr UINT_PTR TIMER_SYNC_TRAY_LAYOUT = 2;

LRESULT CALLBACK MessageWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_wmTaskbarCreated != 0 && msg == g_wmTaskbarCreated) {
        Platform::TrayIcon::Instance().Readd();
        return 0;
    }

    switch (msg) {
    case Platform::WM_TRAYICON: {
        UINT uMsg = LOWORD(lParam);
        if (uMsg == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            Platform::TrayIcon::Instance().ShowContextMenu(pt);
        } else if (uMsg == WM_LBUTTONUP) {
            if (UI::SettingsWindow::Instance().IsVisible()) {
                UI::SettingsWindow::Instance().Hide();
            } else {
                UI::SettingsWindow::Instance().Show();
            }
        }
        return 0;
    }
    case WM_TIMER: {
        if (wParam == TIMER_TRIM_WORKING_SET) {
            // Periodic memory working set optimization
            Platform::TrimWorkingSet();
        } else if (wParam == TIMER_SYNC_TRAY_LAYOUT) {
            HWND fg = GetForegroundWindow();
            if (fg) {
                WORD lang = Core::LayoutEngine::Instance().GetLayout(fg);
                bool paused = Core::HookManager::Instance().IsPaused();
                Platform::TrayIcon::Instance().UpdateLanguage(lang, paused);
            }
        }
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR /*lpCmdLine*/, int nShowCmd) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nShowCmd);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR lpCmdLine, int /*nShowCmd*/) {
    // 1. Single Instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        // Application already running
        HWND hExisting = FindWindowW(L"CapsSwitchSettingsWnd", nullptr);
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // ConfigManager loads config in its constructor; just ensure instance is initialized
    (void)Config::ConfigManager::Instance();

    // 3. Register message-only window for system tray and system messages
    g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MessageWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MSG_WINDOW_CLASS;
    RegisterClassExW(&wc);

    g_hMsgWnd = CreateWindowExW(
        0, MSG_WINDOW_CLASS, L"CapsSwitchMsgReceiver",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr
    );

    // 4. Initialize Settings Window
    UI::SettingsWindow::Instance().Create();

    // Check command line arguments: if "--settings" passed, open settings window
    if (lpCmdLine && wcsstr(lpCmdLine, L"--settings")) {
        UI::SettingsWindow::Instance().Show();
    }

    // 5. Initialize System Tray
    Platform::TrayIcon::Instance().Create(g_hMsgWnd);
    Platform::TrayIcon::Instance().SetOnSettingsClicked([]() {
        UI::SettingsWindow::Instance().Show();
    });
    Platform::TrayIcon::Instance().SetOnPauseToggled([]() {
        bool current = Core::HookManager::Instance().IsPaused();
        bool next = !current;
        Core::HookManager::Instance().SetPaused(next);
        HWND fg = GetForegroundWindow();
        WORD lang = Core::LayoutEngine::Instance().GetLayout(fg);
        Platform::TrayIcon::Instance().UpdateLanguage(lang, next);
        if (UI::SettingsWindow::Instance().IsVisible()) {
            InvalidateRect(UI::SettingsWindow::Instance().GetHwnd(), nullptr, FALSE);
        }
    });
    Platform::TrayIcon::Instance().SetOnExitClicked([]() {
        PostQuitMessage(0);
    });

    // 6. Connect Layout Changed callback to update Tray icon badge
    Core::HookManager::Instance().SetLayoutChangedCallback([](WORD newLang) {
        bool paused = Core::HookManager::Instance().IsPaused();
        Platform::TrayIcon::Instance().UpdateLanguage(newLang, paused);
    });

    // Initial tray language badge query
    WORD initialLang = Core::LayoutEngine::Instance().GetLayout(GetForegroundWindow());
    Platform::TrayIcon::Instance().UpdateLanguage(initialLang, false);

    // 7. Start low-level keyboard hook
    if (!Core::HookManager::Instance().Start()) {
        MessageBoxW(
            nullptr,
            L"Не удалось инициализировать низкоуровневый перехват клавиатуры (WH_KEYBOARD_LL).",
            L"CapsSwitch - Ошибка",
            MB_ICONERROR | MB_OK
        );
        Platform::TrayIcon::Instance().Destroy();
        CloseHandle(hMutex);
        return 1;
    }

    // Timers for working set trim (30s) and background foreground layout sync (500ms)
    SetTimer(g_hMsgWnd, TIMER_TRIM_WORKING_SET, 30000, nullptr);
    SetTimer(g_hMsgWnd, TIMER_SYNC_TRAY_LAYOUT, 500, nullptr);
    Platform::TrimWorkingSet();

    // 8. Main Message Loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 9. Shutdown & Cleanup
    KillTimer(g_hMsgWnd, TIMER_TRIM_WORKING_SET);
    KillTimer(g_hMsgWnd, TIMER_SYNC_TRAY_LAYOUT);
    Core::HookManager::Instance().Stop();
    Platform::TrayIcon::Instance().Destroy();

    if (g_hMsgWnd) {
        DestroyWindow(g_hMsgWnd);
    }
    UnregisterClassW(MSG_WINDOW_CLASS, hInstance);

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
