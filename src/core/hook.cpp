#include "core/hook.hpp"
#include "core/layout_engine.hpp"
#include "core/detector.hpp"
#include "config/config.hpp"
#include "platform/password_guard.hpp"
#include "platform/win32_utils.hpp"

namespace Core {

HHOOK HookManager::s_hKbdHook = nullptr;
HHOOK HookManager::s_hMouseHook = nullptr;
HWINEVENTHOOK HookManager::s_hWinEventHook = nullptr;
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
        pendingSwitchLang_.store(0, std::memory_order_relaxed);
        pendingSwitchHwnd_.store(nullptr, std::memory_order_relaxed);
        doubleCapsPending_.store(false, std::memory_order_relaxed);
        foregroundChangedPending_.store(false, std::memory_order_relaxed);
        pendingForegroundHwnd_.store(nullptr, std::memory_order_relaxed);
        capsHoldHwnd_ = nullptr;
        capsState_.Reset();
        PushResetEvent(true);
        Detector::Instance().Reset();
    } else {
        pendingForegroundHwnd_.store(nullptr, std::memory_order_relaxed);
        resetForegroundTracking_.store(true, std::memory_order_relaxed);
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

    // Install WinEvent hook to detect foreground window switches for per-app layout rules
    s_hWinEventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    // Seed initial foreground window state if it belongs to another process
    HWND fg = GetForegroundWindow();
    if (fg && IsWindow(fg)) {
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        if (pid != 0 && pid != GetCurrentProcessId()) {
            OnForegroundChanged(fg);
        }
    }

    return (s_hKbdHook != nullptr);
}

