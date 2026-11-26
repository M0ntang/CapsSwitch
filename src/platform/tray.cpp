#include "platform/tray.hpp"
#include "core/layout_engine.hpp"
#include "core/hook.hpp"
#include "ui/fluent_menu.hpp"
#include "ui/fluent_theme.hpp"
#include <algorithm>
#include <vector>

namespace Platform {

TrayIcon& TrayIcon::Instance() {
    static TrayIcon s_instance;
    return s_instance;
}

TrayIcon::TrayIcon() {}

TrayIcon::~TrayIcon() {
    Destroy();
}

static HICON CreateIconFromD2DRender(int size, const std::function<void(ID2D1DCRenderTarget*, float)>& renderFn) {
    auto& gfx = UI::FluentGraphics::Instance();
    if (!gfx.EnsureInitialized() || !gfx.D2DFactory()) {
        return nullptr;
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
    if (!hdcMem) return nullptr;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hbmColor) {
        DeleteDC(hdcMem);
        return nullptr;
    }

    HBITMAP hOldColor = static_cast<HBITMAP>(SelectObject(hdcMem, hbmColor));

    ID2D1DCRenderTarget* rt = nullptr;
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f
    );

    if (SUCCEEDED(gfx.D2DFactory()->CreateDCRenderTarget(&props, &rt)) && rt) {
        RECT rcBind = { 0, 0, size, size };
        if (SUCCEEDED(rt->BindDC(hdcMem, &rcBind))) {
            rt->BeginDraw();
            rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            rt->Clear(D2D1::ColorF(0, 0, 0, 0));

            renderFn(rt, static_cast<float>(size));

            rt->EndDraw();
        }
        rt->Release();
    }

    SelectObject(hdcMem, hOldColor);
    DeleteDC(hdcMem);

    int maskStride = ((size + 15) / 16) * 2;
    std::vector<BYTE> zeroMask(static_cast<size_t>(maskStride) * size, 0);
    HBITMAP hbmMask = CreateBitmap(size, size, 1, 1, zeroMask.data());

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;

    HICON hIcon = CreateIconIndirect(&ii);

    if (hbmMask) DeleteObject(hbmMask);
    DeleteObject(hbmColor);
    return hIcon;
}

HICON TrayIcon::CreateFluentBadgeIcon(int sizePx, bool isRu, bool isPaused) {
    return CreateFluentBadgeIcon(sizePx, isRu, isPaused, UI::IsTaskbarDarkMode());
}

HICON TrayIcon::CreateFluentBadgeIcon(int sizePx, bool isRu, bool isPaused, bool /*darkTaskbar*/) {
    int iconSize = (sizePx > 0) ? sizePx : GetSystemMetrics(SM_CXSMICON);
    if (iconSize <= 0) iconSize = 16;

    UINT resId = isPaused ? IDI_TRAY_PAUSED : (isRu ? IDI_TRAY_RU : IDI_TRAY_EN);
    return static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(resId),
        IMAGE_ICON,
        iconSize,
        iconSize,
        LR_DEFAULTCOLOR
    ));
}

HICON TrayIcon::CreateFluentAppIcon(int sizePx) {
    int size = (std::max)(16, sizePx);
    return CreateIconFromD2DRender(size, [](ID2D1DCRenderTarget* rt, float fSize) {
        float inset = (std::max)(0.5f, fSize * 0.04f);
        float radius = fSize * 0.26f;
        D2D1_RECT_F badgeRc = D2D1::RectF(inset, inset, fSize - inset, fSize - inset);
        D2D1_ROUNDED_RECT roundRc = D2D1::RoundedRect(badgeRc, radius, radius);

        // Deep obsidian-slate squircle
        D2D1_GRADIENT_STOP stops[2] = {
            { 0.0f, UI::Rgba(34, 44, 62, 1.0f) },
            { 1.0f, UI::Rgba(15, 23, 42, 1.0f) }
        };
        ID2D1GradientStopCollection* stopCol = nullptr;
        if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &stopCol)) && stopCol) {
            ID2D1LinearGradientBrush* gradBrush = nullptr;
            if (SUCCEEDED(rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, inset), D2D1::Point2F(0, fSize - inset)),
                stopCol, &gradBrush)) && gradBrush) {
                rt->FillRoundedRectangle(roundRc, gradBrush);
                gradBrush->Release();
            }
            stopCol->Release();
        }

        // Specular top-lit border
        D2D1_GRADIENT_STOP rimStops[2] = {
            { 0.0f, UI::Rgba(255, 255, 255, 0.28f) },
            { 1.0f, UI::Rgba(255, 255, 255, 0.06f) }
        };
        ID2D1GradientStopCollection* rimCol = nullptr;
        if (SUCCEEDED(rt->CreateGradientStopCollection(rimStops, 2, &rimCol)) && rimCol) {
            ID2D1LinearGradientBrush* rimBrush = nullptr;
            if (SUCCEEDED(rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, inset), D2D1::Point2F(0, fSize - inset)),
                rimCol, &rimBrush)) && rimBrush) {
                D2D1_ROUNDED_RECT rimRc = D2D1::RoundedRect(
                    D2D1::RectF(inset + 0.5f, inset + 0.5f, fSize - inset - 0.5f, fSize - inset - 0.5f),
                    (std::max)(1.0f, radius - 0.5f),
                    (std::max)(1.0f, radius - 0.5f)
                );
                rt->DrawRoundedRectangle(rimRc, rimBrush, 1.0f);
                rimBrush->Release();
            }
            rimCol->Release();
        }

        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(rt->CreateSolidColorBrush(UI::Rgba(255, 255, 255, 0.96f), &brush)) && brush) {
            ID2D1StrokeStyle* round = UI::FluentGraphics::Instance().RoundStrokeStyle();
            float s = fSize / 16.0f;
            float sw = (std::max)(1.35f, 1.55f * s);

            // Minimalist Caps-Up chevron + stem
            rt->DrawLine(D2D1::Point2F(4.6f * s, 7.6f * s), D2D1::Point2F(8.0f * s, 4.1f * s), brush, sw, round);
            rt->DrawLine(D2D1::Point2F(8.0f * s, 4.1f * s), D2D1::Point2F(11.4f * s, 7.6f * s), brush, sw, round);
            rt->DrawLine(D2D1::Point2F(8.0f * s, 4.6f * s), D2D1::Point2F(8.0f * s, 9.8f * s), brush, sw, round);

            // Dual micro color-coded accent pills at base (Blue EN + Rose RU)
            float pillH = (std::max)(1.8f, 1.7f * s);
            float pillW = 3.4f * s;
            float pillY = 11.4f * s;
            float pillRad = pillH * 0.5f;

            brush->SetColor(UI::Rgba(56, 189, 248, 1.0f));
            rt->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(4.2f * s, pillY, 4.2f * s + pillW, pillY + pillH), pillRad, pillRad),
                brush
            );

            brush->SetColor(UI::Rgba(251, 113, 133, 1.0f));
            rt->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(11.8f * s - pillW, pillY, 11.8f * s, pillY + pillH), pillRad, pillRad),
                brush
            );

            brush->Release();
        }
    });
}

