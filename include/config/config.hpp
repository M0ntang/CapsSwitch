#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace Config {

struct AppConfig {
    bool autoSwitch = true;
    bool capsLockSwitch = true;
    bool doubleCapsConvert = true;
    bool passwordGuard = true;
    bool autostart = false;
    std::vector<std::wstring> exclusions = {
        L"cs2.exe",
        L"dota2.exe",
        L"valorant.exe",
        L"cmd.exe",
        L"powershell.exe",
        L"windowsterminal.exe"
    };

    // Helper functions
    bool IsExcluded(const std::wstring& processName) const;
    void AddExclusion(const std::wstring& processName);
    void RemoveExclusion(const std::wstring& processName);
};

// Singleton configuration manager
class ConfigManager {
public:
    static ConfigManager& Instance();

    const AppConfig& Get() const { return config_; }
    AppConfig& GetMutable() { return config_; }

    bool Load();
    bool Save();

    // Registry autostart management (strictly HKCU\...\Run)
    bool SyncAutostartWithRegistry();
    static bool SetAutostartRegistry(bool enable);
    static bool IsAutostartInRegistry();

    std::wstring GetConfigFilePath() const;
    std::wstring GetExecutablePath() const;

private:
    ConfigManager();
    AppConfig config_;
};

} // namespace Config
