#include "core/layout_engine.hpp"
#include "core/detector.hpp"
#include "platform/win32_utils.hpp"
#include <msctf.h>
#include <unordered_map>
#include <vector>

namespace Core {

std::atomic<bool> LayoutEngine::s_simulatingInput(false);

namespace {

struct SimulatingInputGuard {
    explicit SimulatingInputGuard(std::atomic<bool>& flag) : flag_(flag) {
        flag_.store(true, std::memory_order_release);
    }
    ~SimulatingInputGuard() {
        flag_.store(false, std::memory_order_release);
    }
    std::atomic<bool>& flag_;
};

bool OpenClipboardWithRetry(int maxAttempts = 5) {
    for (int i = 0; i < maxAttempts; ++i) {
        if (OpenClipboard(nullptr)) {
            return true;
        }
        Sleep(5);
    }
    return false;
}

void AppendKeyReleaseIfDown(std::vector<INPUT>& inputs, WORD vk, WORD scan = 0) {
    if ((GetAsyncKeyState(vk) & 0x8000) != 0) {
        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = vk;
        up.ki.wScan = scan;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
}

void SendCtrlShortcut(std::atomic<bool>& simulatingFlag, WORD vkChar, WORD scanChar) {
    SimulatingInputGuard guard(simulatingFlag);
    std::vector<INPUT> inputs;
    inputs.reserve(8);
    AppendKeyReleaseIfDown(inputs, VK_LSHIFT, 0x2A);
    AppendKeyReleaseIfDown(inputs, VK_RSHIFT, 0x36);
    AppendKeyReleaseIfDown(inputs, VK_LMENU, 0x38);
    AppendKeyReleaseIfDown(inputs, VK_RMENU, 0x38);

    INPUT d1 = {}; d1.type = INPUT_KEYBOARD; d1.ki.wVk = VK_LCONTROL; d1.ki.wScan = 0x1D;
    INPUT d2 = {}; d2.type = INPUT_KEYBOARD; d2.ki.wVk = vkChar;      d2.ki.wScan = scanChar;
    INPUT u1 = {}; u1.type = INPUT_KEYBOARD; u1.ki.wVk = vkChar;      u1.ki.wScan = scanChar; u1.ki.dwFlags = KEYEVENTF_KEYUP;
    INPUT u2 = {}; u2.type = INPUT_KEYBOARD; u2.ki.wVk = VK_LCONTROL; u2.ki.wScan = 0x1D;     u2.ki.dwFlags = KEYEVENTF_KEYUP;
    inputs.push_back(d1);
    inputs.push_back(d2);
    inputs.push_back(u1);
    inputs.push_back(u2);

    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

WORD ExtractLangId(HKL hkl) {
    return LOWORD(static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(hkl)));
}

WORD ExtractLayoutId(HKL hkl) {
    return HIWORD(static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(hkl)));
}

bool IsHklMatch(HKL currentHkl, HKL targetHkl, WORD targetLang) {
    if (currentHkl == targetHkl) return true;
    WORD curLang = ExtractLangId(currentHkl);
    WORD curLayout = ExtractLayoutId(currentHkl);
    WORD targetLayout = ExtractLayoutId(targetHkl);
    return ((curLang & 0xFF) == (targetLang & 0xFF)) && (curLayout == targetLayout);
}

} // namespace

static const std::unordered_map<wchar_t, wchar_t> s_enToRuMap = {
    // Lowercase
    {L'q', L'й'}, {L'w', L'ц'}, {L'e', L'у'}, {L'r', L'к'}, {L't', L'е'},
    {L'y', L'н'}, {L'u', L'г'}, {L'i', L'ш'}, {L'o', L'щ'}, {L'p', L'з'},
    {L'[', L'х'}, {L']', L'ъ'}, {L'a', L'ф'}, {L's', L'ы'}, {L'd', L'в'},
    {L'f', L'а'}, {L'g', L'п'}, {L'h', L'р'}, {L'j', L'о'}, {L'k', L'л'},
    {L'l', L'д'}, {L';', L'ж'}, {L'\'', L'э'}, {L'z', L'я'}, {L'x', L'ч'},
    {L'c', L'с'}, {L'v', L'м'}, {L'b', L'и'}, {L'n', L'т'}, {L'm', L'ь'},
    {L',', L'б'}, {L'.', L'ю'}, {L'`', L'ё'},

    // Uppercase
    {L'Q', L'Й'}, {L'W', L'Ц'}, {L'E', L'У'}, {L'R', L'К'}, {L'T', L'Е'},
    {L'Y', L'Н'}, {L'U', L'Г'}, {L'I', L'Ш'}, {L'O', L'Щ'}, {L'P', L'З'},
    {L'{', L'Х'}, {L'}', L'Ъ'}, {L'A', L'Ф'}, {L'S', L'Ы'}, {L'D', L'В'},
    {L'F', L'А'}, {L'G', L'П'}, {L'H', L'Р'}, {L'J', L'О'}, {L'K', L'Л'},
    {L'L', L'Д'}, {L':', L'Ж'}, {L'\"', L'Э'}, {L'Z', L'Я'}, {L'X', L'Ч'},
    {L'C', L'С'}, {L'V', L'М'}, {L'B', L'И'}, {L'N', L'Т'}, {L'M', L'Ь'},
    {L'<', L'Б'}, {L'>', L'Ю'}, {L'~', L'Ё'},

    // Symbols mapping
    {L'@', L'\"'}, {L'#', L'№'}, {L'$', L';'}, {L'^', L':'}, {L'&', L'?'},
    {L'/', L'.'}, {L'?', L','}
};

static const std::unordered_map<wchar_t, wchar_t> s_ruToEnMap = []() {
    std::unordered_map<wchar_t, wchar_t> rev;
    for (const auto& pair : s_enToRuMap) {
        rev[pair.second] = pair.first;
    }
    return rev;
}();

LayoutEngine& LayoutEngine::Instance() {
    static LayoutEngine s_instance;
    return s_instance;
}

LayoutEngine::LayoutEngine() {
    RefreshLayouts();
    hMsCtf_ = LoadLibraryW(L"msctf.dll");
}

LayoutEngine::~LayoutEngine() {
    ShutdownTsf();
    if (hMsCtf_) {
        FreeLibrary(hMsCtf_);
        hMsCtf_ = nullptr;
    }
}

void LayoutEngine::RefreshLayouts() {
    HKL bestRu = nullptr;
    HKL bestEn = nullptr;

    int count = GetKeyboardLayoutList(0, nullptr);
    if (count > 0) {
        std::vector<HKL> list(count);
        GetKeyboardLayoutList(count, list.data());
        for (HKL hkl : list) {
            WORD lang = ExtractLangId(hkl);
            WORD layoutId = ExtractLayoutId(hkl);
            if ((lang & 0xFF) == 0x19) {
                if (!bestRu || (lang == LANG_ID_RU && layoutId == LANG_ID_RU)) {
                    bestRu = hkl;
                }
            } else if ((lang & 0xFF) == 0x09) {
                // Prefer exact standard US QWERTY (0x04090409) over US-International (0xF0010409)
                if (lang == LANG_ID_EN && layoutId == LANG_ID_EN) {
                    bestEn = hkl;
                } else if (!bestEn) {
                    bestEn = hkl;
                }
            }
        }
    }

    hklRu_ = bestRu ? bestRu : LoadKeyboardLayoutW(L"00000419", KLF_NOTELLSHELL | KLF_SUBSTITUTE_OK);
    hklEn_ = bestEn ? bestEn : LoadKeyboardLayoutW(L"00000409", KLF_NOTELLSHELL | KLF_SUBSTITUTE_OK);
}

void LayoutEngine::EnsureTsf() {
    if (!pTsfProfiles_ && hMsCtf_) {
        typedef HRESULT (WINAPI *pfnTF_CreateInputProcessorProfiles)(ITfInputProcessorProfiles **ppipr);
        FARPROC rawProc = GetProcAddress(hMsCtf_, "TF_CreateInputProcessorProfiles");
        auto pCreate = reinterpret_cast<pfnTF_CreateInputProcessorProfiles>(reinterpret_cast<void*>(rawProc));
        if (pCreate) {
            pCreate(&pTsfProfiles_);
        }
    }
}

void LayoutEngine::ShutdownTsf() {
    if (pTsfProfiles_) {
        pTsfProfiles_->Release();
        pTsfProfiles_ = nullptr;
    }
}

void LayoutEngine::NotifyTsfAndTaskbar(WORD targetLang, HKL targetHkl) {
    EnsureTsf();
    if (pTsfProfiles_) {
        static const GUID kGuidNull = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
        pTsfProfiles_->ChangeCurrentLanguage(targetLang);
        pTsfProfiles_->ActivateLanguageProfile(kGuidNull, targetLang, kGuidNull);
    }

    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hTray) {
        PostMessageW(hTray, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(targetHkl));
    }
}

