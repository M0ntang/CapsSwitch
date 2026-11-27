#include "config/config.hpp"
#include "platform/win32_utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Config {

static const wchar_t* REG_KEY_RUN = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* REG_VALUE_NAME = L"CapsSwitch";

static std::wstring SanitizeProcessName(const std::wstring& raw) {
    size_t start = raw.find_first_not_of(L" \t\r\n\"'");
    if (start == std::wstring::npos) return L"";
    size_t end = raw.find_last_not_of(L" \t\r\n\"'");
    std::wstring cleaned = raw.substr(start, end - start + 1);

    size_t lastSlash = cleaned.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        cleaned = cleaned.substr(lastSlash + 1);
    }

    cleaned = Platform::ToLower(cleaned);
    if (cleaned.empty()) return L"";

    if (cleaned.length() < 4 || cleaned.compare(cleaned.length() - 4, 4, L".exe") != 0) {
        cleaned += L".exe";
    }
    if (cleaned == L".exe") return L"";
    return cleaned;
}

static std::wstring SanitizeWord(const std::wstring& raw) {
    size_t start = raw.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = raw.find_last_not_of(L" \t\r\n");
    std::wstring cleaned = raw.substr(start, end - start + 1);
    if (cleaned.empty()) return L"";
    if (cleaned.find_first_of(L" \t\r\n") != std::wstring::npos) {
        return L"";
    }
    return Platform::ToLower(cleaned);
}

static std::string EscapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '\"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:   out += ch; break;
        }
    }
    return out;
}

static bool ParseJsonString(const std::string& text, size_t& pos, std::string& out) {
    if (pos >= text.size() || text[pos] != '\"') return false;
    ++pos;
    out.clear();
    while (pos < text.size()) {
        char ch = text[pos++];
        if (ch == '\\') {
            if (pos >= text.size()) return false;
            char esc = text[pos++];
            switch (esc) {
            case '\"': out += '\"'; break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            default:   out += esc;  break;
            }
        } else if (ch == '\"') {
            return true;
        } else {
            out += ch;
        }
    }
    return false;
}

static size_t FindMatchingBrace(const std::string& text, size_t openBracePos) {
    int depth = 0;
    bool inString = false;
    for (size_t i = openBracePos; i < text.size(); ++i) {
        char ch = text[i];
        if (inString) {
            if (ch == '\\') {
                ++i;
            } else if (ch == '\"') {
                inString = false;
            }
        } else {
            if (ch == '\"') {
                inString = true;
            } else if (ch == '{') {
                ++depth;
            } else if (ch == '}') {
                --depth;
                if (depth == 0) {
                    return i;
                }
            }
        }
    }
    return std::string::npos;
}

bool AppConfig::IsExcluded(const std::wstring& processName) const {
    if (processName.empty()) return false;
    std::wstring lower = Platform::ToLower(processName);
    for (const auto& excl : exclusions) {
        if (Platform::ToLower(excl) == lower) {
            return true;
        }
    }
    return false;
}

void AppConfig::AddExclusion(const std::wstring& processName) {
    std::wstring normalized = SanitizeProcessName(processName);
    if (normalized.empty()) return;
    if (!IsExcluded(normalized)) {
        exclusions.push_back(normalized);
    }
}

void AppConfig::RemoveExclusion(const std::wstring& processName) {
    std::wstring lower = Platform::ToLower(processName);
    exclusions.erase(
        std::remove_if(exclusions.begin(), exclusions.end(),
            [&lower](const std::wstring& s) { return Platform::ToLower(s) == lower; }),
        exclusions.end()
    );
}

AppLayout AppConfig::GetAppLayout(const std::wstring& processName) const {
    std::wstring normalized = SanitizeProcessName(processName);
    if (normalized.empty()) return AppLayout::None;
    for (const auto& rule : appLayouts) {
        if (Platform::ToLower(rule.processName) == normalized) {
            return rule.layout;
        }
    }
    return AppLayout::None;
}

