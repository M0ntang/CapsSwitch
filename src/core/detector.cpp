#include "core/detector.hpp"
#include "core/layout_engine.hpp"
#include "config/config.hpp"
#include "platform/win32_utils.hpp"
#include <unordered_set>
#include <algorithm>
#include <cwchar>

namespace Core {

namespace {

bool IsCyrillicLetter(wchar_t ch) {
    return (ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё';
}

bool IsLatinLetter(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
}

bool IsRussianVowelChar(wchar_t lower) {
    return lower == L'а' || lower == L'е' || lower == L'ё' || lower == L'и' ||
           lower == L'о' || lower == L'у' || lower == L'ы' || lower == L'э' ||
           lower == L'ю' || lower == L'я';
}

bool IsEnglishVowelChar(wchar_t lower) {
    return lower == L'a' || lower == L'e' || lower == L'i' ||
           lower == L'o' || lower == L'u' || lower == L'y';
}

bool IsEnMappedLetterPunct(wchar_t ch) {
    return ch == L'[' || ch == L']' || ch == L'{' || ch == L'}' ||
           ch == L';' || ch == L':' || ch == L'\'' || ch == L'\"' ||
           ch == L',' || ch == L'<' || ch == L'.' || ch == L'>' ||
           ch == L'`' || ch == L'~';
}

bool IsTrailingPunct(wchar_t ch) {
    return ch == L'.' || ch == L',' || ch == L';' || ch == L':' ||
           ch == L'!' || ch == L'?' || ch == L'/' || ch == L'&' ||
           ch == L'^' || ch == L'$';
}

bool IsEnPunctOnRuLayout(wchar_t ch) {
    return ch == L'!' || ch == L'?' || ch == L'/' ||
           ch == L'&' || ch == L'^' || ch == L'$';
}

bool IsRuLayoutChar(wchar_t ch) {
    return (ch >= L'а' && ch <= L'я') || ch == L'ё' ||
           ch == L',' || ch == L'.' || ch == L';' || ch == L':' ||
           ch == L'\'' || ch == L'\"' || ch == L'[' || ch == L']' ||
           ch == L'{' || ch == L'}' || ch == L'`' || ch == L'~' ||
           ch == L'!' || ch == L'?';
}

bool ContainsChar(const wchar_t* set, wchar_t ch) {
    return wcschr(set, ch) != nullptr;
}

} // namespace

static const std::unordered_set<std::wstring> s_commonRuWords = {
    // Basic vocabulary & common Russian words
    L"привет", L"как", L"дела", L"что", L"это", L"быть", L"весь", L"этот", L"один", L"такой",
    L"только", L"сегодня", L"сейчас", L"когда", L"время", L"дело", L"жизнь", L"день", L"рука",
    L"руки", L"руке", L"рукой", L"руку", L"рук",
    L"слово", L"место", L"лицо", L"друг", L"глаз", L"дом", L"дома", L"дому", L"сторона", L"работа", L"город",
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
    L"выделенный", L"фрагмент", L"быстрый", L"быстро", L"быстрому", L"двойной", L"двойному", L"клик",
    L"включена", L"включить", L"последнее", L"последний", L"раскладка", L"раскладку", L"строка",
    L"он", L"она", L"они", L"оно", L"мы", L"вы", L"ты", L"я", L"мне", L"тебе", L"ему", L"ей",
    L"нам", L"вам", L"им", L"меня", L"тебя", L"его", L"ее", L"её", L"нас", L"вас", L"их",
    L"да", L"нет", L"ли", L"же", L"бы", L"не", L"ни", L"но", L"или", L"если", L"хотя",
    L"чтобы", L"потому", L"так", L"где", L"кто", L"чем", L"кем", L"кому", L"чему", L"тоже",
    L"еще", L"ещё", L"уже", L"все", L"всё", L"всех", L"всем", L"всей", L"всеми", L"сам", L"сама",
    // Short words, prepositions, particles & verbs ending in ю/б/ж
    L"на", L"за", L"по", L"от", L"до", L"из", L"об", L"ко", L"со", L"во", L"без", L"для",
    L"при", L"про", L"под", L"над", L"вот", L"тот", L"раз", L"два", L"три", L"год", L"мир",
    L"кот", L"сон", L"лес", L"чай", L"хлеб", L"стол", L"окно", L"вода", L"еда", L"мясо",
    L"зуб", L"дуб", L"гриб", L"чтоб", L"муж", L"лоб", L"ночь", L"учу",
    L"было", L"будет", L"есть", L"могу", L"хочу", L"знаю", L"думаю", L"делаю", L"люблю",
    L"вижу", L"иду", L"жду", L"сплю", L"читаю", L"пишу", L"играю", L"мою", L"твою", L"свою",
    L"чет", L"хреново", L"короче", L"посмотри", L"потести", L"проведи", L"проверки",
    L"исправляй", L"проверяй", L"делай", L"пока", L"давай", L"прикольно", L"нормально"
};

static const std::unordered_set<std::wstring> s_commonEnWords = {
    // Common English words & programming keywords
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
    L"world", L"please", L"thanks", L"thank", L"welcome", L"sorry", L"yes", L"help", L"name",
    L"user", L"open", L"save", L"line", L"item", L"mode", L"data", L"task", L"port", L"view",
    L"path", L"font", L"size", L"card", L"type", L"play", L"send", L"read", L"write", L"menu",
    // Short words, abbreviations & collision guards
    L"am", L"is", L"are", L"was", L"were", L"been", L"has", L"had", L"did", L"does",
    L"vs", L"dj", L"ok", L"hi", L"oh", L"ah", L"ha", L"ye", L"tot", L"herb",
    L"why", L"sky", L"fly", L"try", L"dry", L"cry", L"buy", L"guy", L"gym", L"sync",
    L"photo", L"radio", L"laptop", L"keyboard", L"computer", L"program", L"config", L"docker", L"sudo",
    L"vulnerable", L"vulture", L"vulgar", L"chevron", L"crowd", L"newton", L"bowtie", L"showcase",
    L"cowboy", L"hawking", L"viewpoint", L"snowfall", L"kevlar",
    L"kiev", L"civ", L"pov", L"suv",
    // Common contractions
    L"i'm", L"i've", L"i'll", L"i'd", L"you're", L"you've", L"you'll", L"you'd",
    L"he's", L"he'll", L"he'd", L"she's", L"she'll", L"she'd", L"it's", L"it'll", L"it'd",
    L"we're", L"we've", L"we'll", L"we'd", L"they're", L"they've", L"they'll", L"they'd",
    L"that's", L"what's", L"who's", L"where's", L"when's", L"why's", L"how's",
    L"here's", L"there's", L"let's", L"don't", L"doesn't", L"didn't", L"won't", L"wouldn't",
    L"can't", L"couldn't", L"shouldn't", L"isn't", L"aren't", L"wasn't", L"weren't",
    L"hasn't", L"haven't", L"hadn't", L"mustn't", L"needn't", L"ain't", L"y'all", L"o'clock",
    // Tech, dev & shell tokens
    L"http", L"https", L"html", L"css", L"json", L"xml", L"yaml", L"toml", L"sql", L"mysql",
    L"pgsql", L"sqlite", L"cmake", L"gcc", L"clang", L"llvm", L"github", L"gitlab", L"linux",
    L"macos", L"win32", L"utf8", L"utf16", L"api", L"sdk", L"cli", L"gui", L"tui", L"ide",
    L"url", L"uri", L"dns", L"tcp", L"udp", L"ssh", L"tls", L"ssl", L"jwt", L"oauth",
    L"rest", L"grpc", L"crud", L"mvvm", L"mvc", L"hwnd", L"hkl", L"uia", L"com", L"dll",
    L"exe", L"lib", L"obj", L"pdb", L"src", L"bin", L"tmp", L"var", L"etc", L"usr", L"dev",
    L"devs", L"rev", L"revs", L"nav", L"div", L"divs", L"gov", L"govt", L"fav", L"favs", L"luv",
    L"env", L"prev", L"argv", L"recv", L"srv", L"drv", L"csv", L"tsv", L"wav", L"mov",
    L"mkv", L"flv", L"wmv", L"dvd", L"vcd", L"svg", L"vga", L"hevc", L"jvm", L"kvm", L"lvm",
    L"nvme", L"av", L"cv", L"uv", L"ev",
    L"char", L"bool", L"int", L"long", L"enum", L"impl", L"func", L"proc", L"async", L"await",
    L"yield", L"static", L"union", L"trait", L"match", L"using", L"typedef", L"sizeof", L"typeof",
    L"malloc", L"free", L"printf", L"scanf", L"cin", L"cout", L"endl", L"std", L"vec", L"str",
    L"len", L"min", L"max", L"abs", L"pow", L"sqrt", L"sin", L"cos", L"tan", L"log", L"exp",
    L"init", L"main", L"spec", L"mock", L"stub", L"diff", L"repo", L"git", L"npm", L"npx",
    L"pip", L"venv", L"conda", L"rust", L"cargo", L"crate", L"node", L"deno", L"bun", L"react",
    L"vue", L"next", L"vite", L"tailwind", L"eslint", L"prettier", L"vscode", L"msvc", L"mingw",
    L"w64devkit", L"powershell", L"pwsh", L"cmd", L"bash", L"zsh", L"wsl", L"id", L"ip", L"os",
    L"db", L"ui", L"ux", L"js", L"ts", L"py", L"hr", L"pr", L"mr", L"ms", L"dr", L"tv", L"pc",
    L"hd", L"kb", L"mb", L"gb", L"tb", L"uk", L"eu", L"ru", L"en", L"io", L"ai", L"ml",
    L"ci", L"cd", L"ls", L"rm", L"cp", L"mv", L"ps", L"sh", L"faq", L"iraq", L"qatar",
    L"lengths", L"strengths", L"downstream", L"postscript", L"matchstick", L"catchphrase", L"twelfths"
};

// Impossible n-grams in Russian (standard words never contain these sequences)
static const wchar_t* const s_impossibleRuNgrams[] = {
    L"ъб", L"ъв", L"ъг", L"ъд", L"ъж", L"ъз", L"ък", L"ъл", L"ъм", L"ън", L"ъп", L"ър", L"ъс", L"ът", L"ъф", L"ъх", L"ъц", L"ъч", L"ъш", L"ъщ",
    L"ъа", L"ъо", L"ъу", L"ъы", L"ъэ", L"ъи",
    L"аъ", L"еъ", L"ёъ", L"иъ", L"оъ", L"уъ", L"ыъ", L"эъ", L"юъ", L"яъ",
    L"ьа", L"ьу", L"ьы", L"ьэ",
    L"аь", L"еь", L"ёь", L"иь", L"оь", L"уь", L"ыь", L"эь", L"юь", L"яь", L"йь",
    L"аы", L"еы", L"ёы", L"иы", L"оы", L"уы", L"ыы", L"эы", L"юы", L"яы", L"йы",
    L"гы", L"жы", L"шы", L"щы", L"чы", L"чя", L"щя", L"чю", L"щю",
    L"гь", L"хь",
    L"эы", L"эю", L"эя", L"эё", L"ээ",
    L"щб", L"щв", L"щг", L"щд", L"щж", L"щз", L"щй", L"щк", L"щл", L"щм", L"щп", L"щр", L"щс", L"щт", L"щф", L"щх", L"щц", L"щч", L"щш", L"щщ",
    L"фб", L"фв", L"фг", L"фд", L"фж", L"фз", L"фй", L"фп", L"фх", L"фц", L"фч", L"фш", L"фщ", L"фэ", L"фю", L"фя",
    L"шб", L"шг", L"шд", L"шж", L"шз", L"шф", L"шх", L"шц", L"шч", L"шщ", L"шя", L"шй", L"шш",
    L"гж", L"гз", L"гй", L"гп", L"гф", L"гх", L"гц", L"гш", L"гщ", L"гя",
    L"ъъ", L"ьь", L"ъь", L"ьъ", L"йй",
    L"цш", L"ддщ", L"ьф", L"кщь", L"ащк", L"ршы", L"фму", L"нщг", L"гшд", L"ыек", L"мфд", L"огы", L"цщ", L"сщв"
};

// Common Russian prefixes for morphological analysis
static const wchar_t* const s_commonRuPrefixes[] = {
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
        std::lock_guard<std::mutex> lock(mutex_);
        currentBuffer_.push_back(ch);
    }
}

void Detector::OnBackspace() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!currentBuffer_.empty()) {
        currentBuffer_.pop_back();
    } else if (lastDelimiter_ != 0) {
        lastDelimiter_ = 0;
        currentBuffer_ = lastWord_;
        lastWord_.clear();
    } else if (!lastWord_.empty()) {
        lastWord_.pop_back();
    }
}

