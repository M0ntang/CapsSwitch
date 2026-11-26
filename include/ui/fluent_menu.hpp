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

class FluentMenu {
public:
    static FluentMenu& Instance();

    void Prewarm();
    void Show(
        HWND owner,
        POINT anchorScreenPt,
        bool openUpward,
        const std::vector<FluentMenuItem>& items,
        std::function<void(UINT)> onSelect,
        int themeOverride = -1,
        bool drawItemDividers = false
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

    std::vector<FluentMenuItem> items_;
    std::vector<ItemLayout> itemLayouts_;
    std::function<void(UINT)> onSelect_;

    float menuWidth_ = 210.0f;
    float menuHeight_ = 108.0f;
    float listTop_ = 0.0f;
    float listViewHeight_ = 108.0f;
    float totalContentHeight_ = 108.0f;
    float scrollY_ = 0.0f;
    float scrollTarget_ = 0.0f;

    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    bool trackingMouse_ = false;
    bool openUpward_ = false;
    bool drawItemDividers_ = false;

    bool animRunning_ = false;
    uint64_t lastTickUs_ = 0;

    GpuSurface surface_;
    IDWriteTextFormat* fmtItem_ = nullptr;
};

} // namespace UI
