#include "core/detector.hpp"
#include "core/layout_engine.hpp"
#include "platform/win32_utils.hpp"
#include <unordered_set>
#include <algorithm>

namespace Core {

static const std::unordered_set<std::wstring> s_commonRuWords = {
    // Basic vocabulary & user message terms
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
    L"плохо", L"функция", L"функции", L"присутствуют", L"присутствовать", L"работает", L"работают",
    L"переключение", L"переключать", L"перевод", L"переводить", L"язык", L"языка", L"капслок",
    L"выделенный", L"фрагмент", L"быстрый", L"быстрому", L"двойной", L"двойному", L"клик",
    L"включена", L"включить", L"последнее", L"последний", L"раскладка", L"раскладку",
    L"он", L"она", L"они", L"оно", L"мы", L"вы", L"ты", L"я", L"мне", L"тебе", L"ему", L"ей",
    L"нам", L"вам", L"им", L"меня", L"тебя", L"его", L"ее", L"нас", L"вас", L"их",
    L"да", L"нет", L"ли", L"же", L"бы", L"не", L"ни", L"но", L"или", L"если", L"хотя",
    L"чтобы", L"потому", L"так", L"где", L"кто", L"чем", L"кем", L"кому", L"чему", L"тоже",
    L"еще", L"ещё", L"уже", L"все", L"всё", L"всех", L"всем", L"всей", L"всеми", L"сам", L"сама"
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
    L"ъб", L"ъв", L"ъг", L"ъд", L"ъж", L"ъз", L"ък", L"ъл", L"ъм", L"ън", L"ъп", L"ър", L"ъс", L"ът", L"ъф", L"ъх", L"ъц", L"ъч", L"ъш", L"ъщ",
    L"ъа", L"ъо", L"ъу", L"ъы", L"ъэ", L"ъи",
    L"аъ", L"еъ", L"ёъ", L"иъ", L"оъ", L"уъ", L"ыъ", L"эъ", L"юъ", L"яъ",
    L"ьа", L"ьу", L"ьы", L"ьэ",
    L"жы", L"шы", L"чя", L"щя", L"чю", L"щю",
    L"ъъ", L"ьь", L"ъь", L"ьъ", L"ыы", L"йй",
    L"цш", L"ддщ", L"ьф", L"кщь", L"ащк", L"ршы", L"фму", L"нщг", L"гшд", L"ыек", L"мфд", L"огы", L"цщ", L"сщв"
};

// Common Russian prefixes for morphological analysis
static const wchar_t* s_commonRuPrefixes[] = {
    L"при", L"пре", L"про", L"пере", L"под", L"пред", L"по", L"от", L"до", L"за",
    L"на", L"над", L"не", L"ни", L"раз", L"рас", L"без", L"бес", L"вы", L"со", L"из", L"ис"
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
    } else if (!lastWord_.empty()) {
        lastWord_.pop_back();
    }
}

void Detector::Reset() {
    currentBuffer_.clear();
    lastWord_.clear();
    lastDelimiter_ = 0;
}

std::wstring Detector::GetLastWordOrCurrent() const {
    if (!currentBuffer_.empty()) {
        return currentBuffer_;
    }
    return lastWord_;
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
            lower == L'o' || lower == L'u') {
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

    // If word contains characters mapped from Russian like [, ], ;, ', ,, ., `
    for (wchar_t ch : lower) {
        if (ch == L'[' || ch == L']' || ch == L';' || ch == L'\'' || ch == L',' || ch == L'.' || ch == L'`') {
            return true;
        }
    }

    // English word of length >= 3 without vowels is impossible
    if (lower.length() >= 3 && !HasEnglishVowels(lower)) {
        return true;
    }

    // Check 4+ consecutive consonants in English
    int consecutiveConsonants = 0;
    for (wchar_t ch : lower) {
        if (ch >= L'a' && ch <= L'z') {
            if (ch == L'a' || ch == L'e' || ch == L'i' || ch == L'o' || ch == L'u') {
                consecutiveConsonants = 0;
            } else {
                consecutiveConsonants++;
                if (consecutiveConsonants >= 4) {
                    return true;
                }
            }
        }
    }

    // Rare impossible English letter pairs
    static const wchar_t* s_impossibleEnBigrams[] = {
        L"jx", L"jq", L"jz", L"vf", L"vg", L"vj", L"vk", L"vm", L"vn", L"vp", L"vr", L"vs", L"vt", L"vu", L"vx", L"vz",
        L"gk", L"kj", L"wbb", L"hf", L"jn", L"gth", L"trk", L"ckj", L"jdj", L"ldj", L"jqy", L"scnh"
    };
    for (const auto* bigram : s_impossibleEnBigrams) {
        if (lower.find(bigram) != std::wstring::npos) {
            return true;
        }
    }

    return false;
}

