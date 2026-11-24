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
        capsTogglePending_.store(false, std::memory_order_relaxed);
        doubleCapsPending_.store(false, std::memory_order_relaxed);
        lastCapsPressMs_ = 0;
        PushResetEvent(true);
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

void HookManager::PushResetEvent(bool clearPendingKeys) {
    RawKeyEvent resetEv;
    resetEv.isReset = true;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (clearPendingKeys) {
            std::queue<RawKeyEvent> empty;
            std::swap(eventQueue_, empty);
        }
        eventQueue_.push(resetEv);
    }
    SetEvent(hKeyEvent_);
}

void HookManager::OnCapsPress() {
    uint64_t now = Platform::GetCurrentTimeMicroseconds() / 1000;
    uint64_t diff = (lastCapsPressMs_ != 0 && now >= lastCapsPressMs_)
                        ? (now - lastCapsPressMs_)
                        : UINT64_MAX;

    auto& cfgMgr = Config::ConfigManager::Instance();
    if (cfgMgr.DoubleCapsConvert() && diff <= 450 && diff >= 30) {
        // Quick double-click detected -> translate selection or last word, reset timer to prevent 3rd-click chaining
        lastCapsPressMs_ = 0;
        PostDoubleCapsAction();
    } else {
        lastCapsPressMs_ = now;
        if (cfgMgr.CapsLockSwitch()) {
            // Single click -> toggle layout
            PostCapsToggle();
        }
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
        if (!LayoutEngine::IsSimulatingInput() && s_instance && !s_isPaused.load(std::memory_order_relaxed)) {
            s_instance->PushResetEvent(true);
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

    // Capture modifier keys at the instant of key event
    bool isShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool isCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool isWin = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
    bool isCapsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

    // 2. Caps Lock Interception (Shift+Caps passes through to toggle real Caps Lock)
    if (pKbd->vkCode == VK_CAPITAL && !s_isPaused.load(std::memory_order_relaxed)) {
        auto& cfgMgr = Config::ConfigManager::Instance();
        if (isUp && s_capsIsPressed) {
            // Always clear pressed state on release even if modifiers were pressed mid-hold
            s_capsIsPressed = false;
            return 1;
        }
        if (!isShift && !isCtrl && !isAlt && !isWin && (cfgMgr.CapsLockSwitch() || cfgMgr.DoubleCapsConvert())) {
            if (isDown) {
                // Filter out auto-repeat: trigger only on initial physical keydown
                if (!s_capsIsPressed) {
                    s_capsIsPressed = true;
                    if (s_instance) {
                        s_instance->OnCapsPress();
                    }
                }
            }
            // Suppress standard Caps Lock toggle in OS
            return 1;
        }
    }

    // 3. Navigation and control keys: reset word buffer in strict queue order
    if (isDown) {
        if (pKbd->vkCode == VK_LEFT || pKbd->vkCode == VK_RIGHT || pKbd->vkCode == VK_UP || pKbd->vkCode == VK_DOWN ||
            pKbd->vkCode == VK_HOME || pKbd->vkCode == VK_END || pKbd->vkCode == VK_PRIOR || pKbd->vkCode == VK_NEXT ||
            pKbd->vkCode == VK_ESCAPE || pKbd->vkCode == VK_DELETE) {
            if (s_instance && !s_isPaused.load(std::memory_order_relaxed)) {
                s_instance->PushResetEvent(true);
            }
            return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
        }
    }

    // Hotkey combinations (Ctrl+C, Alt+Tab, Win+Space, Win+R, etc.) reset buffer in strict queue order
    if (isDown && (isCtrl || isAlt || isWin)) {
        if (s_instance && !s_isPaused.load(std::memory_order_relaxed)) {
            s_instance->PushResetEvent(true);
        }
        return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
    }

    // 4. Queue keydown events (excluding bare Shift) for asynchronous processing in worker thread
    if (isDown &&
        pKbd->vkCode != VK_SHIFT && pKbd->vkCode != VK_LSHIFT && pKbd->vkCode != VK_RSHIFT &&
        !s_isPaused.load(std::memory_order_relaxed) && s_instance) {
        RawKeyEvent ev;
        ev.vkCode = pKbd->vkCode;
        ev.scanCode = pKbd->scanCode;
        ev.flags = pKbd->flags;
        ev.isDown = true;
        ev.time = pKbd->time;
        ev.isShift = isShift;
        ev.isCtrl = isCtrl;
        ev.isAlt = isAlt;
        ev.isCapsLock = isCapsLock;
        ev.isReset = false;
        s_instance->PushKeyEvent(ev);
    }

    return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
}

void HookManager::WorkerThreadLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Platform::PasswordGuard::Instance().Initialize();

    HANDLE handles[2] = { hStopEvent_, hKeyEvent_ };

    while (isRunning_.load()) {
        DWORD waitRes = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (waitRes == WAIT_OBJECT_0) {
            // Stop signaled
            break;
        }

        // Drain key/reset queue BEFORE handling double-caps or toggle actions that depend on buffered words
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
    }

    LayoutEngine::Instance().ShutdownTsf();
    Platform::PasswordGuard::Instance().Shutdown();
    CoUninitialize();
}

void HookManager::ProcessKeyEvent(const RawKeyEvent& ev) {
    if (isPaused_.load(std::memory_order_relaxed)) return;

    if (ev.isReset) {
        Detector::Instance().Reset();
        Platform::PasswordGuard::Instance().InvalidateCache();
        return;
    }

    HWND fg = GetForegroundWindow();
    if (!fg) return;

    if (!ev.isDown) {
        return;
    }

    // Never auto-replace across focus-changing Tab presses
    if (ev.vkCode == VK_TAB) {
        Detector::Instance().Reset();
        Platform::PasswordGuard::Instance().InvalidateCache();
        return;
    }

    // Check Password Guard
    if (Platform::PasswordGuard::Instance().CheckAndUpdateFocus(fg)) {
        if (ev.vkCode == VK_RETURN) {
            Platform::PasswordGuard::Instance().InvalidateCache();
        }
        return;
    }

    auto& cfgMgr = Config::ConfigManager::Instance();
    std::wstring procName = Platform::GetProcessNameByHwnd(fg);
    bool isExcluded = cfgMgr.IsProcessExcluded(procName);

    DWORD threadId = 0;
    Platform::ResolveFocusedWindow(fg, &threadId);
    HKL currentHkl = threadId ? GetKeyboardLayout(threadId) : GetKeyboardLayout(0);
    WORD currentLang = LOWORD(static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(currentHkl)));
    if (onLayoutChanged_) {
        onLayoutChanged_(currentLang);
    }

    // Handle Backspace
    if (ev.vkCode == VK_BACK) {
        Detector::Instance().OnBackspace();
        return;
    }

    // Handle Space and Enter delimiters
    wchar_t delimChar = 0;
    if (ev.vkCode == VK_SPACE) delimChar = L' ';
    else if (ev.vkCode == VK_RETURN) delimChar = L'\n';

    if (delimChar != 0) {
        if (ev.vkCode == VK_RETURN) {
            Platform::PasswordGuard::Instance().InvalidateCache();
            if (ev.isShift) {
                // Shift+Enter inserts a soft newline in chat apps; do not replace with plain VK_RETURN
                Detector::Instance().EvaluateWord(delimChar);
                return;
            }
        }
        if (cfgMgr.AutoSwitch() && !isExcluded) {
            DetectedSwitch sw = Detector::Instance().EvaluateWord(delimChar);
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
            }
        } else {
            // Still record lastWord_ on delimiter so explicit 2x Caps Lock works even with auto-switch off or excluded
            Detector::Instance().EvaluateWord(delimChar);
        }
        return;
    }

    // Convert virtual key to Unicode character using captured modifier state (without altering dead-key state)
    BYTE keyState[256] = { 0 };
    if (ev.isShift) {
        keyState[VK_SHIFT] = 0x80;
    }
    if (ev.isCapsLock) {
        keyState[VK_CAPITAL] = 0x01;
    }

    wchar_t unicodeBuf[4] = { 0 };
    int count = ToUnicodeEx(ev.vkCode, ev.scanCode, keyState, unicodeBuf, 4, 0x0004, currentHkl);

    if (count == 1) {
        wchar_t ch = unicodeBuf[0];
        if (ch >= 32) {
            Detector::Instance().OnChar(ch);
        }
    }
}

} // namespace Core
