#include "ui/fluent_theme.hpp"
#include <dwmapi.h>
#include <algorithm>
#include <cmath>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif

#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif

#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif

#ifndef DWMSBT_TABBEDWINDOW
#define DWMSBT_TABBEDWINDOW 4
#endif

namespace UI {

static const IID kIidID2D1Factory1 = {
    0xbb12d362, 0xdaee, 0x4b9a, { 0xaa, 0x1d, 0x14, 0xba, 0x40, 0x1c, 0xfa, 0x1f }
};
static const IID kIidIDWriteFactory = {
    0xb859ee5a, 0xd838, 0x4b5b, { 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48 }
};
static const IID kIidIDXGIDevice = {
    0x54ec77fa, 0x1377, 0x44e6, { 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c }
};
static const IID kIidIDXGIFactory2 = {
    0x50c83a1c, 0xe072, 0x4c48, { 0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0 }
};
static const IID kIidIDXGISurface = {
    0xcafcb56c, 0x6ac3, 0x4889, { 0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec }
};
static const IID kIidIDCompositionDevice = {
    0xc37ea93a, 0xe7aa, 0x450d, { 0xb1, 0x6f, 0x97, 0x46, 0xcb, 0x04, 0x07, 0xf3 }
};

static bool QueryPersonalizeBoolZeroIsDark(const wchar_t* valueName) {
    HKEY hKey = nullptr;
    LONG res = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_QUERY_VALUE,
        &hKey
    );
    if (res != ERROR_SUCCESS || !hKey) {
        return true;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    res = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(hKey);

    if (res == ERROR_SUCCESS && type == REG_DWORD) {
        return value == 0;
    }
    return true;
}

bool IsSystemDarkMode() {
    return QueryPersonalizeBoolZeroIsDark(L"AppsUseLightTheme");
}

bool IsTaskbarDarkMode() {
    return QueryPersonalizeBoolZeroIsDark(L"SystemUsesLightTheme");
}

FluentPalette GetFluentPalette(bool dark) {
    FluentPalette p;
    p.darkMode = dark;

    if (dark) {
        p.sidebarBg          = Rgba(22, 27, 34, 1.0f);
        p.footerBg           = Rgba(22, 27, 34, 1.0f);
        p.shellFallbackBg    = Rgba(22, 27, 34, 1.0f);
        p.menuFallbackBg     = Rgba(44, 44, 46, 1.0f);
        p.workspaceBg        = Rgba(15, 20, 26, 1.0f);
        p.bannerBg           = Rgba(22, 39, 60, 1.0f);
        p.bannerPausedBg     = Rgba(44, 34, 18, 1.0f);
        p.bannerIconBoxBg    = Rgba(29, 49, 74, 1.0f);
        p.cardBg             = Rgba(22, 27, 34, 1.0f);
        p.cardHoverBg        = Rgba(30, 37, 46, 1.0f);
        p.cardPressedBg      = Rgba(19, 24, 30, 1.0f);
        p.rowHoverBg         = Rgba(255, 255, 255, 0.035f);
        p.rowPressedBg       = Rgba(255, 255, 255, 0.06f);
        p.surfaceElevated    = Rgba(255, 255, 255, 0.065f);
        p.navSelectedBg      = Rgba(30, 58, 95, 0.55f);
        p.navSelectedFg      = Rgba(110, 178, 255, 1.0f);
        p.navHoverBg         = Rgba(255, 255, 255, 0.055f);
        p.navItemHover       = p.navHoverBg;
        p.navItemSelected    = p.navSelectedBg;
        p.inputBg            = Rgba(22, 27, 34, 1.0f);
        p.inputFocusBg       = Rgba(18, 23, 30, 1.0f);
        p.borderSubtle       = Rgba(255, 255, 255, 0.085f);
        p.borderStrong       = Rgba(255, 255, 255, 0.16f);
        p.borderTopHighlight = Rgba(255, 255, 255, 0.12f);
        p.divider            = Rgba(255, 255, 255, 0.075f);

        p.textPrimary        = Rgba(240, 244, 248, 1.0f);
        p.textSecondary      = Rgba(148, 163, 184, 1.0f);
        p.textTertiary       = Rgba(100, 116, 139, 1.0f);

        p.accent             = Rgba(52, 126, 214, 1.0f);
        p.accentHover        = Rgba(68, 142, 228, 1.0f);
        p.accentPressed      = Rgba(40, 108, 190, 1.0f);
        p.accentText         = Rgba(255, 255, 255, 1.0f);
        p.accentSoft         = Rgba(52, 126, 214, 0.18f);

        p.toggleOnTrack      = p.accent;
        p.toggleOnHover      = p.accentHover;
        p.toggleOnKnob       = p.accentText;
        p.toggleOffTrack     = Rgba(255, 255, 255, 0.04f);
        p.toggleOffBorder    = Rgba(148, 163, 184, 0.45f);
        p.toggleOffKnob      = Rgba(148, 163, 184, 0.90f);

        p.langEnFill         = Rgba(96, 165, 250, 1.0f);
        p.langEnSoft         = Rgba(59, 130, 246, 0.18f);

        p.langRuFill         = Rgba(251, 113, 133, 1.0f);
        p.langRuSoft         = Rgba(244, 63, 94, 0.18f);

        p.statusActive       = Rgba(52, 211, 153, 1.0f);
        p.statusActiveSoft   = Rgba(52, 211, 153, 0.14f);
        p.statusPaused       = Rgba(251, 191, 36, 1.0f);
        p.statusPausedSoft   = Rgba(251, 191, 36, 0.15f);

        p.danger             = Rgba(248, 113, 113, 1.0f);
        p.dangerHoverBg      = Rgba(220, 38, 38, 0.90f);
        p.dangerSoft         = Rgba(248, 113, 113, 0.14f);
    } else {
        p.sidebarBg          = Rgba(243, 246, 249, 1.0f);
        p.footerBg           = Rgba(243, 246, 249, 1.0f);
        p.shellFallbackBg    = Rgba(243, 246, 249, 1.0f);
        p.menuFallbackBg     = Rgba(249, 249, 249, 1.0f);
        p.workspaceBg        = Rgba(255, 255, 255, 1.0f);
        p.bannerBg           = Rgba(231, 242, 253, 1.0f);
        p.bannerPausedBg     = Rgba(254, 244, 226, 1.0f);
        p.bannerIconBoxBg    = Rgba(255, 255, 255, 1.0f);
        p.cardBg             = Rgba(255, 255, 255, 1.0f);
        p.cardHoverBg        = Rgba(246, 248, 251, 1.0f);
        p.cardPressedBg      = Rgba(238, 242, 247, 1.0f);
        p.rowHoverBg         = Rgba(243, 246, 249, 0.85f);
        p.rowPressedBg       = Rgba(234, 239, 245, 1.0f);
        p.surfaceElevated    = Rgba(255, 255, 255, 0.82f);
        p.navSelectedBg      = Rgba(227, 240, 253, 1.0f);
        p.navSelectedFg      = Rgba(24, 96, 168, 1.0f);
        p.navHoverBg         = Rgba(15, 23, 42, 0.045f);
        p.navItemHover       = p.navHoverBg;
        p.navItemSelected    = p.navSelectedBg;
        p.inputBg            = Rgba(255, 255, 255, 1.0f);
        p.inputFocusBg       = Rgba(255, 255, 255, 1.0f);
        p.borderSubtle       = Rgba(226, 232, 240, 1.0f);
        p.borderStrong       = Rgba(203, 213, 225, 1.0f);
        p.borderTopHighlight = Rgba(0, 0, 0, 0.10f);
        p.divider            = Rgba(230, 235, 241, 1.0f);

        p.textPrimary        = Rgba(17, 24, 39, 1.0f);
        p.textSecondary      = Rgba(100, 116, 139, 1.0f);
        p.textTertiary       = Rgba(148, 163, 184, 1.0f);

        p.accent             = Rgba(28, 92, 166, 1.0f);
        p.accentHover        = Rgba(22, 77, 142, 1.0f);
        p.accentPressed      = Rgba(17, 62, 117, 1.0f);
        p.accentText         = Rgba(255, 255, 255, 1.0f);
        p.accentSoft         = Rgba(28, 92, 166, 0.12f);

        p.toggleOnTrack      = p.accent;
        p.toggleOnHover      = p.accentHover;
        p.toggleOnKnob       = p.accentText;
        p.toggleOffTrack     = Rgba(255, 255, 255, 1.0f);
        p.toggleOffBorder    = Rgba(148, 163, 184, 1.0f);
        p.toggleOffKnob      = Rgba(94, 106, 122, 1.0f);

        p.langEnFill         = Rgba(28, 92, 166, 1.0f);
        p.langEnSoft         = Rgba(227, 240, 253, 1.0f);

        p.langRuFill         = Rgba(196, 43, 59, 1.0f);
        p.langRuSoft         = Rgba(253, 232, 235, 1.0f);

        p.statusActive       = Rgba(22, 163, 74, 1.0f);
        p.statusActiveSoft   = Rgba(22, 163, 74, 0.12f);
        p.statusPaused       = Rgba(217, 119, 6, 1.0f);
        p.statusPausedSoft   = Rgba(217, 119, 6, 0.14f);

        p.danger             = Rgba(220, 38, 38, 1.0f);
        p.dangerHoverBg      = Rgba(220, 38, 38, 0.92f);
        p.dangerSoft         = Rgba(220, 38, 38, 0.10f);
    }

    return p;
}

bool ApplyWindowBackdrop(HWND hWnd, BackdropKind kind, bool dark) {
    if (!hWnd || !IsWindow(hWnd)) return false;

    BOOL darkAttr = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkAttr, sizeof(darkAttr));