void HookManager::Stop() {
    if (!isRunning_.load()) return;

    if (s_hWinEventHook) {
        UnhookWinEvent(s_hWinEventHook);
        s_hWinEventHook = nullptr;
    }

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

void HookManager::PostLayoutSwitch(WORD targetLang, HWND targetHwnd) {
    pendingSwitchHwnd_.store(targetHwnd, std::memory_order_release);
    pendingSwitchLang_.store(targetLang, std::memory_order_release);
    SetEvent(hKeyEvent_);
}

void HookManager::PostDoubleCapsAction() {
    // If a double-click occurs, cancel any pending layout switch and trigger convert
    pendingSwitchLang_.store(0, std::memory_order_release);
    doubleCapsPending_.store(true, std::memory_order_release);
    SetEvent(hKeyEvent_);
}

void HookManager::OnForegroundChanged(HWND hwnd) {
    PushResetEvent(true);
    pendingForegroundHwnd_.store(hwnd, std::memory_order_release);
    foregroundChangedPending_.store(true, std::memory_order_release);
    SetEvent(hKeyEvent_);
}

void CALLBACK HookManager::WinEventProc(HWINEVENTHOOK /*hWinEventHook*/, DWORD event, HWND hwnd,
                                        LONG /*idObject*/, LONG /*idChild*/,
                                        DWORD /*dwEventThread*/, DWORD /*dwmsEventTime*/) {
    if (event != EVENT_SYSTEM_FOREGROUND || !hwnd || !IsWindow(hwnd)) {
        return;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return;
    }
    if (s_instance && !s_isPaused.load(std::memory_order_relaxed)) {
        s_instance->OnForegroundChanged(hwnd);
    }
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

    bool isPaused = s_isPaused.load(std::memory_order_relaxed);
    auto& cfgMgr = Config::ConfigManager::Instance();
    CapsConfig capsCfg{ cfgMgr.CapsLockSwitch(), cfgMgr.CapsHoldSwitch(), cfgMgr.DoubleCapsConvert() };

    // 2. Caps Lock Interception (Win+Caps toggles real Caps Lock; bare or Shift+Caps switches layout)
    if (pKbd->vkCode == VK_CAPITAL && s_instance) {
        uint64_t nowMs = Platform::GetCurrentTimeMicroseconds() / 1000;
        if (isDown) {
            HWND fg = GetForegroundWindow();
            WORD curLang = (!isPaused && !isWin && !isCtrl && !isAlt && (capsCfg.capsLockSwitch || capsCfg.capsHoldSwitch))
                               ? LayoutEngine::Instance().GetLayout(fg)
                               : LANG_ID_EN;
            CapsAction act = s_instance->capsState_.OnCapsDown(nowMs, isShift, isCtrl, isAlt, isWin, isPaused, curLang, capsCfg);
            if (act.resetBuffer) {
                s_instance->PushResetEvent(true);
            }
            if (act.triggerDoubleCaps) {
                s_instance->PostDoubleCapsAction();
            } else if (act.switchLayout && act.targetLang != 0) {
                s_instance->capsHoldHwnd_ = fg;
                LayoutEngine::Instance().QuickRequestSwitchLayout(fg, act.targetLang);
                s_instance->PostLayoutSwitch(act.targetLang, fg);
            }
            if (act.suppressKey) {
                return 1;
            }
            return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
        }
        if (isUp) {
            CapsAction act = s_instance->capsState_.OnCapsUp(nowMs, isPaused, capsCfg);
            if (act.resetBuffer) {
                s_instance->PushResetEvent(true);
            }
            if (act.switchLayout && act.targetLang != 0) {
                HWND fg = GetForegroundWindow();
                HWND targetWnd = (s_instance->capsHoldHwnd_ && fg != s_instance->capsHoldHwnd_ && IsWindow(s_instance->capsHoldHwnd_))
                                     ? s_instance->capsHoldHwnd_
                                     : fg;
                LayoutEngine::Instance().QuickRequestSwitchLayout(targetWnd, act.targetLang);
                if (targetWnd == fg) {
                    s_instance->PostLayoutSwitch(act.targetLang, fg);
                }
                s_instance->capsHoldHwnd_ = nullptr;
            }
            if (act.suppressKey) {
                return 1;
            }
            return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
        }
    }

    // Track keys typed while Caps Lock is held (digits auto-emit shifted symbol of switched layout)
    if (isDown && s_instance) {
        CapsAction otherAct = s_instance->capsState_.OnOtherKeyDown(
            pKbd->vkCode, isShift, isCtrl, isAlt, isWin, isPaused, capsCfg);
        if (otherAct.resetBuffer) {
            s_instance->PushResetEvent(true);
        }
        if (otherAct.emitShiftedDigit && otherAct.targetLang != 0) {
            wchar_t ch = LayoutEngine::Instance().GetShiftedDigitChar(
                static_cast<WORD>(pKbd->vkCode), pKbd->scanCode, otherAct.targetLang);
            if (ch >= 32) {
                LayoutEngine::Instance().SendUnicodeChar(ch);
            }
        }
        if (otherAct.suppressKey) {
            return 1;
        }
        if (otherAct.skipDetectorForKey) {
            return CallNextHookEx(s_hKbdHook, nCode, wParam, lParam);
        }
    }
    if (isUp && s_instance && s_instance->capsState_.ShouldSuppressKeyUp(pKbd->vkCode)) {
        return 1;
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

        // Apply per-app automatic layout rule on foreground window change
        if (foregroundChangedPending_.exchange(false, std::memory_order_acquire)) {
            HWND fgHwnd = pendingForegroundHwnd_.load(std::memory_order_acquire);
            ProcessForegroundChange(fgHwnd);
        }

        // Process pending Double-Caps Action (translate selection or last word)
        if (doubleCapsPending_.exchange(false, std::memory_order_acquire)) {
            LayoutEngine::Instance().ConvertSelectionOrLastWord();
            HWND fg = GetForegroundWindow();
            if (fg && onLayoutChanged_) {
                onLayoutChanged_(LayoutEngine::Instance().GetLayout(fg));
            }
        }

        // Process pending Caps Lock layout switch
        WORD targetLang = pendingSwitchLang_.exchange(0, std::memory_order_acquire);
        if (targetLang != 0) {
            HWND targetWnd = pendingSwitchHwnd_.load(std::memory_order_acquire);
            HWND fg = GetForegroundWindow();
            HWND useWnd = (targetWnd && targetWnd == fg) ? targetWnd : fg;
            if (useWnd) {
                LayoutEngine::Instance().SwitchLayout(useWnd, targetLang);
                WORD newLang = LayoutEngine::Instance().GetLayout(useWnd);
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

void HookManager::ProcessForegroundChange(HWND hwnd) {
    if (!isRunning_.load(std::memory_order_relaxed) || isPaused_.load(std::memory_order_relaxed)) {
        return;
    }

    if (resetForegroundTracking_.exchange(false, std::memory_order_relaxed)) {
        lastForegroundProcess_.clear();
    }

    HWND fg = GetForegroundWindow();
    if (!fg) fg = hwnd;
    if (!fg || !IsWindow(fg)) return;

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) return;

    std::wstring procName = Platform::GetProcessNameByHwnd(fg);
    if (procName.empty()) return;

    auto& cfgMgr = Config::ConfigManager::Instance();
    uint64_t rulesVer = cfgMgr.GetAppLayoutsVersion();
    bool sameProcess = (procName == lastForegroundProcess_);
    bool rulesChanged = (rulesVer != lastAppliedRulesVersion_);

    lastForegroundProcess_ = procName;
    lastAppliedRulesVersion_ = rulesVer;

    auto notifyCurrentLayout = [this](HWND targetHwnd) {
        if (onLayoutChanged_) {
            onLayoutChanged_(LayoutEngine::Instance().GetLayout(targetHwnd));
        }
    };

    if (sameProcess && !rulesChanged) {
        notifyCurrentLayout(fg);
        return;
    }

    if (!cfgMgr.HasAppLayoutRules()) {
        notifyCurrentLayout(fg);
        return;
    }

    Config::AppLayout rule = cfgMgr.GetProcessLayout(procName);
    if (rule == Config::AppLayout::None) {
        notifyCurrentLayout(fg);
        return;
    }

    // PasswordGuard always takes precedence: never auto-switch a password field to Russian
    if (rule == Config::AppLayout::Russian && cfgMgr.PasswordGuardEnabled()) {
        if (Platform::PasswordGuard::Instance().CheckAndUpdateFocus(fg)) {
            notifyCurrentLayout(fg);
            return;
        }
    }

    WORD targetLang = (rule == Config::AppLayout::Russian) ? LANG_ID_RU : LANG_ID_EN;
    WORD currentLang = LayoutEngine::Instance().GetLayout(fg);
    if ((currentLang & 0xFF) != (targetLang & 0xFF)) {
        // Briefly wait if Alt or Win is still held (e.g. tail end of Alt+Tab)
        for (int i = 0; i < 8; ++i) {
            bool modDown = ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
            if (!modDown) break;
            if (!isRunning_.load(std::memory_order_relaxed) || isPaused_.load(std::memory_order_relaxed)) {
                return;
            }
            HWND curFg = GetForegroundWindow();
            if (curFg && curFg != fg) {
                return;
            }
            Sleep(10);
        }

        HWND liveFg = GetForegroundWindow();
        if (liveFg && IsWindow(liveFg)) {
            if (Platform::GetProcessNameByHwnd(liveFg) != procName) {
                return;
            }
            fg = liveFg;
        }

        LayoutEngine::Instance().SwitchLayout(fg, targetLang);
    }

    notifyCurrentLayout(fg);
}

} // namespace Core
