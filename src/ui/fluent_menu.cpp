#include "ui/fluent_menu.hpp"
#include "platform/win32_utils.hpp"
#include <windowsx.h>
#include <algorithm>
#include <cmath>

namespace UI {

constexpr UINT_PTR TIMER_MENU_ANIM = 1;

constexpr float MENU_PAD_X = 0.0f;
constexpr float MENU_PAD_Y = 0.0f;
constexpr float ITEM_HEIGHT = 36.0f;
constexpr float SEPARATOR_HEIGHT = 7.0f;
constexpr float MAX_LIST_VIEW_HEIGHT = 320.0f;

FluentMenu& FluentMenu::Instance() {
    static FluentMenu s_instance;
    return s_instance;
}

FluentMenu::~FluentMenu() {
    Destroy();
}

void FluentMenu::EnsureFormats() {
    auto& gfx = FluentGraphics::Instance();
    if (!gfx.EnsureInitialized()) return;

    if (!fmtItem_) {
        fmtItem_ = gfx.CreateTextFormat(13.5f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtItem_) {
            fmtItem_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
            fmtItem_->SetTrimming(&trim, nullptr);
        }
    }
}

void FluentMenu::ReleaseFormats() {
    if (fmtItem_) { fmtItem_->Release(); fmtItem_ = nullptr; }
}

bool FluentMenu::EnsureWindow() {
    if (hWnd_ && IsWindow(hWnd_)) return true;

    bool gpuAvail = FluentGraphics::Instance().IsGpuAvailable();
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    if (!classRegistered_) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"CapsSwitchFluentMenu";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassExW(&wc);
        classRegistered_ = true;
    }

    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    if (gpuAvail) {
        exStyle |= WS_EX_NOREDIRECTIONBITMAP;
    }

    hWnd_ = CreateWindowExW(
        exStyle,
        L"CapsSwitchFluentMenu",
        L"",
        WS_POPUP,
        0, 0, 210, 108,
        nullptr, nullptr, hInst, this
    );
    return hWnd_ != nullptr;
}

void FluentMenu::Prewarm() {
    if (!EnsureWindow()) return;
    EnsureFormats();
    darkMode_ = IsTaskbarDarkMode();
    if (!backdropApplied_ || darkMode_ != appliedDark_) {
        hasBackdrop_ = ApplyWindowBackdrop(hWnd_, BackdropKind::Acrylic, darkMode_);
        appliedDark_ = darkMode_;
        backdropApplied_ = true;
    }
    surface_.EnsureSize(hWnd_, 210, 108);
}

void FluentMenu::ComputeLayout() {
    EnsureFormats();

    float maxLabelW = 140.0f;
    IDWriteFactory* dw = FluentGraphics::Instance().DWriteFactory();
    if (dw && fmtItem_) {
        for (const auto& item : items_) {
            if (item.isSeparator || item.label.empty()) continue;
            IDWriteTextLayout* layout = nullptr;
            if (SUCCEEDED(dw->CreateTextLayout(item.label.c_str(), static_cast<UINT32>(item.label.size()),
                fmtItem_, 400.0f, ITEM_HEIGHT, &layout)) && layout) {
                DWRITE_TEXT_METRICS tm = {};
                if (SUCCEEDED(layout->GetMetrics(&tm))) {
                    float w = tm.widthIncludingTrailingWhitespace + (item.icon != FluentIcon::None ? 42.0f : 14.0f) + 20.0f;
                    if (w > maxLabelW) maxLabelW = w;
                }
                layout->Release();
            }
        }
    }

    menuWidth_ = (std::clamp)(std::ceil(maxLabelW), 184.0f, 260.0f);
    listTop_ = MENU_PAD_Y;

    itemLayouts_.resize(items_.size());
    float curY = 0.0f;
    for (size_t i = 0; i < items_.size(); ++i) {
        float h = items_[i].isSeparator ? SEPARATOR_HEIGHT : ITEM_HEIGHT;
        itemLayouts_[i].y = curY;
        itemLayouts_[i].height = h;
        curY += h;
    }

    totalContentHeight_ = curY;
    listViewHeight_ = (std::min)(totalContentHeight_, MAX_LIST_VIEW_HEIGHT);
    menuHeight_ = listTop_ + listViewHeight_ + MENU_PAD_Y;
    scrollY_ = 0.0f;
    scrollTarget_ = 0.0f;
}

