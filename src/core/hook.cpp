#include "core/hook.hpp"
#include "core/layout_engine.hpp"
#include "core/detector.hpp"
#include "config/config.hpp"
#include "platform/password_guard.hpp"
#include "platform/win32_utils.hpp"

namespace Core {

HHOOK HookManager::s_hKbdHook = nullptr;
HHOOK HookManager::s_hMouseHook = nullptr;
std::atomic<bool> HookManager::s_isPaused(false);
HookManager* HookManager::s_instance = nullptr;

static bool s_capsIsPressed = false;

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
    s_hKbdHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(nullptr),
        0
    );

    // Install low-level mouse hook to reset word buffer on clicks
    s_hMouseHook = SetWindowsHookExW(
        WH_MOUSE_LL,
        LowLevelMouseProc,
        GetModuleHandle(nullptr),
        0
    );

    return (s_hKbdHook != nullptr);
}

void HookManager::Stop() {
    if (!isRunning_.load()) return;

    if (s_hKbdHook) {
        UnhookWindowsHookEx(s_hKbdHook);
        s_hKbdHook = nullptr;
    }

    if (s_hMouseHook) {
        UnhookWindowsHookEx(s_hMouseHook);
        s_hMouseHook = nullptr;
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

void HookManager::OnCapsPress() {
    uint64_t now = Platform::GetCurrentTimeMicroseconds() / 1000;
    uint64_t diff = now - lastCapsPressMs_;
    lastCapsPressMs_ = now;

    const auto& cfg = Config::ConfigManager::Instance().Get();
    if (cfg.doubleCapsConvert && diff <= 450 && diff >= 40) {
        // Quick double-click detected -> translate selection or last word
        PostDoubleCapsAction();
    } else {
        // Single click -> toggle layout
        PostCapsToggle();
    }
}

void HookManager::PostCapsToggle() {
    capsTogglePending_.store(true, std::memory_order_release);
    SetEvent(hKeyEvent_);
}

void HookManager::PostDoubleCapsAction() {
    // If a double-click occurs, cancel the pending single-click toggle and trigger convert
    capsTogglePending_.store(false, std::memory_order_release);
    doubleCapsPending_.store(true, std::memory_order_release);
    SetEvent(hKeyEvent_);
}

LRESULT CALLBACK HookManager::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN)) {
        if (!LayoutEngine::IsSimulatingInput()) {
            Detector::Instance().Reset();
        }
    }
    return CallNextHookEx(s_hMouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK HookManager::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
    }

    auto* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    // 1. Immediately bypass injected keys to prevent input recursion
    if ((pKbd->flags & LLKHF_INJECTED) || LayoutEngine::IsSimulatingInput()) {
        return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
    }

    bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool isUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    // 2. Caps Lock Interception (zero-lag layout toggling & double click detection)
    if (pKbd->vkCode == VK_CAPITAL && !s_isPaused.load(std::memory_order_relaxed)) {
        const auto& cfg = Config::ConfigManager::Instance().Get();
        if (cfg.capsLockSwitch) {
            if (isDown) {
                // Filter out auto-repeat: trigger only on initial physical keydown
                if (!s_capsIsPressed) {
                    s_capsIsPressed = true;
                    if (s_instance) {
                        s_instance->OnCapsPress();
                    }
                }
            } else if (isUp) {
                s_capsIsPressed = false;
            }
            // Always suppress standard Caps Lock toggle in OS
            return 1;
        }
    }

    // 3. Navigation and control keys: reset word buffer
    if (isDown) {
        if (pKbd->vkCode == VK_LEFT || pKbd->vkCode == VK_RIGHT || pKbd->vkCode == VK_UP || pKbd->vkCode == VK_DOWN ||
            pKbd->vkCode == VK_HOME || pKbd->vkCode == VK_END || pKbd->vkCode == VK_PRIOR || pKbd->vkCode == VK_NEXT ||
            pKbd->vkCode == VK_ESCAPE || pKbd->vkCode == VK_DELETE) {
            Detector::Instance().Reset();
            return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
        }
    }

    // Capture modifier keys at the instant of key press
    bool isShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool isCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

    // Hotkey combinations (Ctrl+C, Ctrl+V, Alt+Tab, etc.) reset buffer
    if (isDown && (isCtrl || isAlt)) {
        Detector::Instance().Reset();
        return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
    }

    // 4. Queue key event for asynchronous processing in worker thread
    if (!s_isPaused.load(std::memory_order_relaxed) && s_instance) {
        s_instance->PushKeyEvent(RawKeyEvent{
            pKbd->vkCode,
            pKbd->scanCode,
            pKbd->flags,
            isDown,
            pKbd->time,
            isShift,
            isCtrl,
            isAlt
        });
    }

    return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
}