void AppConfig::SetAppLayout(const std::wstring& processName, AppLayout layout) {
    std::wstring normalized = SanitizeProcessName(processName);
    if (normalized.empty()) return;
    if (layout == AppLayout::None) {
        RemoveAppLayout(normalized);
        return;
    }
    for (auto& rule : appLayouts) {
        if (Platform::ToLower(rule.processName) == normalized) {
            rule.processName = normalized;
            rule.layout = layout;
            return;
        }
    }
    appLayouts.push_back({ normalized, layout });
}

void AppConfig::RemoveAppLayout(const std::wstring& processName) {
    std::wstring normalized = SanitizeProcessName(processName);
    if (normalized.empty()) return;
    appLayouts.erase(
        std::remove_if(appLayouts.begin(), appLayouts.end(),
            [&normalized](const AppLayoutRule& r) { return Platform::ToLower(r.processName) == normalized; }),
        appLayouts.end()
    );
}

AppLayout AppConfig::GetWordRule(const std::wstring& word) const {
    std::wstring normalized = SanitizeWord(word);
    if (normalized.empty()) return AppLayout::None;
    for (const auto& rule : wordDictionary) {
        if (Platform::ToLower(rule.word) == normalized) {
            return rule.layout;
        }
    }
    return AppLayout::None;
}

void AppConfig::SetWordRule(const std::wstring& word, AppLayout layout) {
    std::wstring normalized = SanitizeWord(word);
    if (normalized.empty()) return;
    if (layout == AppLayout::None) {
        RemoveWordRule(normalized);
        return;
    }
    for (auto& rule : wordDictionary) {
        if (Platform::ToLower(rule.word) == normalized) {
            rule.word = normalized;
            rule.layout = layout;
            return;
        }
    }
    wordDictionary.push_back({ normalized, layout });
}

void AppConfig::RemoveWordRule(const std::wstring& word) {
    std::wstring normalized = SanitizeWord(word);
    if (normalized.empty()) return;
    wordDictionary.erase(
        std::remove_if(wordDictionary.begin(), wordDictionary.end(),
            [&normalized](const WordSwitchRule& r) { return Platform::ToLower(r.word) == normalized; }),
        wordDictionary.end()
    );
}

