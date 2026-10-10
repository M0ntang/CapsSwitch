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

    // Queue management called from LowLevelKeyboardProc
    void PushKeyEvent(const RawKeyEvent& ev);
    void PostCapsToggle();

private:
    HookManager();
    ~HookManager();

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    void WorkerThreadLoop();
    void ProcessKeyEvent(const RawKeyEvent& ev);

    static HHOOK s_hHook;
    static std::atomic<bool> s_isPaused;
    static HookManager* s_instance;

    std::atomic<bool> isRunning_{ false };
    std::atomic<bool> isPaused_{ false };
    std::atomic<bool> capsTogglePending_{ false };

    std::queue<RawKeyEvent> eventQueue_;
    std::mutex queueMutex_;
    HANDLE hKeyEvent_ = nullptr;
    HANDLE hStopEvent_ = nullptr;
    std::thread workerThread_;

    std::function<void(WORD)> onLayoutChanged_;
};

} // namespace Core
