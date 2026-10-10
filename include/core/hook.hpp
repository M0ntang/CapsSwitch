#pragma once

#include <windows.h>
#include <atomic>
#include <queue>
#include <mutex>
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
    void OnCapsPress();
    void PostCapsToggle();
    void PostDoubleCapsAction();

private:
    HookManager();
    ~HookManager();

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    void WorkerThreadLoop();
    void ProcessKeyEvent(const RawKeyEvent& ev);

    static HHOOK s_hKbdHook;
    static HHOOK s_hMouseHook;
    static std::atomic<bool> s_isPaused;
    static HookManager* s_instance;

    std::atomic<bool> isRunning_{ false };
    std::atomic<bool> isPaused_{ false };
    std::atomic<bool> capsTogglePending_{ false };
    std::atomic<bool> doubleCapsPending_{ false };

    uint64_t lastCapsPressMs_ = 0;

    std::queue<RawKeyEvent> eventQueue_;
    std::mutex queueMutex_;
    HANDLE hKeyEvent_ = nullptr;
    HANDLE hStopEvent_ = nullptr;
    std::thread workerThread_;

    std::function<void(WORD)> onLayoutChanged_;
};

} // namespace Core
