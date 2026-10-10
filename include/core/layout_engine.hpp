#pragma once

#include <windows.h>
#include <string>
#include <atomic>

struct ITfInputProcessorProfiles;

namespace Core {

constexpr WORD LANG_ID_EN = 0x0409;
constexpr WORD LANG_ID_RU = 0x0419;

class LayoutEngine {
public:
    static LayoutEngine& Instance();
    ~LayoutEngine();

    // Layout identification & switching
    WORD GetLayout(HWND hwnd);
    bool SwitchLayout(HWND hwnd, WORD targetLang);
    bool ToggleLayout(HWND hwnd);

    // Text translation between layouts
    std::wstring EnToRu(const std::wstring& text);
    std::wstring RuToEn(const std::wstring& text);
    wchar_t CharEnToRu(wchar_t ch);
    wchar_t CharRuToEn(wchar_t ch);

    // Smart bidirectional text conversion (auto-detects source layout)
    std::wstring ConvertTextSmart(const std::wstring& text);
    WORD DetectLanguageOfText(const std::wstring& text);

    // Feature: Translate currently selected fragment or last word
    void ConvertSelectionOrLastWord();

    // Simulation of replacement text via SendInput
    void SimulateReplacement(size_t eraseCount, const std::wstring& newText);

    // Clipboard helpers
    std::wstring GetClipboardText();
    bool SetClipboardText(const std::wstring& text);
    std::wstring CopySelectedText(HWND hwnd);
    void PasteText(HWND hwnd, const std::wstring& text, const std::wstring& restoreClipboard);

    // Notification for Windows Text Services Framework & Taskbar indicator
    void NotifyTsfAndTaskbar(WORD targetLang, HKL targetHkl);

    // Input simulation state flag
    static bool IsSimulatingInput();

private:
    LayoutEngine();
    void EnsureTsf();

    static std::atomic<bool> s_simulatingInput;
    HKL hklRu_ = nullptr;
    HKL hklEn_ = nullptr;
    ITfInputProcessorProfiles* pTsfProfiles_ = nullptr;
    HMODULE hMsCtf_ = nullptr;
};

} // namespace Core