bool LayoutEngine::IsSimulatingInput() {
    return s_simulatingInput.load(std::memory_order_relaxed);
}

WORD LayoutEngine::GetLayout(HWND hwnd) {
    if (!hwnd) hwnd = GetForegroundWindow();
    if (!hwnd) return LANG_ID_EN;

    DWORD threadId = 0;
    Platform::ResolveFocusedWindow(hwnd, &threadId);
    HKL currentHkl = threadId ? GetKeyboardLayout(threadId) : GetKeyboardLayout(0);
    return ExtractLangId(currentHkl);
}

bool LayoutEngine::SwitchLayout(HWND hwnd, WORD targetLang) {
    if (!hklRu_ || !hklEn_) {
        RefreshLayouts();
    }

    if (!hwnd) hwnd = GetForegroundWindow();

    DWORD targetThreadId = 0;
    HWND focusWnd = hwnd ? Platform::ResolveFocusedWindow(hwnd, &targetThreadId) : nullptr;
    if (!focusWnd) focusWnd = hwnd;

    HKL targetHkl = ((targetLang & 0xFF) == 0x19) ? hklRu_ : hklEn_;
    if (!targetHkl) return false;

    HKL currentHkl = targetThreadId ? GetKeyboardLayout(targetThreadId) : GetKeyboardLayout(0);
    if (IsHklMatch(currentHkl, targetHkl, targetLang)) {
        return true;
    }

    // 1. Primary method: Post WM_INPUTLANGCHANGEREQUEST directly to the focused window.
    // This switches both the target thread layout and native Windows taskbar indicator without
    // interfering with physically held Space/Shift keys or AttachThreadInput stalls.
    bool switched = false;
    if (hwnd) {
        PostMessageW(focusWnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(targetHkl));
        if (focusWnd != hwnd) {
            PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(targetHkl));
        }

        for (int i = 0; i < 6; ++i) {
            Sleep(3);
            currentHkl = GetKeyboardLayout(targetThreadId);
            if (IsHklMatch(currentHkl, targetHkl, targetLang)) {
                switched = true;
                break;
            }
        }
    }

    // 2. Fallback: If window ignored WM_INPUTLANGCHANGEREQUEST (e.g., elevated/custom window),
    // simulate user's configured Windows toggle hotkey (1 = Alt+Shift, 2 = Ctrl+Shift; 3 = disabled).
    if (!switched && hwnd) {
        DWORD hotkeyType = 1;
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[16] = { 0 };
            DWORD valSize = sizeof(val);
            if (RegQueryValueExW(hKey, L"Hotkey", nullptr, nullptr, reinterpret_cast<LPBYTE>(val), &valSize) == ERROR_SUCCESS) {
                if (val[0] == L'2') hotkeyType = 2;
                else if (val[0] == L'3') hotkeyType = 3;
            }
            RegCloseKey(hKey);
        }

        if (hotkeyType == 1 || hotkeyType == 2) {
            auto simulateHotkey = [&](DWORD type) {
                SimulatingInputGuard guard(s_simulatingInput);
                std::vector<INPUT> inputs;
                inputs.reserve(8);

                // Release held Shift or Space first so Alt+Space system menu is never triggered
                AppendKeyReleaseIfDown(inputs, VK_LSHIFT, 0x2A);
                AppendKeyReleaseIfDown(inputs, VK_RSHIFT, 0x36);
                AppendKeyReleaseIfDown(inputs, VK_SPACE, 0x39);

                WORD modVk = (type == 2) ? VK_LCONTROL : VK_LMENU;
                WORD modScan = (type == 2) ? 0x1D : 0x38;

                INPUT d1 = {}; d1.type = INPUT_KEYBOARD; d1.ki.wVk = modVk;     d1.ki.wScan = modScan;
                INPUT d2 = {}; d2.type = INPUT_KEYBOARD; d2.ki.wVk = VK_LSHIFT; d2.ki.wScan = 0x2A;
                INPUT u1 = {}; u1.type = INPUT_KEYBOARD; u1.ki.wVk = VK_LSHIFT; u1.ki.wScan = 0x2A; u1.ki.dwFlags = KEYEVENTF_KEYUP;
                INPUT u2 = {}; u2.type = INPUT_KEYBOARD; u2.ki.wVk = modVk;     u2.ki.wScan = modScan; u2.ki.dwFlags = KEYEVENTF_KEYUP;

                inputs.push_back(d1);
                inputs.push_back(d2);
                inputs.push_back(u1);
                inputs.push_back(u2);

                SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
            };

            for (int attempt = 0; attempt < 2 && !switched; ++attempt) {
                simulateHotkey(hotkeyType);
                Sleep(15);
                currentHkl = GetKeyboardLayout(targetThreadId);
                if (IsHklMatch(currentHkl, targetHkl, targetLang)) {
                    switched = true;
                }
            }
        }
    }

    // 3. Synchronize TSF & taskbar indicator if needed, and keep calling thread consistent
    if (!switched) {
        NotifyTsfAndTaskbar(targetLang, targetHkl);
    }
    ActivateKeyboardLayout(targetHkl, KLF_SETFORPROCESS);

    return true;
}