    DWORD cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    if (kind == BackdropKind::None) {
        MARGINS margins = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea(hWnd, &margins);
        DWORD backdrop = DWMSBT_NONE;
        DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        return false;
    }

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hWnd, &margins);

    DWORD backdrop = DWMSBT_MAINWINDOW;
    if (kind == BackdropKind::Acrylic) {
        backdrop = DWMSBT_TRANSIENTWINDOW;
    } else if (kind == BackdropKind::MicaAlt) {
        backdrop = DWMSBT_TABBEDWINDOW;
    }
    HRESULT hr = DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    if (FAILED(hr) && kind == BackdropKind::MicaAlt) {
        backdrop = DWMSBT_MAINWINDOW;
        hr = DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    }
    return SUCCEEDED(hr);
}

UINT GetWindowDpiSafe(HWND hWnd) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static GetDpiForWindowFn fnGetDpiForWindow = []() -> GetDpiForWindowFn {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (!hUser32) return nullptr;
        return reinterpret_cast<GetDpiForWindowFn>(
            reinterpret_cast<void*>(GetProcAddress(hUser32, "GetDpiForWindow"))
        );
    }();

    if (fnGetDpiForWindow && hWnd) {
        UINT dpi = fnGetDpiForWindow(hWnd);
        if (dpi >= 72) return dpi;
    }

    HDC hdc = GetDC(hWnd);
    UINT dpi = 96;
    if (hdc) {
        int val = GetDeviceCaps(hdc, LOGPIXELSX);
        if (val >= 72) dpi = static_cast<UINT>(val);
        ReleaseDC(hWnd, hdc);
    }
    return dpi;
}

