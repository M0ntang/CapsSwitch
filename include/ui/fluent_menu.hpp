#pragma once

#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "ui/fluent_theme.hpp"

namespace UI {

struct FluentMenuItem {
    UINT id = 0;
    std::wstring label;
    std::wstring rightBadge;
    FluentIcon icon = FluentIcon::None;
    bool isSeparator = false;
    bool isDestructive = false;
    bool isDisabled = false;
};

struct FluentMenuHeader {
    bool show = false;
    std::wstring title;
    std::wstring statusText;
    D2D1_COLOR_F statusColor{};
    std::wstring langBadge;
    D2D1_COLOR_F langColor{};
};

class FluentMenu {
public:
    static FluentMenu& Instance();

    void Prewarm();
    void Show(
        HWND owner,
        POINT anchorScreenPt,
        bool openUpward,
        const FluentMenuHeader& header,
        const std::vector<FluentMenuItem>& items,
        std::function<void(UINT)> onSelect
    );
    void Hide();
    void Destroy();
    bool IsVisible() const;

private:
    FluentMenu() = default;
    ~FluentMenu();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool EnsureWindow();
    void EnsureFormats();
    void ReleaseFormats();
    void ComputeLayout();
    void TickAnimations(float dt);
    void RenderNow(HDC fallbackDstDC = nullptr);
    void RequestRedraw();
    int HitTestItem(float dipX, float dipY) const;
    void MoveHoverKeyboard(int dir);
    void ConfirmIndex(int idx);
    void StartAnimTimer();
    void StopAnimTimer();

    struct ItemLayout {
        float y = 0.0f;
        float height = 0.0f;
    };

    HWND hWnd_ = nullptr;
    HWND owner_ = nullptr;
    bool classRegistered_ = false;

    bool darkMode_ = true;
    bool appliedDark_ = true;
    bool backdropApplied_ = false;
    bool hasBackdrop_ = false;
    UINT dpi_ = 96;

    FluentMenuHeader header_{};
    std::vector<FluentMenuItem> items_;
    std::vector<ItemLayout> itemLayouts_;
    std::function<void(UINT)> onSelect_;

    float menuWidth_ = 228.0f;
    float menuHeight_ = 160.0f;
    float headerHeight_ = 0.0f;
    float listTop_ = 6.0f;
    float listViewHeight_ = 140.0f;
    float totalContentHeight_ = 140.0f;
    float scrollY_ = 0.0f;
    float scrollTarget_ = 0.0f;

    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    bool trackingMouse_ = false;
    bool openUpward_ = false;

    float openProgress_ = 1.0f;
    float hoverPillY_ = 0.0f;
    float hoverPillAlpha_ = 0.0f;
    bool animRunning_ = false;
    uint64_t lastTickUs_ = 0;

    GpuSurface surface_;
    IDWriteTextFormat* fmtItem_ = nullptr;
    IDWriteTextFormat* fmtTitle_ = nullptr;
    IDWriteTextFormat* fmtSub_ = nullptr;
    IDWriteTextFormat* fmtBadge_ = nullptr;
};

} // namespace UI