void ParseConfigJson(const std::string& content, AppConfig& cfg) {
    auto parseBool = [&content](const std::string& key, bool defaultVal) -> bool {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) return defaultVal;
        size_t truePos = content.find("true", colon);
        size_t falsePos = content.find("false", colon);
        size_t nextComma = content.find_first_of(",}", colon);
        if (truePos != std::string::npos && (nextComma == std::string::npos || truePos < nextComma)) {
            return true;
        }
        if (falsePos != std::string::npos && (nextComma == std::string::npos || falsePos < nextComma)) {
            return false;
        }
        return defaultVal;
    };

    cfg.autoSwitch = parseBool("auto_switch", true);
    cfg.capsLockSwitch = parseBool("caps_lock_switch", true);
    cfg.capsHoldSwitch = parseBool("caps_hold_switch", true);
    cfg.doubleCapsConvert = parseBool("double_caps_convert", true);
    cfg.passwordGuard = parseBool("password_guard", true);
    cfg.autostart = parseBool("autostart", false);

    // Parse theme
    cfg.theme = ThemeMode::Auto;
    {
        size_t themePos = content.find("\"theme\"");
        if (themePos != std::string::npos) {
            size_t colon = content.find(':', themePos);
            size_t nextComma = (colon != std::string::npos) ? content.find_first_of(",}", colon) : std::string::npos;
            size_t quotePos = (colon != std::string::npos) ? content.find('\"', colon + 1) : std::string::npos;
            if (quotePos != std::string::npos && (nextComma == std::string::npos || quotePos < nextComma)) {
                std::string themeVal;
                size_t p = quotePos;
                if (ParseJsonString(content, p, themeVal)) {
                    std::string lowerTheme = Platform::ToLower(themeVal);
                    if (lowerTheme == "light") {
                        cfg.theme = ThemeMode::Light;
                    } else if (lowerTheme == "dark") {
                        cfg.theme = ThemeMode::Dark;
                    }
                }
            }
        }
    }

    // Parse exclusions array
    size_t exclPos = content.find("\"exclusions\"");
    if (exclPos != std::string::npos) {
        size_t arrayStart = content.find('[', exclPos);
        size_t arrayEnd = (arrayStart != std::string::npos) ? content.find(']', arrayStart) : std::string::npos;
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            cfg.exclusions.clear();
            std::string arrStr = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            size_t idx = 0;
            while (idx < arrStr.length()) {
                size_t q1 = arrStr.find('\"', idx);
                if (q1 == std::string::npos) break;
                std::string item;
                size_t pos = q1;
                if (!ParseJsonString(arrStr, pos, item)) break;
                if (!item.empty()) {
                    cfg.AddExclusion(Platform::Utf8ToWide(item));
                }
                idx = pos;
            }
        }
    }

    // Parse app_layouts object
    cfg.appLayouts.clear();
    size_t appLayoutsPos = content.find("\"app_layouts\"");
    if (appLayoutsPos != std::string::npos) {
        size_t colonPos = content.find(':', appLayoutsPos);
        size_t objStart = (colonPos != std::string::npos) ? content.find('{', colonPos) : std::string::npos;
        size_t objEnd = (objStart != std::string::npos) ? FindMatchingBrace(content, objStart) : std::string::npos;
        if (objStart != std::string::npos && objEnd != std::string::npos) {
            size_t pos = objStart + 1;
            while (pos < objEnd) {
                size_t keyStart = content.find('\"', pos);
                if (keyStart == std::string::npos || keyStart >= objEnd) break;
                std::string key;
                pos = keyStart;
                if (!ParseJsonString(content, pos, key)) break;

                size_t pairColon = content.find(':', pos);
                if (pairColon == std::string::npos || pairColon >= objEnd) break;

                size_t valStart = content.find('\"', pairColon + 1);
                if (valStart == std::string::npos || valStart >= objEnd) break;
                std::string val;
                pos = valStart;
                if (!ParseJsonString(content, pos, val)) break;

                std::string lowerVal = Platform::ToLower(val);
                if (lowerVal == "ru" || lowerVal == "russian") {
                    cfg.SetAppLayout(Platform::Utf8ToWide(key), AppLayout::Russian);
                } else if (lowerVal == "en" || lowerVal == "english") {
                    cfg.SetAppLayout(Platform::Utf8ToWide(key), AppLayout::English);
                }
            }
        }
    }

    // Parse switch_dictionary object (preserve defaults if absent in older config.json)
    size_t dictPos = content.find("\"switch_dictionary\"");
    if (dictPos != std::string::npos) {
        size_t colonPos = content.find(':', dictPos);
        size_t objStart = (colonPos != std::string::npos) ? content.find('{', colonPos) : std::string::npos;
        size_t objEnd = (objStart != std::string::npos) ? FindMatchingBrace(content, objStart) : std::string::npos;
        if (objStart != std::string::npos && objEnd != std::string::npos) {
            cfg.wordDictionary.clear();
            size_t pos = objStart + 1;
            while (pos < objEnd) {
                size_t keyStart = content.find('\"', pos);
                if (keyStart == std::string::npos || keyStart >= objEnd) break;
                std::string key;
                pos = keyStart;
                if (!ParseJsonString(content, pos, key)) break;

                size_t pairColon = content.find(':', pos);
                if (pairColon == std::string::npos || pairColon >= objEnd) break;

                size_t valStart = content.find('\"', pairColon + 1);
                if (valStart == std::string::npos || valStart >= objEnd) break;
                std::string val;
                pos = valStart;
                if (!ParseJsonString(content, pos, val)) break;

                std::string lowerVal = Platform::ToLower(val);
                if (lowerVal == "ru" || lowerVal == "russian") {
                    cfg.SetWordRule(Platform::Utf8ToWide(key), AppLayout::Russian);
                } else if (lowerVal == "en" || lowerVal == "english") {
                    cfg.SetWordRule(Platform::Utf8ToWide(key), AppLayout::English);
                }
            }
        }
    }
}