float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float EaseOutCubic(float t) {
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float EaseOutQuint(float t) {
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv * inv * inv;
}

float ExpDecay(float current, float target, float speed, float dt) {
    if (dt <= 0.0f) return current;
    float factor = 1.0f - std::exp(-speed * dt);
    float next = current + (target - current) * factor;
    if (std::fabs(target - next) < 0.0005f) {
        return target;
    }
    return next;
}

D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t) {
    t = Clamp01(t);
    return D2D1::ColorF(
        Lerp(a.r, b.r, t),
        Lerp(a.g, b.g, t),
        Lerp(a.b, b.b, t),
        Lerp(a.a, b.a, t)
    );
}

D2D1_COLOR_F WithAlpha(const D2D1_COLOR_F& c, float alpha) {
    return D2D1::ColorF(c.r, c.g, c.b, Clamp01(alpha));
}

FluentGraphics& FluentGraphics::Instance() {
    static FluentGraphics s_instance;
    return s_instance;
}

FluentGraphics::~FluentGraphics() {
    Shutdown();
}

bool FluentGraphics::HasFontFamily(const wchar_t* familyName) {
    if (!dwriteFactory_ || !familyName) return false;
    IDWriteFontCollection* collection = nullptr;
    if (FAILED(dwriteFactory_->GetSystemFontCollection(&collection, FALSE)) || !collection) {
        return false;
    }
    UINT32 index = 0;
    BOOL exists = FALSE;
    HRESULT hr = collection->FindFamilyName(familyName, &index, &exists);
    collection->Release();
    return SUCCEEDED(hr) && exists == TRUE;
}

void FluentGraphics::BuildCachedGeometries() {
    if (!d2dFactory_) return;

    if (!shieldGeo_ && SUCCEEDED(d2dFactory_->CreatePathGeometry(&shieldGeo_)) && shieldGeo_) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(shieldGeo_->Open(&sink)) && sink) {
            sink->BeginFigure(D2D1::Point2F(8.0f, 2.0f), D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine(D2D1::Point2F(13.2f, 4.0f));
            sink->AddLine(D2D1::Point2F(13.2f, 7.8f));
            sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(13.2f, 11.2f), D2D1::Point2F(10.8f, 13.2f), D2D1::Point2F(8.0f, 14.3f)));
            sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(5.2f, 13.2f), D2D1::Point2F(2.8f, 11.2f), D2D1::Point2F(2.8f, 7.8f)));
            sink->AddLine(D2D1::Point2F(2.8f, 4.0f));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();
        }
    }

    if (!playGeo_ && SUCCEEDED(d2dFactory_->CreatePathGeometry(&playGeo_)) && playGeo_) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(playGeo_->Open(&sink)) && sink) {
            sink->BeginFigure(D2D1::Point2F(4.5f, 3.2f), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(12.8f, 8.0f));
            sink->AddLine(D2D1::Point2F(4.5f, 12.8f));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();
        }
    }

    if (!powerGeo_ && SUCCEEDED(d2dFactory_->CreatePathGeometry(&powerGeo_)) && powerGeo_) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(powerGeo_->Open(&sink)) && sink) {
            sink->BeginFigure(D2D1::Point2F(4.8f, 4.2f), D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddArc(D2D1::ArcSegment(
                D2D1::Point2F(11.2f, 4.2f),
                D2D1::SizeF(5.2f, 5.2f),
                0.0f,
                D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                D2D1_ARC_SIZE_LARGE
            ));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();
            sink->Release();
        }
    }

    if (!folderGeo_ && SUCCEEDED(d2dFactory_->CreatePathGeometry(&folderGeo_)) && folderGeo_) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(folderGeo_->Open(&sink)) && sink) {
            sink->BeginFigure(D2D1::Point2F(2.2f, 3.8f), D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine(D2D1::Point2F(6.4f, 3.8f));
            sink->AddLine(D2D1::Point2F(7.8f, 5.4f));
            sink->AddLine(D2D1::Point2F(13.8f, 5.4f));
            sink->AddLine(D2D1::Point2F(13.8f, 12.6f));
            sink->AddLine(D2D1::Point2F(2.2f, 12.6f));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();
        }
    }

    if (!moonGeo_ && SUCCEEDED(d2dFactory_->CreatePathGeometry(&moonGeo_)) && moonGeo_) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(moonGeo_->Open(&sink)) && sink) {
            sink->BeginFigure(D2D1::Point2F(9.4f, 2.9f), D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddArc(D2D1::ArcSegment(
                D2D1::Point2F(12.8f, 10.2f),
                D2D1::SizeF(5.3f, 5.3f),
                0.0f,
                D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                D2D1_ARC_SIZE_LARGE
            ));
            sink->AddArc(D2D1::ArcSegment(
                D2D1::Point2F(9.4f, 2.9f),
                D2D1::SizeF(4.4f, 4.4f),
                0.0f,
                D2D1_SWEEP_DIRECTION_CLOCKWISE,
                D2D1_ARC_SIZE_SMALL
            ));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();
        }
    }
}

