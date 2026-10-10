#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace UI {

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
    void DrawExclusionsCard(HDC hdc);
    void DrawFooter(HDC hdc);
    void DrawToggle(HDC hdc, int x, int y, bool state, const wchar_t* title, const wchar_t* subtitle, RECT& hitRect);

    void UpdateControls();
    void SaveAndApply();

    HWND hWnd_ = nullptr;
    HWND hListBox_ = nullptr;
    HWND hEditExcl_ = nullptr;
    HWND hBtnAdd_ = nullptr;
    HWND hBtnRemove_ = nullptr;
    HWND hBtnSave_ = nullptr;
    HWND hBtnClose_ = nullptr;

    HFONT hFontTitle_ = nullptr;
    HFONT hFontSub_ = nullptr;
    HFONT hFontRegular_ = nullptr;
    HFONT hFontBold_ = nullptr;

    RECT rcToggleAuto_ = {};
    RECT rcToggleCaps_ = {};
    RECT rcToggleDoubleCaps_ = {};
    RECT rcTogglePwd_ = {};
    RECT rcToggleAutoStart_ = {};

    HBRUSH hBrBackground_ = nullptr;
    HBRUSH hBrCard_ = nullptr;
    HBRUSH hBrBorder_ = nullptr;
};

} // namespace UI
