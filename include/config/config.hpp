#pragma once

#include <atomic>
#include <functional>
#include <mutex>
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

// Thread-safe singleton configuration manager
class ConfigManager {
public:
    static ConfigManager& Instance();

    const AppConfig& Get() const { return config_; }
    AppConfig& GetMutable() { return config_; }
    AppConfig GetSnapshot() const;

    // Lock-free atomic flag accessors for low-level hooks and worker threads
    bool AutoSwitch() const { return autoSwitchAtomic_.load(std::memory_order_relaxed); }
    bool CapsLockSwitch() const { return capsLockSwitchAtomic_.load(std::memory_order_relaxed); }
    bool DoubleCapsConvert() const { return doubleCapsConvertAtomic_.load(std::memory_order_relaxed); }
    bool PasswordGuardEnabled() const { return passwordGuardAtomic_.load(std::memory_order_relaxed); }

    bool IsProcessExcluded(const std::wstring& processName) const;
    void Update(const std::function<void(AppConfig&)>& updater);

    bool Load();
    bool Save();

    // Registry autostart management (strictly HKCU\...\Run)
    bool SyncAutostartWithRegistry();
    static bool SetAutostartRegistry(bool enable);
    static bool IsAutostartInRegistry();

    std::wstring GetConfigFilePath() const;
    static std::wstring GetExecutablePath();

private:
    ConfigManager();
    bool SaveLocked();
    bool SyncAutostartLocked();
    void SyncAtomicsLocked();

    mutable std::mutex mutex_;
    AppConfig config_;
    std::atomic<bool> autoSwitchAtomic_{ true };
    std::atomic<bool> capsLockSwitchAtomic_{ true };
    std::atomic<bool> doubleCapsConvertAtomic_{ true };
    std::atomic<bool> passwordGuardAtomic_{ true };
};

} // namespace Config