void FluentGraphics::ReleaseCachedGeometries() {
    if (shieldGeo_) { shieldGeo_->Release(); shieldGeo_ = nullptr; }
    if (playGeo_)   { playGeo_->Release();   playGeo_ = nullptr; }
    if (powerGeo_)  { powerGeo_->Release();  powerGeo_ = nullptr; }
    if (folderGeo_) { folderGeo_->Release(); folderGeo_ = nullptr; }
    if (moonGeo_)   { moonGeo_->Release();   moonGeo_ = nullptr; }
}

bool FluentGraphics::EnsureInitialized() {
    if (initialized_) return (d2dFactory_ != nullptr && dwriteFactory_ != nullptr);
    initialized_ = true;

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        kIidID2D1Factory1,
        nullptr,
        reinterpret_cast<void**>(&d2dFactory_)
    );
    if (FAILED(hr) || !d2dFactory_) {
        d2dFactory_ = nullptr;
        return false;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        kIidIDWriteFactory,
        reinterpret_cast<IUnknown**>(&dwriteFactory_)
    );
    if (FAILED(hr) || !dwriteFactory_) {
        dwriteFactory_ = nullptr;
        return false;
    }

    D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND
    );
    d2dFactory_->CreateStrokeStyle(&strokeProps, nullptr, 0, &roundStroke_);
    BuildCachedGeometries();

    hasVariableText_ = HasFontFamily(L"Segoe UI Variable Text");
    hasVariableDisplay_ = HasFontFamily(L"Segoe UI Variable Display");
    return true;
}

bool FluentGraphics::EnsureGpuInitialized() {
    if (!EnsureInitialized()) return false;
    if (gpuAttempted_) return gpuAvailable_;
    gpuAttempted_ = true;

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3
    };
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &d3dDevice_,
        nullptr,
        nullptr
    );
    if (FAILED(hr) || !d3dDevice_) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            &d3dDevice_,
            nullptr,
            nullptr
        );
    }
    if (FAILED(hr) || !d3dDevice_) {
        ReleaseGpuDevices();
        return false;
    }

    hr = d3dDevice_->QueryInterface(kIidIDXGIDevice, reinterpret_cast<void**>(&dxgiDevice_));
    if (FAILED(hr) || !dxgiDevice_) {
        ReleaseGpuDevices();
        return false;
    }

    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDevice_->GetAdapter(&adapter);
    if (FAILED(hr) || !adapter) {
        ReleaseGpuDevices();
        return false;
    }
    hr = adapter->GetParent(kIidIDXGIFactory2, reinterpret_cast<void**>(&dxgiFactory_));
    adapter->Release();
    if (FAILED(hr) || !dxgiFactory_) {
        ReleaseGpuDevices();
        return false;
    }

    hr = d2dFactory_->CreateDevice(dxgiDevice_, &d2dDevice_);
    if (FAILED(hr) || !d2dDevice_) {
        ReleaseGpuDevices();
        return false;
    }

    hr = DCompositionCreateDevice(dxgiDevice_, kIidIDCompositionDevice, reinterpret_cast<void**>(&dcompDevice_));
    if (FAILED(hr) || !dcompDevice_) {
        ReleaseGpuDevices();
        return false;
    }

    gpuAvailable_ = true;
    return true;
}

bool FluentGraphics::IsGpuAvailable() {
    return EnsureGpuInitialized();
}

void FluentGraphics::ReleaseGpuDevices() {
    if (dcompDevice_) { dcompDevice_->Release(); dcompDevice_ = nullptr; }
    if (d2dDevice_)   { d2dDevice_->Release();   d2dDevice_ = nullptr; }
    if (dxgiFactory_) { dxgiFactory_->Release(); dxgiFactory_ = nullptr; }
    if (dxgiDevice_)  { dxgiDevice_->Release();  dxgiDevice_ = nullptr; }
    if (d3dDevice_)   { d3dDevice_->Release();   d3dDevice_ = nullptr; }
    gpuAvailable_ = false;
}

void FluentGraphics::InvalidateGpuDevice() {
    ReleaseGpuDevices();
    gpuAttempted_ = false;
}

void FluentGraphics::Shutdown() {
    ReleaseGpuDevices();
    gpuAttempted_ = false;
    ReleaseCachedGeometries();
    if (roundStroke_) {
        roundStroke_->Release();
        roundStroke_ = nullptr;
    }
    if (dwriteFactory_) {
        dwriteFactory_->Release();
        dwriteFactory_ = nullptr;
    }
    if (d2dFactory_) {
        d2dFactory_->Release();
        d2dFactory_ = nullptr;
    }
    initialized_ = false;
}

IDWriteTextFormat* FluentGraphics::CreateTextFormat(
    float dipSize,
    DWRITE_FONT_WEIGHT weight,
    bool display,
    DWRITE_TEXT_ALIGNMENT hAlign,
    DWRITE_PARAGRAPH_ALIGNMENT vAlign,
    DWRITE_FONT_STRETCH stretch
) {
    if (!EnsureInitialized() || !dwriteFactory_) return nullptr;

    const wchar_t* family = L"Segoe UI";
    if (display && hasVariableDisplay_) {
        family = L"Segoe UI Variable Display";
    } else if (hasVariableText_) {
        family = L"Segoe UI Variable Text";
    }

    IDWriteTextFormat* fmt = nullptr;
    HRESULT hr = dwriteFactory_->CreateTextFormat(
        family,
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        stretch,
        dipSize,
        L"ru-RU",
        &fmt
    );
    if (FAILED(hr) || !fmt) return nullptr;

    fmt->SetTextAlignment(hAlign);
    fmt->SetParagraphAlignment(vAlign);
    return fmt;
}

