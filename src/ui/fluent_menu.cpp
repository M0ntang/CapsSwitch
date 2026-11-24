#include "ui/fluent_menu.hpp"
#include "platform/win32_utils.hpp"
#include <windowsx.h>
#include <algorithm>
#include <cmath>

namespace UI {

constexpr UINT_PTR TIMER_MENU_ANIM = 1;

constexpr float MENU_PAD_X = 4.0f;
constexpr float MENU_PAD_Y = 4.0f;
constexpr float ITEM_HEIGHT = 32.0f;
constexpr float SEPARATOR_HEIGHT = 9.0f;
constexpr float HEADER_HEIGHT = 44.0f;
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
        fmtItem_ = gfx.CreateTextFormat(13.0f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtItem_) {
            fmtItem_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
            fmtItem_->SetTrimming(&trim, nullptr);
        }
    }
    if (!fmtTitle_) {
        fmtTitle_ = gfx.CreateTextFormat(13.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtSub_) {
        fmtSub_ = gfx.CreateTextFormat(11.0f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtBadge_) {
        fmtBadge_ = gfx.CreateTextFormat(10.5f, DWRITE_FONT_WEIGHT_BOLD, false,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}

void FluentMenu::ReleaseFormats() {
    if (fmtItem_)  { fmtItem_->Release();  fmtItem_ = nullptr; }
    if (fmtTitle_) { fmtTitle_->Release(); fmtTitle_ = nullptr; }
    if (fmtSub_)   { fmtSub_->Release();   fmtSub_ = nullptr; }
    if (fmtBadge_) { fmtBadge_->Release(); fmtBadge_ = nullptr; }
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
        0, 0, 228, 160,
        nullptr, nullptr, hInst, this
    );
    return hWnd_ != nullptr;
}

void FluentMenu::Prewarm() {
    if (!EnsureWindow()) return;
    EnsureFormats();
    darkMode_ = IsSystemDarkMode();
    if (!backdropApplied_ || darkMode_ != appliedDark_) {
        hasBackdrop_ = ApplyWindowBackdrop(hWnd_, BackdropKind::Acrylic, darkMode_);
        appliedDark_ = darkMode_;
        backdropApplied_ = true;
    }
    surface_.EnsureSize(hWnd_, 228, 160);
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
                    float w = tm.widthIncludingTrailingWhitespace + (item.icon != FluentIcon::None ? 32.0f : 12.0f)
                        + (item.rightBadge.empty() ? 24.0f : 64.0f);
                    if (w > maxLabelW) maxLabelW = w;
                }
                layout->Release();
            }
        }
    }

    float minW = header_.show ? 220.0f : 180.0f;
    menuWidth_ = (std::clamp)(std::ceil(maxLabelW + MENU_PAD_X * 2.0f), minW, 300.0f);

    headerHeight_ = header_.show ? HEADER_HEIGHT : 0.0f;
    listTop_ = MENU_PAD_Y + headerHeight_ + (header_.show ? 5.0f : 0.0f);

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
    const FluentMenuHeader& header,
    const std::vector<FluentMenuItem>& items,
    std::function<void(UINT)> onSelect
) {
    if (items.empty()) return;
    if (!EnsureWindow()) return;

    owner_ = owner;
    header_ = header;
    items_ = items;
    onSelect_ = std::move(onSelect);
    openUpward_ = openUpward;
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    hoverPillAlpha_ = 0.0f;
    hoverPillY_ = 0.0f;
    openProgress_ = 0.0f;
    lastTickUs_ = Platform::GetCurrentTimeMicroseconds();

    darkMode_ = IsSystemDarkMode();
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

    StartAnimTimer();
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

    if (openProgress_ < 1.0f) {
        openProgress_ = (std::min)(1.0f, openProgress_ + dt / 0.085f);
        if (openProgress_ < 1.0f) needMore = true;
    }

    float maxScroll = (std::max)(0.0f, totalContentHeight_ - listViewHeight_);
    scrollTarget_ = (std::clamp)(scrollTarget_, 0.0f, maxScroll);
    if (std::fabs(scrollY_ - scrollTarget_) > 0.1f) {
        scrollY_ = ExpDecay(scrollY_, scrollTarget_, 32.0f, dt);
        needMore = true;
    } else {
        scrollY_ = scrollTarget_;
    }

    if (hoveredIndex_ >= 0 && static_cast<size_t>(hoveredIndex_) < itemLayouts_.size()) {
        float targetY = listTop_ + itemLayouts_[hoveredIndex_].y - scrollY_;
        if (hoverPillAlpha_ < 0.02f) {
            hoverPillY_ = targetY;
        } else {
            hoverPillY_ = ExpDecay(hoverPillY_, targetY, 42.0f, dt);
        }
        hoverPillAlpha_ = ExpDecay(hoverPillAlpha_, 1.0f, 38.0f, dt);
        if (std::fabs(hoverPillY_ - targetY) > 0.1f || std::fabs(hoverPillAlpha_ - 1.0f) > 0.01f) {
            needMore = true;
        }
    } else {
        hoverPillAlpha_ = ExpDecay(hoverPillAlpha_, 0.0f, 38.0f, dt);
        if (hoverPillAlpha_ > 0.01f) needMore = true;
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
        if (brush) {
            D2D1_COLOR_F wash = darkMode_ ? Rgba(30, 32, 38, 0.34f) : Rgba(255, 255, 255, 0.45f);
            brush->SetColor(wash);
            rt->FillRectangle(D2D1::RectF(0, 0, menuWidth_, menuHeight_), brush);
        }
    } else {
        rt->Clear(pal.menuFallbackBg);
    }

    float openEase = EaseOutQuint(openProgress_);
    float slideOffset = (1.0f - openEase) * (openUpward_ ? 4.0f : -4.0f);
    float contentAlpha = openEase;

    rt->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, slideOffset) * D2D1::Matrix3x2F::Scale(scale, scale));

    auto setCol = [&](const D2D1_COLOR_F& col) -> bool {
        if (!brush) return false;
        brush->SetColor(WithAlpha(col, col.a * contentAlpha));
        return true;
    };

    if (setCol(pal.borderStrong)) {
        rt->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, menuWidth_ - 0.5f, menuHeight_ - 0.5f), 8.0f, 8.0f),
            brush, 1.0f
        );
    }

    if (header_.show) {
        D2D1_RECT_F rcHdr = D2D1::RectF(MENU_PAD_X, MENU_PAD_Y, menuWidth_ - MENU_PAD_X, MENU_PAD_Y + headerHeight_);
        if (setCol(pal.cardBg)) {
            rt->FillRoundedRectangle(D2D1::RoundedRect(rcHdr, 6.0f, 6.0f), brush);
        }
        if (setCol(pal.borderSubtle)) {
            rt->DrawRoundedRectangle(D2D1::RoundedRect(rcHdr, 6.0f, 6.0f), brush, 1.0f);
        }

        if (fmtTitle_ && !header_.title.empty() && setCol(pal.textPrimary)) {
            rt->DrawText(header_.title.c_str(), static_cast<UINT32>(header_.title.size()), fmtTitle_,
                D2D1::RectF(rcHdr.left + 10.0f, rcHdr.top + 5.0f, rcHdr.right - 52.0f, rcHdr.top + 22.0f), brush);
        }
        if (fmtSub_ && !header_.statusText.empty()) {
            float dotX = rcHdr.left + 14.0f;
            float dotY = rcHdr.top + 31.5f;
            if (setCol(header_.statusColor)) {
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 3.0f, 3.0f), brush);
            }
            if (setCol(pal.textSecondary)) {
                rt->DrawText(header_.statusText.c_str(), static_cast<UINT32>(header_.statusText.size()), fmtSub_,
                    D2D1::RectF(rcHdr.left + 22.0f, rcHdr.top + 23.0f, rcHdr.right - 52.0f, rcHdr.top + 40.0f), brush);
            }
        }
        if (fmtBadge_ && !header_.langBadge.empty()) {
            D2D1_RECT_F rcLang = D2D1::RectF(rcHdr.right - 42.0f, rcHdr.top + 11.0f, rcHdr.right - 10.0f, rcHdr.top + 33.0f);
            if (setCol(header_.langColor)) {
                rt->FillRoundedRectangle(D2D1::RoundedRect(rcLang, 5.5f, 5.5f), brush);
            }
            if (setCol(Rgba(255, 255, 255, 1.0f))) {
                rt->DrawText(header_.langBadge.c_str(), static_cast<UINT32>(header_.langBadge.size()), fmtBadge_, rcLang, brush);
            }
        }

        float divY = MENU_PAD_Y + headerHeight_ + 2.5f;
        if (setCol(pal.divider)) {
            rt->DrawLine(D2D1::Point2F(MENU_PAD_X + 4.0f, divY), D2D1::Point2F(menuWidth_ - MENU_PAD_X - 4.0f, divY), brush, 1.0f);
        }
    }

    D2D1_RECT_F clipRc = D2D1::RectF(0.0f, listTop_, menuWidth_, listTop_ + listViewHeight_);
    rt->PushAxisAlignedClip(clipRc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (hoverPillAlpha_ > 0.01f && hoveredIndex_ >= 0 && static_cast<size_t>(hoveredIndex_) < items_.size()) {
        bool isDestr = items_[hoveredIndex_].isDestructive;
        D2D1_COLOR_F pillCol = isDestr
            ? WithAlpha(pal.dangerSoft, pal.dangerSoft.a * hoverPillAlpha_ * 1.4f)
            : WithAlpha(pal.navItemSelected, (darkMode_ ? 0.07f : 0.05f) * hoverPillAlpha_);
        if (pressedIndex_ == hoveredIndex_) {
            pillCol = isDestr ? WithAlpha(pal.dangerSoft, 0.25f) : pal.cardPressedBg;
        }
        D2D1_RECT_F rcPill = D2D1::RectF(MENU_PAD_X, hoverPillY_ + 1.0f, menuWidth_ - MENU_PAD_X, hoverPillY_ + ITEM_HEIGHT - 1.0f);
        if (setCol(pillCol)) {
            rt->FillRoundedRectangle(D2D1::RoundedRect(rcPill, 4.5f, 4.5f), brush);
        }
    }

    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        float itemY = listTop_ + itemLayouts_[i].y - scrollY_;
        float itemH = itemLayouts_[i].height;
        if (itemY + itemH < listTop_ || itemY > listTop_ + listViewHeight_) continue;

        if (item.isSeparator) {
            float sepY = itemY + itemH * 0.5f;
            if (setCol(pal.divider)) {
                rt->DrawLine(D2D1::Point2F(MENU_PAD_X + 6.0f, sepY), D2D1::Point2F(menuWidth_ - MENU_PAD_X - 6.0f, sepY), brush, 1.0f);
            }
            continue;
        }

        bool isHovered = (static_cast<int>(i) == hoveredIndex_);
        D2D1_COLOR_F fgCol = item.isDisabled
            ? pal.textTertiary
            : (item.isDestructive ? (isHovered ? pal.danger : WithAlpha(pal.danger, 0.90f)) : pal.textPrimary);

        float textLeft = MENU_PAD_X + 10.0f;
        if (item.icon != FluentIcon::None) {
            D2D1_RECT_F iconRc = D2D1::RectF(MENU_PAD_X + 10.0f, itemY + 9.0f, MENU_PAD_X + 24.0f, itemY + 23.0f);
            D2D1_COLOR_F iconCol = item.isDestructive ? fgCol : (isHovered ? pal.textPrimary : pal.textSecondary);
            DrawFluentIcon(rt, item.icon, iconRc, WithAlpha(iconCol, iconCol.a * contentAlpha), 1.4f, brush);
            textLeft = MENU_PAD_X + 32.0f;
        }

        float textRight = menuWidth_ - MENU_PAD_X - 10.0f;
        if (fmtItem_ && !item.label.empty() && setCol(fgCol)) {
            rt->DrawText(item.label.c_str(), static_cast<UINT32>(item.label.size()), fmtItem_,
                D2D1::RectF(textLeft, itemY, textRight, itemY + itemH), brush);
        }
    }

    if (totalContentHeight_ > listViewHeight_ + 1.0f) {
        float ratio = listViewHeight_ / totalContentHeight_;
        float barH = (std::max)(24.0f, listViewHeight_ * ratio);
        float maxScroll = totalContentHeight_ - listViewHeight_;
        float barY = listTop_ + (maxScroll > 0.0f ? (scrollY_ / maxScroll) * (listViewHeight_ - barH) : 0.0f);
        if (setCol(pal.textTertiary)) {
            rt->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(menuWidth_ - 5.0f, barY + 2.0f, menuWidth_ - 2.0f, barY + barH - 2.0f), 1.5f, 1.5f),
                brush
            );
        }
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
            StartAnimTimer();
            RequestRedraw();
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        trackingMouse_ = false;
        if (hoveredIndex_ != -1 || pressedIndex_ != -1) {
            hoveredIndex_ = -1;
            pressedIndex_ = -1;
            StartAnimTimer();
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
