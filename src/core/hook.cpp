#include "core/hook.hpp"
#include "core/layout_engine.hpp"
#include "core/detector.hpp"
#include "config/config.hpp"
#include "platform/password_guard.hpp"
#include "platform/win32_utils.hpp"

namespace Core {

HHOOK HookManager::s_hHook = nullptr;
std::atomic<bool> HookManager::s_isPaused(false);
HookManager* HookManager::s_instance = nullptr;

HookManager& HookManager::Instance() {
    static HookManager s_instanceObj;
    return s_instanceObj;
}

HookManager::HookManager() {
    s_instance = this;
    hKeyEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    hStopEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

HookManager::~HookManager() {
    Stop();
    if (hKeyEvent_) CloseHandle(hKeyEvent_);
    if (hStopEvent_) CloseHandle(hStopEvent_);
}

void HookManager::SetPaused(bool paused) {
    isPaused_.store(paused, std::memory_order_relaxed);
    s_isPaused.store(paused, std::memory_order_relaxed);
    if (paused) {
        Detector::Instance().Reset();
    }
}

bool HookManager::Start() {
    if (isRunning_.load()) return true;

    ResetEvent(hStopEvent_);
    ResetEvent(hKeyEvent_);

    // Start worker thread first
    isRunning_.store(true);
    workerThread_ = std::thread(&HookManager::WorkerThreadLoop, this);

    // Install low-level keyboard hook
    s_hHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(nullptr),
        0
    );

    return (s_hHook != nullptr);
}

void HookManager::Stop() {
    if (!isRunning_.load()) return;

    if (s_hHook) {
        UnhookWindowsHookEx(s_hHook);
        s_hHook = nullptr;
    }

    isRunning_.store(false);
    SetEvent(hStopEvent_);

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void HookManager::PushKeyEvent(const RawKeyEvent& ev) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.push(ev);
    }
    SetEvent(hKeyEvent_);
}

void HookManager::PostCapsToggle() {
    capsTogglePending_.store(true, std::memory_order_release);
    SetEvent(hKeyEvent_);
}

LRESULT CALLBACK HookManager::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    auto* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    // 1. Immediately bypass injected keys to prevent input recursion
    if ((pKbd->flags & LLKHF_INJECTED) || LayoutEngine::IsSimulatingInput()) {
        return CallNextHookEx(s_hHook, nCode, wParam, lParam);
    }

    bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

    // 2. Caps Lock Interception (zero-lag layout toggling)
    if (pKbd->vkCode == VK_CAPITAL && !s_isPaused.load(std::memory_order_relaxed)) {
        const auto& cfg = Config::ConfigManager::Instance().Get();
        if (cfg.capsLockSwitch) {
            if (isDown && s_instance) {
                s_instance->PostCapsToggle();
            }
            // Suppress default Caps Lock state toggle in Windows
            return 1;
        }
    }

    // 3. Queue key event for asynchronous processing in worker thread
    if (!s_isPaused.load(std::memory_order_relaxed) && s_instance) {
        s_instance->PushKeyEvent(RawKeyEvent{
            pKbd->vkCode,
            pKbd->scanCode,
            pKbd->flags,
            isDown,
            pKbd->time
        });
    }

    return CallNextHookEx(s_hHook, nCode, wParam, lParam);
}

void HookManager::WorkerThreadLoop() {
    HANDLE handles[2] = { hStopEvent_, hKeyEvent_ };

    while (isRunning_.load()) {
        DWORD waitRes = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (waitRes == WAIT_OBJECT_0) {
            // Stop signaled
            break;
        }

        // Process pending Caps Lock toggle
        if (capsTogglePending_.exchange(false, std::memory_order_acquire)) {
            HWND fg = GetForegroundWindow();
            if (fg) {
                LayoutEngine::Instance().ToggleLayout(fg);
                WORD newLang = LayoutEngine::Instance().GetLayout(fg);
                if (onLayoutChanged_) {
                    onLayoutChanged_(newLang);
                }
            }
        }

        // Drain event queue
        while (isRunning_.load()) {
            RawKeyEvent ev;
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                if (eventQueue_.empty()) {
                    break;
                }
                ev = eventQueue_.front();
                eventQueue_.pop();
            }

            ProcessKeyEvent(ev);
        }
    }
}