void TrayIcon::EnsureIconsLocked() {
    int sysSize = GetSystemMetrics(SM_CXSMICON);
    if (sysSize <= 0) sysSize = 16;
    bool darkBar = UI::IsTaskbarDarkMode();

    if (iconSize_ != sysSize || darkTaskbar_ != darkBar || !hIconEn_ || !hIconRu_ || !hIconPaused_) {
        DestroyIconsLocked();
        iconSize_ = sysSize;
        darkTaskbar_ = darkBar;
        hIconEn_ = CreateFluentBadgeIcon(sysSize, false, false, darkBar);
        hIconRu_ = CreateFluentBadgeIcon(sysSize, true, false, darkBar);
        hIconPaused_ = CreateFluentBadgeIcon(sysSize, false, true, darkBar);
    }
}

void TrayIcon::DestroyIconsLocked() {
    if (hIconEn_)     { DestroyIcon(hIconEn_);     hIconEn_ = nullptr; }
    if (hIconRu_)     { DestroyIcon(hIconRu_);     hIconRu_ = nullptr; }
    if (hIconPaused_) { DestroyIcon(hIconPaused_); hIconPaused_ = nullptr; }
    iconSize_ = 0;
}

void TrayIcon::ApplyStateLocked() {
    if (currentPaused_) {
        nid_.hIcon = hIconPaused_;
        wcscpy_s(nid_.szTip, L"CapsSwitch (На паузе)");
    } else if (currentIsRu_) {
        nid_.hIcon = hIconRu_;
        wcscpy_s(nid_.szTip, L"CapsSwitch (Активен — RU)");
    } else {
        nid_.hIcon = hIconEn_;
        wcscpy_s(nid_.szTip, L"CapsSwitch (Активен — EN)");
    }
}

bool TrayIcon::Create(HWND hWndOwner) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isVisible_) return true;

    hWndOwner_ = hWndOwner;
    EnsureIconsLocked();

    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hWndOwner_;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;

    ApplyStateLocked();

    isVisible_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    return isVisible_;
}

bool TrayIcon::Readd() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hWndOwner_) return false;

    EnsureIconsLocked();
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    ApplyStateLocked();
    isVisible_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    return isVisible_;
}

void TrayIcon::Destroy() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isVisible_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        isVisible_ = false;
    }
    DestroyIconsLocked();
}

void TrayIcon::UpdateLanguage(WORD langId, bool isPaused) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isVisible_) return;

    bool isRu = ((langId & 0xFF) == 0x19);
    if (hasState_ && isRu == currentIsRu_ && isPaused == currentPaused_) {
        return;
    }

    currentIsRu_ = isRu;
    currentPaused_ = isPaused;
    hasState_ = true;

    EnsureIconsLocked();
    ApplyStateLocked();
    nid_.uFlags = NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::RefreshTheme() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isVisible_) return;

    bool darkBar = UI::IsTaskbarDarkMode();
    if (darkBar == darkTaskbar_ && hIconEn_ && hIconRu_ && hIconPaused_) {
        return;
    }

    EnsureIconsLocked();
    ApplyStateLocked();
    nid_.uFlags = NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::ShowContextMenu(POINT pt) {
    bool paused = Core::HookManager::Instance().IsPaused();

    std::vector<UI::FluentMenuItem> items = {
        { ID_TRAY_SETTINGS, L"Настройки", L"", UI::FluentIcon::Settings, false, false, false },
        { ID_TRAY_PAUSE, paused ? L"Возобновить" : L"Приостановить", L"",
          paused ? UI::FluentIcon::Play : UI::FluentIcon::Pause, false, false, false },
        { ID_TRAY_EXIT, L"Выход", L"", UI::FluentIcon::Power, false, false, false }
    };

    UI::FluentMenu::Instance().Show(
        hWndOwner_,
        pt,
        true,
        items,
        [this](UINT cmd) {
            if (cmd == ID_TRAY_SETTINGS && onSettings_) {
                onSettings_();
            } else if (cmd == ID_TRAY_PAUSE && onPause_) {
                onPause_();
            } else if (cmd == ID_TRAY_EXIT && onExit_) {
                onExit_();
            }
        },
        -1,
        true
    );
}

} // namespace Platform