// ============================================================================
// Software D2DSurface (used for tray icons or non-GPU fallback)
// ============================================================================

D2DSurface::~D2DSurface() {
    Release();
}

void D2DSurface::ReleaseBitmap() {
    if (hdcMem_) {
        if (hbmOld_) {
            SelectObject(hdcMem_, hbmOld_);
            hbmOld_ = nullptr;
        }
        if (hbm_) {
            DeleteObject(hbm_);
            hbm_ = nullptr;
        }
        DeleteDC(hdcMem_);
        hdcMem_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
}

void D2DSurface::ReleaseRenderTarget() {
    if (brush_) {
        brush_->Release();
        brush_ = nullptr;
    }
    if (rt_) {
        rt_->Release();
        rt_ = nullptr;
    }
}

void D2DSurface::Release() {
    ReleaseRenderTarget();
    ReleaseBitmap();
}

bool D2DSurface::EnsureSize(HDC refDC, int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (!FluentGraphics::Instance().EnsureInitialized()) return false;

    if (!rt_) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f,
            96.0f,
            D2D1_RENDER_TARGET_USAGE_NONE,
            D2D1_FEATURE_LEVEL_DEFAULT
        );
        HRESULT hr = FluentGraphics::Instance().D2DFactory()->CreateDCRenderTarget(&props, &rt_);
        if (FAILED(hr) || !rt_) {
            rt_ = nullptr;
            return false;
        }
        rt_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
    }

    if (hdcMem_ && hbm_ && width_ == width && height_ == height) {
        return true;
    }

    ReleaseBitmap();

    HDC baseDC = refDC ? refDC : GetDC(nullptr);
    hdcMem_ = CreateCompatibleDC(baseDC);
    if (!refDC && baseDC) ReleaseDC(nullptr, baseDC);
    if (!hdcMem_) return false;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    hbm_ = CreateDIBSection(hdcMem_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm_) {
        ReleaseBitmap();
        return false;
    }

    hbmOld_ = static_cast<HBITMAP>(SelectObject(hdcMem_, hbm_));
    width_ = width;
    height_ = height;
    return true;
}

bool D2DSurface::BeginDraw(float dpiScale) {
    if (!rt_ || !hdcMem_ || width_ <= 0 || height_ <= 0) return false;

    RECT rc = { 0, 0, width_, height_ };
    if (FAILED(rt_->BindDC(hdcMem_, &rc))) {
        return false;
    }

    rt_->BeginDraw();
    rt_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    rt_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    rt_->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale, dpiScale));
    drawing_ = true;
    return true;
}

HRESULT D2DSurface::EndDrawAndBlit(HDC dstDC) {
    if (!rt_ || !drawing_) return E_FAIL;

    rt_->SetTransform(D2D1::Matrix3x2F::Identity());
    HRESULT hr = rt_->EndDraw();
    drawing_ = false;

    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseRenderTarget();
        return hr;
    }

    if (SUCCEEDED(hr) && dstDC && hdcMem_) {
        BitBlt(dstDC, 0, 0, width_, height_, hdcMem_, 0, 0, SRCCOPY);
    }
    return hr;
}

// ============================================================================
// GPU Direct2D 1.1 + DirectComposition Surface
// ============================================================================

GpuSurface::~GpuSurface() {
    Release();
}

void GpuSurface::ReleaseTargetBitmap() {
    if (dc_) {
        dc_->SetTarget(nullptr);
    }
    if (targetBmp_) {
        targetBmp_->Release();
        targetBmp_ = nullptr;
    }
}

void GpuSurface::ReleaseGpu() {
    ReleaseTargetBitmap();
    if (brush_)       { brush_->Release();       brush_ = nullptr; }
    if (dc_)          { dc_->Release();          dc_ = nullptr; }
    if (dcompVisual_) { dcompVisual_->Release(); dcompVisual_ = nullptr; }
    if (dcompTarget_) { dcompTarget_->Release(); dcompTarget_ = nullptr; }
    if (swapChain_)   { swapChain_->Release();   swapChain_ = nullptr; }
    usingGpu_ = false;
}

void GpuSurface::Release() {
    ReleaseGpu();
    fallback_.Release();
    hWnd_ = nullptr;
    width_ = 0;
    height_ = 0;
    drawing_ = false;
}

bool GpuSurface::BindTargetBitmap() {
    if (!swapChain_ || !dc_) return false;
    ReleaseTargetBitmap();

    IDXGISurface* surface = nullptr;
    HRESULT hr = swapChain_->GetBuffer(0, kIidIDXGISurface, reinterpret_cast<void**>(&surface));
    if (FAILED(hr) || !surface) return false;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f
    );
    hr = dc_->CreateBitmapFromDxgiSurface(surface, &props, &targetBmp_);
    surface->Release();
    if (FAILED(hr) || !targetBmp_) {
        targetBmp_ = nullptr;
        return false;
    }

    dc_->SetTarget(targetBmp_);
    return true;
}