void HookManager::ProcessKeyEvent(const RawKeyEvent& ev) {
    if (isPaused_.load(std::memory_order_relaxed)) return;

    HWND fg = GetForegroundWindow();
    if (!fg) return;

    // Check process exclusion list
    std::wstring procName = Platform::GetProcessNameByHwnd(fg);
    if (Config::ConfigManager::Instance().Get().IsExcluded(procName)) {
        Detector::Instance().Reset();
        return;
    }

    // Check Password Guard
    if (Platform::PasswordGuard::Instance().CheckAndUpdateFocus(fg)) {
        // Drop buffered keystrokes inside password fields
        return;
    }

    if (!ev.isDown) {
        // We only buffer and inspect on key press (down)
        return;
    }

    DWORD threadId = GetWindowThreadProcessId(fg, nullptr);
    HKL currentHkl = GetKeyboardLayout(threadId);

    // Handle Backspace
    if (ev.vkCode == VK_BACK) {
        Detector::Instance().OnBackspace();
        return;
    }

    // Handle delimiters
    bool isSpace = (ev.vkCode == VK_SPACE);
    bool isReturn = (ev.vkCode == VK_RETURN);
    bool isTab = (ev.vkCode == VK_TAB);

    wchar_t delimChar = 0;
    if (isSpace) delimChar = L' ';
    else if (isReturn) delimChar = L'\n';
    else if (isTab) delimChar = L'\t';

    if (delimChar != 0) {
        if (Config::ConfigManager::Instance().Get().autoSwitch) {
            DetectedSwitch sw = Detector::Instance().EvaluateWord(delimChar);
            if (sw.shouldSwitch) {
                // Erase previous word and retype in new layout
                size_t eraseCount = sw.originalWord.length() + 1; // including delimiter
                std::wstring replacement = sw.convertedWord + sw.delimiter;

                LayoutEngine::Instance().SimulateReplacement(eraseCount, replacement);

                // Switch layout in foreground window
                WORD targetLang = (sw.targetLang == WordLanguage::Russian) ? LANG_ID_RU : LANG_ID_EN;
                LayoutEngine::Instance().SwitchLayout(fg, targetLang);

                if (onLayoutChanged_) {
                    onLayoutChanged_(targetLang);
                }
            }
        } else {
            Detector::Instance().Reset();
        }
        return;
    }

    // Convert virtual key to Unicode character
    BYTE keyState[256] = { 0 };
    GetKeyboardState(keyState);

    wchar_t unicodeBuf[4] = { 0 };
    int count = ToUnicodeEx(ev.vkCode, ev.scanCode, keyState, unicodeBuf, 4, 0, currentHkl);

    if (count == 1) {
        wchar_t ch = unicodeBuf[0];
        // Check punctuation delimiters like .,;:?!
        if (ch == L'.' || ch == L',' || ch == L'!' || ch == L'?' || ch == L';' || ch == L':') {
            if (Config::ConfigManager::Instance().Get().autoSwitch) {
                DetectedSwitch sw = Detector::Instance().EvaluateWord(ch);
                if (sw.shouldSwitch) {
                    size_t eraseCount = sw.originalWord.length() + 1;
                    std::wstring replacement = sw.convertedWord + sw.delimiter;
                    LayoutEngine::Instance().SimulateReplacement(eraseCount, replacement);

                    WORD targetLang = (sw.targetLang == WordLanguage::Russian) ? LANG_ID_RU : LANG_ID_EN;
                    LayoutEngine::Instance().SwitchLayout(fg, targetLang);

                    if (onLayoutChanged_) {
                        onLayoutChanged_(targetLang);
                    }
                    return;
                }
            }
            Detector::Instance().Reset();
        } else if (ch >= 32) {
            Detector::Instance().OnChar(ch);
        }
    }
}

} // namespace Core
