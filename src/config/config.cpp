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

ConfigManager& ConfigManager::Instance() {
    static ConfigManager s_instance;
    return s_instance;
}

ConfigManager::ConfigManager() {
    Load();
}

void ConfigManager::SyncAtomicsLocked() {
    autoSwitchAtomic_.store(config_.autoSwitch, std::memory_order_relaxed);
    capsLockSwitchAtomic_.store(config_.capsLockSwitch, std::memory_order_relaxed);
    doubleCapsConvertAtomic_.store(config_.doubleCapsConvert, std::memory_order_relaxed);
    passwordGuardAtomic_.store(config_.passwordGuard, std::memory_order_relaxed);
}

AppConfig ConfigManager::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool ConfigManager::IsProcessExcluded(const std::wstring& processName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.IsExcluded(processName);
}

void ConfigManager::Update(const std::function<void(AppConfig&)>& updater) {
    std::lock_guard<std::mutex> lock(mutex_);
    updater(config_);
    SyncAtomicsLocked();
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
        SyncAtomicsLocked();
        SaveLocked();
        return true;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Lightweight parsing of JSON fields
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

    config_.autoSwitch = parseBool("auto_switch", true);
    config_.capsLockSwitch = parseBool("caps_lock_switch", true);
    config_.doubleCapsConvert = parseBool("double_caps_convert", true);
    config_.passwordGuard = parseBool("password_guard", true);
    config_.autostart = parseBool("autostart", false);

    // Parse exclusions array
    size_t exclPos = content.find("\"exclusions\"");
    if (exclPos != std::string::npos) {
        size_t arrayStart = content.find('[', exclPos);
        size_t arrayEnd = (arrayStart != std::string::npos) ? content.find(']', arrayStart) : std::string::npos;
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            config_.exclusions.clear();
            std::string arrStr = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            size_t idx = 0;
            while (idx < arrStr.length()) {
                size_t q1 = arrStr.find('\"', idx);
                if (q1 == std::string::npos) break;
                size_t q2 = arrStr.find('\"', q1 + 1);
                if (q2 == std::string::npos) break;
                std::string item = arrStr.substr(q1 + 1, q2 - q1 - 1);
                if (!item.empty()) {
                    config_.AddExclusion(Platform::Utf8ToWide(item));
                }
                idx = q2 + 1;
            }
        }
    }

    SyncAtomicsLocked();
    SyncAutostartLocked();
    return true;
}

bool ConfigManager::SaveLocked() {
    std::wstring configPath = GetConfigFilePath();
    std::ofstream file(configPath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"auto_switch\": " << (config_.autoSwitch ? "true" : "false") << ",\n";
    ss << "  \"caps_lock_switch\": " << (config_.capsLockSwitch ? "true" : "false") << ",\n";
    ss << "  \"double_caps_convert\": " << (config_.doubleCapsConvert ? "true" : "false") << ",\n";
    ss << "  \"password_guard\": " << (config_.passwordGuard ? "true" : "false") << ",\n";
    ss << "  \"autostart\": " << (config_.autostart ? "true" : "false") << ",\n";
    ss << "  \"exclusions\": [\n";

    for (size_t i = 0; i < config_.exclusions.size(); ++i) {
        std::string utf8Item = Platform::WideToUtf8(config_.exclusions[i]);
        ss << "    \"" << utf8Item << "\"";
        if (i + 1 < config_.exclusions.size()) {
            ss << ",";
        }
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";

    std::string out = ss.str();
    file.write(out.data(), out.size());
    file.close();

    SyncAtomicsLocked();
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
