#include "platform/password_guard.hpp"
#include "platform/win32_utils.hpp"
#include "core/layout_engine.hpp"
#include "core/detector.hpp"
#include "config/config.hpp"

namespace Platform {

static const GUID s_CLSID_CUIAutomation = { 0xff48dba4, 0x60ef, 0x4201, { 0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e } };
static const GUID s_IID_IUIAutomation = { 0x30cbe57d, 0xd9d0, 0x452a, { 0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee } };

PasswordGuard& PasswordGuard::Instance() {
    static PasswordGuard s_instance;
    return s_instance;
}

PasswordGuard::PasswordGuard() {}

PasswordGuard::~PasswordGuard() {
    Shutdown();
}

bool PasswordGuard::Initialize() {
    if (pAutomation_) return true;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        comInitialized_ = true;
    }
    // If hr == RPC_E_CHANGED_MODE, COM is already initialized on this thread in another apartment;
    // do not call CoUninitialize() for it, so leave comInitialized_ = false.

    hr = CoCreateInstance(
        s_CLSID_CUIAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        s_IID_IUIAutomation,
        reinterpret_cast<void**>(&pAutomation_)
    );

    return SUCCEEDED(hr) && pAutomation_ != nullptr;
}

void PasswordGuard::Shutdown() {
    if (pAutomation_) {
        pAutomation_->Release();
        pAutomation_ = nullptr;
    }
    if (comInitialized_) {
        CoUninitialize();
        comInitialized_ = false;
    }
}

void PasswordGuard::InvalidateCache() {
    cacheInvalidated_.store(true, std::memory_order_relaxed);
}

bool PasswordGuard::CheckWin32Password(HWND hFocus, bool& outKnownNonPassword) {
    outKnownNonPassword = false;
    if (!hFocus) return false;

    wchar_t className[64] = { 0 };
    if (GetClassNameW(hFocus, className, 64) <= 0) {
        return false;
    }

    if (_wcsicmp(className, L"Edit") == 0 ||
        _wcsicmp(className, L"RichEdit") == 0 ||
        _wcsicmp(className, L"RichEdit20W") == 0 ||
        _wcsicmp(className, L"RichEdit50W") == 0) {

        LONG_PTR style = GetWindowLongPtrW(hFocus, GWL_STYLE);
        if (style & ES_PASSWORD) {
            return true;
        }

        DWORD_PTR pwdChar = 0;
        if (SendMessageTimeoutW(hFocus, EM_GETPASSWORDCHAR, 0, 0,
                                SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &pwdChar) != 0 && pwdChar != 0) {
            return true;
        }

        // Standard Win32 text edit without ES_PASSWORD or password char -> skip slow cross-process UIA
        outKnownNonPassword = true;
        return false;
    }

    if (_wcsicmp(className, L"Scintilla") == 0 ||
        _wcsicmp(className, L"ConsoleWindowClass") == 0) {
        outKnownNonPassword = true;
    }

    return false;
}

bool PasswordGuard::CheckUiaPassword() {
    if (!pAutomation_) return false;

    IUIAutomationElement* pFocused = nullptr;
    HRESULT hr = pAutomation_->GetFocusedElement(&pFocused);
    if (FAILED(hr) || !pFocused) {
        return false;
    }

    VARIANT var;
    VariantInit(&var);
    BOOL isPassword = FALSE;

    hr = pFocused->GetCurrentPropertyValue(UIA_IsPasswordPropertyId, &var);
    if (SUCCEEDED(hr) && var.vt == VT_BOOL && var.boolVal == VARIANT_TRUE) {
        isPassword = TRUE;
    }

    VariantClear(&var);
    pFocused->Release();

    return (isPassword == TRUE);
}

bool PasswordGuard::CheckAndUpdateFocus(HWND foregroundHwnd) {
    if (!Config::ConfigManager::Instance().PasswordGuardEnabled()) {
        isPasswordFocused_.store(false, std::memory_order_relaxed);
        wasPasswordFocused_ = false;
        lastPasswordHwnd_ = nullptr;
        lastCheckedHwnd_ = nullptr;
        cacheInvalidated_.store(true, std::memory_order_relaxed);
        return false;
    }

    HWND hFocus = Platform::ResolveFocusedWindow(foregroundHwnd);
    if (!hFocus) hFocus = foregroundHwnd;

    DWORD nowTick = GetTickCount();
    bool isPassword = false;
    bool invalidated = cacheInvalidated_.exchange(false, std::memory_order_relaxed);
    bool midWord = !Core::Detector::Instance().GetCurrentBuffer().empty();

    // Reuse cached focus check when focus HWND is unchanged (and mid-word or within 1500ms)
    if (!invalidated && hFocus == lastCheckedHwnd_ && (midWord || (nowTick - lastCheckTick_ < 1500))) {
        isPassword = cachedIsPassword_;
    } else {
        lastCheckedHwnd_ = hFocus;
        lastCheckTick_ = nowTick;

        bool knownNonPwd = false;
        if (CheckWin32Password(hFocus, knownNonPwd)) {
            isPassword = true;
        } else if (!knownNonPwd && CheckUiaPassword()) {
            isPassword = true;
        }

        cachedIsPassword_ = isPassword;
    }

    isPasswordFocused_.store(isPassword, std::memory_order_relaxed);

    if (isPassword) {
        Core::Detector::Instance().Reset();

        if (hFocus != lastPasswordHwnd_) {
            lastPasswordHwnd_ = hFocus;
            wasPasswordFocused_ = true;
            Core::LayoutEngine::Instance().SwitchLayout(foregroundHwnd, Core::LANG_ID_EN);
        }
    } else {
        lastPasswordHwnd_ = nullptr;
        wasPasswordFocused_ = false;
    }

    return isPassword;
}

} // namespace Platform