std::string SerializeConfigJson(const AppConfig& cfg) {
    const char* themeStr = "auto";
    if (cfg.theme == ThemeMode::Light) themeStr = "light";
    else if (cfg.theme == ThemeMode::Dark) themeStr = "dark";

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"auto_switch\": " << (cfg.autoSwitch ? "true" : "false") << ",\n";
    ss << "  \"caps_lock_switch\": " << (cfg.capsLockSwitch ? "true" : "false") << ",\n";
    ss << "  \"caps_hold_switch\": " << (cfg.capsHoldSwitch ? "true" : "false") << ",\n";
    ss << "  \"double_caps_convert\": " << (cfg.doubleCapsConvert ? "true" : "false") << ",\n";
    ss << "  \"password_guard\": " << (cfg.passwordGuard ? "true" : "false") << ",\n";
    ss << "  \"autostart\": " << (cfg.autostart ? "true" : "false") << ",\n";
    ss << "  \"theme\": \"" << themeStr << "\",\n";
    ss << "  \"exclusions\": [\n";

    for (size_t i = 0; i < cfg.exclusions.size(); ++i) {
        std::string utf8Item = EscapeJsonString(Platform::WideToUtf8(cfg.exclusions[i]));
        ss << "    \"" << utf8Item << "\"";
        if (i + 1 < cfg.exclusions.size()) {
            ss << ",";
        }
        ss << "\n";
    }

    ss << "  ],\n";
    if (cfg.appLayouts.empty()) {
        ss << "  \"app_layouts\": {},\n";
    } else {
        ss << "  \"app_layouts\": {\n";
        for (size_t i = 0; i < cfg.appLayouts.size(); ++i) {
            const auto& rule = cfg.appLayouts[i];
            std::string utf8Proc = EscapeJsonString(Platform::WideToUtf8(rule.processName));
            const char* langStr = (rule.layout == AppLayout::Russian) ? "ru" : "en";
            ss << "    \"" << utf8Proc << "\": \"" << langStr << "\"";
            if (i + 1 < cfg.appLayouts.size()) {
                ss << ",";
            }
            ss << "\n";
        }
        ss << "  },\n";
    }

    if (cfg.wordDictionary.empty()) {
        ss << "  \"switch_dictionary\": {}\n";
    } else {
        ss << "  \"switch_dictionary\": {\n";
        for (size_t i = 0; i < cfg.wordDictionary.size(); ++i) {
            const auto& rule = cfg.wordDictionary[i];
            std::string utf8Word = EscapeJsonString(Platform::WideToUtf8(rule.word));
            const char* langStr = (rule.layout == AppLayout::Russian) ? "ru" : "en";
            ss << "    \"" << utf8Word << "\": \"" << langStr << "\"";
            if (i + 1 < cfg.wordDictionary.size()) {
                ss << ",";
            }
            ss << "\n";
        }
        ss << "  }\n";
    }
    ss << "}\n";
    return ss.str();
}

ConfigManager& ConfigManager::Instance() {
    static ConfigManager s_instance;
    return s_instance;
}

ConfigManager::ConfigManager() {
    Load();
}

void ConfigManager::SyncAtomicsLocked(bool bumpAppLayoutsVersion, bool bumpWordDictVersion) {
    autoSwitchAtomic_.store(config_.autoSwitch, std::memory_order_relaxed);
    capsLockSwitchAtomic_.store(config_.capsLockSwitch, std::memory_order_relaxed);
    capsHoldSwitchAtomic_.store(config_.capsHoldSwitch, std::memory_order_relaxed);
    doubleCapsConvertAtomic_.store(config_.doubleCapsConvert, std::memory_order_relaxed);
    passwordGuardAtomic_.store(config_.passwordGuard, std::memory_order_relaxed);
    hasAppLayoutRulesAtomic_.store(!config_.appLayouts.empty(), std::memory_order_relaxed);
    hasWordDictionaryAtomic_.store(!config_.wordDictionary.empty(), std::memory_order_relaxed);
    if (bumpAppLayoutsVersion) {
        appLayoutsVersion_.fetch_add(1, std::memory_order_relaxed);
    }
    if (bumpWordDictVersion) {
        wordDictionaryVersion_.fetch_add(1, std::memory_order_relaxed);
    }
}

