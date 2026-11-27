#include "ui/settings_window.hpp"
#include "core/hook.hpp"
#include "core/layout_engine.hpp"
#include "platform/tray.hpp"
#include "platform/win32_utils.hpp"
#include "ui/fluent_menu.hpp"
#include <commdlg.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>

namespace UI {

constexpr float WND_LOGICAL_W = 680.0f;
constexpr float WND_LOGICAL_H = 572.0f;

constexpr float SIDEBAR_W = 192.0f;
constexpr float FOOTER_H = 42.0f;
constexpr float FOOTER_Y = WND_LOGICAL_H - FOOTER_H;
constexpr float TOP_DRAG_H = 32.0f;
constexpr float CAPTION_BTN_W = 42.0f;
constexpr float CAPTION_BTN_H = 30.0f;

// Navigation rail geometry
constexpr float NAV_X = 10.0f;
constexpr float NAV_W = 172.0f;
constexpr float NAV_START_Y = 52.0f;
constexpr float NAV_ITEM_H = 38.0f;
constexpr float NAV_GAP = 4.0f;

// Workspace pane geometry
constexpr float WORK_PAD_X = 218.0f;
constexpr float WORK_RIGHT = 654.0f;
constexpr float WORK_W = WORK_RIGHT - WORK_PAD_X; // 436

constexpr float HEADER_Y = 32.0f;
constexpr float BODY_TOP = 68.0f;
constexpr float BODY_BOTTOM = FOOTER_Y - 1.0f;

// General section rows
constexpr float SEC1_ROW0_Y = 166.0f;
constexpr float SEC2_ROW0_Y = 396.0f;
constexpr float ROW_H = 48.0f;

// AppLayouts, Dictionary & Exclusions toolbar and list
constexpr float TOOLBAR_Y = 78.0f;
constexpr float TOOLBAR_H = 34.0f;
constexpr float LIST_Y = 124.0f;
constexpr float LIST_H = 386.0f;
constexpr float APP_ROW_H = 44.0f;
constexpr float DICT_ROW_H = 48.0f;
constexpr float EXCL_ROW_H = 44.0f;

constexpr UINT_PTR TIMER_ANIM = 101;
constexpr UINT WM_SETTINGS_STATUS_SYNC = WM_APP + 201;

constexpr UINT ID_PICKER_PROC_BASE = 4000;
constexpr UINT ID_PICKER_BROWSE = 4999;

namespace {

D2D1_RECT_F RcBtnMinimize() {
    return D2D1::RectF(WND_LOGICAL_W - CAPTION_BTN_W * 2.0f, 0.0f, WND_LOGICAL_W - CAPTION_BTN_W, CAPTION_BTN_H);
}

D2D1_RECT_F RcBtnClose() {
    return D2D1::RectF(WND_LOGICAL_W - CAPTION_BTN_W, 0.0f, WND_LOGICAL_W, CAPTION_BTN_H);
}

D2D1_RECT_F RcThemeToggle() {
    return D2D1::RectF(WORK_RIGHT - 32.0f, 32.0f, WORK_RIGHT, 64.0f);
}

D2D1_RECT_F RcNavItem(int idx) {
    float y = NAV_START_Y + idx * (NAV_ITEM_H + NAV_GAP);
    return D2D1::RectF(NAV_X, y, NAV_X + NAV_W, y + NAV_ITEM_H);
}

D2D1_RECT_F RcFooterPauseBtn() {
    return D2D1::RectF(WND_LOGICAL_W - 164.0f, FOOTER_Y + 6.0f, WND_LOGICAL_W - 14.0f, FOOTER_Y + 36.0f);
}

D2D1_RECT_F RcToggleRow(int idx) {
    float y = (idx < 4) ? (SEC1_ROW0_Y + idx * ROW_H) : (SEC2_ROW0_Y + (idx - 4) * ROW_H);
    return D2D1::RectF(WORK_PAD_X - 8.0f, y, WORK_RIGHT + 8.0f, y + ROW_H);
}

D2D1_RECT_F RcAppInput()  { return D2D1::RectF(WORK_PAD_X, TOOLBAR_Y, WORK_PAD_X + 174.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcAppPick()   { return D2D1::RectF(WORK_PAD_X + 182.0f, TOOLBAR_Y, WORK_PAD_X + 256.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcAppLangEn() { return D2D1::RectF(WORK_PAD_X + 264.0f, TOOLBAR_Y, WORK_PAD_X + 304.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcAppLangRu() { return D2D1::RectF(WORK_PAD_X + 304.0f, TOOLBAR_Y, WORK_PAD_X + 344.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcAppAdd()    { return D2D1::RectF(WORK_PAD_X + 352.0f, TOOLBAR_Y, WORK_RIGHT, TOOLBAR_Y + TOOLBAR_H); }

D2D1_RECT_F RcDictInput()  { return D2D1::RectF(WORK_PAD_X, TOOLBAR_Y, WORK_PAD_X + 256.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcDictLangEn() { return D2D1::RectF(WORK_PAD_X + 264.0f, TOOLBAR_Y, WORK_PAD_X + 304.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcDictLangRu() { return D2D1::RectF(WORK_PAD_X + 304.0f, TOOLBAR_Y, WORK_PAD_X + 344.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcDictAdd()    { return D2D1::RectF(WORK_PAD_X + 352.0f, TOOLBAR_Y, WORK_RIGHT, TOOLBAR_Y + TOOLBAR_H); }

D2D1_RECT_F RcExclInput() { return D2D1::RectF(WORK_PAD_X, TOOLBAR_Y, WORK_PAD_X + 262.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcExclPick()  { return D2D1::RectF(WORK_PAD_X + 270.0f, TOOLBAR_Y, WORK_PAD_X + 344.0f, TOOLBAR_Y + TOOLBAR_H); }
D2D1_RECT_F RcExclAdd()   { return D2D1::RectF(WORK_PAD_X + 352.0f, TOOLBAR_Y, WORK_RIGHT, TOOLBAR_Y + TOOLBAR_H); }

D2D1_RECT_F RcListBox()   { return D2D1::RectF(WORK_PAD_X, LIST_Y, WORK_RIGHT, LIST_Y + LIST_H); }

bool PointInRectF(const D2D1_RECT_F& rc, float x, float y) {
    return x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom;
}

D2D1_RECT_F ShiftRectY(const D2D1_RECT_F& rc, float dy) {
    return D2D1::RectF(rc.left, rc.top + dy, rc.right, rc.bottom + dy);
}

} // namespace

SettingsWindow& SettingsWindow::Instance() {
    static SettingsWindow s_instance;
    return s_instance;
}

SettingsWindow::~SettingsWindow() {
    Destroy();
}

void SettingsWindow::EnsureFormats() {
    auto& gfx = FluentGraphics::Instance();
    if (!gfx.EnsureInitialized()) return;

    if (!fmtBrand_) {
        fmtBrand_ = gfx.CreateTextFormat(13.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtNav_) {
        fmtNav_ = gfx.CreateTextFormat(13.5f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtNavSelected_) {
        fmtNavSelected_ = gfx.CreateTextFormat(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtPageTitle_) {
        fmtPageTitle_ = gfx.CreateTextFormat(22.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtSubtitle_) {
        fmtSubtitle_ = gfx.CreateTextFormat(12.0f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtGroupLabel_) {
        fmtGroupLabel_ = gfx.CreateTextFormat(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtRowTitle_) {
        fmtRowTitle_ = gfx.CreateTextFormat(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtRowSub_) {
        fmtRowSub_ = gfx.CreateTextFormat(11.5f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtRowSub_) {
            fmtRowSub_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
            fmtRowSub_->SetTrimming(&trim, nullptr);
        }
    }
    if (!fmtBadge_) {
        fmtBadge_ = gfx.CreateTextFormat(11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtBtn_) {
        fmtBtn_ = gfx.CreateTextFormat(13.0f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!fmtInput_) {
        fmtInput_ = gfx.CreateTextFormat(13.0f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtInput_) {
            fmtInput_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }
    if (!fmtSmall_) {
        fmtSmall_ = gfx.CreateTextFormat(11.5f, DWRITE_FONT_WEIGHT_NORMAL, false,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}

void SettingsWindow::ReleaseFormats() {
    if (fmtBrand_)       { fmtBrand_->Release();       fmtBrand_ = nullptr; }
    if (fmtNav_)         { fmtNav_->Release();         fmtNav_ = nullptr; }
    if (fmtNavSelected_) { fmtNavSelected_->Release(); fmtNavSelected_ = nullptr; }
    if (fmtPageTitle_)   { fmtPageTitle_->Release();   fmtPageTitle_ = nullptr; }
    if (fmtSubtitle_)   { fmtSubtitle_->Release();   fmtSubtitle_ = nullptr; }
    if (fmtGroupLabel_) { fmtGroupLabel_->Release(); fmtGroupLabel_ = nullptr; }
    if (fmtRowTitle_)   { fmtRowTitle_->Release();   fmtRowTitle_ = nullptr; }
    if (fmtRowSub_)     { fmtRowSub_->Release();     fmtRowSub_ = nullptr; }
    if (fmtBadge_)      { fmtBadge_->Release();      fmtBadge_ = nullptr; }
    if (fmtBtn_)        { fmtBtn_->Release();        fmtBtn_ = nullptr; }
    if (fmtInput_)      { fmtInput_->Release();      fmtInput_ = nullptr; }
    if (fmtSmall_)      { fmtSmall_->Release();      fmtSmall_ = nullptr; }
}

void SettingsWindow::RefreshWindowIcons() {
    if (!hWnd_) return;
    if (hIconBig_ && hIconSmall_) return;

    HICON newBig = Platform::TrayIcon::CreateFluentAppIcon(32);
    HICON newSmall = Platform::TrayIcon::CreateFluentAppIcon(16);

    if (newBig) {
        SendMessageW(hWnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(newBig));
        if (hIconBig_) DestroyIcon(hIconBig_);
        hIconBig_ = newBig;
    }
    if (newSmall) {
        SendMessageW(hWnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(newSmall));
        if (hIconSmall_) DestroyIcon(hIconSmall_);
        hIconSmall_ = newSmall;
    }
}

void SettingsWindow::RefreshSnapshot(bool snapAnimations) {
    cfg_ = Config::ConfigManager::Instance().GetSnapshot();
    livePaused_ = Core::HookManager::Instance().IsPaused();
    HWND fg = GetForegroundWindow();
    if (fg && fg != hWnd_) {
        WORD lang = Core::LayoutEngine::Instance().GetLayout(fg);
        liveIsRu_ = ((lang & 0xFF) == 0x19);
    }

    if (snapAnimations) {
        toggleAnim_[0] = cfg_.autoSwitch ? 1.0f : 0.0f;
        toggleAnim_[1] = cfg_.capsLockSwitch ? 1.0f : 0.0f;
        toggleAnim_[2] = cfg_.capsHoldSwitch ? 1.0f : 0.0f;
        toggleAnim_[3] = cfg_.doubleCapsConvert ? 1.0f : 0.0f;
        toggleAnim_[4] = cfg_.passwordGuard ? 1.0f : 0.0f;
        toggleAnim_[5] = cfg_.autostart ? 1.0f : 0.0f;
        statusPausedAnim_ = livePaused_ ? 1.0f : 0.0f;
        langChipRuAnim_ = liveIsRu_ ? 1.0f : 0.0f;
        langSelectorAnim_ = (selectedAppLayout_ == Config::AppLayout::Russian) ? 1.0f : 0.0f;
        dictLangSelectorAnim_ = (selectedDictLayout_ == Config::AppLayout::Russian) ? 1.0f : 0.0f;
        pageTransition_ = 1.0f;
    }
}

bool SettingsWindow::ResolveDarkMode() const {
    if (cfg_.theme == Config::ThemeMode::Light) return false;
    if (cfg_.theme == Config::ThemeMode::Dark) return true;
    return IsSystemDarkMode();
}

void SettingsWindow::ToggleTheme() {
    bool nextDark = !darkMode_;
    Config::ThemeMode nextMode = nextDark ? Config::ThemeMode::Dark : Config::ThemeMode::Light;
    Config::ConfigManager::Instance().Update([nextMode](Config::AppConfig& cfg) {
        cfg.theme = nextMode;
    });
    Config::ConfigManager::Instance().Save();
    cfg_.theme = nextMode;
    darkMode_ = nextDark;
    if (hWnd_ && IsWindow(hWnd_)) {
        hasBackdrop_ = ApplyWindowBackdrop(hWnd_, BackdropKind::None, darkMode_);
        appliedDark_ = darkMode_;
        backdropApplied_ = true;
    }
    TriggerSavedToast();
    RequestRedraw();
}

bool SettingsWindow::Create() {
    if (hWnd_ && IsWindow(hWnd_)) return true;

    bool gpuAvail = FluentGraphics::Instance().IsGpuAvailable();
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CapsSwitchSettingsWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_DBLCLKS;
    RegisterClassExW(&wc);

    cfg_ = Config::ConfigManager::Instance().GetSnapshot();
    darkMode_ = ResolveDarkMode();

    DWORD exStyle = gpuAvail ? WS_EX_NOREDIRECTIONBITMAP : 0;
    DWORD style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX;
    hWnd_ = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        L"CapsSwitch — Настройки",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(WND_LOGICAL_W), static_cast<int>(WND_LOGICAL_H),
        nullptr, nullptr, hInst, this
    );
    if (!hWnd_) return false;

    dpi_ = GetWindowDpiSafe(hWnd_);
    float scale = dpi_ / 96.0f;
    int pxW = static_cast<int>(std::round(WND_LOGICAL_W * scale));
    int pxH = static_cast<int>(std::round(WND_LOGICAL_H * scale));

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hWnd_, nullptr, (scrW - pxW) / 2, (scrH - pxH) / 2, pxW, pxH,
        SWP_NOZORDER | SWP_NOACTIVATE);

    hasBackdrop_ = ApplyWindowBackdrop(hWnd_, BackdropKind::None, darkMode_);
    appliedDark_ = darkMode_;
    backdropApplied_ = true;

    surface_.EnsureSize(hWnd_, pxW, pxH);

    appInput_.rect = RcAppInput();
    appInput_.placeholder = L"telegram.exe";
    dictInput_.rect = RcDictInput();
    dictInput_.placeholder = L"слово (напр. спс или docker)";
    exclInput_.rect = RcExclInput();
    exclInput_.placeholder = L"cs2.exe";

    EnsureFormats();
    RefreshSnapshot(true);
    RefreshWindowIcons();
    return true;
}

void SettingsWindow::Show() {
    if (!hWnd_ || !IsWindow(hWnd_)) {
        Create();
    }
    if (!hWnd_) return;

    RefreshSnapshot(true);
    darkMode_ = ResolveDarkMode();
    if (!backdropApplied_ || darkMode_ != appliedDark_) {
        hasBackdrop_ = ApplyWindowBackdrop(hWnd_, BackdropKind::None, darkMode_);
        appliedDark_ = darkMode_;
        backdropApplied_ = true;
    }

    POINT cursorPt = {};
    GetCursorPos(&cursorPt);
    HMONITOR hMon = MonitorFromPoint(cursorPt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfoW(hMon, &mi)) {
        dpi_ = GetWindowDpiSafe(hWnd_);
        float scale = dpi_ / 96.0f;
        int winW = static_cast<int>(std::round(WND_LOGICAL_W * scale));
        int winH = static_cast<int>(std::round(WND_LOGICAL_H * scale));
        RECT wr = mi.rcWork;
        int posX = wr.left + ((wr.right - wr.left) - winW) / 2;
        int posY = wr.top + ((wr.bottom - wr.top) - winH) / 2;
        SetWindowPos(hWnd_, nullptr, posX, posY, winW, winH, SWP_NOZORDER | SWP_NOACTIVATE);
        surface_.EnsureSize(hWnd_, winW, winH);
    }

    RefreshSnapshot(true);
    RefreshWindowIcons();
    ShowWindow(hWnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hWnd_);
    RenderNow();
}

void SettingsWindow::Hide() {
    FluentMenu::Instance().Hide();
    StopAnimation();
    appInput_.focused = false;
    dictInput_.focused = false;
    exclInput_.focused = false;
    if (hWnd_ && IsWindow(hWnd_)) {
        ShowWindow(hWnd_, SW_HIDE);
    }
}

void SettingsWindow::Destroy() {
    StopAnimation();
    if (hWnd_ && IsWindow(hWnd_)) {
        DestroyWindow(hWnd_);
        hWnd_ = nullptr;
    }
    backdropApplied_ = false;
    if (hIconBig_)   { DestroyIcon(hIconBig_);   hIconBig_ = nullptr; }
    if (hIconSmall_) { DestroyIcon(hIconSmall_); hIconSmall_ = nullptr; }
    surface_.Release();
    ReleaseFormats();
}

bool SettingsWindow::IsVisible() const {
    return hWnd_ && IsWindowVisible(hWnd_);
}

void SettingsWindow::NotifyStatusChanged(WORD langId, bool isPaused) {
    bool isRu = ((langId & 0xFF) == 0x19);
    if (hWnd_ && IsWindow(hWnd_)) {
        PostMessageW(hWnd_, WM_SETTINGS_STATUS_SYNC, isRu ? 1 : 0, isPaused ? 1 : 0);
    }
}

void SettingsWindow::EnsureAnimationRunning() {
    if (!animRunning_ && hWnd_ && IsWindowVisible(hWnd_)) {
        lastTickUs_ = Platform::GetCurrentTimeMicroseconds();
        SetTimer(hWnd_, TIMER_ANIM, 10, nullptr);
        animRunning_ = true;
    }
}

void SettingsWindow::StopAnimation() {
    if (animRunning_ && hWnd_) {
        KillTimer(hWnd_, TIMER_ANIM);
        animRunning_ = false;
    }
}

void SettingsWindow::RequestRedraw() {
    if (!hWnd_) return;
    if (surface_.IsGpu()) {
        RenderNow();
    } else {
        InvalidateRect(hWnd_, nullptr, FALSE);
    }
}

void SettingsWindow::SwitchTab(SettingsTab nextTab) {
    if (activeTab_ == nextTab) return;
    prevTab_ = activeTab_;
    pageDir_ = (static_cast<int>(nextTab) > static_cast<int>(activeTab_)) ? 1 : -1;
    activeTab_ = nextTab;
    pageTransition_ = 0.0f;
    appInput_.focused = false;
    dictInput_.focused = false;
    exclInput_.focused = false;
    FluentMenu::Instance().Hide();
    EnsureAnimationRunning();
    RequestRedraw();
}

void SettingsWindow::TriggerSavedToast() {
    toastAlpha_ = 1.0f;
    toastHoldSec_ = 1.3f;
    EnsureAnimationRunning();
}

void SettingsWindow::TogglePause() {
    bool next = !Core::HookManager::Instance().IsPaused();
    Core::HookManager::Instance().SetPaused(next);
    livePaused_ = next;
    HWND fg = GetForegroundWindow();
    WORD lang = Core::LayoutEngine::Instance().GetLayout(fg);
    Platform::TrayIcon::Instance().UpdateLanguage(lang, next);
    EnsureAnimationRunning();
    RequestRedraw();
}

void SettingsWindow::TickAnimations(float dt) {
    bool needMore = false;

    auto animTo = [&](float& val, float target, float speed) {
        float next = ExpDecay(val, target, speed, dt);
        if (next != target) needMore = true;
        val = next;
    };

    const bool targets[6] = {
        cfg_.autoSwitch, cfg_.capsLockSwitch, cfg_.capsHoldSwitch, cfg_.doubleCapsConvert, cfg_.passwordGuard, cfg_.autostart
    };
    for (int i = 0; i < 6; ++i) {
        animTo(toggleAnim_[i], targets[i] ? 1.0f : 0.0f, 34.0f);

        bool rowHov = (activeTab_ == SettingsTab::General &&
            hovered_.zone == static_cast<HitZone>(static_cast<int>(HitZone::ToggleRow0) + i));
        bool rowPrs = (activeTab_ == SettingsTab::General &&
            pressed_.zone == static_cast<HitZone>(static_cast<int>(HitZone::ToggleRow0) + i));
        animTo(rowHover_[i], rowHov ? 1.0f : 0.0f, 38.0f);
        animTo(toggleHover_[i], rowHov ? 1.0f : 0.0f, 38.0f);
        animTo(togglePress_[i], rowPrs ? 1.0f : 0.0f, 40.0f);
    }

    for (int i = 0; i < 4; ++i) {
        bool nHov = (hovered_.zone == static_cast<HitZone>(static_cast<int>(HitZone::NavGeneral) + i));
        animTo(navHover_[i], nHov ? 1.0f : 0.0f, 38.0f);
    }

    if (pageTransition_ < 1.0f) {
        pageTransition_ = (std::min)(1.0f, pageTransition_ + dt / 0.11f);
        if (pageTransition_ < 1.0f) needMore = true;
    }

    animTo(captionHover_[0], hovered_.zone == HitZone::BtnMinimize ? 1.0f : 0.0f, 38.0f);
    animTo(captionHover_[1], hovered_.zone == HitZone::BtnClose ? 1.0f : 0.0f, 38.0f);
    animTo(themeBtnHover_, hovered_.zone == HitZone::ThemeToggle ? 1.0f : 0.0f, 38.0f);
    animTo(footerPauseHover_, hovered_.zone == HitZone::FooterPauseBtn ? 1.0f : 0.0f, 38.0f);
    animTo(statusPausedAnim_, livePaused_ ? 1.0f : 0.0f, 28.0f);
    animTo(langChipRuAnim_, liveIsRu_ ? 1.0f : 0.0f, 30.0f);
    animTo(langSelectorAnim_, selectedAppLayout_ == Config::AppLayout::Russian ? 1.0f : 0.0f, 38.0f);
    animTo(dictLangSelectorAnim_, selectedDictLayout_ == Config::AppLayout::Russian ? 1.0f : 0.0f, 38.0f);

    bool pickHov = (hovered_.zone == HitZone::AppPickBtn || hovered_.zone == HitZone::ExclPickBtn);
    bool addHov = (hovered_.zone == HitZone::AppAddBtn || hovered_.zone == HitZone::DictAddBtn || hovered_.zone == HitZone::ExclAddBtn);
    animTo(btnPickHover_, pickHov ? 1.0f : 0.0f, 38.0f);
    animTo(btnAddHover_, addHov ? 1.0f : 0.0f, 38.0f);

    animTo(appInput_.focusAnim, appInput_.focused ? 1.0f : 0.0f, 38.0f);
    animTo(appInput_.hoverAnim, appInput_.hovered ? 1.0f : 0.0f, 38.0f);
    animTo(dictInput_.focusAnim, dictInput_.focused ? 1.0f : 0.0f, 38.0f);
    animTo(dictInput_.hoverAnim, dictInput_.hovered ? 1.0f : 0.0f, 38.0f);
    animTo(exclInput_.focusAnim, exclInput_.focused ? 1.0f : 0.0f, 38.0f);
    animTo(exclInput_.hoverAnim, exclInput_.hovered ? 1.0f : 0.0f, 38.0f);

    if (appInput_.focused) {
        appInput_.caretPhase += dt * 1.9f;
        if (appInput_.caretPhase >= 2.0f) appInput_.caretPhase -= 2.0f;
        needMore = true;
    }
    if (dictInput_.focused) {
        dictInput_.caretPhase += dt * 1.9f;
        if (dictInput_.caretPhase >= 2.0f) dictInput_.caretPhase -= 2.0f;
        needMore = true;
    }
    if (exclInput_.focused) {
        exclInput_.caretPhase += dt * 1.9f;
        if (exclInput_.caretPhase >= 2.0f) exclInput_.caretPhase -= 2.0f;
        needMore = true;
    }

    float maxAppScroll = (std::max)(0.0f, static_cast<float>(cfg_.appLayouts.size()) * APP_ROW_H + 8.0f - LIST_H);
    appScrollTarget_ = (std::clamp)(appScrollTarget_, 0.0f, maxAppScroll);
    animTo(appScrollY_, appScrollTarget_, 32.0f);

    float maxDictScroll = (std::max)(0.0f, static_cast<float>(cfg_.wordDictionary.size()) * DICT_ROW_H + 8.0f - LIST_H);
    dictScrollTarget_ = (std::clamp)(dictScrollTarget_, 0.0f, maxDictScroll);
    animTo(dictScrollY_, dictScrollTarget_, 32.0f);

    float maxExclScroll = (std::max)(0.0f, static_cast<float>(cfg_.exclusions.size()) * EXCL_ROW_H + 8.0f - LIST_H);
    exclScrollTarget_ = (std::clamp)(exclScrollTarget_, 0.0f, maxExclScroll);
    animTo(exclScrollY_, exclScrollTarget_, 32.0f);

    if (toastHoldSec_ > 0.0f) {
        toastHoldSec_ = (std::max)(0.0f, toastHoldSec_ - dt);
        needMore = true;
    } else if (toastAlpha_ > 0.0f) {
        animTo(toastAlpha_, 0.0f, 14.0f);
    }

    if (!needMore) {
        StopAnimation();
    }
}

void SettingsWindow::ShowAppPickerMenu(const D2D1_RECT_F& anchorDip, bool forAppLayouts) {
    std::vector<std::wstring> procs = Platform::GetRunningWindowProcesses();
    std::vector<FluentMenuItem> items;

    if (procs.empty()) {
        items.push_back({ 0, L"(Нет активных окон)", L"", FluentIcon::None, false, false, true });
    } else {
        size_t limit = (std::min)(procs.size(), static_cast<size_t>(40));
        for (size_t i = 0; i < limit; ++i) {
            items.push_back({ ID_PICKER_PROC_BASE + static_cast<UINT>(i), procs[i], L"",
                FluentIcon::WindowPicker, false, false, false });
        }
    }
    items.push_back({ 0, L"", L"", FluentIcon::None, true, false, false });
    items.push_back({ ID_PICKER_BROWSE, L"Выбрать файл .exe...", L"", FluentIcon::Folder, false, false, false });

    float scale = dpi_ / 96.0f;
    POINT pt = {
        static_cast<LONG>(std::round(anchorDip.left * scale)),
        static_cast<LONG>(std::round((anchorDip.bottom + 4.0f) * scale))
    };
    ClientToScreen(hWnd_, &pt);

    FluentMenu::Instance().Show(hWnd_, pt, false, items, [this, procs, forAppLayouts](UINT cmd) {
        FluentTextInput& target = forAppLayouts ? appInput_ : exclInput_;
        if (cmd == ID_PICKER_BROWSE) {
            wchar_t fileBuf[MAX_PATH] = { 0 };
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd_;
            ofn.lpstrFilter = L"Программы (*.exe)\0*.exe\0Все файлы (*.*)\0*.*\0";
            ofn.lpstrFile = fileBuf;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&ofn)) {
                std::wstring fullPath(fileBuf);
                size_t lastSlash = fullPath.find_last_of(L"\\/");
                std::wstring fileName = (lastSlash != std::wstring::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
                target.text = Platform::ToLower(fileName);
                target.caretPos = target.text.size();
                target.selAnchor = target.caretPos;
                target.focused = true;
                target.caretPhase = 0.0f;
                EnsureInputCaretVisible(target);
                EnsureAnimationRunning();
                RequestRedraw();
            }
        } else if (cmd >= ID_PICKER_PROC_BASE && cmd < ID_PICKER_PROC_BASE + procs.size()) {
            target.text = procs[cmd - ID_PICKER_PROC_BASE];
            target.caretPos = target.text.size();
            target.selAnchor = target.caretPos;
            target.focused = true;
            target.caretPhase = 0.0f;
            EnsureInputCaretVisible(target);
            EnsureAnimationRunning();
            RequestRedraw();
        }
    }, darkMode_ ? 1 : 0, false);
}

void SettingsWindow::AddAppLayoutFromInput() {
    if (appInput_.text.empty()) return;
    std::wstring proc = appInput_.text;
    Config::AppLayout chosen = selectedAppLayout_;
    Config::ConfigManager::Instance().Update([&proc, chosen](Config::AppConfig& cfg) {
        cfg.SetAppLayout(proc, chosen);
    });
    Config::ConfigManager::Instance().Save();
    appInput_.text.clear();
    appInput_.caretPos = 0;
    appInput_.selAnchor = 0;
    appInput_.scrollX = 0.0f;
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

void SettingsWindow::AddExclusionFromInput() {
    if (exclInput_.text.empty()) return;
    std::wstring proc = exclInput_.text;
    Config::ConfigManager::Instance().Update([&proc](Config::AppConfig& cfg) {
        cfg.AddExclusion(proc);
    });
    Config::ConfigManager::Instance().Save();
    exclInput_.text.clear();
    exclInput_.caretPos = 0;
    exclInput_.selAnchor = 0;
    exclInput_.scrollX = 0.0f;
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

void SettingsWindow::ToggleAppRuleLayout(size_t index) {
    if (index >= cfg_.appLayouts.size()) return;
    std::wstring proc = cfg_.appLayouts[index].processName;
    Config::AppLayout next = (cfg_.appLayouts[index].layout == Config::AppLayout::Russian)
        ? Config::AppLayout::English
        : Config::AppLayout::Russian;
    Config::ConfigManager::Instance().Update([&proc, next](Config::AppConfig& cfg) {
        cfg.SetAppLayout(proc, next);
    });
    Config::ConfigManager::Instance().Save();
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

void SettingsWindow::RemoveAppRule(size_t index) {
    if (index >= cfg_.appLayouts.size()) return;
    std::wstring proc = cfg_.appLayouts[index].processName;
    Config::ConfigManager::Instance().Update([&proc](Config::AppConfig& cfg) {
        cfg.RemoveAppLayout(proc);
    });
    Config::ConfigManager::Instance().Save();
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

void SettingsWindow::RemoveExclusion(size_t index) {
    if (index >= cfg_.exclusions.size()) return;
    std::wstring proc = cfg_.exclusions[index];
    Config::ConfigManager::Instance().Update([&proc](Config::AppConfig& cfg) {
        cfg.RemoveExclusion(proc);
    });
    Config::ConfigManager::Instance().Save();
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

void SettingsWindow::AutoSelectDictLayoutFromInput() {
    bool hasCyr = false;
    bool hasLat = false;
    for (wchar_t ch : dictInput_.text) {
        if ((ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё') {
            hasCyr = true;
            break;
        }
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z')) {
            hasLat = true;
        }
    }
    if (hasCyr) {
        selectedDictLayout_ = Config::AppLayout::Russian;
        EnsureAnimationRunning();
    } else if (hasLat) {
        selectedDictLayout_ = Config::AppLayout::English;
        EnsureAnimationRunning();
    }
}

void SettingsWindow::AddDictWordFromInput() {
    if (dictInput_.text.empty()) return;
    std::wstring word = dictInput_.text;
    Config::AppLayout chosen = selectedDictLayout_;
    Config::ConfigManager::Instance().Update([&word, chosen](Config::AppConfig& cfg) {
        cfg.SetWordRule(word, chosen);
    });
    Config::ConfigManager::Instance().Save();
    dictInput_.text.clear();
    dictInput_.caretPos = 0;
    dictInput_.selAnchor = 0;
    dictInput_.scrollX = 0.0f;
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

void SettingsWindow::ToggleDictWordLayout(size_t index) {
    if (index >= cfg_.wordDictionary.size()) return;
    std::wstring word = cfg_.wordDictionary[index].word;
    Config::AppLayout next = (cfg_.wordDictionary[index].layout == Config::AppLayout::Russian)
        ? Config::AppLayout::English
        : Config::AppLayout::Russian;
    Config::ConfigManager::Instance().Update([&word, next](Config::AppConfig& cfg) {
        cfg.SetWordRule(word, next);
    });
    Config::ConfigManager::Instance().Save();
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

void SettingsWindow::RemoveDictWord(size_t index) {
    if (index >= cfg_.wordDictionary.size()) return;
    std::wstring word = cfg_.wordDictionary[index].word;
    Config::ConfigManager::Instance().Update([&word](Config::AppConfig& cfg) {
        cfg.RemoveWordRule(word);
    });
    Config::ConfigManager::Instance().Save();
    RefreshSnapshot(false);
    TriggerSavedToast();
    RequestRedraw();
}

FluentTextInput* SettingsWindow::ActiveTextInput() {
    if (activeTab_ == SettingsTab::AppLayouts && appInput_.focused) return &appInput_;
    if (activeTab_ == SettingsTab::Dictionary && dictInput_.focused) return &dictInput_;
    if (activeTab_ == SettingsTab::Exclusions && exclInput_.focused) return &exclInput_;
    return nullptr;
}

size_t SettingsWindow::HitTestTextCaret(const FluentTextInput& input, float localX) {
    if (input.text.empty()) return 0;
    IDWriteFactory* dw = FluentGraphics::Instance().DWriteFactory();
    if (!dw || !fmtInput_) return input.text.size();

    IDWriteTextLayout* layout = nullptr;
    if (FAILED(dw->CreateTextLayout(input.text.c_str(), static_cast<UINT32>(input.text.size()),
        fmtInput_, 4096.0f, input.rect.bottom - input.rect.top, &layout)) || !layout) {
        return input.text.size();
    }

    BOOL isTrailing = FALSE;
    BOOL isInside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics = {};
    float relX = (localX - (input.rect.left + 10.0f)) + input.scrollX;
    layout->HitTestPoint(relX, 12.0f, &isTrailing, &isInside, &metrics);
    layout->Release();

    size_t pos = metrics.textPosition + (isTrailing ? 1 : 0);
    return (std::min)(pos, input.text.size());
}

void SettingsWindow::EnsureInputCaretVisible(FluentTextInput& input) {
    float innerW = (input.rect.right - input.rect.left) - 20.0f;
    if (innerW <= 0.0f) return;

    IDWriteFactory* dw = FluentGraphics::Instance().DWriteFactory();
    if (!dw || !fmtInput_) return;

    IDWriteTextLayout* layout = nullptr;
    if (FAILED(dw->CreateTextLayout(input.text.c_str(), static_cast<UINT32>(input.text.size()),
        fmtInput_, 4096.0f, input.rect.bottom - input.rect.top, &layout)) || !layout) {
        return;
    }

    DWRITE_HIT_TEST_METRICS cm = {};
    float caretX = 0.0f;
    float caretY = 0.0f;
    layout->HitTestTextPosition(static_cast<UINT32>(input.caretPos), FALSE, &caretX, &caretY, &cm);

    DWRITE_TEXT_METRICS tm = {};
    layout->GetMetrics(&tm);
    layout->Release();

    if (caretX - input.scrollX > innerW - 4.0f) {
        input.scrollX = caretX - (innerW - 4.0f);
    } else if (caretX - input.scrollX < 0.0f) {
        input.scrollX = caretX;
    }
    float maxScroll = (std::max)(0.0f, tm.widthIncludingTrailingWhitespace - innerW + 8.0f);
    input.scrollX = (std::clamp)(input.scrollX, 0.0f, maxScroll);
}

bool SettingsWindow::HandleInputKeyDown(FluentTextInput& input, WPARAM vk) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    auto deleteSelection = [&]() -> bool {
        if (!input.HasSelection()) return false;
        size_t s = 0, e = 0;
        input.GetSelectionRange(s, e);
        input.text.erase(s, e - s);
        input.caretPos = s;
        input.selAnchor = s;
        return true;
    };

    switch (vk) {
    case VK_LEFT:
        if (input.caretPos > 0) --input.caretPos;
        if (!shift) input.ClearSelection();
        break;
    case VK_RIGHT:
        if (input.caretPos < input.text.size()) ++input.caretPos;
        if (!shift) input.ClearSelection();
        break;
    case VK_HOME:
        input.caretPos = 0;
        if (!shift) input.ClearSelection();
        break;
    case VK_END:
        input.caretPos = input.text.size();
        if (!shift) input.ClearSelection();
        break;
    case VK_BACK:
        if (!deleteSelection() && input.caretPos > 0) {
            input.text.erase(input.caretPos - 1, 1);
            --input.caretPos;
            input.ClearSelection();
        }
        break;
    case VK_DELETE:
        if (!deleteSelection() && input.caretPos < input.text.size()) {
            input.text.erase(input.caretPos, 1);
            input.ClearSelection();
        }
        break;
    case VK_RETURN:
        if (&input == &appInput_) AddAppLayoutFromInput();
        else if (&input == &dictInput_) AddDictWordFromInput();
        else AddExclusionFromInput();
        return true;
    case VK_ESCAPE:
        input.focused = false;
        EnsureAnimationRunning();
        RequestRedraw();
        return true;
    case 'A':
        if (ctrl) {
            input.selAnchor = 0;
            input.caretPos = input.text.size();
            break;
        }
        return false;
    case 'C':
    case 'X':
        if (ctrl && input.HasSelection()) {
            size_t s = 0, e = 0;
            input.GetSelectionRange(s, e);
            std::wstring sub = input.text.substr(s, e - s);
            if (OpenClipboard(hWnd_)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (sub.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    void* ptr = GlobalLock(hMem);
                    if (ptr) {
                        memcpy(ptr, sub.c_str(), (sub.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                }
                CloseClipboard();
            }
            if (vk == 'X') deleteSelection();
            break;
        }
        return false;
    case 'V':
        if (ctrl && OpenClipboard(hWnd_)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                const wchar_t* clip = static_cast<const wchar_t*>(GlobalLock(hData));
                if (clip) {
                    deleteSelection();
                    std::wstring paste;
                    for (const wchar_t* p = clip; *p; ++p) {
                        if (*p >= 32 && *p != L'\r' && *p != L'\n') paste.push_back(*p);
                    }
                    input.text.insert(input.caretPos, paste);
                    input.caretPos += paste.size();
                    input.ClearSelection();
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
            if (&input == &dictInput_) AutoSelectDictLayoutFromInput();
            break;
        }
        return false;
    default:
        return false;
    }

    input.caretPhase = 0.0f;
    EnsureInputCaretVisible(input);
    RequestRedraw();
    return true;
}

bool SettingsWindow::HandleInputChar(FluentTextInput& input, wchar_t ch) {
    if (ch < 32 || ch == 127) return false;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) return false;

    if (input.HasSelection()) {
        size_t s = 0, e = 0;
        input.GetSelectionRange(s, e);
        input.text.erase(s, e - s);
        input.caretPos = s;
    }
    if (input.text.size() < 128) {
        input.text.insert(input.caretPos, 1, ch);
        ++input.caretPos;
    }
    input.ClearSelection();
    input.caretPhase = 0.0f;
    if (&input == &dictInput_) AutoSelectDictLayoutFromInput();
    EnsureInputCaretVisible(input);
    RequestRedraw();
    return true;
}

SettingsWindow::HitResult SettingsWindow::HitTest(float dipX, float dipY) const {
    if (PointInRectF(RcBtnMinimize(), dipX, dipY))    return { HitZone::BtnMinimize, -1 };
    if (PointInRectF(RcBtnClose(), dipX, dipY))       return { HitZone::BtnClose, -1 };
    if (PointInRectF(RcThemeToggle(), dipX, dipY))    return { HitZone::ThemeToggle, -1 };

    if (PointInRectF(RcNavItem(0), dipX, dipY))       return { HitZone::NavGeneral, -1 };
    if (PointInRectF(RcNavItem(1), dipX, dipY))       return { HitZone::NavAppLayouts, -1 };
    if (PointInRectF(RcNavItem(2), dipX, dipY))       return { HitZone::NavDictionary, -1 };
    if (PointInRectF(RcNavItem(3), dipX, dipY))       return { HitZone::NavExclusions, -1 };
    if (PointInRectF(RcFooterPauseBtn(), dipX, dipY)) return { HitZone::FooterPauseBtn, -1 };

    if (activeTab_ == SettingsTab::General) {
        for (int i = 0; i < 6; ++i) {
            if (PointInRectF(RcToggleRow(i), dipX, dipY)) {
                return { static_cast<HitZone>(static_cast<int>(HitZone::ToggleRow0) + i), i };
            }
        }
    } else if (activeTab_ == SettingsTab::AppLayouts) {
        if (PointInRectF(RcAppInput(), dipX, dipY))  return { HitZone::AppInput, -1 };
        if (PointInRectF(RcAppPick(), dipX, dipY))   return { HitZone::AppPickBtn, -1 };
        if (PointInRectF(RcAppLangEn(), dipX, dipY)) return { HitZone::AppLangEn, -1 };
        if (PointInRectF(RcAppLangRu(), dipX, dipY)) return { HitZone::AppLangRu, -1 };
        if (PointInRectF(RcAppAdd(), dipX, dipY))    return { HitZone::AppAddBtn, -1 };

        D2D1_RECT_F listRc = RcListBox();
        if (PointInRectF(listRc, dipX, dipY)) {
            float relY = (dipY - listRc.top - 4.0f) + appScrollY_;
            int row = static_cast<int>(std::floor(relY / APP_ROW_H));
            if (relY >= 0.0f && row >= 0 && static_cast<size_t>(row) < cfg_.appLayouts.size()) {
                float rowY = listRc.top + 4.0f + row * APP_ROW_H - appScrollY_;
                D2D1_RECT_F delRc = D2D1::RectF(listRc.right - 40.0f, rowY + 6.0f, listRc.right - 12.0f, rowY + APP_ROW_H - 6.0f);
                D2D1_RECT_F badgeRc = D2D1::RectF(listRc.right - 92.0f, rowY + 9.0f, listRc.right - 48.0f, rowY + APP_ROW_H - 9.0f);
                if (PointInRectF(delRc, dipX, dipY))   return { HitZone::AppRowDelete, row };
                if (PointInRectF(badgeRc, dipX, dipY)) return { HitZone::AppRowBadge, row };
                return { HitZone::AppRow, row };
            }
        }
    } else if (activeTab_ == SettingsTab::Dictionary) {
        if (PointInRectF(RcDictInput(), dipX, dipY))  return { HitZone::DictInput, -1 };
        if (PointInRectF(RcDictLangEn(), dipX, dipY)) return { HitZone::DictLangEn, -1 };
        if (PointInRectF(RcDictLangRu(), dipX, dipY)) return { HitZone::DictLangRu, -1 };
        if (PointInRectF(RcDictAdd(), dipX, dipY))    return { HitZone::DictAddBtn, -1 };

        D2D1_RECT_F listRc = RcListBox();
        if (PointInRectF(listRc, dipX, dipY)) {
            float relY = (dipY - listRc.top - 4.0f) + dictScrollY_;
            int row = static_cast<int>(std::floor(relY / DICT_ROW_H));
            if (relY >= 0.0f && row >= 0 && static_cast<size_t>(row) < cfg_.wordDictionary.size()) {
                float rowY = listRc.top + 4.0f + row * DICT_ROW_H - dictScrollY_;
                D2D1_RECT_F delRc = D2D1::RectF(listRc.right - 40.0f, rowY + 6.0f, listRc.right - 12.0f, rowY + DICT_ROW_H - 6.0f);
                D2D1_RECT_F badgeRc = D2D1::RectF(listRc.right - 92.0f, rowY + 9.0f, listRc.right - 48.0f, rowY + DICT_ROW_H - 9.0f);
                if (PointInRectF(delRc, dipX, dipY))   return { HitZone::DictRowDelete, row };
                if (PointInRectF(badgeRc, dipX, dipY)) return { HitZone::DictRowBadge, row };
                return { HitZone::DictRow, row };
            }
        }
    } else if (activeTab_ == SettingsTab::Exclusions) {
        if (PointInRectF(RcExclInput(), dipX, dipY)) return { HitZone::ExclInput, -1 };
        if (PointInRectF(RcExclPick(), dipX, dipY))  return { HitZone::ExclPickBtn, -1 };
        if (PointInRectF(RcExclAdd(), dipX, dipY))   return { HitZone::ExclAddBtn, -1 };

        D2D1_RECT_F listRc = RcListBox();
        if (PointInRectF(listRc, dipX, dipY)) {
            float relY = (dipY - listRc.top - 4.0f) + exclScrollY_;
            int row = static_cast<int>(std::floor(relY / EXCL_ROW_H));
            if (relY >= 0.0f && row >= 0 && static_cast<size_t>(row) < cfg_.exclusions.size()) {
                float rowY = listRc.top + 4.0f + row * EXCL_ROW_H - exclScrollY_;
                D2D1_RECT_F delRc = D2D1::RectF(listRc.right - 40.0f, rowY + 6.0f, listRc.right - 12.0f, rowY + EXCL_ROW_H - 6.0f);
                if (PointInRectF(delRc, dipX, dipY)) return { HitZone::ExclRowDelete, row };
                return { HitZone::ExclRow, row };
            }
        }
    }

    return { HitZone::None, -1 };
}

void SettingsWindow::HandleMouseMove(float dipX, float dipY) {
    if (appInput_.dragging) {
        appInput_.caretPos = HitTestTextCaret(appInput_, dipX);
        EnsureInputCaretVisible(appInput_);
        RequestRedraw();
        return;
    }
    if (dictInput_.dragging) {
        dictInput_.caretPos = HitTestTextCaret(dictInput_, dipX);
        EnsureInputCaretVisible(dictInput_);
        RequestRedraw();
        return;
    }
    if (exclInput_.dragging) {
        exclInput_.caretPos = HitTestTextCaret(exclInput_, dipX);
        EnsureInputCaretVisible(exclInput_);
        RequestRedraw();
        return;
    }

    HitResult hit = HitTest(dipX, dipY);
    bool appHov = (hit.zone == HitZone::AppInput);
    bool dictHov = (hit.zone == HitZone::DictInput);
    bool exclHov = (hit.zone == HitZone::ExclInput);

    if (hit.zone != hovered_.zone || hit.index != hovered_.index ||
        appHov != appInput_.hovered || dictHov != dictInput_.hovered || exclHov != exclInput_.hovered) {
        hovered_ = hit;
        appInput_.hovered = appHov;
        dictInput_.hovered = dictHov;
        exclInput_.hovered = exclHov;
        EnsureAnimationRunning();
        RequestRedraw();
    }
}

void SettingsWindow::HandleMouseDown(float dipX, float dipY, bool doubleClick) {
    SetFocus(hWnd_);
    HitResult hit = HitTest(dipX, dipY);
    pressed_ = hit;

    bool prevAppFocus = appInput_.focused;
    bool prevDictFocus = dictInput_.focused;
    bool prevExclFocus = exclInput_.focused;
    appInput_.focused = (hit.zone == HitZone::AppInput);
    dictInput_.focused = (hit.zone == HitZone::DictInput);
    exclInput_.focused = (hit.zone == HitZone::ExclInput);

    if (appInput_.focused) {
        if (doubleClick) {
            appInput_.selAnchor = 0;
            appInput_.caretPos = appInput_.text.size();
        } else {
            appInput_.caretPos = HitTestTextCaret(appInput_, dipX);
            if ((GetKeyState(VK_SHIFT) & 0x8000) == 0) appInput_.ClearSelection();
            appInput_.dragging = true;
            SetCapture(hWnd_);
        }
        appInput_.caretPhase = 0.0f;
        EnsureInputCaretVisible(appInput_);
    } else if (dictInput_.focused) {
        if (doubleClick) {
            dictInput_.selAnchor = 0;
            dictInput_.caretPos = dictInput_.text.size();
        } else {
            dictInput_.caretPos = HitTestTextCaret(dictInput_, dipX);
            if ((GetKeyState(VK_SHIFT) & 0x8000) == 0) dictInput_.ClearSelection();
            dictInput_.dragging = true;
            SetCapture(hWnd_);
        }
        dictInput_.caretPhase = 0.0f;
        EnsureInputCaretVisible(dictInput_);
    } else if (exclInput_.focused) {
        if (doubleClick) {
            exclInput_.selAnchor = 0;
            exclInput_.caretPos = exclInput_.text.size();
        } else {
            exclInput_.caretPos = HitTestTextCaret(exclInput_, dipX);
            if ((GetKeyState(VK_SHIFT) & 0x8000) == 0) exclInput_.ClearSelection();
            exclInput_.dragging = true;
            SetCapture(hWnd_);
        }
        exclInput_.caretPhase = 0.0f;
        EnsureInputCaretVisible(exclInput_);
    } else if (doubleClick && hit.zone == HitZone::AppRow && hit.index >= 0) {
        ToggleAppRuleLayout(static_cast<size_t>(hit.index));
    } else if (doubleClick && hit.zone == HitZone::DictRow && hit.index >= 0) {
        ToggleDictWordLayout(static_cast<size_t>(hit.index));
    }

    if (prevAppFocus != appInput_.focused || prevDictFocus != dictInput_.focused ||
        prevExclFocus != exclInput_.focused || hit.zone != HitZone::None) {
        EnsureAnimationRunning();
        RequestRedraw();
    }
}

void SettingsWindow::HandleMouseUp(float dipX, float dipY) {
    if (appInput_.dragging || dictInput_.dragging || exclInput_.dragging) {
        appInput_.dragging = false;
        dictInput_.dragging = false;
        exclInput_.dragging = false;
        ReleaseCapture();
    }

    HitResult hit = HitTest(dipX, dipY);
    HitResult prs = pressed_;
    pressed_ = {};

    if (hit.zone != prs.zone || hit.index != prs.index) {
        EnsureAnimationRunning();
        RequestRedraw();
        return;
    }

    switch (hit.zone) {
    case HitZone::BtnMinimize:
        ShowWindow(hWnd_, SW_MINIMIZE);
        return;
    case HitZone::BtnClose:
        Config::ConfigManager::Instance().Save();
        Hide();
        return;
    case HitZone::ThemeToggle:
        ToggleTheme();
        return;
    case HitZone::NavGeneral:
        SwitchTab(SettingsTab::General);
        return;
    case HitZone::NavAppLayouts:
        SwitchTab(SettingsTab::AppLayouts);
        return;
    case HitZone::NavDictionary:
        SwitchTab(SettingsTab::Dictionary);
        return;
    case HitZone::NavExclusions:
        SwitchTab(SettingsTab::Exclusions);
        return;
    case HitZone::FooterPauseBtn:
        TogglePause();
        return;
    case HitZone::ToggleRow0:
    case HitZone::ToggleRow1:
    case HitZone::ToggleRow2:
    case HitZone::ToggleRow3:
    case HitZone::ToggleRow4:
    case HitZone::ToggleRow5: {
        int idx = hit.index;
        Config::ConfigManager::Instance().Update([idx](Config::AppConfig& cfg) {
            if (idx == 0) cfg.autoSwitch = !cfg.autoSwitch;
            else if (idx == 1) cfg.capsLockSwitch = !cfg.capsLockSwitch;
            else if (idx == 2) cfg.capsHoldSwitch = !cfg.capsHoldSwitch;
            else if (idx == 3) cfg.doubleCapsConvert = !cfg.doubleCapsConvert;
            else if (idx == 4) cfg.passwordGuard = !cfg.passwordGuard;
            else if (idx == 5) cfg.autostart = !cfg.autostart;
        });
        Config::ConfigManager::Instance().Save();
        RefreshSnapshot(false);
        TriggerSavedToast();
        RequestRedraw();
        return;
    }
    case HitZone::AppPickBtn:
        ShowAppPickerMenu(RcAppPick(), true);
        return;
    case HitZone::AppLangEn:
        selectedAppLayout_ = Config::AppLayout::English;
        EnsureAnimationRunning();
        RequestRedraw();
        return;
    case HitZone::AppLangRu:
        selectedAppLayout_ = Config::AppLayout::Russian;
        EnsureAnimationRunning();
        RequestRedraw();
        return;
    case HitZone::AppAddBtn:
        AddAppLayoutFromInput();
        return;
    case HitZone::AppRowBadge:
        if (hit.index >= 0) ToggleAppRuleLayout(static_cast<size_t>(hit.index));
        return;
    case HitZone::AppRowDelete:
        if (hit.index >= 0) RemoveAppRule(static_cast<size_t>(hit.index));
        return;
    case HitZone::DictLangEn:
        selectedDictLayout_ = Config::AppLayout::English;
        EnsureAnimationRunning();
        RequestRedraw();
        return;
    case HitZone::DictLangRu:
        selectedDictLayout_ = Config::AppLayout::Russian;
        EnsureAnimationRunning();
        RequestRedraw();
        return;
    case HitZone::DictAddBtn:
        AddDictWordFromInput();
        return;
    case HitZone::DictRowBadge:
        if (hit.index >= 0) ToggleDictWordLayout(static_cast<size_t>(hit.index));
        return;
    case HitZone::DictRowDelete:
        if (hit.index >= 0) RemoveDictWord(static_cast<size_t>(hit.index));
        return;
    case HitZone::ExclPickBtn:
        ShowAppPickerMenu(RcExclPick(), false);
        return;
    case HitZone::ExclAddBtn:
        AddExclusionFromInput();
        return;
    case HitZone::ExclRowDelete:
        if (hit.index >= 0) RemoveExclusion(static_cast<size_t>(hit.index));
        return;
    default:
        break;
    }

    EnsureAnimationRunning();
    RequestRedraw();
}

void SettingsWindow::HandleMouseWheel(short delta, float dipX, float dipY) {
    if (!PointInRectF(RcListBox(), dipX, dipY)) return;
    float step = -(delta / 120.0f) * 52.0f;
    if (activeTab_ == SettingsTab::AppLayouts) {
        float maxScroll = (std::max)(0.0f, static_cast<float>(cfg_.appLayouts.size()) * APP_ROW_H + 8.0f - LIST_H);
        appScrollTarget_ = (std::clamp)(appScrollTarget_ + step, 0.0f, maxScroll);
        EnsureAnimationRunning();
        RequestRedraw();
    } else if (activeTab_ == SettingsTab::Dictionary) {
        float maxScroll = (std::max)(0.0f, static_cast<float>(cfg_.wordDictionary.size()) * DICT_ROW_H + 8.0f - LIST_H);
        dictScrollTarget_ = (std::clamp)(dictScrollTarget_ + step, 0.0f, maxScroll);
        EnsureAnimationRunning();
        RequestRedraw();
    } else if (activeTab_ == SettingsTab::Exclusions) {
        float maxScroll = (std::max)(0.0f, static_cast<float>(cfg_.exclusions.size()) * EXCL_ROW_H + 8.0f - LIST_H);
        exclScrollTarget_ = (std::clamp)(exclScrollTarget_ + step, 0.0f, maxScroll);
        EnsureAnimationRunning();
        RequestRedraw();
    }
}

void SettingsWindow::DrawNavigationRail(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush) {
    // Cool-gray left sidebar background
    brush->SetColor(pal.sidebarBg);
    rt->FillRectangle(D2D1::RectF(0.0f, 0.0f, SIDEBAR_W, FOOTER_Y), brush);

    // Subtle brand header
    D2D1_RECT_F iconBox = D2D1::RectF(16.0f, 15.0f, 34.0f, 33.0f);
    brush->SetColor(Rgba(24, 34, 52, 1.0f));
    rt->FillRoundedRectangle(D2D1::RoundedRect(iconBox, 5.0f, 5.0f), brush);
    ID2D1StrokeStyle* round = FluentGraphics::Instance().RoundStrokeStyle();
    float ix = iconBox.left;
    float iy = iconBox.top;
    float is = 18.0f / 16.0f;
    brush->SetColor(Rgba(255, 255, 255, 0.96f));
    rt->DrawLine(D2D1::Point2F(ix + 4.8f * is, iy + 7.6f * is), D2D1::Point2F(ix + 8.0f * is, iy + 4.2f * is), brush, 1.5f, round);
    rt->DrawLine(D2D1::Point2F(ix + 8.0f * is, iy + 4.2f * is), D2D1::Point2F(ix + 11.2f * is, iy + 7.6f * is), brush, 1.5f, round);
    rt->DrawLine(D2D1::Point2F(ix + 8.0f * is, iy + 4.6f * is), D2D1::Point2F(ix + 8.0f * is, iy + 9.6f * is), brush, 1.5f, round);
    brush->SetColor(Rgba(56, 189, 248, 1.0f));
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(ix + 4.2f * is, iy + 11.4f * is, ix + 7.6f * is, iy + 12.9f * is), 0.75f, 0.75f), brush);
    brush->SetColor(Rgba(251, 113, 133, 1.0f));
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(ix + 8.4f * is, iy + 11.4f * is, ix + 11.8f * is, iy + 12.9f * is), 0.75f, 0.75f), brush);

    if (fmtBrand_) {
        brush->SetColor(pal.textPrimary);
        rt->DrawText(L"CapsSwitch", 10, fmtBrand_, D2D1::RectF(42.0f, 14.0f, SIDEBAR_W - 10.0f, 34.0f), brush);
    }

    // Navigation items
    struct NavDef {
        FluentIcon icon;
        const wchar_t* label;
        int count;
    };
    const NavDef navs[4] = {
        { FluentIcon::Overview, L"Обзор", -1 },
        { FluentIcon::Apps, L"Приложения", static_cast<int>(cfg_.appLayouts.size()) },
        { FluentIcon::Book, L"Словарь", static_cast<int>(cfg_.wordDictionary.size()) },
        { FluentIcon::Block, L"Исключения", static_cast<int>(cfg_.exclusions.size()) }
    };

    for (int i = 0; i < 4; ++i) {
        D2D1_RECT_F rc = RcNavItem(i);
        bool selected = (static_cast<int>(activeTab_) == i);
        float hov = navHover_[i];

        if (selected) {
            brush->SetColor(pal.navSelectedBg);
            rt->FillRoundedRectangle(D2D1::RoundedRect(rc, 8.0f, 8.0f), brush);
        } else if (hov > 0.01f) {
            brush->SetColor(FadeColor(pal.navHoverBg, hov));
            rt->FillRoundedRectangle(D2D1::RoundedRect(rc, 8.0f, 8.0f), brush);
        }

        D2D1_COLOR_F iconCol = selected ? pal.navSelectedFg : (hov > 0.1f ? pal.textPrimary : pal.textSecondary);
        DrawFluentIcon(rt, navs[i].icon,
            D2D1::RectF(rc.left + 12.0f, rc.top + 11.0f, rc.left + 28.0f, rc.top + 27.0f),
            iconCol, 1.4f, brush);

        IDWriteTextFormat* navFmt = selected ? (fmtNavSelected_ ? fmtNavSelected_ : fmtNav_) : fmtNav_;
        if (navFmt) {
            brush->SetColor(selected ? pal.navSelectedFg : pal.textPrimary);
            float textRight = (navs[i].count > 0) ? (rc.right - 28.0f) : (rc.right - 10.0f);
            rt->DrawText(navs[i].label, static_cast<UINT32>(wcslen(navs[i].label)), navFmt,
                D2D1::RectF(rc.left + 38.0f, rc.top, textRight, rc.bottom), brush);
        }

        if (navs[i].count > 0 && fmtSmall_) {
            std::wstring cntStr = std::to_wstring(navs[i].count);
            brush->SetColor(selected ? pal.navSelectedFg : pal.textTertiary);
            rt->DrawText(cntStr.c_str(), static_cast<UINT32>(cntStr.size()), fmtSmall_,
                D2D1::RectF(rc.right - 28.0f, rc.top, rc.right - 10.0f, rc.bottom), brush);
        }
    }
}

void SettingsWindow::DrawWorkspaceShell(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush) {
    brush->SetColor(pal.workspaceBg);
    rt->FillRectangle(D2D1::RectF(SIDEBAR_W, 0.0f, WND_LOGICAL_W, FOOTER_Y), brush);

    brush->SetColor(pal.divider);
    rt->DrawLine(D2D1::Point2F(SIDEBAR_W + 0.5f, 0.0f), D2D1::Point2F(SIDEBAR_W + 0.5f, FOOTER_Y), brush, 1.0f);
}

void SettingsWindow::DrawFooterBar(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush) {
    brush->SetColor(pal.footerBg);
    rt->FillRectangle(D2D1::RectF(0.0f, FOOTER_Y, WND_LOGICAL_W, WND_LOGICAL_H), brush);

    brush->SetColor(pal.divider);
    rt->DrawLine(D2D1::Point2F(0.0f, FOOTER_Y + 0.5f), D2D1::Point2F(WND_LOGICAL_W, FOOTER_Y + 0.5f), brush, 1.0f);

    // Left: status dot + concise live state + saved indicator
    float cy = FOOTER_Y + FOOTER_H * 0.5f;
    D2D1_COLOR_F dotCol = LerpColor(pal.statusActive, pal.statusPaused, statusPausedAnim_);
    brush->SetColor(dotCol);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(20.0f, cy), 3.5f, 3.5f), brush);

    std::wstring statusText = livePaused_
        ? L"На паузе"
        : (liveIsRu_ ? L"Активен · RU" : L"Активен · EN");
    if (fmtSubtitle_) {
        brush->SetColor(pal.textSecondary);
        rt->DrawText(statusText.c_str(), static_cast<UINT32>(statusText.size()), fmtSubtitle_,
            D2D1::RectF(32.0f, FOOTER_Y, 180.0f, WND_LOGICAL_H), brush);
    }

    if (toastAlpha_ > 0.01f && fmtSmall_) {
        brush->SetColor(WithAlpha(pal.textTertiary, toastAlpha_));
        rt->DrawText(L"· Сохранено", 11, fmtSmall_,
            D2D1::RectF(140.0f, FOOTER_Y, 240.0f, WND_LOGICAL_H), brush);
    }

    // Right: clickable pause/resume action
    D2D1_RECT_F pauseRc = RcFooterPauseBtn();
    if (footerPauseHover_ > 0.01f) {
        brush->SetColor(FadeColor(pal.navHoverBg, footerPauseHover_));
        rt->FillRoundedRectangle(D2D1::RoundedRect(pauseRc, 6.0f, 6.0f), brush);
    }

    D2D1_COLOR_F actionCol = footerPauseHover_ > 0.1f ? pal.textPrimary : pal.textSecondary;
    DrawFluentIcon(rt, livePaused_ ? FluentIcon::Play : FluentIcon::Power,
        D2D1::RectF(pauseRc.left + 12.0f, cy - 7.5f, pauseRc.left + 27.0f, cy + 7.5f),
        actionCol, 1.4f, brush);

    if (fmtNav_) {
        const wchar_t* stLabel = livePaused_ ? L"Возобновить" : L"Приостановить";
        brush->SetColor(actionCol);
        rt->DrawText(stLabel, static_cast<UINT32>(wcslen(stLabel)), fmtNav_,
            D2D1::RectF(pauseRc.left + 34.0f, pauseRc.top, pauseRc.right - 10.0f, pauseRc.bottom), brush);
    }
}

void SettingsWindow::DrawCaptionButtons(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush) {
    D2D1_RECT_F minRc = RcBtnMinimize();
    if (captionHover_[0] > 0.01f) {
        brush->SetColor(WithAlpha(pal.textPrimary, (darkMode_ ? 0.08f : 0.05f) * captionHover_[0]));
        rt->FillRectangle(minRc, brush);
    }
    DrawFluentIcon(rt, FluentIcon::Minimize,
        D2D1::RectF(minRc.left + 15.0f, minRc.top + 9.0f, minRc.left + 27.0f, minRc.top + 21.0f),
        pal.textSecondary, 1.25f, brush);

    D2D1_RECT_F clsRc = RcBtnClose();
    if (captionHover_[1] > 0.01f) {
        brush->SetColor(WithAlpha(pal.dangerHoverBg, captionHover_[1]));
        rt->FillRectangle(clsRc, brush);
    }
    D2D1_COLOR_F clsFg = LerpColor(pal.textSecondary, Rgba(255, 255, 255, 1.0f), captionHover_[1]);
    DrawFluentIcon(rt, FluentIcon::Close,
        D2D1::RectF(clsRc.left + 15.0f, clsRc.top + 9.0f, clsRc.left + 27.0f, clsRc.top + 21.0f),
        clsFg, 1.25f, brush);
}

void SettingsWindow::DrawPageHeader(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha) {
    const wchar_t* title = L"Обзор";
    if (activeTab_ == SettingsTab::AppLayouts) title = L"Приложения";
    else if (activeTab_ == SettingsTab::Dictionary) title = L"Словарь";
    else if (activeTab_ == SettingsTab::Exclusions) title = L"Исключения";

    if (fmtPageTitle_) {
        brush->SetColor(WithAlpha(pal.textPrimary, alpha));
        rt->DrawText(title, static_cast<UINT32>(wcslen(title)), fmtPageTitle_,
            D2D1::RectF(WORK_PAD_X, HEADER_Y + offsetY * 0.4f, WORK_RIGHT - 44.0f, HEADER_Y + 32.0f + offsetY * 0.4f), brush);
    }

    D2D1_RECT_F themeRc = RcThemeToggle();
    if (themeBtnHover_ > 0.01f) {
        brush->SetColor(FadeColor(pal.navHoverBg, themeBtnHover_));
        rt->FillRoundedRectangle(D2D1::RoundedRect(themeRc, 8.0f, 8.0f), brush);
    }
    D2D1_COLOR_F themeIconCol = (themeBtnHover_ > 0.1f) ? pal.textPrimary : pal.textSecondary;
    DrawFluentIcon(rt, darkMode_ ? FluentIcon::Sun : FluentIcon::Moon,
        D2D1::RectF(themeRc.left + 8.0f, themeRc.top + 8.0f, themeRc.right - 8.0f, themeRc.bottom - 8.0f),
        themeIconCol, 1.4f, brush);
}

void SettingsWindow::DrawGeneralPage(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha) {
    // 1. Status Hero Banner
    D2D1_RECT_F bannerRc = ShiftRectY(D2D1::RectF(WORK_PAD_X, 74.0f, WORK_RIGHT, 130.0f), offsetY);
    D2D1_COLOR_F bannerBg = LerpColor(pal.bannerBg, pal.bannerPausedBg, statusPausedAnim_);
    brush->SetColor(FadeColor(bannerBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(bannerRc, 10.0f, 10.0f), brush);

    // Left squircle icon box
    D2D1_RECT_F sqRc = D2D1::RectF(bannerRc.left + 14.0f, bannerRc.top + 10.0f, bannerRc.left + 50.0f, bannerRc.top + 46.0f);
    brush->SetColor(FadeColor(pal.bannerIconBoxBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(sqRc, 8.0f, 8.0f), brush);
    brush->SetColor(FadeColor(pal.borderSubtle, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(sqRc, 8.0f, 8.0f), brush, 1.0f);

    DrawFluentIcon(rt, livePaused_ ? FluentIcon::Pause : FluentIcon::CheckCircle,
        D2D1::RectF(sqRc.left + 10.0f, sqRc.top + 10.0f, sqRc.right - 10.0f, sqRc.bottom - 10.0f),
        FadeColor(livePaused_ ? pal.statusPaused : pal.navSelectedFg, alpha), 1.5f, brush);

    int activeCount = (cfg_.autoSwitch ? 1 : 0) + (cfg_.capsLockSwitch ? 1 : 0)
        + (cfg_.capsHoldSwitch ? 1 : 0) + (cfg_.doubleCapsConvert ? 1 : 0)
        + (cfg_.passwordGuard ? 1 : 0) + (cfg_.autostart ? 1 : 0);

    if (fmtRowTitle_) {
        const wchar_t* bannerTitle = livePaused_ ? L"Переключение на паузе" : L"CapsSwitch активен";
        brush->SetColor(WithAlpha(pal.textPrimary, alpha));
        rt->DrawText(bannerTitle, static_cast<UINT32>(wcslen(bannerTitle)), fmtRowTitle_,
            D2D1::RectF(bannerRc.left + 64.0f, bannerRc.top + 10.0f, bannerRc.right - 64.0f, bannerRc.top + 29.0f), brush);
    }
    if (fmtRowSub_) {
        std::wstring bannerSub = livePaused_
            ? L"Нажмите «Возобновить» внизу"
            : (L"Активно модулей: " + std::to_wstring(activeCount) + L" из 6");
        brush->SetColor(FadeColor(pal.textSecondary, alpha));
        rt->DrawText(bannerSub.c_str(), static_cast<UINT32>(bannerSub.size()), fmtRowSub_,
            D2D1::RectF(bannerRc.left + 64.0f, bannerRc.top + 29.0f, bannerRc.right - 64.0f, bannerRc.top + 47.0f), brush);
    }

    D2D1_RECT_F langPillRc = D2D1::RectF(bannerRc.right - 52.0f, bannerRc.top + 16.0f, bannerRc.right - 14.0f, bannerRc.top + 40.0f);
    brush->SetColor(FadeColor(pal.bannerIconBoxBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(langPillRc, 6.0f, 6.0f), brush);
    brush->SetColor(FadeColor(pal.borderSubtle, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(langPillRc, 6.0f, 6.0f), brush, 1.0f);
    if (fmtBadge_) {
        brush->SetColor(WithAlpha(LerpColor(pal.langEnFill, pal.langRuFill, langChipRuAnim_), alpha));
        rt->DrawText(liveIsRu_ ? L"RU" : L"EN", 2, fmtBadge_, langPillRc, brush);
    }

    // 2. Section headings
    if (fmtGroupLabel_) {
        brush->SetColor(WithAlpha(pal.textPrimary, alpha));
        rt->DrawText(L"Управление", 10, fmtGroupLabel_,
            ShiftRectY(D2D1::RectF(WORK_PAD_X, 142.0f, WORK_RIGHT, 164.0f), offsetY), brush);
        rt->DrawText(L"Система и защита", 16, fmtGroupLabel_,
            ShiftRectY(D2D1::RectF(WORK_PAD_X, 372.0f, WORK_RIGHT, 394.0f), offsetY), brush);
    }

    // 3. Flat settings rows with hairline dividers
    struct RowDef {
        FluentIcon icon;
        const wchar_t* title;
        const wchar_t* subtitle;
    };
    const RowDef rows[6] = {
        { FluentIcon::AutoSwitch, L"Автопереключение раскладки",
          L"Исправление слов и работа пользовательского словаря" },
        { FluentIcon::CapsLock, L"Переключение по Caps Lock",
          L"Короткое нажатие — RU ⇄ EN; Win + Caps — Caps Lock" },
        { FluentIcon::Swap, L"Удержание Caps Lock (с Shift)",
          L"Смена раскладки пока зажат Caps, возврат при отпускании" },
        { FluentIcon::DoubleCaps, L"Перевод по 2× Caps Lock",
          L"Конвертация выделенного или последнего слова" },
        { FluentIcon::Shield, L"Защита полей паролей",
          L"Авто-EN и отключение буфера в скрытых полях" },
        { FluentIcon::Rocket, L"Автозапуск с Windows",
          L"Запуск при входе в систему" }
    };

    for (int i = 0; i < 6; ++i) {
        D2D1_RECT_F rc = ShiftRectY(RcToggleRow(i), offsetY);
        float hov = rowHover_[i];
        float prs = togglePress_[i];
        float tAnim = toggleAnim_[i];

        if (hov > 0.01f || prs > 0.01f) {
            D2D1_RECT_F hovRc = D2D1::RectF(WORK_PAD_X - 8.0f, rc.top + 2.0f, WORK_RIGHT + 8.0f, rc.bottom - 2.0f);
            D2D1_COLOR_F hovCol = LerpColor(pal.rowHoverBg, pal.rowPressedBg, prs);
            brush->SetColor(FadeColor(hovCol, (std::max)(hov, prs) * alpha));
            rt->FillRoundedRectangle(D2D1::RoundedRect(hovRc, 8.0f, 8.0f), brush);
        }

        if (i == 0 || i == 1 || i == 2 || i == 4) {
            brush->SetColor(FadeColor(pal.divider, alpha));
            rt->DrawLine(D2D1::Point2F(WORK_PAD_X, rc.bottom), D2D1::Point2F(WORK_RIGHT, rc.bottom), brush, 1.0f);
        }

        D2D1_COLOR_F iconCol = LerpColor(pal.textSecondary, pal.textPrimary, hov * 0.6f);
        DrawFluentIcon(rt, rows[i].icon,
            D2D1::RectF(WORK_PAD_X + 2.0f, rc.top + 16.0f, WORK_PAD_X + 18.0f, rc.top + 32.0f),
            FadeColor(iconCol, alpha), 1.4f, brush);

        if (fmtRowTitle_) {
            brush->SetColor(WithAlpha(pal.textPrimary, alpha));
            rt->DrawText(rows[i].title, static_cast<UINT32>(wcslen(rows[i].title)), fmtRowTitle_,
                D2D1::RectF(WORK_PAD_X + 32.0f, rc.top + 6.0f, WORK_RIGHT - 56.0f, rc.top + 25.0f), brush);
        }
        if (fmtRowSub_) {
            brush->SetColor(FadeColor(pal.textSecondary, alpha));
            rt->DrawText(rows[i].subtitle, static_cast<UINT32>(wcslen(rows[i].subtitle)), fmtRowSub_,
                D2D1::RectF(WORK_PAD_X + 32.0f, rc.top + 26.0f, WORK_RIGHT - 56.0f, rc.top + 43.0f), brush);
        }

        // Clean 38x20 ToggleSwitch without text label
        float pillW = 38.0f;
        float pillH = 20.0f;
        float pillX = WORK_RIGHT - pillW;
        float pillY = (rc.top + rc.bottom - pillH) * 0.5f;
        D2D1_RECT_F pillRc = D2D1::RectF(pillX, pillY, pillX + pillW, pillY + pillH);

        D2D1_COLOR_F trackCol = LerpColor(pal.toggleOffTrack, LerpColor(pal.toggleOnTrack, pal.toggleOnHover, toggleHover_[i]), tAnim);
        brush->SetColor(FadeColor(trackCol, alpha));
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillRc, 10.0f, 10.0f), brush);

        if (tAnim < 0.99f) {
            brush->SetColor(FadeColor(pal.toggleOffBorder, (1.0f - tAnim) * alpha));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(pillRc, 10.0f, 10.0f), brush, 1.15f);
        }

        float knobD = 12.0f + 1.0f * toggleHover_[i] - 0.8f * prs;
        float knobW = knobD + 2.0f * prs;
        float knobX = Lerp(pillX + 4.0f, pillX + pillW - 4.0f - knobW, tAnim);
        float knobY = pillY + (pillH - knobD) * 0.5f;
        brush->SetColor(FadeColor(LerpColor(pal.toggleOffKnob, pal.toggleOnKnob, tAnim), alpha));
        rt->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(knobX, knobY, knobX + knobW, knobY + knobD), knobD * 0.5f, knobD * 0.5f),
            brush
        );
    }
}

void SettingsWindow::DrawTextInput(
    ID2D1RenderTarget* rt,
    const FluentPalette& pal,
    ID2D1SolidColorBrush* brush,
    FluentTextInput& input,
    float offsetY,
    float alpha
) {
    D2D1_RECT_F rc = ShiftRectY(input.rect, offsetY);
    D2D1_COLOR_F bg = LerpColor(pal.inputBg, pal.inputFocusBg, input.focusAnim);
    if (!input.focused && input.hoverAnim > 0.01f) {
        bg = LerpColor(bg, pal.cardHoverBg, input.hoverAnim * 0.5f);
    }
    brush->SetColor(FadeColor(bg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(rc, 7.0f, 7.0f), brush);

    D2D1_COLOR_F inpBorder = LerpColor(pal.borderSubtle, pal.borderStrong, (std::max)(input.focusAnim, input.hoverAnim));
    if (input.focusAnim > 0.01f) {
        inpBorder = LerpColor(inpBorder, pal.accent, input.focusAnim);
    }
    brush->SetColor(FadeColor(inpBorder, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(rc, 7.0f, 7.0f), brush, 1.0f);

    D2D1_RECT_F textClip = D2D1::RectF(rc.left + 9.0f, rc.top + 2.0f, rc.right - 9.0f, rc.bottom - 2.0f);
    rt->PushAxisAlignedClip(textClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (input.text.empty()) {
        if (fmtInput_) {
            brush->SetColor(FadeColor(pal.textTertiary, alpha));
            rt->DrawText(input.placeholder.c_str(), static_cast<UINT32>(input.placeholder.size()),
                fmtInput_, textClip, brush);
        }
    } else if (fmtInput_) {
        IDWriteFactory* dw = FluentGraphics::Instance().DWriteFactory();
        IDWriteTextLayout* layout = nullptr;
        if (dw && SUCCEEDED(dw->CreateTextLayout(input.text.c_str(), static_cast<UINT32>(input.text.size()),
            fmtInput_, 4096.0f, rc.bottom - rc.top, &layout)) && layout) {
            float drawX = rc.left + 10.0f - input.scrollX;

            if (input.focused && input.HasSelection()) {
                size_t s = 0, e = 0;
                input.GetSelectionRange(s, e);
                UINT32 hitCount = 0;
                layout->HitTestTextRange(static_cast<UINT32>(s), static_cast<UINT32>(e - s), drawX, rc.top, nullptr, 0, &hitCount);
                if (hitCount > 0) {
                    std::vector<DWRITE_HIT_TEST_METRICS> htm(hitCount);
                    if (SUCCEEDED(layout->HitTestTextRange(static_cast<UINT32>(s), static_cast<UINT32>(e - s),
                        drawX, rc.top, htm.data(), hitCount, &hitCount))) {
                        brush->SetColor(WithAlpha(pal.accent, 0.30f * alpha));
                        for (UINT32 k = 0; k < hitCount; ++k) {
                            rt->FillRoundedRectangle(
                                D2D1::RoundedRect(D2D1::RectF(htm[k].left, rc.top + 6.0f, htm[k].left + htm[k].width, rc.bottom - 6.0f), 2.0f, 2.0f),
                                brush
                            );
                        }
                    }
                }
            }

            brush->SetColor(WithAlpha(pal.textPrimary, alpha));
            rt->DrawTextLayout(D2D1::Point2F(drawX, rc.top), layout, brush);

            if (input.focused && input.caretPhase < 1.0f) {
                float cx = 0.0f, cy = 0.0f;
                DWRITE_HIT_TEST_METRICS cm = {};
                layout->HitTestTextPosition(static_cast<UINT32>(input.caretPos), FALSE, &cx, &cy, &cm);
                float caretDrawX = drawX + cx;
                brush->SetColor(WithAlpha(pal.textPrimary, alpha));
                rt->FillRectangle(D2D1::RectF(caretDrawX, rc.top + 7.0f, caretDrawX + 1.4f, rc.bottom - 7.0f), brush);
            }
            layout->Release();
        }
    }

    rt->PopAxisAlignedClip();
}

void SettingsWindow::DrawFluentButton(
    ID2D1RenderTarget* rt,
    const FluentPalette& pal,
    ID2D1SolidColorBrush* brush,
    const D2D1_RECT_F& rc,
    const std::wstring& label,
    FluentIcon icon,
    bool primary,
    float hoverAnim,
    bool pressed,
    float alpha
) {
    D2D1_COLOR_F bg{};
    D2D1_COLOR_F fg{};
    D2D1_COLOR_F brd{};

    if (primary) {
        bg = LerpColor(pal.accent, pal.accentHover, hoverAnim);
        if (pressed) bg = pal.accentPressed;
        fg = pal.accentText;
        brd = Rgba(255, 255, 255, 0.16f);
    } else {
        bg = LerpColor(pal.cardBg, pal.cardHoverBg, hoverAnim);
        if (pressed) bg = pal.cardPressedBg;
        fg = pal.textPrimary;
        brd = pal.borderStrong;
    }

    brush->SetColor(FadeColor(bg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(rc, 7.0f, 7.0f), brush);
    brush->SetColor(FadeColor(brd, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(rc, 7.0f, 7.0f), brush, 1.0f);

    if (icon == FluentIcon::ChevronDown) {
        float cy = (rc.top + rc.bottom) * 0.5f;
        DrawFluentIcon(rt, icon, D2D1::RectF(rc.right - 19.0f, cy - 5.5f, rc.right - 8.0f, cy + 5.5f),
            FadeColor(pal.textSecondary, alpha), 1.35f, brush);
        if (fmtBtn_) {
            brush->SetColor(FadeColor(fg, alpha));
            rt->DrawText(label.c_str(), static_cast<UINT32>(label.size()), fmtBtn_,
                D2D1::RectF(rc.left + 8.0f, rc.top, rc.right - 19.0f, rc.bottom), brush);
        }
    } else if (fmtBtn_) {
        brush->SetColor(FadeColor(fg, alpha));
        rt->DrawText(label.c_str(), static_cast<UINT32>(label.size()), fmtBtn_, rc, brush);
    }
}

void SettingsWindow::DrawScrollbar(
    ID2D1RenderTarget* rt,
    const FluentPalette& pal,
    ID2D1SolidColorBrush* brush,
    const D2D1_RECT_F& listRc,
    float scrollY,
    float contentH,
    float alpha
) {
    float viewH = listRc.bottom - listRc.top;
    if (contentH <= viewH + 1.0f) return;

    float ratio = viewH / contentH;
    float barH = (std::max)(24.0f, viewH * ratio);
    float maxScroll = contentH - viewH;
    float barY = listRc.top + 4.0f + (maxScroll > 0.0f ? (scrollY / maxScroll) * (viewH - barH - 8.0f) : 0.0f);
    brush->SetColor(FadeColor(pal.textTertiary, alpha));
    rt->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(listRc.right - 6.0f, barY, listRc.right - 2.5f, barY + barH), 1.75f, 1.75f),
        brush
    );
}

void SettingsWindow::DrawAppLayoutsPage(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha) {
    DrawTextInput(rt, pal, brush, appInput_, offsetY, alpha);

    DrawFluentButton(rt, pal, brush, ShiftRectY(RcAppPick(), offsetY), L"Окна", FluentIcon::ChevronDown,
        false, hovered_.zone == HitZone::AppPickBtn ? btnPickHover_ : 0.0f,
        pressed_.zone == HitZone::AppPickBtn, alpha);

    // Calm tinted EN / RU segmented toggle
    D2D1_RECT_F rcEn = ShiftRectY(RcAppLangEn(), offsetY);
    D2D1_RECT_F rcRu = ShiftRectY(RcAppLangRu(), offsetY);
    D2D1_RECT_F segRc = D2D1::RectF(rcEn.left, rcEn.top, rcRu.right, rcRu.bottom);
    brush->SetColor(FadeColor(pal.inputBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(segRc, 7.0f, 7.0f), brush);
    brush->SetColor(FadeColor(pal.borderSubtle, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(segRc, 7.0f, 7.0f), brush, 1.0f);

    float segW = rcEn.right - rcEn.left;
    float pillX = Lerp(rcEn.left + 2.5f, rcRu.left + 2.5f, langSelectorAnim_);
    D2D1_COLOR_F pillBg = LerpColor(pal.langEnSoft, pal.langRuSoft, langSelectorAnim_);
    D2D1_COLOR_F pillBrd = LerpColor(pal.langEnFill, pal.langRuFill, langSelectorAnim_);
    D2D1_RECT_F pillRc = D2D1::RectF(pillX, segRc.top + 2.5f, pillX + segW - 5.0f, segRc.bottom - 2.5f);
    brush->SetColor(FadeColor(pillBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(pillRc, 5.0f, 5.0f), brush);
    brush->SetColor(WithAlpha(pillBrd, 0.35f * alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(pillRc, 5.0f, 5.0f), brush, 1.0f);

    if (fmtBadge_) {
        brush->SetColor(WithAlpha(LerpColor(pal.langEnFill, pal.textSecondary, langSelectorAnim_), alpha));
        rt->DrawText(L"EN", 2, fmtBadge_, rcEn, brush);
        brush->SetColor(WithAlpha(LerpColor(pal.textSecondary, pal.langRuFill, langSelectorAnim_), alpha));
        rt->DrawText(L"RU", 2, fmtBadge_, rcRu, brush);
    }

    DrawFluentButton(rt, pal, brush, ShiftRectY(RcAppAdd(), offsetY), L"Добавить", FluentIcon::None,
        true, hovered_.zone == HitZone::AppAddBtn ? btnAddHover_ : 0.0f,
        pressed_.zone == HitZone::AppAddBtn, alpha);

    D2D1_RECT_F listRc = ShiftRectY(RcListBox(), offsetY);
    brush->SetColor(FadeColor(pal.workspaceBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(listRc, 10.0f, 10.0f), brush);
    brush->SetColor(FadeColor(pal.borderSubtle, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(listRc, 10.0f, 10.0f), brush, 1.0f);

    rt->PushAxisAlignedClip(D2D1::RectF(listRc.left + 1.0f, listRc.top + 1.0f, listRc.right - 1.0f, listRc.bottom - 1.0f),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (cfg_.appLayouts.empty()) {
        if (fmtRowTitle_) {
            fmtRowTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            brush->SetColor(FadeColor(pal.textSecondary, alpha));
            const wchar_t* emptyT = L"Нет правил";
            rt->DrawText(emptyT, static_cast<UINT32>(wcslen(emptyT)), fmtRowTitle_,
                D2D1::RectF(listRc.left + 20.0f, listRc.top + 150.0f, listRc.right - 20.0f, listRc.top + 172.0f), brush);
            fmtRowTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        if (fmtRowSub_) {
            fmtRowSub_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            brush->SetColor(FadeColor(pal.textTertiary, alpha));
            const wchar_t* emptyS = L"Введите имя .exe или выберите окно";
            rt->DrawText(emptyS, static_cast<UINT32>(wcslen(emptyS)), fmtRowSub_,
                D2D1::RectF(listRc.left + 20.0f, listRc.top + 174.0f, listRc.right - 20.0f, listRc.top + 194.0f), brush);
            fmtRowSub_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    } else {
        for (size_t i = 0; i < cfg_.appLayouts.size(); ++i) {
            float rowY = listRc.top + 4.0f + static_cast<float>(i) * APP_ROW_H - appScrollY_;
            if (rowY + APP_ROW_H < listRc.top || rowY > listRc.bottom) continue;

            const auto& rule = cfg_.appLayouts[i];
            bool rowHov = (hovered_.index == static_cast<int>(i) &&
                (hovered_.zone == HitZone::AppRow || hovered_.zone == HitZone::AppRowBadge || hovered_.zone == HitZone::AppRowDelete));

            D2D1_RECT_F rowRc = D2D1::RectF(listRc.left + 4.0f, rowY + 2.0f, listRc.right - 8.0f, rowY + APP_ROW_H - 2.0f);
            if (rowHov) {
                brush->SetColor(FadeColor(pal.rowHoverBg, alpha));
                rt->FillRoundedRectangle(D2D1::RoundedRect(rowRc, 6.0f, 6.0f), brush);
            }
            if (i + 1 < cfg_.appLayouts.size()) {
                brush->SetColor(FadeColor(pal.divider, alpha));
                rt->DrawLine(D2D1::Point2F(listRc.left + 14.0f, rowY + APP_ROW_H), D2D1::Point2F(listRc.right - 14.0f, rowY + APP_ROW_H), brush, 1.0f);
            }

            if (fmtRowTitle_) {
                brush->SetColor(WithAlpha(pal.textPrimary, alpha));
                rt->DrawText(rule.processName.c_str(), static_cast<UINT32>(rule.processName.size()), fmtRowTitle_,
                    D2D1::RectF(rowRc.left + 12.0f, rowY, rowRc.right - 96.0f, rowY + APP_ROW_H), brush);
            }

            bool isRu = (rule.layout == Config::AppLayout::Russian);
            D2D1_RECT_F badgeRc = D2D1::RectF(listRc.right - 92.0f, rowY + 10.0f, listRc.right - 48.0f, rowY + APP_ROW_H - 10.0f);
            brush->SetColor(FadeColor(isRu ? pal.langRuSoft : pal.langEnSoft, alpha));
            rt->FillRoundedRectangle(D2D1::RoundedRect(badgeRc, 6.0f, 6.0f), brush);
            if (fmtBadge_) {
                brush->SetColor(WithAlpha(isRu ? pal.langRuFill : pal.langEnFill, alpha));
                rt->DrawText(isRu ? L"RU" : L"EN", 2, fmtBadge_, badgeRc, brush);
            }

            D2D1_RECT_F delRc = D2D1::RectF(listRc.right - 40.0f, rowY + 7.0f, listRc.right - 12.0f, rowY + APP_ROW_H - 7.0f);
            bool delHov = (hovered_.zone == HitZone::AppRowDelete && hovered_.index == static_cast<int>(i));
            if (delHov) {
                brush->SetColor(FadeColor(pal.dangerSoft, alpha));
                rt->FillRoundedRectangle(D2D1::RoundedRect(delRc, 6.0f, 6.0f), brush);
            }
            D2D1_COLOR_F delCol = delHov ? pal.danger : (rowHov ? pal.textSecondary : pal.textTertiary);
            DrawFluentIcon(rt, FluentIcon::Trash,
                D2D1::RectF(delRc.left + 7.5f, delRc.top + 8.5f, delRc.right - 7.5f, delRc.bottom - 8.5f),
                FadeColor(delCol, alpha), 1.4f, brush);
        }

        DrawScrollbar(rt, pal, brush, listRc, appScrollY_, static_cast<float>(cfg_.appLayouts.size()) * APP_ROW_H + 8.0f, alpha);
    }

    rt->PopAxisAlignedClip();
}

void SettingsWindow::DrawDictionaryPage(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha) {
    DrawTextInput(rt, pal, brush, dictInput_, offsetY, alpha);

    // Calm tinted EN / RU segmented toggle
    D2D1_RECT_F rcEn = ShiftRectY(RcDictLangEn(), offsetY);
    D2D1_RECT_F rcRu = ShiftRectY(RcDictLangRu(), offsetY);
    D2D1_RECT_F segRc = D2D1::RectF(rcEn.left, rcEn.top, rcRu.right, rcRu.bottom);
    brush->SetColor(FadeColor(pal.inputBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(segRc, 7.0f, 7.0f), brush);
    brush->SetColor(FadeColor(pal.borderSubtle, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(segRc, 7.0f, 7.0f), brush, 1.0f);

    float segW = rcEn.right - rcEn.left;
    float pillX = Lerp(rcEn.left + 2.5f, rcRu.left + 2.5f, dictLangSelectorAnim_);
    D2D1_COLOR_F pillBg = LerpColor(pal.langEnSoft, pal.langRuSoft, dictLangSelectorAnim_);
    D2D1_COLOR_F pillBrd = LerpColor(pal.langEnFill, pal.langRuFill, dictLangSelectorAnim_);
    D2D1_RECT_F pillRc = D2D1::RectF(pillX, segRc.top + 2.5f, pillX + segW - 5.0f, segRc.bottom - 2.5f);
    brush->SetColor(FadeColor(pillBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(pillRc, 5.0f, 5.0f), brush);
    brush->SetColor(WithAlpha(pillBrd, 0.35f * alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(pillRc, 5.0f, 5.0f), brush, 1.0f);

    if (fmtBadge_) {
        brush->SetColor(WithAlpha(LerpColor(pal.langEnFill, pal.textSecondary, dictLangSelectorAnim_), alpha));
        rt->DrawText(L"EN", 2, fmtBadge_, rcEn, brush);
        brush->SetColor(WithAlpha(LerpColor(pal.textSecondary, pal.langRuFill, dictLangSelectorAnim_), alpha));
        rt->DrawText(L"RU", 2, fmtBadge_, rcRu, brush);
    }

    DrawFluentButton(rt, pal, brush, ShiftRectY(RcDictAdd(), offsetY), L"Добавить", FluentIcon::None,
        true, hovered_.zone == HitZone::DictAddBtn ? btnAddHover_ : 0.0f,
        pressed_.zone == HitZone::DictAddBtn, alpha);

    D2D1_RECT_F listRc = ShiftRectY(RcListBox(), offsetY);
    brush->SetColor(FadeColor(pal.workspaceBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(listRc, 10.0f, 10.0f), brush);
    brush->SetColor(FadeColor(pal.borderSubtle, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(listRc, 10.0f, 10.0f), brush, 1.0f);

    rt->PushAxisAlignedClip(D2D1::RectF(listRc.left + 1.0f, listRc.top + 1.0f, listRc.right - 1.0f, listRc.bottom - 1.0f),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (cfg_.wordDictionary.empty()) {
        if (fmtRowTitle_) {
            fmtRowTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            brush->SetColor(FadeColor(pal.textSecondary, alpha));
            const wchar_t* emptyT = L"Словарь пуст";
            rt->DrawText(emptyT, static_cast<UINT32>(wcslen(emptyT)), fmtRowTitle_,
                D2D1::RectF(listRc.left + 20.0f, listRc.top + 160.0f, listRc.right - 20.0f, listRc.top + 182.0f), brush);
            fmtRowTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        if (fmtRowSub_) {
            fmtRowSub_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            brush->SetColor(FadeColor(pal.textTertiary, alpha));
            const wchar_t* emptyS = L"Введите слово и выберите целевую раскладку";
            rt->DrawText(emptyS, static_cast<UINT32>(wcslen(emptyS)), fmtRowSub_,
                D2D1::RectF(listRc.left + 20.0f, listRc.top + 184.0f, listRc.right - 20.0f, listRc.top + 204.0f), brush);
            fmtRowSub_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    } else {
        auto& le = Core::LayoutEngine::Instance();
        for (size_t i = 0; i < cfg_.wordDictionary.size(); ++i) {
            float rowY = listRc.top + 4.0f + static_cast<float>(i) * DICT_ROW_H - dictScrollY_;
            if (rowY + DICT_ROW_H < listRc.top || rowY > listRc.bottom) continue;

            const auto& rule = cfg_.wordDictionary[i];
            bool rowHov = (hovered_.index == static_cast<int>(i) &&
                (hovered_.zone == HitZone::DictRow || hovered_.zone == HitZone::DictRowBadge || hovered_.zone == HitZone::DictRowDelete));

            D2D1_RECT_F rowRc = D2D1::RectF(listRc.left + 4.0f, rowY + 2.0f, listRc.right - 8.0f, rowY + DICT_ROW_H - 2.0f);
            if (rowHov) {
                brush->SetColor(FadeColor(pal.rowHoverBg, alpha));
                rt->FillRoundedRectangle(D2D1::RoundedRect(rowRc, 6.0f, 6.0f), brush);
            }
            if (i + 1 < cfg_.wordDictionary.size()) {
                brush->SetColor(FadeColor(pal.divider, alpha));
                rt->DrawLine(D2D1::Point2F(listRc.left + 14.0f, rowY + DICT_ROW_H), D2D1::Point2F(listRc.right - 14.0f, rowY + DICT_ROW_H), brush, 1.0f);
            }

            bool isRu = (rule.layout == Config::AppLayout::Russian);
            bool hasCyr = false;
            for (wchar_t ch : rule.word) {
                if ((ch >= L'а' && ch <= L'я') || (ch >= L'А' && ch <= L'Я') || ch == L'ё' || ch == L'Ё') {
                    hasCyr = true;
                    break;
                }
            }
            std::wstring targetDisp = rule.word;
            std::wstring triggerDisp;
            if (isRu) {
                if (hasCyr) {
                    triggerDisp = Platform::ToLower(le.RuToEn(rule.word));
                } else {
                    triggerDisp = rule.word;
                    targetDisp = Platform::ToLower(le.EnToRu(rule.word));
                }
            } else {
                if (!hasCyr) {
                    triggerDisp = Platform::ToLower(le.EnToRu(rule.word));
                } else {
                    triggerDisp = rule.word;
                    targetDisp = Platform::ToLower(le.RuToEn(rule.word));
                }
            }
            std::wstring subText = triggerDisp + L" \u2192 " + targetDisp;

            if (fmtRowTitle_) {
                brush->SetColor(WithAlpha(pal.textPrimary, alpha));
                rt->DrawText(targetDisp.c_str(), static_cast<UINT32>(targetDisp.size()), fmtRowTitle_,
                    D2D1::RectF(rowRc.left + 12.0f, rowY + 4.0f, rowRc.right - 96.0f, rowY + 24.0f), brush);
            }
            if (fmtRowSub_) {
                brush->SetColor(FadeColor(pal.textSecondary, alpha));
                rt->DrawText(subText.c_str(), static_cast<UINT32>(subText.size()), fmtRowSub_,
                    D2D1::RectF(rowRc.left + 12.0f, rowY + 25.0f, rowRc.right - 96.0f, rowY + 42.0f), brush);
            }

            D2D1_RECT_F badgeRc = D2D1::RectF(listRc.right - 92.0f, rowY + 12.0f, listRc.right - 48.0f, rowY + DICT_ROW_H - 12.0f);
            brush->SetColor(FadeColor(isRu ? pal.langRuSoft : pal.langEnSoft, alpha));
            rt->FillRoundedRectangle(D2D1::RoundedRect(badgeRc, 6.0f, 6.0f), brush);
            if (fmtBadge_) {
                brush->SetColor(WithAlpha(isRu ? pal.langRuFill : pal.langEnFill, alpha));
                rt->DrawText(isRu ? L"RU" : L"EN", 2, fmtBadge_, badgeRc, brush);
            }

            D2D1_RECT_F delRc = D2D1::RectF(listRc.right - 40.0f, rowY + 9.0f, listRc.right - 12.0f, rowY + DICT_ROW_H - 9.0f);
            bool delHov = (hovered_.zone == HitZone::DictRowDelete && hovered_.index == static_cast<int>(i));
            if (delHov) {
                brush->SetColor(FadeColor(pal.dangerSoft, alpha));
                rt->FillRoundedRectangle(D2D1::RoundedRect(delRc, 6.0f, 6.0f), brush);
            }
            D2D1_COLOR_F delCol = delHov ? pal.danger : (rowHov ? pal.textSecondary : pal.textTertiary);
            DrawFluentIcon(rt, FluentIcon::Trash,
                D2D1::RectF(delRc.left + 7.5f, delRc.top + 8.5f, delRc.right - 7.5f, delRc.bottom - 8.5f),
                FadeColor(delCol, alpha), 1.4f, brush);
        }

        DrawScrollbar(rt, pal, brush, listRc, dictScrollY_, static_cast<float>(cfg_.wordDictionary.size()) * DICT_ROW_H + 8.0f, alpha);
    }

    rt->PopAxisAlignedClip();
}

void SettingsWindow::DrawExclusionsPage(ID2D1RenderTarget* rt, const FluentPalette& pal, ID2D1SolidColorBrush* brush, float offsetY, float alpha) {
    DrawTextInput(rt, pal, brush, exclInput_, offsetY, alpha);

    DrawFluentButton(rt, pal, brush, ShiftRectY(RcExclPick(), offsetY), L"Окна", FluentIcon::ChevronDown,
        false, hovered_.zone == HitZone::ExclPickBtn ? btnPickHover_ : 0.0f,
        pressed_.zone == HitZone::ExclPickBtn, alpha);

    DrawFluentButton(rt, pal, brush, ShiftRectY(RcExclAdd(), offsetY), L"Добавить", FluentIcon::None,
        true, hovered_.zone == HitZone::ExclAddBtn ? btnAddHover_ : 0.0f,
        pressed_.zone == HitZone::ExclAddBtn, alpha);

    D2D1_RECT_F listRc = ShiftRectY(RcListBox(), offsetY);
    brush->SetColor(FadeColor(pal.workspaceBg, alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(listRc, 10.0f, 10.0f), brush);
    brush->SetColor(FadeColor(pal.borderSubtle, alpha));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(listRc, 10.0f, 10.0f), brush, 1.0f);

    rt->PushAxisAlignedClip(D2D1::RectF(listRc.left + 1.0f, listRc.top + 1.0f, listRc.right - 1.0f, listRc.bottom - 1.0f),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (cfg_.exclusions.empty()) {
        if (fmtRowTitle_) {
            fmtRowTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            brush->SetColor(FadeColor(pal.textSecondary, alpha));
            const wchar_t* emptyT = L"Список исключений пуст";
            rt->DrawText(emptyT, static_cast<UINT32>(wcslen(emptyT)), fmtRowTitle_,
                D2D1::RectF(listRc.left + 20.0f, listRc.top + 158.0f, listRc.right - 20.0f, listRc.top + 180.0f), brush);
            fmtRowTitle_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    } else {
        for (size_t i = 0; i < cfg_.exclusions.size(); ++i) {
            float rowY = listRc.top + 4.0f + static_cast<float>(i) * EXCL_ROW_H - exclScrollY_;
            if (rowY + EXCL_ROW_H < listRc.top || rowY > listRc.bottom) continue;

            const auto& proc = cfg_.exclusions[i];
            bool rowHov = (hovered_.index == static_cast<int>(i) &&
                (hovered_.zone == HitZone::ExclRow || hovered_.zone == HitZone::ExclRowDelete));

            D2D1_RECT_F rowRc = D2D1::RectF(listRc.left + 4.0f, rowY + 2.0f, listRc.right - 8.0f, rowY + EXCL_ROW_H - 2.0f);
            if (rowHov) {
                brush->SetColor(FadeColor(pal.rowHoverBg, alpha));
                rt->FillRoundedRectangle(D2D1::RoundedRect(rowRc, 6.0f, 6.0f), brush);
            }
            if (i + 1 < cfg_.exclusions.size()) {
                brush->SetColor(FadeColor(pal.divider, alpha));
                rt->DrawLine(D2D1::Point2F(listRc.left + 14.0f, rowY + EXCL_ROW_H), D2D1::Point2F(listRc.right - 14.0f, rowY + EXCL_ROW_H), brush, 1.0f);
            }

            if (fmtRowTitle_) {
                brush->SetColor(WithAlpha(pal.textPrimary, alpha));
                rt->DrawText(proc.c_str(), static_cast<UINT32>(proc.size()), fmtRowTitle_,
                    D2D1::RectF(rowRc.left + 12.0f, rowY, rowRc.right - 44.0f, rowY + EXCL_ROW_H), brush);
            }

            D2D1_RECT_F delRc = D2D1::RectF(listRc.right - 40.0f, rowY + 7.0f, listRc.right - 12.0f, rowY + EXCL_ROW_H - 7.0f);
            bool delHov = (hovered_.zone == HitZone::ExclRowDelete && hovered_.index == static_cast<int>(i));
            if (delHov) {
                brush->SetColor(FadeColor(pal.dangerSoft, alpha));
                rt->FillRoundedRectangle(D2D1::RoundedRect(delRc, 6.0f, 6.0f), brush);
            }
            D2D1_COLOR_F delCol = delHov ? pal.danger : (rowHov ? pal.textSecondary : pal.textTertiary);
            DrawFluentIcon(rt, FluentIcon::Trash,
                D2D1::RectF(delRc.left + 7.5f, delRc.top + 8.5f, delRc.right - 7.5f, delRc.bottom - 8.5f),
                FadeColor(delCol, alpha), 1.4f, brush);
        }

        DrawScrollbar(rt, pal, brush, listRc, exclScrollY_, static_cast<float>(cfg_.exclusions.size()) * EXCL_ROW_H + 8.0f, alpha);
    }

    rt->PopAxisAlignedClip();
}

void SettingsWindow::RenderNow(HDC fallbackDstDC) {
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
        rt->Clear(pal.workspaceBg);
    }

    if (brush) {
        DrawNavigationRail(rt, pal, brush);
        DrawWorkspaceShell(rt, pal, brush);
        DrawCaptionButtons(rt, pal, brush);

        float ease = EaseOutQuint(pageTransition_);
        float inOffsetY = (1.0f - ease) * 8.0f * static_cast<float>(pageDir_);
        float inAlpha = ease;

        DrawPageHeader(rt, pal, brush, inOffsetY, inAlpha);

        rt->PushAxisAlignedClip(D2D1::RectF(SIDEBAR_W + 1.0f, BODY_TOP, WND_LOGICAL_W - 1.0f, BODY_BOTTOM),
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        if (pageTransition_ < 0.99f && prevTab_ != activeTab_) {
            float outOffsetY = -ease * 6.0f * static_cast<float>(pageDir_);
            float outAlpha = (std::max)(0.0f, 1.0f - ease * 1.6f);
            if (outAlpha > 0.01f) {
                if (prevTab_ == SettingsTab::General) DrawGeneralPage(rt, pal, brush, outOffsetY, outAlpha);
                else if (prevTab_ == SettingsTab::AppLayouts) DrawAppLayoutsPage(rt, pal, brush, outOffsetY, outAlpha);
                else if (prevTab_ == SettingsTab::Dictionary) DrawDictionaryPage(rt, pal, brush, outOffsetY, outAlpha);
                else DrawExclusionsPage(rt, pal, brush, outOffsetY, outAlpha);
            }
        }

        if (activeTab_ == SettingsTab::General) DrawGeneralPage(rt, pal, brush, inOffsetY, inAlpha);
        else if (activeTab_ == SettingsTab::AppLayouts) DrawAppLayoutsPage(rt, pal, brush, inOffsetY, inAlpha);
        else if (activeTab_ == SettingsTab::Dictionary) DrawDictionaryPage(rt, pal, brush, inOffsetY, inAlpha);
        else DrawExclusionsPage(rt, pal, brush, inOffsetY, inAlpha);

        rt->PopAxisAlignedClip();

        DrawFooterBar(rt, pal, brush);

        brush->SetColor(pal.borderSubtle);
        rt->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, WND_LOGICAL_W - 0.5f, WND_LOGICAL_H - 0.5f), 8.0f, 8.0f),
            brush, 1.0f
        );
    }

    HRESULT hr = surface_.EndDrawAndPresent(fallbackDstDC);
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED) {
        RequestRedraw();
    }
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        float scale = dpi_ / 96.0f;
        float dipX = pt.x / scale;
        float dipY = pt.y / scale;
        if (dipY >= 0.0f && dipY < TOP_DRAG_H) {
            if (!PointInRectF(RcBtnMinimize(), dipX, dipY) &&
                !PointInRectF(RcBtnClose(), dipX, dipY) &&
                !PointInRectF(RcThemeToggle(), dipX, dipY)) {
                return HTCAPTION;
            }
        }
        return HTCLIENT;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RenderNow(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == TIMER_ANIM) {
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
        HandleMouseMove(GET_X_LPARAM(lParam) / scale, GET_Y_LPARAM(lParam) / scale);
        return 0;
    }
    case WM_MOUSELEAVE: {
        trackingMouse_ = false;
        if (hovered_.zone != HitZone::None || appInput_.hovered || dictInput_.hovered || exclInput_.hovered) {
            hovered_ = {};
            appInput_.hovered = false;
            dictInput_.hovered = false;
            exclInput_.hovered = false;
            EnsureAnimationRunning();
            RequestRedraw();
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        float scale = dpi_ / 96.0f;
        HandleMouseDown(GET_X_LPARAM(lParam) / scale, GET_Y_LPARAM(lParam) / scale, false);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        float scale = dpi_ / 96.0f;
        HandleMouseDown(GET_X_LPARAM(lParam) / scale, GET_Y_LPARAM(lParam) / scale, true);
        return 0;
    }
    case WM_LBUTTONUP: {
        float scale = dpi_ / 96.0f;
        HandleMouseUp(GET_X_LPARAM(lParam) / scale, GET_Y_LPARAM(lParam) / scale);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        float scale = dpi_ / 96.0f;
        HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), pt.x / scale, pt.y / scale);
        return 0;
    }
    case WM_KEYDOWN: {
        if (FluentTextInput* active = ActiveTextInput()) {
            if (HandleInputKeyDown(*active, wParam)) return 0;
        } else if (wParam == VK_ESCAPE) {
            Hide();
            return 0;
        }
        break;
    }
    case WM_CHAR: {
        if (FluentTextInput* active = ActiveTextInput()) {
            if (HandleInputChar(*active, static_cast<wchar_t>(wParam))) return 0;
        }
        break;
    }
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            int w = suggested->right - suggested->left;
            int h = suggested->bottom - suggested->top;
            SetWindowPos(hWnd, nullptr, suggested->left, suggested->top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
            surface_.EnsureSize(hWnd, w, h);
        }
        RequestRedraw();
        return 0;
    }
    case WM_SETTINGCHANGE: {
        Platform::TrayIcon::Instance().RefreshTheme();
        bool nextDark = ResolveDarkMode();
        if (nextDark != darkMode_) {
            darkMode_ = nextDark;
            hasBackdrop_ = ApplyWindowBackdrop(hWnd, BackdropKind::None, darkMode_);
            appliedDark_ = darkMode_;
            backdropApplied_ = true;
            RequestRedraw();
        }
        return 0;
    }
    case WM_SETTINGS_STATUS_SYNC: {
        bool isRu = (wParam != 0);
        bool isPaused = (lParam != 0);
        if (isRu != liveIsRu_ || isPaused != livePaused_) {
            liveIsRu_ = isRu;
            livePaused_ = isPaused;
            if (IsWindowVisible(hWnd)) {
                EnsureAnimationRunning();
                RequestRedraw();
            }
        }
        return 0;
    }
    case WM_CLOSE:
        Hide();
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace UI
