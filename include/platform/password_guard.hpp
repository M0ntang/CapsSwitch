#pragma once

#include <windows.h>
#include <uiautomation.h>
#include <atomic>

namespace Platform {

class PasswordGuard {
public:
    static PasswordGuard& Instance();

    bool Initialize();
    void Shutdown();

    // Check if currently focused control is a password field
    bool CheckAndUpdateFocus(HWND foregroundHwnd);
    void InvalidateCache();
    bool IsPasswordFocused() const { return isPasswordFocused_.load(std::memory_order_relaxed); }

private:
    PasswordGuard();
    ~PasswordGuard();

    bool CheckWin32Password(HWND hFocus, bool& outKnownNonPassword);
    bool CheckUiaPassword();

    IUIAutomation* pAutomation_ = nullptr;
    std::atomic<bool> isPasswordFocused_{ false };
    std::atomic<bool> cacheInvalidated_{ true };
    bool wasPasswordFocused_ = false;
    bool comInitialized_ = false;
    HWND lastCheckedHwnd_ = nullptr;
    HWND lastPasswordHwnd_ = nullptr;
    DWORD lastCheckTick_ = 0;
    bool cachedIsPassword_ = false;
};

} // namespace Platform