void Detector::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    currentBuffer_.clear();
    lastWord_.clear();
    lastDelimiter_ = 0;
}

std::wstring Detector::GetCurrentBuffer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentBuffer_;
}

std::wstring Detector::GetLastWordOrCurrent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!currentBuffer_.empty()) {
        return currentBuffer_;
    }
    return lastWord_;
}

wchar_t Detector::GetLastDelimiter() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastDelimiter_;
}

void Detector::SetLastWord(const std::wstring& word, wchar_t delim) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastWord_ = word;
    lastDelimiter_ = delim;
    currentBuffer_.clear();
}

bool Detector::HasRussianVowels(const std::wstring& word) const {
    for (wchar_t ch : word) {
        if (IsRussianVowelChar(Platform::ToLowerChar(ch))) {
            return true;
        }
    }
    return false;
}

bool Detector::HasEnglishVowels(const std::wstring& word) const {
    for (wchar_t ch : word) {
        if (IsEnglishVowelChar(Platform::ToLowerChar(ch))) {
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

    // Cannot start with Й unless followed by О or Е (йод, йогурт, йети)
    if (first == L'й' && lower.length() >= 2 && lower[1] != L'о' && lower[1] != L'е') {
        return true;
    }

    // Cannot start with Ц + consonant other than В (цвет, цвести)
    if (first == L'ц' && lower.length() >= 2 && !IsRussianVowelChar(lower[1]) && lower[1] != L'в') {
        return true;
    }

    // Check impossible n-grams
    for (const auto* ngram : s_impossibleRuNgrams) {
        if (lower.find(ngram) != std::wstring::npos) {
            return true;
        }
    }

    // 5+ consecutive Russian consonants are virtually impossible (e.g. контрвстреча is 5, English on RU hits this often)
    int consecutiveConsonants = 0;
    for (wchar_t ch : lower) {
        if ((ch >= L'а' && ch <= L'я') || ch == L'ё') {
            if (IsRussianVowelChar(ch) || ch == L'ь' || ch == L'ъ') {
                consecutiveConsonants = 0;
            } else {
                consecutiveConsonants++;
                if (consecutiveConsonants >= 5) {
                    return true;
                }
            }
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

    // English word of length >= 3 without vowels (including 'y') is impossible
    if (lower.length() >= 3 && !HasEnglishVowels(lower)) {
        return true;
    }

    // 5+ consecutive consonants in English (y counted as vowel; avoids false hits on abstract/strength/workspace)
    int consecutiveConsonants = 0;
    for (wchar_t ch : lower) {
        if (ch >= L'a' && ch <= L'z') {
            if (IsEnglishVowelChar(ch)) {
                consecutiveConsonants = 0;
            } else {
                consecutiveConsonants++;
                if (consecutiveConsonants >= 5) {
                    return true;
                }
            }
        }
    }

    // Phonotactic checks on Latin-only stem
    const size_t len = lower.length();
    if (len >= 2) {
        // 'x' at index 0 is only followed by vowels or 'm'/'l' (xml, xsl)
        if (lower[0] == L'x' && !IsEnglishVowelChar(lower[1]) && lower[1] != L'm' && lower[1] != L'l') {
            return true;
        }
        // 'k' at index 0 is only followed by vowels or 'n'/'l'/'r'
        if (lower[0] == L'k' && !IsEnglishVowelChar(lower[1]) && lower[1] != L'n' && lower[1] != L'l' && lower[1] != L'r') {
            return true;
        }
        // 'w' at index 0 is only followed by vowels or 'h'/'r'
        if (lower[0] == L'w' && !IsEnglishVowelChar(lower[1]) && lower[1] != L'h' && lower[1] != L'r') {
            return true;
        }
        // 'h' + 't' at index 0 (only http/https/html, already in common dict)
        if (lower[0] == L'h' && lower[1] == L't') {
            return true;
        }
    }

    for (size_t i = 0; i < len; ++i) {
        wchar_t ch = lower[i];
        wchar_t next = (i + 1 < len) ? lower[i + 1] : 0;

        // 'j' ('о' on RU) never precedes a consonant and never ends a word (except 2-char "dj")
        if (ch == L'j') {
            if (next == 0 && len >= 3) return true;
            if (next != 0 && next >= L'a' && next <= L'z' && !IsEnglishVowelChar(next)) return true;
        }
        // 'q' ('й' on RU) in English must be followed by 'u'
        if (ch == L'q' && next != L'u') {
            return true;
        }
        // 'v' ('м' on RU) never ends a word and never precedes impossible consonants
        if (ch == L'v') {
            if (next == 0) return true;
            if (next >= L'a' && next <= L'z' && !IsEnglishVowelChar(next) &&
                ContainsChar(L"bfjkpqtvwxz", next)) {
                return true;
            }
        }
        // 'w' ('ц' on RU) never precedes j/q/v/x/z
        if (ch == L'w' && ContainsChar(L"jqvxz", next)) {
            return true;
        }
    }

    // 'z' ('я' on RU) at end of word after a consonant other than 't' (waltz) or 'z' (jazz)
    if (len >= 2 && lower[len - 1] == L'z') {
        wchar_t prev = lower[len - 2];
        if (prev >= L'a' && prev <= L'z' && !IsEnglishVowelChar(prev) && prev != L't' && prev != L'z') {
            return true;
        }
    }

    // Impossible English n-grams that typically occur when Russian is typed on English layout
    static const wchar_t* const s_impossibleEnNgrams[] = {
        L"jx", L"jq", L"jz", L"vf", L"vg", L"vj", L"vk", L"vm", L"vn", L"vp", L"vt", L"vx", L"vz",
        L"gk", L"kj", L"wbb", L"hf", L"jn", L"gth", L"trk", L"ckj", L"jdj", L"ldj", L"jqy", L"scnh", L"cgh", L"ghb", L"juj", L"jcn"
    };
    for (const auto* ngram : s_impossibleEnNgrams) {
        if (lower.find(ngram) != std::wstring::npos) {
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

bool Detector::IsEnglishContractionOrPossessive(const std::wstring& lower) const {
    size_t apos = lower.find(L'\'');
    if (apos == std::wstring::npos || apos == 0 || apos + 1 >= lower.length()) {
        return false;
    }
    if (lower.find(L'\'', apos + 1) != std::wstring::npos) {
        return false;
    }
    std::wstring base = lower.substr(0, apos);
    std::wstring suffix = lower.substr(apos + 1);
    if (suffix != L"s" && suffix != L"t" && suffix != L"m" && suffix != L"d" &&
        suffix != L"re" && suffix != L"ve" && suffix != L"ll") {
        return false;
    }
    for (wchar_t ch : base) {
        if (ch < L'a' || ch > L'z') return false;
    }
    return !IsImpossibleEnglish(base);
}

bool Detector::IsCommonRussianWord(const std::wstring& word) const {
    return s_commonRuWords.find(Platform::ToLower(word)) != s_commonRuWords.end();
}

bool Detector::IsCommonEnglishWord(const std::wstring& word) const {
    return s_commonEnWords.find(Platform::ToLower(word)) != s_commonEnWords.end();
}

void Detector::SyncCustomDictionaryLocked() {
    uint64_t ver = Config::ConfigManager::Instance().GetWordDictionaryVersion();
    if (ver == cachedWordDictVersion_) {
        return;
    }
    cachedWordDictVersion_ = ver;

    customEnToRuTriggers_.clear();
    customRuToEnTriggers_.clear();
    customRuProtected_.clear();
    customEnProtected_.clear();

    auto rules = Config::ConfigManager::Instance().GetWordDictionarySnapshot();
    for (const auto& rule : rules) {
        std::wstring w = Platform::ToLower(rule.word);
        if (w.empty()) continue;

        bool hasCyr = false;
        bool hasLat = false;
        for (wchar_t ch : w) {
            if (IsCyrillicLetter(ch)) hasCyr = true;
            else if (IsLatinLetter(ch)) hasLat = true;
        }

        if (rule.layout == Config::AppLayout::Russian) {
            if (hasCyr) {
                customRuProtected_.insert(w);
                std::wstring trig = Platform::ToLower(LayoutEngine::Instance().RuToEn(w));
                if (!trig.empty() && trig != w) {
                    customEnToRuTriggers_.insert(trig);
                }
            } else {
                customEnToRuTriggers_.insert(w);
                std::wstring tgt = Platform::ToLower(LayoutEngine::Instance().EnToRu(w));
                if (!tgt.empty() && tgt != w) {
                    customRuProtected_.insert(tgt);
                }
            }
        } else if (rule.layout == Config::AppLayout::English) {
            if (hasLat) {
                customEnProtected_.insert(w);
                std::wstring trig = Platform::ToLower(LayoutEngine::Instance().EnToRu(w));
                if (!trig.empty() && trig != w) {
                    customRuToEnTriggers_.insert(trig);
                }
            } else {
                customRuToEnTriggers_.insert(w);
                std::wstring tgt = Platform::ToLower(LayoutEngine::Instance().RuToEn(w));
                if (!tgt.empty() && tgt != w) {
                    customEnProtected_.insert(tgt);
                }
            }
        }
    }
}

DetectedSwitch Detector::EvaluateWord(wchar_t delimiter) {
    std::lock_guard<std::mutex> lock(mutex_);
    DetectedSwitch result;
    if (currentBuffer_.empty()) {
        lastWord_.clear();
        lastDelimiter_ = 0;
        return result;
    }

    std::wstring word = currentBuffer_;
    lastWord_ = word;
    lastDelimiter_ = delimiter;
    currentBuffer_.clear();

    SyncCustomDictionaryLocked();

    std::wstring wordLower = Platform::ToLower(word);
    bool hasLatinLetter = false;
    bool hasCyrillicLetter = false;
    for (wchar_t ch : wordLower) {
        if (IsLatinLetter(ch)) hasLatinLetter = true;
        else if (IsCyrillicLetter(ch)) hasCyrillicLetter = true;
    }

    // 0. Check user custom auto-switch dictionary first (overrides length/vowel heuristics and collisions)
    if (!hasCyrillicLetter && (!customEnToRuTriggers_.empty() || !customEnProtected_.empty())) {
        size_t enStemEnd = wordLower.length();
        while (enStemEnd > 0 && IsEnPunctOnRuLayout(wordLower[enStemEnd - 1])) {
            enStemEnd--;
        }
        std::wstring enStemForRu = wordLower.substr(0, enStemEnd);
        if (customEnToRuTriggers_.count(wordLower) ||
            (!enStemForRu.empty() && customEnToRuTriggers_.count(enStemForRu))) {
            result.shouldSwitch = true;
            result.currentLang = WordLanguage::English;
            result.targetLang = WordLanguage::Russian;
            result.originalWord = word;
            result.convertedWord = LayoutEngine::Instance().EnToRu(word);
            result.delimiter = delimiter;
            return result;
        }
    }

    if (hasCyrillicLetter && (!customRuToEnTriggers_.empty() || !customEnProtected_.empty())) {
        size_t ruStemEnd = wordLower.length();
        while (ruStemEnd > 0 && IsTrailingPunct(wordLower[ruStemEnd - 1])) {
            ruStemEnd--;
        }
        std::wstring ruStemForEn = wordLower.substr(0, ruStemEnd);

        std::wstring convertedToEn = LayoutEngine::Instance().RuToEn(word);
        std::wstring convertedEnLower = Platform::ToLower(convertedToEn);
        size_t enStemEnd = convertedEnLower.length();
        while (enStemEnd > 0 && IsTrailingPunct(convertedEnLower[enStemEnd - 1])) {
            enStemEnd--;
        }
        std::wstring enStemFromRu = convertedEnLower.substr(0, enStemEnd);

        if (customRuToEnTriggers_.count(wordLower) ||
            (!ruStemForEn.empty() && customRuToEnTriggers_.count(ruStemForEn)) ||
            (!enStemFromRu.empty() && customEnProtected_.count(enStemFromRu))) {
            result.shouldSwitch = true;
            result.currentLang = WordLanguage::Russian;
            result.targetLang = WordLanguage::English;
            result.originalWord = word;
            result.convertedWord = convertedToEn;
            result.delimiter = delimiter;
            return result;
        }
    }

    // Protect words already in their target dictionary layout from heuristic switching
    if (hasCyrillicLetter && !customRuProtected_.empty()) {
        size_t plainEnd = wordLower.length();
        while (plainEnd > 0 && IsTrailingPunct(wordLower[plainEnd - 1])) {
            plainEnd--;
        }
        std::wstring plainStem = wordLower.substr(0, plainEnd);
        if (customRuProtected_.count(wordLower) || (!plainStem.empty() && customRuProtected_.count(plainStem))) {
            return result;
        }
    }
    if (hasLatinLetter && !customEnProtected_.empty()) {
        size_t plainEnd = wordLower.length();
        while (plainEnd > 0 && IsTrailingPunct(wordLower[plainEnd - 1])) {
            plainEnd--;
        }
        std::wstring plainStem = wordLower.substr(0, plainEnd);
        if (customEnProtected_.count(wordLower) || (!plainStem.empty() && customEnProtected_.count(plainStem))) {
            return result;
        }
    }

    // Check for numbers, code symbols, or mixed non-word symbols
    bool hasAlphaOrMapped = false;
    for (wchar_t ch : word) {
        if (IsLatinLetter(ch) || IsCyrillicLetter(ch)) {
            hasAlphaOrMapped = true;
        } else if ((ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'\\' ||
                   ch == L'@' || ch == L'#' || ch == L'=' || ch == L'+' ||
                   ch == L'*' || ch == L'%' || ch == L'|' || ch == L'-') {
            return result; // Code, path, hyphenated token, or number
        }
    }

    if (!hasAlphaOrMapped || word.length() < 2) {
        return result;
    }

    // 1. Check if word is typed in EN layout
    bool isEnLayout = true;
    hasLatinLetter = false;
    for (wchar_t ch : wordLower) {
        if (IsLatinLetter(ch)) {
            hasLatinLetter = true;
        } else if (!IsEnMappedLetterPunct(ch) && !IsTrailingPunct(ch)) {
            isEnLayout = false;
            break;
        }
    }

    // 2. Check if word is typed in RU layout
    bool isRuLayout = true;
    hasCyrillicLetter = false;
    for (wchar_t ch : wordLower) {
        if ((ch >= L'а' && ch <= L'я') || ch == L'ё') {
            hasCyrillicLetter = true;
        } else if (!IsRuLayoutChar(ch)) {
            isRuLayout = false;
            break;
        }
    }

    if (isEnLayout && hasLatinLetter) {
        // Strip trailing sentence punctuation (e.g. "test.", "this,", "ghbdtn?", "ghbdtn/")
        size_t stemEnd = wordLower.length();
        while (stemEnd > 0 && IsTrailingPunct(wordLower[stemEnd - 1])) {
            stemEnd--;
        }
        if (stemEnd == 0) {
            return result;
        }

        std::wstring stemLower = wordLower.substr(0, stemEnd);

        // If any non-letter punctuation like '/', '?', '&', '^', '$', '!' appears inside the stem, treat as path/code
        for (wchar_t ch : stemLower) {
            if (ch == L'/' || ch == L'?' || ch == L'&' || ch == L'^' || ch == L'$' || ch == L'!') {
                return result;
            }
        }

        // Guard against falsely translating real English words, contractions, or English words with trailing punctuation
        if (IsCommonEnglishWord(wordLower) || IsCommonEnglishWord(stemLower) ||
            IsEnglishContractionOrPossessive(stemLower)) {
            return result;
        }

        // Check internal layout punctuation inside the stem only (ignores trailing . or , on real English words)
        bool hasInternalLayoutPunct = false;
        for (wchar_t ch : stemLower) {
            if (IsEnMappedLetterPunct(ch)) {
                hasInternalLayoutPunct = true;
                break;
            }
        }

        std::wstring fullConverted = LayoutEngine::Instance().EnToRu(word);
        std::wstring fullConvertedLower = Platform::ToLower(fullConverted);
        std::wstring stemRuLower = Platform::ToLower(LayoutEngine::Instance().EnToRu(word.substr(0, stemEnd)));

        bool isKnownRuFull = IsCommonRussianWord(fullConvertedLower);
        bool isKnownRuStem = IsCommonRussianWord(stemRuLower);
        bool impossibleEn = IsImpossibleEnglish(stemLower);
        bool impossibleRuFull = IsImpossibleRussian(fullConvertedLower);
        bool impossibleRuStem = IsImpossibleRussian(stemRuLower);
        bool hasRuVowelsFull = HasRussianVowels(fullConvertedLower);
        bool hasRuVowelsStem = HasRussianVowels(stemRuLower);

        bool shouldConvert = false;
        if (isKnownRuFull || isKnownRuStem) {
            shouldConvert = true;
        } else if (hasInternalLayoutPunct && hasRuVowelsFull && !impossibleRuFull) {
            // Words like ,scnhj (быстро), gkj[j (плохо), vj;yj (можно), 'nj (это), k.,k. (люблю)
            shouldConvert = true;
        } else if (impossibleEn && hasRuVowelsFull && !impossibleRuFull && stemLower.length() >= 3) {
            // Russian words with non-English stems, including trailing '.' -> 'ю' (e.g. htif. -> решаю) or trailing punct (ghbdtn?)
            shouldConvert = true;
        } else if (impossibleEn && hasRuVowelsStem && !impossibleRuStem && stemLower.length() >= 3) {
            shouldConvert = true;
        }

        if (shouldConvert) {
            result.shouldSwitch = true;
            result.currentLang = WordLanguage::English;
            result.targetLang = WordLanguage::Russian;
            result.originalWord = word;
            result.convertedWord = fullConverted;
            result.delimiter = delimiter;
            return result;
        }
    } else if (isRuLayout && hasCyrillicLetter) {
        // Strip trailing Russian punctuation (e.g. "привет,", "рук.", "црфе,")
        size_t ruStemEnd = wordLower.length();
        while (ruStemEnd > 0 && IsTrailingPunct(wordLower[ruStemEnd - 1])) {
            ruStemEnd--;
        }
        if (ruStemEnd == 0) {
            return result;
        }

        std::wstring ruStemLower = wordLower.substr(0, ruStemEnd);
        if (IsCommonRussianWord(wordLower) || IsCommonRussianWord(ruStemLower)) {
            return result; // Explicitly a known Russian word!
        }

        std::wstring convertedToEn = LayoutEngine::Instance().RuToEn(word);
        std::wstring convertedEnLower = Platform::ToLower(convertedToEn);

        // Strip trailing EN punctuation from converted text (covers "руддщб" -> "hello,", "цщкдвю" -> "world.", "црфе," -> "what?")
        size_t enStemEnd = convertedEnLower.length();
        while (enStemEnd > 0 && IsTrailingPunct(convertedEnLower[enStemEnd - 1])) {
            enStemEnd--;
        }
        if (enStemEnd == 0) {
            return result;
        }
        std::wstring stemEnLower = convertedEnLower.substr(0, enStemEnd);

        bool isKnownEn = IsCommonEnglishWord(convertedEnLower) ||
                         IsCommonEnglishWord(stemEnLower) ||
                         IsEnglishContractionOrPossessive(stemEnLower);
        bool impossibleRu = IsImpossibleRussian(ruStemLower);
        bool impossibleEn = IsImpossibleEnglish(stemEnLower);
        bool hasEnVowels = HasEnglishVowels(stemEnLower);

        bool shouldConvert = false;
        if (isKnownEn) {
            shouldConvert = true;
        } else if (impossibleRu && hasEnVowels && !impossibleEn) {
            shouldConvert = true;
        }

        if (shouldConvert) {
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
    // Reserved for future: real-time detection of impossible letter sequences
    // during typing (before word delimiter is pressed)
    return DetectedSwitch{};
}

} // namespace Core
