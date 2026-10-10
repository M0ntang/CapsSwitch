#include "core/detector.hpp"
#include "core/layout_engine.hpp"
#include "platform/win32_utils.hpp"
#include <unordered_set>
#include <algorithm>

namespace Core {

static const std::unordered_set<std::wstring> s_commonRuWords = {
    L"привет", L"как", L"дела", L"что", L"это", L"быть", L"весь", L"этот", L"один", L"такой",
    L"только", L"сегодня", L"сейчас", L"когда", L"время", L"дело", L"жизнь", L"день", L"рука",
    L"слово", L"место", L"лицо", L"друг", L"глаз", L"дом", L"сторона", L"работа", L"город",
    L"конец", L"дверь", L"земля", L"голова", L"вдруг", L"видеть", L"пойти", L"стоять", L"думать",
    L"спросить", L"жить", L"смотреть", L"понять", L"иметь", L"сделать", L"сидеть", L"взять",
    L"хотеть", L"любить", L"говорить", L"знать", L"стать", L"сказать", L"мочь", L"человек",
    L"спасибо", L"пожалуйста", L"хорошо", L"отлично", L"просто", L"вопрос", L"ответ", L"книга",
    L"писать", L"читать", L"номер", L"программа", L"файл", L"папка", L"компьютер", L"экран",
    L"пользователь", L"настройка", L"сервер", L"система", L"версия", L"зачем", L"почему",
    L"завтра", L"вчера", L"утром", L"вечером", L"ночью", L"вместе", L"снова", L"опять",
    L"потом", L"всегда", L"никогда", L"иногда", L"совсем", L"очень", L"много", L"мало",
    L"немного", L"больше", L"меньше", L"лучше", L"хуже", L"точно", L"ладно", L"конечно",
    L"может", L"нужно", L"надо", L"можно", L"нельзя", L"правда", L"верно", L"скоро",
    L"здесь", L"тут", L"там", L"куда", L"откуда", L"везде", L"нигде", L"кстати", L"впрочем",
    L"приветник", L"проверка", L"тест", L"разработка", L"сборка", L"ошибка", L"проект",
    L"он", L"она", L"они", L"оно", L"мы", L"вы", L"ты", L"я", L"мне", L"тебе", L"ему", L"ей",
    L"нам", L"вам", L"им", L"меня", L"тебя", L"его", L"ее", L"нас", L"вас", L"их",
    L"да", L"нет", L"ли", L"же", L"бы", L"не", L"ни", L"но", L"или", L"если", L"хотя",
    L"чтобы", L"потому", L"так", L"как", L"где", L"кто", L"чем", L"кем", L"кому", L"чему",
    L"все", L"всё", L"всех", L"всем", L"всей", L"всеми", L"сам", L"сама", L"сами", L"само"
};

static const std::unordered_set<std::wstring> s_commonEnWords = {
    L"the", L"be", L"to", L"of", L"and", L"a", L"in", L"that", L"have", L"it",
    L"for", L"not", L"on", L"with", L"he", L"as", L"you", L"do", L"at", L"this",
    L"but", L"his", L"by", L"from", L"they", L"we", L"say", L"her", L"she", L"or",
    L"an", L"will", L"my", L"one", L"all", L"would", L"there", L"their", L"what", L"so",
    L"up", L"out", L"if", L"about", L"who", L"get", L"which", L"go", L"me", L"when",
    L"make", L"can", L"like", L"time", L"no", L"just", L"him", L"know", L"take", L"people",
    L"into", L"year", L"your", L"good", L"some", L"could", L"them", L"see", L"other", L"than",
    L"then", L"now", L"look", L"only", L"come", L"its", L"over", L"think", L"also", L"back",
    L"after", L"use", L"two", L"how", L"our", L"work", L"first", L"well", L"way", L"even",
    L"new", L"want", L"because", L"any", L"these", L"give", L"day", L"most", L"us", L"hello",
    L"code", L"class", L"build", L"test", L"string", L"value", L"input", L"window", L"system",
    L"file", L"error", L"select", L"update", L"delete", L"insert", L"function", L"return",
    L"const", L"public", L"private", L"protected", L"include", L"import", L"export", L"default",
    L"break", L"continue", L"switch", L"case", L"while", L"struct", L"void", L"auto",
    L"true", L"false", L"null", L"nullptr", L"undefined", L"object", L"number", L"boolean",
    L"world", L"please", L"thanks", L"thank", L"welcome", L"sorry", L"yes", L"help", L"name"
};

// Impossible bigrams in Russian (standard words never contain these)
static const wchar_t* s_impossibleRuBigrams[] = {
    // Ъ followed by consonants or incorrect vowels
    L"ъб", L"ъв", L"ъг", L"ъд", L"ъж", L"ъз", L"ък", L"ъл", L"ъм", L"ън", L"ъп", L"ър", L"ъс", L"ът", L"ъф", L"ъх", L"ъц", L"ъч", L"ъш", L"ъщ",
    L"ъа", L"ъо", L"ъу", L"ъы", L"ъэ", L"ъи",
    // Vowels followed by Ъ
    L"аъ", L"еъ", L"ёъ", L"иъ", L"оъ", L"уъ", L"ыъ", L"эъ", L"юъ", L"яъ",
    // Ь followed by incorrect vowels
    L"ьа", L"ьу", L"ьы", L"ьэ",
    // Combinations impossible in Russian (usually from typing EN on RU layout)
    L"жы", L"шы", L"чя", L"щя", L"чю", L"щю",
    L"ъъ", L"ьь", L"ъь", L"ьъ", L"ыы", L"йй",
    L"цш", L"ддщ", L"ьф", L"кщь", L"ащк", L"ршы", L"фму", L"нщг", L"гшд", L"ыек", L"мфд", L"огы", L"цщ", L"сщв"
};

Detector& Detector::Instance() {
    static Detector s_instance;
    return s_instance;
}

Detector::Detector() {}

void Detector::OnChar(wchar_t ch) {
    if (ch >= 32) {
        currentBuffer_.push_back(ch);
    }
}

void Detector::OnBackspace() {
    if (!currentBuffer_.empty()) {
        currentBuffer_.pop_back();
    }
}

void Detector::Reset() {
    currentBuffer_.clear();
}

bool Detector::HasRussianVowels(const std::wstring& word) const {
    for (wchar_t ch : word) {
        wchar_t lower = ::towlower(ch);
        if (lower == L'а' || lower == L'е' || lower == L'ё' || lower == L'и' ||
            lower == L'о' || lower == L'у' || lower == L'ы' || lower == L'э' ||
            lower == L'ю' || lower == L'я') {
            return true;
        }
    }
    return false;
}

bool Detector::HasEnglishVowels(const std::wstring& word) const {
    for (wchar_t ch : word) {
        wchar_t lower = ::towlower(ch);
        if (lower == L'a' || lower == L'e' || lower == L'i' ||
            lower == L'o' || lower == L'u' || lower == L'y') {
            return true;
        }
    }
    return false;
}

bool Detector::IsImpossibleRussian(const std::wstring& word) const {
    if (word.empty()) return false;
    std::wstring lower = Platform::ToLower(word);

    // Cannot start with Ъ, Ь, Ы
    wchar_t first = lower[0];
    if (first == L'ъ' || first == L'ь' || first == L'ы') {
        return true;
    }

    // Check impossible bigrams
    for (const auto* bigram : s_impossibleRuBigrams) {
        if (lower.find(bigram) != std::wstring::npos) {
            return true;
        }
    }

    // Russian word of length >= 3 must have at least one vowel
    if (lower.length() >= 3 && !HasRussianVowels(lower)) {
        return true;
    }

    return false;
}

bool Detector::IsImpossibleEnglish(const std::wstring& word) const {
    if (word.empty()) return false;
    std::wstring lower = Platform::ToLower(word);

    // English word of length >= 3 without vowels is extremely abnormal (e.g. ghbdtn, cghjc)
    if (lower.length() >= 3 && !HasEnglishVowels(lower)) {
        return true;
    }

    // Check 4+ consecutive consonants in English
    int consecutiveConsonants = 0;
    for (wchar_t ch : lower) {
        if (ch >= L'a' && ch <= L'z') {
            if (ch == L'a' || ch == L'e' || ch == L'i' || ch == L'o' || ch == L'u' || ch == L'y') {
                consecutiveConsonants = 0;
            } else {
                consecutiveConsonants++;
                if (consecutiveConsonants >= 4) {
                    return true;
                }
            }
        }
    }

    // Rare impossible bigrams
    if (lower.find(L"q") != std::wstring::npos) {
        // Q almost always followed by U in English words
        for (size_t i = 0; i < lower.length(); ++i) {
            if (lower[i] == L'q') {
                if (i + 1 >= lower.length() || lower[i + 1] != L'u') {
                    if (lower.length() > 2) return true;
                }
            }
        }
    }

    return false;
}

bool Detector::IsCommonRussianWord(const std::wstring& word) const {
    return s_commonRuWords.find(Platform::ToLower(word)) != s_commonRuWords.end();
}

bool Detector::IsCommonEnglishWord(const std::wstring& word) const {
    return s_commonEnWords.find(Platform::ToLower(word)) != s_commonEnWords.end();
}

DetectedSwitch Detector::EvaluateWord(wchar_t delimiter) {
    DetectedSwitch result;
    if (currentBuffer_.empty()) {
        Reset();
        return result;
    }

    std::wstring word = currentBuffer_;
    Reset();

    // Check if word looks like code / URL / numbers
    bool hasLetters = false;
    bool hasDigitsOrSpecial = false;
    for (wchar_t ch : word) {
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё') {
            hasLetters = true;
        } else if (ch >= L'0' && ch <= L'9' || ch == L'_' || ch == L'/' || ch == L'\\' || ch == L'@' || ch == L'#') {
            hasDigitsOrSpecial = true;
        }
    }