bool GpuSurface::InitSwapChain(HWND hWnd, int width, int height) {
    auto& gfx = FluentGraphics::Instance();
    if (!gfx.EnsureGpuInitialized()) return false;

    HRESULT hr = gfx.D2DDevice()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc_);
    if (FAILED(hr) || !dc_) {
        ReleaseGpu();
        return false;
    }
    dc_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags = 0;

    hr = gfx.DXGIFactory()->CreateSwapChainForComposition(gfx.DXGIDevice(), &desc, nullptr, &swapChain_);
    if (FAILED(hr) || !swapChain_) {
        ReleaseGpu();
        return false;
    }

    if (!BindTargetBitmap()) {
        ReleaseGpu();
        return false;
    }

    hr = gfx.DCompDevice()->CreateTargetForHwnd(hWnd, TRUE, &dcompTarget_);
    if (FAILED(hr) || !dcompTarget_) {
        ReleaseGpu();
        return false;
    }

    hr = gfx.DCompDevice()->CreateVisual(&dcompVisual_);
    if (FAILED(hr) || !dcompVisual_) {
        ReleaseGpu();
        return false;
    }

    dcompVisual_->SetContent(swapChain_);
    dcompTarget_->SetRoot(dcompVisual_);
    gfx.DCompDevice()->Commit();

    usingGpu_ = true;
    width_ = width;
    height_ = height;
    return true;
}

bool GpuSurface::EnsureSize(HWND hWnd, int width, int height) {
    if (!hWnd || width <= 0 || height <= 0) return false;
    hWnd_ = hWnd;

    if (!usingGpu_) {
        if (InitSwapChain(hWnd, width, height)) {
            return true;
        }
        // Fallback to software D2DSurface
        HDC dc = GetDC(hWnd);
        bool ok = fallback_.EnsureSize(dc, width, height);
        if (dc) ReleaseDC(hWnd, dc);
        if (ok) {
            width_ = width;
            height_ = height;
        }
        return ok;
    }

    if (width_ == width && height_ == height && targetBmp_) {
        return true;
    }

    ReleaseTargetBitmap();
    HRESULT hr = swapChain_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
        DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        ReleaseGpu();
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            FluentGraphics::Instance().InvalidateGpuDevice();
        }
        return InitSwapChain(hWnd, width, height);
    }

    if (!BindTargetBitmap()) {
        ReleaseGpu();
        return false;
    }
    FluentGraphics::Instance().DCompDevice()->Commit();
    width_ = width;
    height_ = height;
    return true;
}

ID2D1RenderTarget* GpuSurface::BeginDraw(float dpiScale, HDC fallbackRefDC) {
    if (usingGpu_) {
        if (!dc_ || !targetBmp_) return nullptr;
        dc_->BeginDraw();
        dc_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        dc_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        dc_->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale, dpiScale));
        drawing_ = true;
        return dc_;
    }

    if (width_ > 0 && height_ > 0) {
        if (!fallback_.EnsureSize(fallbackRefDC, width_, height_)) return nullptr;
        if (!fallback_.BeginDraw(dpiScale)) return nullptr;
        drawing_ = true;
        return fallback_.RT();
    }
    return nullptr;
}

HRESULT GpuSurface::EndDrawAndPresent(HDC fallbackDstDC) {
    if (!drawing_) return E_FAIL;
    drawing_ = false;

    if (usingGpu_) {
        dc_->SetTransform(D2D1::Matrix3x2F::Identity());
        HRESULT hr = dc_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            ReleaseGpu();
            FluentGraphics::Instance().InvalidateGpuDevice();
            return hr;
        }
        if (SUCCEEDED(hr) && swapChain_) {
            hr = swapChain_->Present(0, 0);
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                ReleaseGpu();
                FluentGraphics::Instance().InvalidateGpuDevice();
            }
        }
        return hr;
    }

    return fallback_.EndDrawAndBlit(fallbackDstDC);
}

// ============================================================================
// Vector Icon Renderer
// ============================================================================

