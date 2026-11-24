#pragma once

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <queue>
#include <mutex>
#include <string>
#include <thread>
#include <functional>

namespace Core {

struct RawKeyEvent {
    DWORD vkCode = 0;
    DWORD scanCode = 0;
    DWORD flags = 0;
    bool isDown = false;
    DWORD time = 0;
    bool isShift = false;
    bool isCtrl = false;
    bool isAlt = false;
    bool isCapsLock = false;
    bool isReset = false;
};

class HookManager {
public:
    static HookManager& Instance();

    bool Start();
    void Stop();

    void SetPaused(bool paused);
    bool IsPaused() const { return isPaused_.load(std::memory_order_relaxed); }

    // Callback when layout changes (for tray icon update)
    void SetLayoutChangedCallback(std::function<void(WORD)> cb) { onLayoutChanged_ = cb; }

    // Queue & Action management
    void PushKeyEvent(const RawKeyEvent& ev);
    void PushResetEvent(bool clearPendingKeys = true);
    void OnCapsPress();
    void PostCapsToggle();
    void PostDoubleCapsAction();
    void OnForegroundChanged(HWND hwnd);

private:
    HookManager();
    ~HookManager();

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hWinEventHook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD dwEventThread,
        DWORD dwmsEventTime
    );

    void WorkerThreadLoop();
    void ProcessKeyEvent(const RawKeyEvent& ev);
    void ProcessForegroundChange(HWND hwnd);

    static HHOOK s_hKbdHook;
    static HHOOK s_hMouseHook;
    static HWINEVENTHOOK s_hWinEventHook;
    static std::atomic<bool> s_isPaused;
    static HookManager* s_instance;

    std::atomic<bool> isRunning_{ false };
    std::atomic<bool> isPaused_{ false };
    std::atomic<bool> capsTogglePending_{ false };
    std::atomic<bool> doubleCapsPending_{ false };
    std::atomic<bool> foregroundChangedPending_{ false };
    std::atomic<HWND> pendingForegroundHwnd_{ nullptr };
    std::atomic<bool> resetForegroundTracking_{ false };

    uint64_t lastCapsPressMs_ = 0;
    std::wstring lastForegroundProcess_;
    uint64_t lastAppliedRulesVersion_ = 0;

    std::queue<RawKeyEvent> eventQueue_;
    std::mutex queueMutex_;
    HANDLE hKeyEvent_ = nullptr;
    HANDLE hStopEvent_ = nullptr;
    std::thread workerThread_;

    std::function<void(WORD)> onLayoutChanged_;
};

} // namespace Core
