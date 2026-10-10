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
    bool IsPasswordFocused() const { return isPasswordFocused_.load(std::memory_order_relaxed); }

private:
    PasswordGuard();
    ~PasswordGuard();

    bool CheckWin32Password(HWND hwnd, DWORD threadId);
    bool CheckUiaPassword();

    IUIAutomation* pAutomation_ = nullptr;
    std::atomic<bool> isPasswordFocused_{ false };
    bool wasPasswordFocused_ = false;
    bool comInitialized_ = false;
};

} // namespace Platform
