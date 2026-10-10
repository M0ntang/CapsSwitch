#include "core/layout_engine.hpp"
#include "core/detector.hpp"
#include <unordered_map>
#include <vector>

namespace Core {

std::atomic<bool> LayoutEngine::s_simulatingInput(false);

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
    int count = GetKeyboardLayoutList(0, nullptr);
    if (count > 0) {
        std::vector<HKL> list(count);
        GetKeyboardLayoutList(count, list.data());
        for (HKL hkl : list) {
            WORD lang = LOWORD(reinterpret_cast<DWORD_PTR>(hkl));
            if ((lang & 0xFF) == 0x19) {
                hklRu_ = hkl;
            } else if ((lang & 0xFF) == 0x09) {
                hklEn_ = hkl;
            }
        }
    }
    if (!hklRu_) hklRu_ = LoadKeyboardLayoutW(L"00000419", KLF_NOTELLSHELL | KLF_SUBSTITUTE_OK);
    if (!hklEn_) hklEn_ = LoadKeyboardLayoutW(L"00000409", KLF_NOTELLSHELL | KLF_SUBSTITUTE_OK);
}

bool LayoutEngine::IsSimulatingInput() {
    return s_simulatingInput.load(std::memory_order_relaxed);
}

WORD LayoutEngine::GetLayout(HWND hwnd) {
    if (!hwnd) hwnd = GetForegroundWindow();
    if (!hwnd) return LANG_ID_EN;

    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus) {
        threadId = GetWindowThreadProcessId(gti.hwndFocus, nullptr);
    }

    HKL currentHkl = GetKeyboardLayout(threadId);
    return LOWORD(reinterpret_cast<DWORD_PTR>(currentHkl));
}

bool LayoutEngine::SwitchLayout(HWND hwnd, WORD targetLang) {
    if (!hwnd) hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    DWORD targetThreadId = GetWindowThreadProcessId(hwnd, nullptr);
    DWORD curThreadId = GetCurrentThreadId();

    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    HWND focusWnd = hwnd;
    if (GetGUIThreadInfo(targetThreadId, &gti) && gti.hwndFocus) {
        focusWnd = gti.hwndFocus;
        targetThreadId = GetWindowThreadProcessId(focusWnd, nullptr);
    }

    HKL targetHkl = (targetLang == LANG_ID_RU) ? hklRu_ : hklEn_;
    if (!targetHkl) return false;

    // 1. PostMessage to focused window and top-level window with wParam = 0
    // (wParam = 0 specifies that lParam holds the requested HKL, avoiding unwanted circular forward cycling)
    PostMessageW(focusWnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(targetHkl));
    if (focusWnd != hwnd) {
        PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(targetHkl));
    }

    // 2. Also send synchronous SendMessageTimeoutW to ensure immediate dispatch in standard Win32 apps
    DWORD_PTR dwResult = 0;
    SendMessageTimeoutW(focusWnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(targetHkl),
                        SMTO_ABORTIFHUNG | SMTO_NORMAL, 30, &dwResult);

    // 3. AttachThreadInput and ActivateKeyboardLayout
    if (targetThreadId != curThreadId) {
        AttachThreadInput(curThreadId, targetThreadId, TRUE);
        ActivateKeyboardLayout(targetHkl, KLF_SETFORPROCESS);
        AttachThreadInput(curThreadId, targetThreadId, FALSE);
    } else {
        ActivateKeyboardLayout(targetHkl, KLF_SETFORPROCESS);
    }

    // 4. Verification: check if target thread layout switched
    HKL checkHkl = GetKeyboardLayout(targetThreadId);
    WORD currentLang = LOWORD(reinterpret_cast<DWORD_PTR>(checkHkl));

    bool isMatch = (((targetLang & 0xFF) == 0x19 && (currentLang & 0xFF) == 0x19) ||
                    ((targetLang & 0xFF) == 0x09 && (currentLang & 0xFF) == 0x09));

    // Fallback: if after 20ms the layout still did not switch (e.g. console, UWP, or elevated app),
    // simulate system layout hotkey (Alt+Shift)
    if (!isMatch) {
        Sleep(20);
        checkHkl = GetKeyboardLayout(targetThreadId);
        currentLang = LOWORD(reinterpret_cast<DWORD_PTR>(checkHkl));
        isMatch = (((targetLang & 0xFF) == 0x19 && (currentLang & 0xFF) == 0x19) ||
                   ((targetLang & 0xFF) == 0x09 && (currentLang & 0xFF) == 0x09));

        if (!isMatch) {
            s_simulatingInput.store(true, std::memory_order_release);
            INPUT inputs[4] = {};
            inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_MENU;
            inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = VK_SHIFT;
            inputs[2].type = INPUT_KEYBOARD; inputs[2].ki.wVk = VK_SHIFT; inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
            inputs[3].type = INPUT_KEYBOARD; inputs[3].ki.wVk = VK_MENU; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(4, inputs, sizeof(INPUT));
            s_simulatingInput.store(false, std::memory_order_release);
        }
    }

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
    for (wchar_t ch : text) {
        if ((ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё') {
            cyrillicCount++;
        } else if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                   ch == L'[' || ch == L']' || ch == L';' || ch == L'\'' || ch == L',' || ch == L'.' || ch == L'`') {
            latinCount++;
        }
    }

    if (cyrillicCount > latinCount) {
        return RuToEn(text);
    } else {
        return EnToRu(text);
    }
}

