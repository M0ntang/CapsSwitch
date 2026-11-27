#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>

namespace Config {

enum class AppLayout {
    None = 0,
    Russian,
    English
};

enum class ThemeMode {
    Auto = 0,
    Light,
    Dark
};

struct AppLayoutRule {
    std::wstring processName;
    AppLayout layout = AppLayout::English;

    bool operator==(const AppLayoutRule& other) const {
        return processName == other.processName && layout == other.layout;
    }
};

struct WordSwitchRule {
    std::wstring word;
    AppLayout layout = AppLayout::Russian;

    bool operator==(const WordSwitchRule& other) const {
        return word == other.word && layout == other.layout;
    }
};

struct AppConfig {
    bool autoSwitch = true;
    bool capsLockSwitch = true;
    bool capsHoldSwitch = true;
    bool doubleCapsConvert = true;
    bool passwordGuard = true;
    bool autostart = false;
    ThemeMode theme = ThemeMode::Auto;
    std::vector<std::wstring> exclusions = {
        L"cs2.exe",
        L"dota2.exe",
        L"valorant.exe",
        L"cmd.exe",
        L"powershell.exe",
        L"windowsterminal.exe"
    };
    std::vector<AppLayoutRule> appLayouts;
    std::vector<WordSwitchRule> wordDictionary = {
        { L"спс", AppLayout::Russian },
        { L"го", AppLayout::Russian },
        { L"хз", AppLayout::Russian },
        { L"пж", AppLayout::Russian },
        { L"норм", AppLayout::Russian },
        { L"кст", AppLayout::Russian },
        { L"мб", AppLayout::Russian },
        { L"docker", AppLayout::English },
        { L"steam", AppLayout::English },
        { L"discord", AppLayout::English },
        { L"telegram", AppLayout::English },
        { L"youtube", AppLayout::English },
        { L"google", AppLayout::English },
        { L"github", AppLayout::English },
        { L"chrome", AppLayout::English },
        { L"vpn", AppLayout::English }
    };

    // Helper functions
    bool IsExcluded(const std::wstring& processName) const;
    void AddExclusion(const std::wstring& processName);
    void RemoveExclusion(const std::wstring& processName);

    AppLayout GetAppLayout(const std::wstring& processName) const;
    void SetAppLayout(const std::wstring& processName, AppLayout layout);
    void RemoveAppLayout(const std::wstring& processName);

    AppLayout GetWordRule(const std::wstring& word) const;
    void SetWordRule(const std::wstring& word, AppLayout layout);
    void RemoveWordRule(const std::wstring& word);
};

std::string SerializeConfigJson(const AppConfig& cfg);
void ParseConfigJson(const std::string& content, AppConfig& cfg);

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
    bool CapsHoldSwitch() const { return capsHoldSwitchAtomic_.load(std::memory_order_relaxed); }
    bool DoubleCapsConvert() const { return doubleCapsConvertAtomic_.load(std::memory_order_relaxed); }
    bool PasswordGuardEnabled() const { return passwordGuardAtomic_.load(std::memory_order_relaxed); }
    bool HasAppLayoutRules() const { return hasAppLayoutRulesAtomic_.load(std::memory_order_relaxed); }
    uint64_t GetAppLayoutsVersion() const { return appLayoutsVersion_.load(std::memory_order_relaxed); }
    bool HasWordDictionaryRules() const { return hasWordDictionaryAtomic_.load(std::memory_order_relaxed); }
    uint64_t GetWordDictionaryVersion() const { return wordDictionaryVersion_.load(std::memory_order_relaxed); }

    bool IsProcessExcluded(const std::wstring& processName) const;
    AppLayout GetProcessLayout(const std::wstring& processName) const;
    std::vector<WordSwitchRule> GetWordDictionarySnapshot() const;
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
    void SyncAtomicsLocked(bool bumpAppLayoutsVersion = false, bool bumpWordDictVersion = false);

    mutable std::mutex mutex_;
    AppConfig config_;
    std::atomic<bool> autoSwitchAtomic_{ true };
    std::atomic<bool> capsLockSwitchAtomic_{ true };
    std::atomic<bool> capsHoldSwitchAtomic_{ true };
    std::atomic<bool> doubleCapsConvertAtomic_{ true };
    std::atomic<bool> passwordGuardAtomic_{ true };
    std::atomic<bool> hasAppLayoutRulesAtomic_{ false };
    std::atomic<uint64_t> appLayoutsVersion_{ 0 };
    std::atomic<bool> hasWordDictionaryAtomic_{ true };
    std::atomic<uint64_t> wordDictionaryVersion_{ 1 };
};

} // namespace Config
