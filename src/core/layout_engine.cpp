#include "core/layout_engine.hpp"
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
    hklRu_ = LoadKeyboardLayoutW(L"00000419", KLF_NOTELLSHELL | KLF_SUBSTITUTE_OK);
    hklEn_ = LoadKeyboardLayoutW(L"00000409", KLF_NOTELLSHELL | KLF_SUBSTITUTE_OK);
}

bool LayoutEngine::IsSimulatingInput() {
    return s_simulatingInput.load(std::memory_order_relaxed);
}

WORD LayoutEngine::GetLayout(HWND hwnd) {
    if (!hwnd) hwnd = GetForegroundWindow();
    if (!hwnd) return LANG_ID_EN;

    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    HKL currentHkl = GetKeyboardLayout(threadId);
    return LOWORD(reinterpret_cast<DWORD_PTR>(currentHkl));
}

bool LayoutEngine::SwitchLayout(HWND hwnd, WORD targetLang) {
    if (!hwnd) hwnd = GetForegroundWindow();
    HKL targetHkl = (targetLang == LANG_ID_RU) ? hklRu_ : hklEn_;
    if (!targetHkl) return false;

    if (hwnd) {
        // Send language change request to the target window
        PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(targetHkl));
    }
    ActivateKeyboardLayout(targetHkl, KLF_SETFORPROCESS);
    return true;
}

bool LayoutEngine::ToggleLayout(HWND hwnd) {
    WORD current = GetLayout(hwnd);
    WORD target = (current == LANG_ID_RU) ? LANG_ID_EN : LANG_ID_RU;
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