std::wstring LayoutEngine::GetClipboardText() {
    std::wstring text;
    if (!OpenClipboard(nullptr)) return text;
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
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(wchar_t));
    if (hGlob) {
        wchar_t* pBuf = static_cast<wchar_t*>(GlobalLock(hGlob));
        if (pBuf) {
            wcscpy_s(pBuf, text.length() + 1, text.c_str());
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
    }
    CloseClipboard();
    return true;
}

std::wstring LayoutEngine::CopySelectedText(HWND /*hwnd*/) {
    std::wstring originalClipboard = GetClipboardText();

    // Temporarily clear clipboard to detect if Ctrl+C placed new text
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        CloseClipboard();
    }

    // Send Ctrl+C
    s_simulatingInput.store(true, std::memory_order_release);
    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'C';
    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'C'; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
    s_simulatingInput.store(false, std::memory_order_release);

    Sleep(45);

    std::wstring selectedText = GetClipboardText();

    if (selectedText.empty()) {
        // Nothing was selected; restore original clipboard
        if (!originalClipboard.empty()) {
            SetClipboardText(originalClipboard);
        }
    }

    return selectedText;
}

void LayoutEngine::PasteText(HWND /*hwnd*/, const std::wstring& text, const std::wstring& restoreClipboard) {
    SetClipboardText(text);

    s_simulatingInput.store(true, std::memory_order_release);
    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'V';
    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'V'; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
    s_simulatingInput.store(false, std::memory_order_release);

    Sleep(45);

    // Restore user's clipboard history
    if (!restoreClipboard.empty()) {
        SetClipboardText(restoreClipboard);
    }
}

void LayoutEngine::ConvertSelectionOrLastWord() {
    HWND fg = GetForegroundWindow();
    if (!fg) return;

    std::wstring originalClipboard = GetClipboardText();

    // 1. Check if user has selected text
    std::wstring selected = CopySelectedText(fg);
    if (!selected.empty()) {
        std::wstring converted = ConvertTextSmart(selected);
        WORD targetLang = DetectLanguageOfText(converted);

        PasteText(fg, converted, originalClipboard);
        SwitchLayout(fg, targetLang);
        Detector::Instance().SetLastWord(converted, 0);
        return;
    }

    // 2. If no selection, convert last typed word
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

    // 3. Fallback: select word to the left via Ctrl+Shift+Left, then convert
    s_simulatingInput.store(true, std::memory_order_release);
    INPUT selWord[6] = {};
    selWord[0].type = INPUT_KEYBOARD; selWord[0].ki.wVk = VK_CONTROL;
    selWord[1].type = INPUT_KEYBOARD; selWord[1].ki.wVk = VK_SHIFT;
    selWord[2].type = INPUT_KEYBOARD; selWord[2].ki.wVk = VK_LEFT;
    selWord[3].type = INPUT_KEYBOARD; selWord[3].ki.wVk = VK_LEFT; selWord[3].ki.dwFlags = KEYEVENTF_KEYUP;
    selWord[4].type = INPUT_KEYBOARD; selWord[4].ki.wVk = VK_SHIFT; selWord[4].ki.dwFlags = KEYEVENTF_KEYUP;
    selWord[5].type = INPUT_KEYBOARD; selWord[5].ki.wVk = VK_CONTROL; selWord[5].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(6, selWord, sizeof(INPUT));
    s_simulatingInput.store(false, std::memory_order_release);

    Sleep(40);

    selected = CopySelectedText(fg);
    if (!selected.empty()) {
        std::wstring converted = ConvertTextSmart(selected);
        WORD targetLang = DetectLanguageOfText(converted);

        PasteText(fg, converted, originalClipboard);
        SwitchLayout(fg, targetLang);
        Detector::Instance().SetLastWord(converted, 0);
    }
}

void LayoutEngine::SimulateReplacement(size_t eraseCount, const std::wstring& newText) {
    s_simulatingInput.store(true, std::memory_order_release);

    std::vector<INPUT> inputs;
    inputs.reserve(eraseCount * 2 + newText.length() * 2);

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

    // 2. Type replacement characters via Unicode events
    for (wchar_t ch : newText) {
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

    if (!inputs.empty()) {
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }

    s_simulatingInput.store(false, std::memory_order_release);
}

} // namespace Core