void FluentMenu::Show(
    HWND owner,
    POINT anchorScreenPt,
    bool openUpward,
    const std::vector<FluentMenuItem>& items,
    std::function<void(UINT)> onSelect,
    int themeOverride,
    bool drawItemDividers
) {
    if (items.empty()) return;
    if (!EnsureWindow()) return;

    owner_ = owner;
    items_ = items;
    onSelect_ = std::move(onSelect);
    openUpward_ = openUpward;
    drawItemDividers_ = drawItemDividers;
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    lastTickUs_ = Platform::GetCurrentTimeMicroseconds();

    darkMode_ = (themeOverride >= 0) ? (themeOverride != 0) : IsTaskbarDarkMode();
    if (!backdropApplied_ || darkMode_ != appliedDark_) {
        hasBackdrop_ = ApplyWindowBackdrop(hWnd_, BackdropKind::Acrylic, darkMode_);
        appliedDark_ = darkMode_;
        backdropApplied_ = true;
    }

    HMONITOR hMon = MonitorFromPoint(anchorScreenPt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoW(hMon, &mi);

    dpi_ = GetWindowDpiSafe(owner ? owner : hWnd_);
    float scale = dpi_ / 96.0f;

    ComputeLayout();

    int pxW = static_cast<int>(std::ceil(menuWidth_ * scale));
    int pxH = static_cast<int>(std::ceil(menuHeight_ * scale));
    int margin = static_cast<int>(std::round(8.0f * scale));

    int posX = anchorScreenPt.x;
    int posY = openUpward ? (anchorScreenPt.y - pxH) : anchorScreenPt.y;

    RECT wr = mi.rcWork;
    if (posX + pxW > wr.right - margin) posX = wr.right - margin - pxW;
    if (posX < wr.left + margin) posX = wr.left + margin;
    if (posY + pxH > wr.bottom - margin) {
        posY = anchorScreenPt.y - pxH;
        if (posY + pxH > wr.bottom - margin) posY = wr.bottom - margin - pxH;
    }
    if (posY < wr.top + margin) posY = wr.top + margin;

    SetWindowPos(hWnd_, HWND_TOPMOST, posX, posY, pxW, pxH, SWP_NOACTIVATE);
    surface_.EnsureSize(hWnd_, pxW, pxH);
    ShowWindow(hWnd_, SW_SHOW);
    SetForegroundWindow(hWnd_);
    SetFocus(hWnd_);

    RenderNow();
}

void FluentMenu::Hide() {
    StopAnimTimer();
    if (hWnd_ && IsWindowVisible(hWnd_)) {
        ShowWindow(hWnd_, SW_HIDE);
    }
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
}

void FluentMenu::Destroy() {
    Hide();
    if (hWnd_) {
        DestroyWindow(hWnd_);
        hWnd_ = nullptr;
    }
    backdropApplied_ = false;
    surface_.Release();
    ReleaseFormats();
}

bool FluentMenu::IsVisible() const {
    return hWnd_ && IsWindowVisible(hWnd_);
}

void FluentMenu::StartAnimTimer() {
    if (!animRunning_ && hWnd_) {
        SetTimer(hWnd_, TIMER_MENU_ANIM, 10, nullptr);
        animRunning_ = true;
    }
}

void FluentMenu::StopAnimTimer() {
    if (animRunning_ && hWnd_) {
        KillTimer(hWnd_, TIMER_MENU_ANIM);
        animRunning_ = false;
    }
}

void FluentMenu::RequestRedraw() {
    if (!hWnd_) return;
    if (surface_.IsGpu()) {
        RenderNow();
    } else {
        InvalidateRect(hWnd_, nullptr, FALSE);
    }
}

int FluentMenu::HitTestItem(float dipX, float dipY) const {
    if (dipX < MENU_PAD_X || dipX > menuWidth_ - MENU_PAD_X) return -1;
    if (dipY < listTop_ || dipY > listTop_ + listViewHeight_) return -1;

    float contentY = dipY - listTop_ + scrollY_;
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].isSeparator || items_[i].isDisabled) continue;
        if (contentY >= itemLayouts_[i].y && contentY < itemLayouts_[i].y + itemLayouts_[i].height) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void FluentMenu::MoveHoverKeyboard(int dir) {
    if (items_.empty()) return;
    int count = static_cast<int>(items_.size());
    int start = hoveredIndex_;
    if (start < 0 || start >= count) {
        start = (dir > 0) ? -1 : count;
    }
    for (int step = 1; step <= count; ++step) {
        int idx = (start + dir * step + count * 2) % count;
        if (!items_[idx].isSeparator && !items_[idx].isDisabled) {
            hoveredIndex_ = idx;
            float itemTop = itemLayouts_[idx].y;
            float itemBot = itemTop + itemLayouts_[idx].height;
            if (itemTop < scrollTarget_) scrollTarget_ = itemTop;
            if (itemBot > scrollTarget_ + listViewHeight_) scrollTarget_ = itemBot - listViewHeight_;
            StartAnimTimer();
            RequestRedraw();
            return;
        }
    }
}