    if (!hasLetters || hasDigitsOrSpecial) {
        return result;
    }

    std::wstring wordLower = Platform::ToLower(word);

    // 1. Check if word is Latin (typed in EN layout)
    bool isLatin = true;
    for (wchar_t ch : wordLower) {
        if (ch < L'a' || ch > L'z') {
            isLatin = false;
            break;
        }
    }

    // 2. Check if word is Cyrillic (typed in RU layout)
    bool isCyrillic = true;
    for (wchar_t ch : wordLower) {
        if (!((ch >= L'а' && ch <= L'я') || ch == L'ё')) {
            isCyrillic = false;
            break;
        }
    }

    if (isLatin) {
        // Current layout is EN. Check if it was meant to be RU!
        std::wstring convertedToRu = LayoutEngine::Instance().EnToRu(word);
        std::wstring convertedLower = Platform::ToLower(convertedToRu);

        // A. Is converted word in common Russian words?
        bool isKnownRu = IsCommonRussianWord(convertedLower);

        // B. Is current word impossible in English, but valid in Russian?
        bool impossibleEn = IsImpossibleEnglish(wordLower);
        bool impossibleRu = IsImpossibleRussian(convertedLower);

        if (isKnownRu || (impossibleEn && !impossibleRu && HasRussianVowels(convertedLower))) {
            result.shouldSwitch = true;
            result.currentLang = WordLanguage::English;
            result.targetLang = WordLanguage::Russian;
            result.originalWord = word;
            result.convertedWord = convertedToRu;
            result.delimiter = delimiter;
            return result;
        }
    } else if (isCyrillic) {
        // Current layout is RU. Check if it was meant to be EN!
        std::wstring convertedToEn = LayoutEngine::Instance().RuToEn(word);
        std::wstring convertedLower = Platform::ToLower(convertedToEn);

        // A. Is converted word in common English words?
        bool isKnownEn = IsCommonEnglishWord(convertedLower);

        // B. Is current word impossible in Russian, but valid in English?
        bool impossibleRu = IsImpossibleRussian(wordLower);
        bool impossibleEn = IsImpossibleEnglish(convertedLower);

        if (isKnownEn || (impossibleRu && !impossibleEn && HasEnglishVowels(convertedLower))) {
            result.shouldSwitch = true;
            result.currentLang = WordLanguage::Russian;
            result.targetLang = WordLanguage::English;
            result.originalWord = word;
            result.convertedWord = convertedToEn;
            result.delimiter = delimiter;
            return result;
        }
    }

    return result;
}

DetectedSwitch Detector::CheckImmediate() {
    DetectedSwitch result;
    if (currentBuffer_.length() < 3) {
        return result;
    }

    std::wstring word = currentBuffer_;
    std::wstring wordLower = Platform::ToLower(word);

    // If Latin: check for immediate impossible sequence (e.g. 5 consonants in a row)
    bool isLatin = true;
    for (wchar_t ch : wordLower) {
        if (ch < L'a' || ch > L'z') {
            isLatin = false;
            break;
        }
    }

    if (isLatin) {
        std::wstring convertedToRu = LayoutEngine::Instance().EnToRu(word);
        std::wstring convertedLower = Platform::ToLower(convertedToRu);

        if (IsCommonRussianWord(convertedLower)) {
            result.shouldSwitch = true;
            result.currentLang = WordLanguage::English;
            result.targetLang = WordLanguage::Russian;
            result.originalWord = word;
            result.convertedWord = convertedToRu;
            result.delimiter = 0;
            Reset();
            return result;
        }
    }

    return result;
}

} // namespace Core