void HookManager::WorkerThreadLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HANDLE handles[2] = { hStopEvent_, hKeyEvent_ };

    while (isRunning_.load()) {
        DWORD waitRes = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (waitRes == WAIT_OBJECT_0) {
            // Stop signaled
            break;
        }

        // Process pending Double-Caps Action (translate selection or last word)
        if (doubleCapsPending_.exchange(false, std::memory_order_acquire)) {
            LayoutEngine::Instance().ConvertSelectionOrLastWord();
            HWND fg = GetForegroundWindow();
            if (fg && onLayoutChanged_) {
                onLayoutChanged_(LayoutEngine::Instance().GetLayout(fg));
            }
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

    CoUninitialize();
}

void HookManager::ProcessKeyEvent(const RawKeyEvent& ev) {
    if (isPaused_.load(std::memory_order_relaxed)) return;

    HWND fg = GetForegroundWindow();
    if (!fg) return;

    const auto& cfg = Config::ConfigManager::Instance().Get();

    // Check process exclusion list
    std::wstring procName = Platform::GetProcessNameByHwnd(fg);
    if (cfg.IsExcluded(procName)) {
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
    WORD currentLang = LOWORD(reinterpret_cast<DWORD_PTR>(currentHkl));
    if (onLayoutChanged_) {
        onLayoutChanged_(currentLang);
    }

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
        if (cfg.autoSwitch) {
            DetectedSwitch sw = Detector::Instance().EvaluateWord(delimChar);
            if (sw.shouldSwitch) {
                // Erase previous word + delimiter
                size_t eraseCount = sw.originalWord.length() + 1;
                std::wstring replacement = sw.convertedWord + sw.delimiter;

                LayoutEngine::Instance().SimulateReplacement(eraseCount, replacement);

                // Switch layout in foreground window
                WORD targetLang = (sw.targetLang == WordLanguage::Russian) ? LANG_ID_RU : LANG_ID_EN;
                LayoutEngine::Instance().SwitchLayout(fg, targetLang);

                if (onLayoutChanged_) {
                    onLayoutChanged_(targetLang);
                }
                Detector::Instance().SetLastWord(sw.convertedWord, sw.delimiter);
            }
        } else {
            Detector::Instance().Reset();
        }
        return;
    }

    // Convert virtual key to Unicode character using captured modifier state
    BYTE keyState[256] = { 0 };
    if (ev.isShift) {
        keyState[VK_SHIFT] = 0x80;
    }

    wchar_t unicodeBuf[4] = { 0 };
    int count = ToUnicodeEx(ev.vkCode, ev.scanCode, keyState, unicodeBuf, 4, 0, currentHkl);

    if (count == 1) {
        wchar_t ch = unicodeBuf[0];
        // Check sentence end punctuation delimiters (like !, ?)
        if (ch == L'!' || ch == L'?' || ch == L':') {
            if (cfg.autoSwitch) {
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
                    Detector::Instance().SetLastWord(sw.convertedWord, sw.delimiter);
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