bool LayoutEngine::ToggleLayout(HWND hwnd) {
    if (!hwnd) hwnd = GetForegroundWindow();
    WORD current = GetLayout(hwnd);
    bool isRu = ((current & 0xFF) == 0x19);
    WORD target = isRu ? LANG_ID_EN : LANG_ID_RU;
    return SwitchLayout(hwnd, target);
}

wchar_t LayoutEngine::CharEnToRu(wchar_t ch) {
    auto it = s_enToRuMap.find(ch);
    return (it != s_enToRuMap.end()) ? it->second : ch;
}

wchar_t LayoutEngine::CharRuToEn(wchar_t ch) {
    auto it = s_ruToEnMap.find(ch);
    return (it != s_ruToEnMap.end()) ? it->second : ch;
}

std::wstring LayoutEngine::EnToRu(const std::wstring& text) {
    std::wstring result = text;
    for (auto& ch : result) {
        ch = CharEnToRu(ch);
    }
    return result;
}

std::wstring LayoutEngine::RuToEn(const std::wstring& text) {
    std::wstring result = text;
    for (auto& ch : result) {
        ch = CharRuToEn(ch);
    }
    return result;
}

WORD LayoutEngine::DetectLanguageOfText(const std::wstring& text) {
    int cyrillicCount = 0;
    int latinCount = 0;
    for (wchar_t ch : text) {
        if ((ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё') {
            cyrillicCount++;
        } else if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z')) {
            latinCount++;
        }
    }
    if (cyrillicCount > latinCount) return LANG_ID_RU;
    if (latinCount > cyrillicCount) return LANG_ID_EN;
    return (cyrillicCount > 0) ? LANG_ID_RU : LANG_ID_EN;
}

