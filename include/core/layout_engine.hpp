#pragma once

#include <windows.h>
#include <string>
#include <atomic>

namespace Core {

constexpr WORD LANG_ID_EN = 0x0409;
constexpr WORD LANG_ID_RU = 0x0419;

class LayoutEngine {
public:
    static LayoutEngine& Instance();

    // Layout identification & switching
    WORD GetLayout(HWND hwnd);
    bool SwitchLayout(HWND hwnd, WORD targetLang);
    bool ToggleLayout(HWND hwnd);

    // Text translation between layouts
    std::wstring EnToRu(const std::wstring& text);
    std::wstring RuToEn(const std::wstring& text);
    wchar_t CharEnToRu(wchar_t ch);
    wchar_t CharRuToEn(wchar_t ch);

    // Simulation of replacement text via SendInput
    void SimulateReplacement(size_t eraseCount, const std::wstring& newText);

    // Input simulation state flag
    static bool IsSimulatingInput();

private:
    LayoutEngine();
    static std::atomic<bool> s_simulatingInput;
    HKL hklRu_ = nullptr;
    HKL hklEn_ = nullptr;
};

} // namespace Core