void FluentMenu::ConfirmIndex(int idx) {
    if (idx < 0 || static_cast<size_t>(idx) >= items_.size()) return;
    const auto& item = items_[idx];
    if (item.isSeparator || item.isDisabled) return;

    UINT chosenId = item.id;
    auto cb = onSelect_;
    Hide();
    if (cb) {
        cb(chosenId);
    }
}

void FluentMenu::TickAnimations(float dt) {
    bool needMore = false;

    float maxScroll = (std::max)(0.0f, totalContentHeight_ - listViewHeight_);
    scrollTarget_ = (std::clamp)(scrollTarget_, 0.0f, maxScroll);
    if (std::fabs(scrollY_ - scrollTarget_) > 0.1f) {
        scrollY_ = ExpDecay(scrollY_, scrollTarget_, 32.0f, dt);
        needMore = true;
    } else {
        scrollY_ = scrollTarget_;
    }

    if (!needMore) {
        StopAnimTimer();
    }
}

void FluentMenu::RenderNow(HDC fallbackDstDC) {
    if (!hWnd_) return;
    RECT rcClient{};
    GetClientRect(hWnd_, &rcClient);
    int pxW = rcClient.right - rcClient.left;
    int pxH = rcClient.bottom - rcClient.top;
    if (pxW <= 0 || pxH <= 0) return;

    EnsureFormats();
    float scale = dpi_ / 96.0f;
    if (!surface_.EnsureSize(hWnd_, pxW, pxH)) return;
    ID2D1RenderTarget* rt = surface_.BeginDraw(scale, fallbackDstDC);
    if (!rt) return;

    ID2D1SolidColorBrush* brush = surface_.Brush();
    FluentPalette pal = GetFluentPalette(darkMode_);

    if (hasBackdrop_) {
        rt->Clear(D2D1::ColorF(0, 0, 0, 0));
    } else {
        rt->Clear(pal.menuFallbackBg);
        if (brush) {
            brush->SetColor(pal.borderStrong);
            rt->DrawRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, menuWidth_ - 0.5f, menuHeight_ - 0.5f), 8.0f, 8.0f),
                brush, 1.0f
            );
        }
    }

    D2D1_RECT_F clipRc = D2D1::RectF(0.0f, listTop_, menuWidth_, listTop_ + listViewHeight_);
    rt->PushAxisAlignedClip(clipRc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    D2D1_COLOR_F dividerCol = darkMode_ ? Rgba(255, 255, 255, 0.09f) : Rgba(0, 0, 0, 0.08f);
    D2D1_COLOR_F textNormalCol = darkMode_ ? Rgba(255, 255, 255, 1.0f) : Rgba(20, 20, 20, 1.0f);
    D2D1_COLOR_F iconNormalCol = darkMode_ ? Rgba(245, 245, 245, 0.92f) : Rgba(24, 24, 24, 0.90f);

    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        float itemY = listTop_ + itemLayouts_[i].y - scrollY_;
        float itemH = itemLayouts_[i].height;
        if (itemY + itemH < listTop_ || itemY > listTop_ + listViewHeight_) continue;

        if (item.isSeparator) {
            if (brush) {
                float sepY = itemY + itemH * 0.5f;
                brush->SetColor(dividerCol);
                rt->DrawLine(D2D1::Point2F(0.0f, sepY), D2D1::Point2F(menuWidth_, sepY), brush, 1.0f);
            }
            continue;
        }

        if (drawItemDividers_ && i > 0 && !items_[i - 1].isSeparator && brush) {
            brush->SetColor(dividerCol);
            rt->DrawLine(D2D1::Point2F(0.0f, itemY + 0.5f), D2D1::Point2F(menuWidth_, itemY + 0.5f), brush, 1.0f);
        }

        bool isHovered = (static_cast<int>(i) == hoveredIndex_);
        bool isPressed = (static_cast<int>(i) == pressedIndex_);
        if ((isHovered || isPressed) && brush) {
            D2D1_COLOR_F hlCol;
            if (item.isDestructive) {
                hlCol = WithAlpha(pal.dangerSoft, isPressed ? 0.22f : 0.14f);
            } else if (darkMode_) {
                hlCol = Rgba(255, 255, 255, isPressed ? 0.105f : 0.075f);
            } else {
                hlCol = Rgba(0, 0, 0, isPressed ? 0.085f : 0.055f);
            }
            D2D1_RECT_F rcHl = D2D1::RectF(4.0f, itemY + 3.0f, menuWidth_ - 4.0f, itemY + itemH - 3.0f);
            brush->SetColor(hlCol);
            rt->FillRoundedRectangle(D2D1::RoundedRect(rcHl, 5.0f, 5.0f), brush);
        }

        D2D1_COLOR_F fgCol = item.isDisabled
            ? pal.textTertiary
            : (item.isDestructive ? (isHovered ? pal.danger : WithAlpha(pal.danger, 0.90f)) : textNormalCol);

        float textLeft = 14.0f;
        if (item.icon != FluentIcon::None) {
            float iconTop = itemY + (itemH - 16.0f) * 0.5f;
            D2D1_RECT_F iconRc = D2D1::RectF(14.0f, iconTop, 30.0f, iconTop + 16.0f);
            D2D1_COLOR_F iconCol = item.isDestructive
                ? fgCol
                : (item.isDisabled ? pal.textTertiary : iconNormalCol);
            DrawFluentIcon(rt, item.icon, iconRc, iconCol, 1.2f, brush);
            textLeft = 42.0f;
        }

        float textRight = menuWidth_ - 14.0f;
        if (fmtItem_ && !item.label.empty() && brush) {
            brush->SetColor(fgCol);
            rt->DrawText(item.label.c_str(), static_cast<UINT32>(item.label.size()), fmtItem_,
                D2D1::RectF(textLeft, itemY, textRight, itemY + itemH), brush);
        }
    }

    if (totalContentHeight_ > listViewHeight_ + 1.0f && brush) {
        float ratio = listViewHeight_ / totalContentHeight_;
        float barH = (std::max)(24.0f, listViewHeight_ * ratio);
        float maxScroll = totalContentHeight_ - listViewHeight_;
        float barY = listTop_ + (maxScroll > 0.0f ? (scrollY_ / maxScroll) * (listViewHeight_ - barH) : 0.0f);
        brush->SetColor(pal.textTertiary);
        rt->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(menuWidth_ - 5.0f, barY + 2.0f, menuWidth_ - 2.0f, barY + barH - 2.0f), 1.5f, 1.5f),
            brush
        );
    }

    rt->PopAxisAlignedClip();

    HRESULT hr = surface_.EndDrawAndPresent(fallbackDstDC);
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED) {
        RequestRedraw();
    }
}