std::wstring LayoutEngine::ConvertTextSmart(const std::wstring& text) {
    int cyrillicCount = 0;
    int latinCount = 0;
    int enLayoutPunctCount = 0;
    for (wchar_t ch : text) {
        if ((ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё') {
            cyrillicCount++;
        } else if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z')) {
            latinCount++;
        } else if (ch == L'[' || ch == L']' || ch == L'{' || ch == L'}' ||
                   ch == L';' || ch == L':' || ch == L'\'' || ch == L'\"' ||
                   ch == L',' || ch == L'<' || ch == L'.' || ch == L'>' ||
                   ch == L'`' || ch == L'~') {
            enLayoutPunctCount++;
        }
    }

    if (cyrillicCount > latinCount) {
        return RuToEn(text);
    }
    if (latinCount > cyrillicCount) {
        return EnToRu(text);
    }
    if (cyrillicCount == 0 && enLayoutPunctCount > 0) {
        return EnToRu(text);
    }
    return RuToEn(text);
}

std::wstring LayoutEngine::GetClipboardText() {
    std::wstring text;
    if (!OpenClipboardWithRetry()) return text;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pText) {
            text = pText;
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return text;
}

bool LayoutEngine::SetClipboardText(const std::wstring& text) {
    if (!OpenClipboardWithRetry()) return false;
    EmptyClipboard();
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(wchar_t));
    bool ok = false;
    if (hGlob) {
        wchar_t* pBuf = static_cast<wchar_t*>(GlobalLock(hGlob));
        if (pBuf) {
            wcscpy_s(pBuf, text.length() + 1, text.c_str());
            GlobalUnlock(hGlob);
            if (SetClipboardData(CF_UNICODETEXT, hGlob)) {
                ok = true;
            } else {
                GlobalFree(hGlob);
            }
        } else {
            GlobalFree(hGlob);
        }
    }
    CloseClipboard();
    return ok;
}

