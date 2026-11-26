#pragma once

#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <cstdint>
#include <string>
#include <vector>
#include "config/config.hpp"
#include "ui/fluent_theme.hpp"

namespace UI {

enum class SettingsTab {
    General = 0,
    AppLayouts = 1,
    Exclusions = 2
};

struct FluentTextInput {
    std::wstring text;
    std::wstring placeholder;
    D2D1_RECT_F rect{};
    size_t caretPos = 0;
    size_t selAnchor = 0;
    bool focused = false;
    bool hovered = false;
    bool dragging = false;
    float focusAnim = 0.0f;
    float hoverAnim = 0.0f;
    float caretPhase = 0.0f;
    float scrollX = 0.0f;

    bool HasSelection() const { return caretPos != selAnchor; }
    void GetSelectionRange(size_t& start, size_t& end) const {
        start = (caretPos < selAnchor) ? caretPos : selAnchor;
        end = (caretPos < selAnchor) ? selAnchor : caretPos;
    }
    void ClearSelection() { selAnchor = caretPos; }
};

class SettingsWindow {
public:
    static SettingsWindow& Instance();

    bool Create();
    void Show();
    void Hide();
    void Destroy();
    bool IsVisible() const;

    void NotifyStatusChanged(WORD langId, bool isPaused);
    HWND GetHwnd() const { return hWnd_; }

private:
    SettingsWindow() = default;
    ~SettingsWindow();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void EnsureFormats();
    void ReleaseFormats();
    void RefreshWindowIcons();
    void RefreshSnapshot(bool snapAnimations = false);
    bool ResolveDarkMode() const;
    void ToggleTheme();

    void RenderNow(HDC fallbackDstDC = nullptr);
    void RequestRedraw();

    void DrawNavigationRail(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush);
    void DrawWorkspaceShell(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush);
    void DrawFooterBar(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush);
    void DrawCaptionButtons(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush);
    void DrawPageHeader(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha);
    void DrawGeneralPage(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha);
    void DrawAppLayoutsPage(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha);
    void DrawExclusionsPage(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha);
    void DrawTextInput(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, FluentTextInput& input, float offsetY, float alpha);
    void DrawFluentButton(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush,
        const D2D1_RECT_F& rc, const std::wstring& label, FluentIcon icon, bool primary, float hoverAnim, bool pressed, float alpha);
    void DrawScrollbar(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush,
        const D2D1_RECT_F& listRc, float scrollY, float contentH, float alpha);

    void EnsureAnimationRunning();
    void StopAnimation();
    void TickAnimations(float dt);

    void SwitchTab(SettingsTab nextTab);
    void TriggerSavedToast();
    void TogglePause();
    void ShowAppPickerMenu(const D2D1_RECT_F& anchorDip, bool forAppLayouts);
    void AddAppLayoutFromInput();
    void AddExclusionFromInput();
    void ToggleAppRuleLayout(size_t index);
    void RemoveAppRule(size_t index);
    void RemoveExclusion(size_t index);

    void HandleMouseMove(float dipX, float dipY);
    void HandleMouseDown(float dipX, float dipY, bool doubleClick);
    void HandleMouseUp(float dipX, float dipY);
    void HandleMouseWheel(short delta, float dipX, float dipY);
    bool HandleInputKeyDown(FluentTextInput& input, WPARAM vk);
    bool HandleInputChar(FluentTextInput& input, wchar_t ch);
    size_t HitTestTextCaret(const FluentTextInput& input, float localX);
    void EnsureInputCaretVisible(FluentTextInput& input);

    FluentTextInput* ActiveTextInput();

    enum class HitZone {
        None = 0,
        BtnMinimize,
        BtnClose,
        ThemeToggle,
        NavGeneral,
        NavAppLayouts,
        NavExclusions,
        FooterPauseBtn,
        ToggleRow0,
        ToggleRow1,
        ToggleRow2,
        ToggleRow3,
        ToggleRow4,
        AppInput,
        AppPickBtn,
        AppLangEn,
        AppLangRu,
        AppAddBtn,
        AppRow,
        AppRowBadge,
        AppRowDelete,
        ExclInput,
        ExclPickBtn,
        ExclAddBtn,
        ExclRow,
        ExclRowDelete
    };

    struct HitResult {
        HitZone zone = HitZone::None;
        int index = -1;
    };
    HitResult HitTest(float dipX, float dipY) const;

    HWND hWnd_ = nullptr;
    HICON hIconBig_ = nullptr;
    HICON hIconSmall_ = nullptr;

    bool darkMode_ = true;
    bool appliedDark_ = true;
    bool backdropApplied_ = false;
    bool hasBackdrop_ = false;
    UINT dpi_ = 96;

    Config::AppConfig cfg_{};
    bool livePaused_ = false;
    bool liveIsRu_ = false;

    SettingsTab activeTab_ = SettingsTab::General;
    SettingsTab prevTab_ = SettingsTab::General;
    Config::AppLayout selectedAppLayout_ = Config::AppLayout::Russian;

    FluentTextInput appInput_;
    FluentTextInput exclInput_;

    float appScrollY_ = 0.0f;
    float appScrollTarget_ = 0.0f;
    float exclScrollY_ = 0.0f;
    float exclScrollTarget_ = 0.0f;

    HitResult hovered_{};
    HitResult pressed_{};
    bool trackingMouse_ = false;

    float toggleAnim_[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
    float toggleHover_[5] = {};
    float togglePress_[5] = {};
    float rowHover_[5] = {};

    float navHover_[3] = {};
    float pageTransition_ = 1.0f;
    int pageDir_ = 1;

    float captionHover_[2] = {};
    float themeBtnHover_ = 0.0f;
    float footerPauseHover_ = 0.0f;
    float statusPausedAnim_ = 0.0f;
    float langChipRuAnim_ = 0.0f;
    float langSelectorAnim_ = 1.0f;

    float btnPickHover_ = 0.0f;
    float btnAddHover_ = 0.0f;

    float toastAlpha_ = 0.0f;
    float toastHoldSec_ = 0.0f;

    bool animRunning_ = false;
    uint64_t lastTickUs_ = 0;

    GpuSurface surface_;
    IDWriteTextFormat* fmtBrand_ = nullptr;
    IDWriteTextFormat* fmtNav_ = nullptr;
    IDWriteTextFormat* fmtNavSelected_ = nullptr;
    IDWriteTextFormat* fmtPageTitle_ = nullptr;
    IDWriteTextFormat* fmtSubtitle_ = nullptr;
    IDWriteTextFormat* fmtGroupLabel_ = nullptr;
    IDWriteTextFormat* fmtRowTitle_ = nullptr;
    IDWriteTextFormat* fmtRowSub_ = nullptr;
    IDWriteTextFormat* fmtBadge_ = nullptr;
    IDWriteTextFormat* fmtBtn_ = nullptr;
    IDWriteTextFormat* fmtInput_ = nullptr;
    IDWriteTextFormat* fmtSmall_ = nullptr;
};

} // namespace UI