LRESULT CALLBACK FluentMenu::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FluentMenu* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<FluentMenu*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<FluentMenu*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    if (pThis) {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT FluentMenu::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RenderNow(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == TIMER_MENU_ANIM) {
            uint64_t now = Platform::GetCurrentTimeMicroseconds();
            float dt = (lastTickUs_ > 0 && now > lastTickUs_)
                ? (std::min)(0.05f, static_cast<float>(now - lastTickUs_) / 1000000.0f)
                : 0.010f;
            lastTickUs_ = now;
            TickAnimations(dt);
            RequestRedraw();
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!trackingMouse_) {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            trackingMouse_ = true;
        }
        float scale = dpi_ / 96.0f;
        int hit = HitTestItem(GET_X_LPARAM(lParam) / scale, GET_Y_LPARAM(lParam) / scale);
        if (hit != hoveredIndex_) {
            hoveredIndex_ = hit;
            RequestRedraw();
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        trackingMouse_ = false;
        if (hoveredIndex_ != -1 || pressedIndex_ != -1) {
            hoveredIndex_ = -1;
            pressedIndex_ = -1;
            RequestRedraw();
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        float scale = dpi_ / 96.0f;
        int hit = HitTestItem(GET_X_LPARAM(lParam) / scale, GET_Y_LPARAM(lParam) / scale);
        pressedIndex_ = hit;
        hoveredIndex_ = hit;
        RequestRedraw();
        return 0;
    }
    case WM_LBUTTONUP: {
        float scale = dpi_ / 96.0f;
        int hit = HitTestItem(GET_X_LPARAM(lParam) / scale, GET_Y_LPARAM(lParam) / scale);
        int pressed = pressedIndex_;
        pressedIndex_ = -1;
        if (hit >= 0 && hit == pressed) {
            ConfirmIndex(hit);
        } else {
            RequestRedraw();
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (totalContentHeight_ > listViewHeight_) {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            scrollTarget_ -= (delta / 120.0f) * ITEM_HEIGHT * 1.6f;
            float maxScroll = (std::max)(0.0f, totalContentHeight_ - listViewHeight_);
            scrollTarget_ = (std::clamp)(scrollTarget_, 0.0f, maxScroll);
            StartAnimTimer();
            RequestRedraw();
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            Hide();
            return 0;
        }
        if (wParam == VK_DOWN) {
            MoveHoverKeyboard(1);
            return 0;
        }
        if (wParam == VK_UP) {
            MoveHoverKeyboard(-1);
            return 0;
        }
        if (wParam == VK_RETURN && hoveredIndex_ >= 0) {
            ConfirmIndex(hoveredIndex_);
            return 0;
        }
        return 0;
    }
    case WM_ACTIVATE: {
        if (LOWORD(wParam) == WA_INACTIVE) {
            Hide();
        }
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace UI