bool Detector::MatchesRussianMorphology(const std::wstring& word) const {
    std::wstring lower = Platform::ToLower(word);
    for (const auto* prefix : s_commonRuPrefixes) {
        if (lower.rfind(prefix, 0) == 0 && lower.length() > wcslen(prefix)) {
            return true;
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
    lastWord_ = word;
    lastDelimiter_ = delimiter;
    currentBuffer_.clear();

    // Check for numbers or code path indicators
    bool hasLetters = false;
    for (wchar_t ch : word) {
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё' ||
            ch == L'[' || ch == L']' || ch == L';' || ch == L'\'' || ch == L',' || ch == L'.' || ch == L'`') {
            hasLetters = true;
        } else if (ch >= L'0' && ch <= L'9' || ch == L'_' || ch == L'\\' || ch == L'@' || ch == L'#') {
            return result; // Code or number
        }
    }

    if (!hasLetters || word.length() < 2) {
        return result;
    }

    std::wstring wordLower = Platform::ToLower(word);

    // 1. Check if word is typed in EN layout (Latin + punctuation keys)
    bool isEnLayout = true;
    for (wchar_t ch : wordLower) {
        if (!((ch >= L'a' && ch <= L'z') || ch == L'[' || ch == L']' || ch == L';' || ch == L'\'' || ch == L',' || ch == L'.' || ch == L'`')) {
            isEnLayout = false;
            break;
        }
    }

    // 2. Check if word is typed in RU layout (Cyrillic)
    bool isRuLayout = true;
    for (wchar_t ch : wordLower) {
        if (!((ch >= L'а' && ch <= L'я') || ch == L'ё')) {
            isRuLayout = false;
            break;
        }
    }

    if (isEnLayout && !IsCommonEnglishWord(wordLower)) {
        // Test translation to Russian
        std::wstring convertedToRu = LayoutEngine::Instance().EnToRu(word);
        std::wstring convertedLower = Platform::ToLower(convertedToRu);

        bool isKnownRu = IsCommonRussianWord(convertedLower);
        bool impossibleEn = IsImpossibleEnglish(wordLower);
        bool impossibleRu = IsImpossibleRussian(convertedLower);
        bool hasRuMorph = MatchesRussianMorphology(convertedLower);
        bool hasRuVowels = HasRussianVowels(convertedLower);

        if (isKnownRu || (hasRuVowels && !impossibleRu && (impossibleEn || hasRuMorph || wordLower.length() >= 4))) {
            result.shouldSwitch = true;
            result.currentLang = WordLanguage::English;
            result.targetLang = WordLanguage::Russian;
            result.originalWord = word;
            result.convertedWord = convertedToRu;
            result.delimiter = delimiter;
            return result;
        }
    } else if (isRuLayout && !IsCommonRussianWord(wordLower)) {
        // Test translation to English
        std::wstring convertedToEn = LayoutEngine::Instance().RuToEn(word);
        std::wstring convertedLower = Platform::ToLower(convertedToEn);

        bool isKnownEn = IsCommonEnglishWord(convertedLower);
        bool impossibleRu = IsImpossibleRussian(wordLower);
        bool impossibleEn = IsImpossibleEnglish(convertedLower);
        bool hasEnVowels = HasEnglishVowels(convertedLower);

        if (isKnownEn || (hasEnVowels && !impossibleEn && (impossibleRu || wordLower.length() >= 4))) {
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
    return result;
}

} // namespace Core
