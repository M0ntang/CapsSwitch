#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include "config/config.hpp"

namespace UI {

enum class ProgramsTab {
    AppLayouts,
    Exclusions
};

class SettingsWindow {
public:
    static SettingsWindow& Instance();

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const;

    HWND GetHwnd() const { return hWnd_; }

private:
    SettingsWindow();
    ~SettingsWindow();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hdc);
    void DrawHeader(HDC hdc);
    void DrawSettingsCard(HDC hdc);
    void DrawProgramsCard(HDC hdc);

    void DrawToggle(HDC hdc, int x, int y, bool state, const wchar_t* title, const wchar_t* subtitle, RECT& hitRect);

    void UpdateControls();
    void UpdateTabVisibility();
    void ShowAppPickerMenu(HWND anchorBtn, HWND targetEdit);
    void ToggleSelectedAppLayout();
    void SaveAndApply();

    HWND hWnd_ = nullptr;

    // Tab 0: Per-app layout rules
    HWND hListAppLayouts_ = nullptr;
    HWND hEditAppProc_ = nullptr;
    HWND hBtnPickApp_ = nullptr;
    HWND hBtnAppAdd_ = nullptr;
    HWND hBtnAppToggleLang_ = nullptr;
    HWND hBtnAppRemove_ = nullptr;

    // Tab 1: Exclusions
    HWND hListBox_ = nullptr;
    HWND hEditExcl_ = nullptr;
    HWND hBtnPickExcl_ = nullptr;
    HWND hBtnAdd_ = nullptr;
    HWND hBtnRemove_ = nullptr;

    // Footer
    HWND hBtnSave_ = nullptr;
    HWND hBtnClose_ = nullptr;

    HFONT hFontTitle_ = nullptr;
    HFONT hFontSub_ = nullptr;
    HFONT hFontRegular_ = nullptr;
    HFONT hFontBold_ = nullptr;

    ProgramsTab activeTab_ = ProgramsTab::AppLayouts;
    Config::AppLayout selectedAppLayout_ = Config::AppLayout::Russian;
    std::vector<Config::AppLayoutRule> displayedAppRules_;

    RECT rcStatusBadge_ = {};
    RECT rcToggleAuto_ = {};
    RECT rcToggleCaps_ = {};
    RECT rcToggleDoubleCaps_ = {};
    RECT rcTogglePwd_ = {};
    RECT rcToggleAutoStart_ = {};
    RECT rcTabAppLayouts_ = {};
    RECT rcTabExclusions_ = {};
    RECT rcLangEn_ = {};
    RECT rcLangRu_ = {};

    HBRUSH hBrBackground_ = nullptr;
    HBRUSH hBrCard_ = nullptr;
    HBRUSH hBrBorder_ = nullptr;
};

} // namespace UI