std::wstring LayoutEngine::CopySelectedText(HWND /*hwnd*/) {
    DWORD seqBefore = GetClipboardSequenceNumber();

    SendCtrlShortcut(s_simulatingInput, 'C', 0x2E);

    bool changed = false;
    for (int i = 0; i < 12; ++i) {
        Sleep(5);
        if (GetClipboardSequenceNumber() != seqBefore) {
            changed = true;
            break;
        }
    }

    if (!changed) {
        return L"";
    }
    return GetClipboardText();
}

void LayoutEngine::PasteText(HWND /*hwnd*/, const std::wstring& text, const std::wstring& restoreClipboard) {
    if (!SetClipboardText(text)) return;

    SendCtrlShortcut(s_simulatingInput, 'V', 0x2F);

    Sleep(45);

    if (!restoreClipboard.empty()) {
        SetClipboardText(restoreClipboard);
    }
}

void LayoutEngine::ConvertSelectionOrLastWord() {
    HWND fg = GetForegroundWindow();
    if (!fg) return;

    // 1. Check last typed / buffered word FIRST so we never send spurious Ctrl+C in IDEs or terminals
    std::wstring currentBuf = Detector::Instance().GetCurrentBuffer();
    std::wstring word = Detector::Instance().GetLastWordOrCurrent();
    wchar_t delim = Detector::Instance().GetLastDelimiter();

    if (!word.empty()) {
        std::wstring converted = ConvertTextSmart(word);
        WORD targetLang = DetectLanguageOfText(converted);

        size_t eraseCount = word.length();
        std::wstring replacement = converted;

        // If the word was completed with a delimiter (currentBuffer_ is empty, but lastWord is set)
        if (currentBuf.empty() && delim != 0) {
            eraseCount += 1;
            replacement += delim;
        }

        SimulateReplacement(eraseCount, replacement);
        SwitchLayout(fg, targetLang);
        Detector::Instance().SetLastWord(converted, (currentBuf.empty() ? delim : 0));
        return;
    }

    // 2. Buffer is empty: try translating currently selected text via clipboard (never in terminals!)
    std::wstring procName = Platform::GetProcessNameByHwnd(fg);
    if (Platform::IsConsoleOrTerminalProcess(procName)) {
        return;
    }

    std::wstring originalClipboard = GetClipboardText();
    std::wstring selected = CopySelectedText(fg);
    if (!selected.empty()) {
        std::wstring converted = ConvertTextSmart(selected);
        WORD targetLang = DetectLanguageOfText(converted);

        PasteText(fg, converted, originalClipboard);
        SwitchLayout(fg, targetLang);
        if (converted.length() <= 64 &&
            converted.find(L'\r') == std::wstring::npos &&
            converted.find(L'\n') == std::wstring::npos) {
            Detector::Instance().SetLastWord(converted, 0);
        } else {
            Detector::Instance().Reset();
        }
    }
}

void LayoutEngine::SimulateReplacement(size_t eraseCount, const std::wstring& newText) {
    SimulatingInputGuard guard(s_simulatingInput);

    std::vector<INPUT> inputs;
    inputs.reserve(4 + eraseCount * 2 + newText.length() * 2);

    // Release held Shift so Backspace/Return are not interpreted as Shift+Backspace / Shift+Enter
    AppendKeyReleaseIfDown(inputs, VK_LSHIFT, 0x2A);
    AppendKeyReleaseIfDown(inputs, VK_RSHIFT, 0x36);

    // 1. Simulate Backspace x eraseCount
    for (size_t i = 0; i < eraseCount; ++i) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = VK_BACK;
        inputs.push_back(down);

        INPUT up = {};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = VK_BACK;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    // 2. Type replacement characters via VK_RETURN (for '\n') or Unicode events
    for (wchar_t ch : newText) {
        if (ch == L'\r') {
            continue;
        }
        if (ch == L'\n') {
            INPUT down = {};
            down.type = INPUT_KEYBOARD;
            down.ki.wVk = VK_RETURN;
            inputs.push_back(down);

            INPUT up = {};
            up.type = INPUT_KEYBOARD;
            up.ki.wVk = VK_RETURN;
            up.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(up);
        } else {
            INPUT down = {};
            down.type = INPUT_KEYBOARD;
            down.ki.wScan = ch;
            down.ki.dwFlags = KEYEVENTF_UNICODE;
            inputs.push_back(down);

            INPUT up = {};
            up.type = INPUT_KEYBOARD;
            up.ki.wScan = ch;
            up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs.push_back(up);
        }
    }

    if (!inputs.empty()) {
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }
}

} // namespace Core
