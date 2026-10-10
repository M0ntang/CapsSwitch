#include "platform/password_guard.hpp"
#include "core/layout_engine.hpp"
#include "core/detector.hpp"
#include "config/config.hpp"

namespace Platform {

static const GUID s_CLSID_CUIAutomation = { 0xff48dba4, 0x60ef, 0x4201, { 0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e } };
static const GUID s_IID_IUIAutomation = { 0x30cbe57d, 0xd9d0, 0x452a, { 0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee } };

static HWND s_lastPasswordHwnd = nullptr;

PasswordGuard& PasswordGuard::Instance() {
    static PasswordGuard s_instance;
    return s_instance;
}

PasswordGuard::PasswordGuard() {
    Initialize();
}

PasswordGuard::~PasswordGuard() {
    Shutdown();
}

bool PasswordGuard::Initialize() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        comInitialized_ = true;
    }

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

bool PasswordGuard::CheckWin32Password(HWND hwnd, DWORD threadId) {
    if (!hwnd) return false;

    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus) {
        HWND hFocus = gti.hwndFocus;

        wchar_t className[64] = { 0 };
        GetClassNameW(hFocus, className, 64);

        // Only check edit-type controls to prevent false positives in custom/web windows
        if (_wcsicmp(className, L"Edit") == 0 ||
            _wcsicmp(className, L"RichEdit") == 0 ||
            _wcsicmp(className, L"RichEdit20W") == 0 ||
            _wcsicmp(className, L"RichEdit50W") == 0) {

            LONG_PTR style = GetWindowLongPtrW(hFocus, GWL_STYLE);
            if (style & ES_PASSWORD) {
                return true;
            }

            LRESULT pwdChar = SendMessageW(hFocus, EM_GETPASSWORDCHAR, 0, 0);
            if (pwdChar != 0) {
                return true;
            }
        }
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
    if (!Config::ConfigManager::Instance().Get().passwordGuard) {
        isPasswordFocused_.store(false, std::memory_order_relaxed);
        wasPasswordFocused_ = false;
        s_lastPasswordHwnd = nullptr;
        return false;
    }

    DWORD threadId = GetWindowThreadProcessId(foregroundHwnd, nullptr);
    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    HWND hFocus = foregroundHwnd;
    if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus) {
        hFocus = gti.hwndFocus;
    }

    bool isPassword = false;

    // 1. Precise Win32 check
    if (CheckWin32Password(foregroundHwnd, threadId)) {
        isPassword = true;
    }
    // 2. Microsoft UI Automation check for modern XAML / Chromium / Electron controls
    else if (CheckUiaPassword()) {
        isPassword = true;
    }

    isPasswordFocused_.store(isPassword, std::memory_order_relaxed);

    if (isPassword) {
        // Stop buffering immediately
        Core::Detector::Instance().Reset();

        // Enforce English layout ONCE upon initial focus transition into password control
        if (hFocus != s_lastPasswordHwnd) {
            s_lastPasswordHwnd = hFocus;
            wasPasswordFocused_ = true;
            Core::LayoutEngine::Instance().SwitchLayout(foregroundHwnd, Core::LANG_ID_EN);
        }
    } else {
        s_lastPasswordHwnd = nullptr;
        wasPasswordFocused_ = false;
    }

    return isPassword;
}

} // namespace Platform