AppConfig ConfigManager::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool ConfigManager::IsProcessExcluded(const std::wstring& processName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.IsExcluded(processName);
}

AppLayout ConfigManager::GetProcessLayout(const std::wstring& processName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.GetAppLayout(processName);
}

std::vector<WordSwitchRule> ConfigManager::GetWordDictionarySnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.wordDictionary;
}

void ConfigManager::Update(const std::function<void(AppConfig&)>& updater) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AppLayoutRule> prevRules = config_.appLayouts;
    std::vector<WordSwitchRule> prevWords = config_.wordDictionary;
    updater(config_);
    bool rulesChanged = (prevRules != config_.appLayouts);
    bool wordsChanged = (prevWords != config_.wordDictionary);
    SyncAtomicsLocked(rulesChanged, wordsChanged);
}

std::wstring ConfigManager::GetExecutablePath() {
    wchar_t buffer[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::wstring(buffer);
}

std::wstring ConfigManager::GetConfigFilePath() const {
    std::wstring exePath = GetExecutablePath();
    size_t lastSlash = exePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return exePath.substr(0, lastSlash + 1) + L"config.json";
    }
    return L"config.json";
}

bool ConfigManager::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstring configPath = GetConfigFilePath();
    std::ifstream file(configPath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        // If config doesn't exist, create it with default values next to .exe
        config_.autostart = IsAutostartInRegistry();
        SyncAtomicsLocked(true, true);
        SaveLocked();
        return true;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    ParseConfigJson(content, config_);

    SyncAtomicsLocked(true, true);
    SyncAutostartLocked();
    return true;
}

bool ConfigManager::SaveLocked() {
    std::wstring configPath = GetConfigFilePath();
    std::ofstream file(configPath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;

    std::string out = SerializeConfigJson(config_);
    file.write(out.data(), out.size());
    file.close();

    SyncAtomicsLocked(false, false);
    SetAutostartRegistry(config_.autostart);
    return true;
}

bool ConfigManager::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    return SaveLocked();
}

bool ConfigManager::SyncAutostartLocked() {
    bool inReg = IsAutostartInRegistry();
    if (config_.autostart != inReg) {
        SetAutostartRegistry(config_.autostart);
    }
    return true;
}

bool ConfigManager::SyncAutostartWithRegistry() {
    std::lock_guard<std::mutex> lock(mutex_);
    return SyncAutostartLocked();
}

bool ConfigManager::SetAutostartRegistry(bool enable) {
    HKEY hKey;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_RUN, 0, KEY_SET_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    if (enable) {
        std::wstring exePath = GetExecutablePath();
        std::wstring cmd = L"\"" + exePath + L"\"";
        res = RegSetValueExW(
            hKey,
            REG_VALUE_NAME,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(cmd.c_str()),
            static_cast<DWORD>((cmd.length() + 1) * sizeof(wchar_t))
        );
    } else {
        res = RegDeleteValueW(hKey, REG_VALUE_NAME);
    }

    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS || (!enable && res == ERROR_FILE_NOT_FOUND));
}

bool ConfigManager::IsAutostartInRegistry() {
    HKEY hKey;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_RUN, 0, KEY_QUERY_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    res = RegQueryValueExW(hKey, REG_VALUE_NAME, nullptr, &type, nullptr, &size);
    RegCloseKey(hKey);

    return (res == ERROR_SUCCESS);
}

} // namespace Config
