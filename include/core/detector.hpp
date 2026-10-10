#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace Core {

enum class WordLanguage {
    Unknown,
    Russian,
    English
};

struct DetectedSwitch {
    bool shouldSwitch = false;
    WordLanguage currentLang = WordLanguage::Unknown;
    WordLanguage targetLang = WordLanguage::Unknown;
    std::wstring originalWord;
    std::wstring convertedWord;
    wchar_t delimiter = 0;
};

class Detector {
public:
    static Detector& Instance();

    // Input buffer tracking
    void OnChar(wchar_t ch);
    void OnBackspace();
    void Reset();

    // Evaluation on word delimiter (Space, Enter, Tab, Punctuation)
    DetectedSwitch EvaluateWord(wchar_t delimiter);

    // Immediate check during typing for impossible letter sequences
    DetectedSwitch CheckImmediate();

    const std::wstring& GetCurrentBuffer() const { return currentBuffer_; }
    std::wstring GetLastWordOrCurrent() const;

private:
    Detector();

    // Linguistic checks
    bool IsImpossibleRussian(const std::wstring& word) const;
    bool IsImpossibleEnglish(const std::wstring& word) const;
    bool HasRussianVowels(const std::wstring& word) const;
    bool HasEnglishVowels(const std::wstring& word) const;
    bool MatchesRussianMorphology(const std::wstring& word) const;

    // Dictionary lookups
    bool IsCommonRussianWord(const std::wstring& word) const;
    bool IsCommonEnglishWord(const std::wstring& word) const;

    std::wstring currentBuffer_;
    std::wstring lastWord_;
    wchar_t lastDelimiter_ = 0;
};

} // namespace Core