void DrawFluentIcon(
    ID2D1RenderTarget* rt,
    FluentIcon icon,
    const D2D1_RECT_F& rect,
    const D2D1_COLOR_F& color,
    float strokeWidth,
    ID2D1SolidColorBrush* reuseBrush
) {
    if (!rt || icon == FluentIcon::None) return;

    float w = rect.right - rect.left;
    float h = rect.bottom - rect.top;
    float s = (std::min)(w, h) / 16.0f;
    if (s <= 0.0f) return;

    float ox = rect.left + (w - 16.0f * s) * 0.5f;
    float oy = rect.top + (h - 16.0f * s) * 0.5f;
    auto pt = [&](float x, float y) -> D2D1_POINT_2F {
        return D2D1::Point2F(ox + x * s, oy + y * s);
    };
    auto r = [&](float x1, float y1, float x2, float y2) -> D2D1_RECT_F {
        return D2D1::RectF(ox + x1 * s, oy + y1 * s, ox + x2 * s, oy + y2 * s);
    };

    ID2D1SolidColorBrush* brush = reuseBrush;
    bool ownedBrush = false;
    if (brush) {
        brush->SetColor(color);
    } else {
        if (FAILED(rt->CreateSolidColorBrush(color, &brush)) || !brush) return;
        ownedBrush = true;
    }

    auto& gfx = FluentGraphics::Instance();
    ID2D1StrokeStyle* round = gfx.RoundStrokeStyle();
    float sw = strokeWidth * s;

    auto drawCachedGeo = [&](ID2D1PathGeometry* geo, bool fill) {
        if (!geo) return;
        D2D1_MATRIX_3X2_F prevTransform{};
        rt->GetTransform(&prevTransform);
        rt->SetTransform(D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(ox, oy) * prevTransform);
        if (fill) {
            rt->FillGeometry(geo, brush);
        } else {
            rt->DrawGeometry(geo, brush, strokeWidth, round);
        }
        rt->SetTransform(prevTransform);
    };

    switch (icon) {
    case FluentIcon::Settings: {
        rt->DrawEllipse(D2D1::Ellipse(pt(8.0f, 8.0f), 4.2f * s, 4.2f * s), brush, sw, round);
        rt->DrawEllipse(D2D1::Ellipse(pt(8.0f, 8.0f), 1.8f * s, 1.8f * s), brush, sw, round);
        constexpr float kPi = 3.14159265f;
        for (int i = 0; i < 6; ++i) {
            float ang = i * (kPi / 3.0f);
            float dx = std::cos(ang);
            float dy = std::sin(ang);
            rt->DrawLine(pt(8.0f + dx * 4.4f, 8.0f + dy * 4.4f), pt(8.0f + dx * 6.3f, 8.0f + dy * 6.3f), brush, sw * 1.15f, round);
        }
        break;
    }
    case FluentIcon::AutoSwitch: {
        rt->DrawLine(pt(3.0f, 5.2f), pt(12.6f, 5.2f), brush, sw, round);
        rt->DrawLine(pt(10.0f, 2.7f), pt(12.8f, 5.2f), brush, sw, round);
        rt->DrawLine(pt(10.0f, 7.7f), pt(12.8f, 5.2f), brush, sw, round);
        rt->DrawLine(pt(13.0f, 10.8f), pt(3.4f, 10.8f), brush, sw, round);
        rt->DrawLine(pt(6.0f, 8.3f), pt(3.2f, 10.8f), brush, sw, round);
        rt->DrawLine(pt(6.0f, 13.3f), pt(3.2f, 10.8f), brush, sw, round);
        break;
    }
    case FluentIcon::CapsLock: {
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(2.0f, 2.0f, 14.0f, 14.0f), 2.5f * s, 2.5f * s), brush, sw, round);
        rt->DrawLine(pt(8.0f, 4.6f), pt(5.0f, 7.8f), brush, sw, round);
        rt->DrawLine(pt(8.0f, 4.6f), pt(11.0f, 7.8f), brush, sw, round);
        rt->DrawLine(pt(8.0f, 5.0f), pt(8.0f, 10.6f), brush, sw, round);
        rt->DrawLine(pt(5.2f, 11.8f), pt(10.8f, 11.8f), brush, sw, round);
        break;
    }
    case FluentIcon::DoubleCaps: {
        rt->DrawLine(pt(4.0f, 7.5f), pt(8.0f, 3.6f), brush, sw, round);
        rt->DrawLine(pt(8.0f, 3.6f), pt(12.0f, 7.5f), brush, sw, round);
        rt->DrawLine(pt(4.0f, 11.8f), pt(8.0f, 7.9f), brush, sw, round);
        rt->DrawLine(pt(8.0f, 7.9f), pt(12.0f, 11.8f), brush, sw, round);
        rt->DrawLine(pt(4.5f, 13.8f), pt(11.5f, 13.8f), brush, sw, round);
        break;
    }
    case FluentIcon::Shield: {
        drawCachedGeo(gfx.ShieldGeometry(), false);
        rt->DrawLine(pt(5.6f, 8.2f), pt(7.3f, 9.9f), brush, sw, round);
        rt->DrawLine(pt(7.3f, 9.9f), pt(10.6f, 6.4f), brush, sw, round);
        break;
    }
    case FluentIcon::Rocket: {
        rt->DrawEllipse(D2D1::Ellipse(pt(8.0f, 8.0f), 5.6f * s, 5.6f * s), brush, sw, round);
        rt->DrawLine(pt(8.0f, 11.2f), pt(8.0f, 4.8f), brush, sw, round);
        rt->DrawLine(pt(5.5f, 7.4f), pt(8.0f, 4.8f), brush, sw, round);
        rt->DrawLine(pt(10.5f, 7.4f), pt(8.0f, 4.8f), brush, sw, round);
        break;
    }
    case FluentIcon::Overview: {
        float rad = 1.1f * s;
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(2.5f, 2.5f, 6.8f, 6.8f), rad, rad), brush, sw, round);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(9.2f, 2.5f, 13.5f, 6.8f), rad, rad), brush, sw, round);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(2.5f, 9.2f, 6.8f, 13.5f), rad, rad), brush, sw, round);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(9.2f, 9.2f, 13.5f, 13.5f), rad, rad), brush, sw, round);
        break;
    }
    case FluentIcon::Apps: {
        float rad = 1.0f * s;
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(2.5f, 3.5f, 6.7f, 7.7f), rad, rad), brush, sw, round);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(2.5f, 9.3f, 6.7f, 13.5f), rad, rad), brush, sw, round);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(8.3f, 9.3f, 12.5f, 13.5f), rad, rad), brush, sw, round);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(9.7f, 2.0f, 13.9f, 6.2f), rad, rad), brush, sw, round);
        break;
    }
    case FluentIcon::Block: {
        rt->DrawEllipse(D2D1::Ellipse(pt(8.0f, 8.0f), 5.6f * s, 5.6f * s), brush, sw, round);
        rt->DrawLine(pt(4.1f, 4.1f), pt(11.9f, 11.9f), brush, sw, round);
        break;
    }
    case FluentIcon::Pause: {
        rt->FillRoundedRectangle(D2D1::RoundedRect(r(4.3f, 3.0f, 6.7f, 13.0f), 0.55f * s, 0.55f * s), brush);
        rt->FillRoundedRectangle(D2D1::RoundedRect(r(9.3f, 3.0f, 11.7f, 13.0f), 0.55f * s, 0.55f * s), brush);
        break;
    }
    case FluentIcon::Play: {
        drawCachedGeo(gfx.PlayGeometry(), true);
        break;
    }
    case FluentIcon::Power: {
        rt->DrawLine(pt(8.0f, 2.2f), pt(8.0f, 7.6f), brush, sw, round);
        drawCachedGeo(gfx.PowerGeometry(), false);
        break;
    }
    case FluentIcon::Plus: {
        rt->DrawLine(pt(8.0f, 3.0f), pt(8.0f, 13.0f), brush, sw, round);
        rt->DrawLine(pt(3.0f, 8.0f), pt(13.0f, 8.0f), brush, sw, round);
        break;
    }
    case FluentIcon::Trash: {
        rt->DrawLine(pt(3.0f, 4.2f), pt(13.0f, 4.2f), brush, sw, round);
        rt->DrawLine(pt(6.2f, 2.5f), pt(9.8f, 2.5f), brush, sw, round);
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(4.4f, 4.2f, 11.6f, 13.4f), 1.4f * s, 1.4f * s), brush, sw, round);
        rt->DrawLine(pt(6.8f, 6.8f), pt(6.8f, 11.0f), brush, sw, round);
        rt->DrawLine(pt(9.2f, 6.8f), pt(9.2f, 11.0f), brush, sw, round);
        break;
    }
    case FluentIcon::WindowPicker: {
        rt->DrawRoundedRectangle(D2D1::RoundedRect(r(2.2f, 2.8f, 13.8f, 13.2f), 2.0f * s, 2.0f * s), brush, sw, round);
        rt->DrawLine(pt(2.4f, 5.8f), pt(13.6f, 5.8f), brush, sw, round);
        rt->FillEllipse(D2D1::Ellipse(pt(4.5f, 4.3f), 0.7f * s, 0.7f * s), brush);
        rt->FillEllipse(D2D1::Ellipse(pt(6.6f, 4.3f), 0.7f * s, 0.7f * s), brush);
        break;
    }
    case FluentIcon::Folder: {
        drawCachedGeo(gfx.FolderGeometry(), false);
        break;
    }
    case FluentIcon::Close: {
        rt->DrawLine(pt(4.0f, 4.0f), pt(12.0f, 12.0f), brush, sw, round);
        rt->DrawLine(pt(12.0f, 4.0f), pt(4.0f, 12.0f), brush, sw, round);
        break;
    }
    case FluentIcon::Minimize: {
        rt->DrawLine(pt(3.5f, 8.0f), pt(12.5f, 8.0f), brush, sw, round);
        break;
    }
    case FluentIcon::ChevronDown: {
        rt->DrawLine(pt(3.6f, 6.0f), pt(8.0f, 10.4f), brush, sw, round);
        rt->DrawLine(pt(8.0f, 10.4f), pt(12.4f, 6.0f), brush, sw, round);
        break;
    }
    case FluentIcon::Check: {
        rt->DrawLine(pt(3.2f, 8.4f), pt(6.5f, 11.7f), brush, sw, round);
        rt->DrawLine(pt(6.5f, 11.7f), pt(12.8f, 4.6f), brush, sw, round);
        break;
    }
    case FluentIcon::CheckCircle: {
        rt->DrawEllipse(D2D1::Ellipse(pt(8.0f, 8.0f), 5.8f * s, 5.8f * s), brush, sw, round);
        rt->DrawLine(pt(5.4f, 8.2f), pt(7.2f, 10.0f), brush, sw, round);
        rt->DrawLine(pt(7.2f, 10.0f), pt(10.8f, 6.2f), brush, sw, round);
        break;
    }
    case FluentIcon::Moon: {
        drawCachedGeo(gfx.MoonGeometry(), false);
        break;
    }
    case FluentIcon::Sun: {
        rt->DrawEllipse(D2D1::Ellipse(pt(8.0f, 8.0f), 2.9f * s, 2.9f * s), brush, sw, round);
        constexpr float kPi = 3.14159265f;
        for (int i = 0; i < 8; ++i) {
            float ang = i * (kPi / 4.0f);
            float dx = std::cos(ang);
            float dy = std::sin(ang);
            rt->DrawLine(pt(8.0f + dx * 4.8f, 8.0f + dy * 4.8f), pt(8.0f + dx * 6.2f, 8.0f + dy * 6.2f), brush, sw, round);
        }
        break;
    }
    case FluentIcon::Swap: {
        rt->DrawLine(pt(3.0f, 5.5f), pt(12.5f, 5.5f), brush, sw, round);
        rt->DrawLine(pt(10.0f, 3.2f), pt(12.8f, 5.5f), brush, sw, round);
        rt->DrawLine(pt(13.0f, 10.5f), pt(3.5f, 10.5f), brush, sw, round);
        rt->DrawLine(pt(6.0f, 12.8f), pt(3.2f, 10.5f), brush, sw, round);
        break;
    }
    default:
        break;
    }

    if (ownedBrush && brush) {
        brush->Release();
    }
}

} // namespace UI
