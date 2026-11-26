#pragma once

#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <cstdint>
#include <string>

namespace UI {

enum class BackdropKind {
    None,
    Mica,
    MicaAlt,
    Acrylic
};

enum class FluentIcon {
    None = 0,
    Settings,
    Overview,
    AutoSwitch,
    CapsLock,
    DoubleCaps,
    Shield,
    Rocket,
    Apps,
    Block,
    Pause,
    Play,
    Power,
    Plus,
    Trash,
    WindowPicker,
    Folder,
    Close,
    Minimize,
    ChevronDown,
    Check,
    CheckCircle,
    Swap,
    Moon,
    Sun
};

struct FluentPalette {
    bool darkMode = true;

    D2D1_COLOR_F shellFallbackBg{};
    D2D1_COLOR_F menuFallbackBg{};
    D2D1_COLOR_F sidebarBg{};
    D2D1_COLOR_F footerBg{};
    D2D1_COLOR_F workspaceBg{};
    D2D1_COLOR_F bannerBg{};
    D2D1_COLOR_F bannerPausedBg{};
    D2D1_COLOR_F bannerIconBoxBg{};
    D2D1_COLOR_F cardBg{};
    D2D1_COLOR_F cardHoverBg{};
    D2D1_COLOR_F cardPressedBg{};
    D2D1_COLOR_F rowHoverBg{};
    D2D1_COLOR_F rowPressedBg{};
    D2D1_COLOR_F surfaceElevated{};
    D2D1_COLOR_F navItemHover{};
    D2D1_COLOR_F navItemSelected{};
    D2D1_COLOR_F navSelectedBg{};
    D2D1_COLOR_F navSelectedFg{};
    D2D1_COLOR_F navHoverBg{};
    D2D1_COLOR_F inputBg{};
    D2D1_COLOR_F inputFocusBg{};
    D2D1_COLOR_F borderSubtle{};
    D2D1_COLOR_F borderStrong{};
    D2D1_COLOR_F borderTopHighlight{};
    D2D1_COLOR_F divider{};

    D2D1_COLOR_F textPrimary{};
    D2D1_COLOR_F textSecondary{};
    D2D1_COLOR_F textTertiary{};

    D2D1_COLOR_F accent{};
    D2D1_COLOR_F accentHover{};
    D2D1_COLOR_F accentPressed{};
    D2D1_COLOR_F accentText{};
    D2D1_COLOR_F accentSoft{};

    D2D1_COLOR_F toggleOnTrack{};
    D2D1_COLOR_F toggleOnHover{};
    D2D1_COLOR_F toggleOnKnob{};
    D2D1_COLOR_F toggleOffTrack{};
    D2D1_COLOR_F toggleOffBorder{};
    D2D1_COLOR_F toggleOffKnob{};

    D2D1_COLOR_F langEnFill{};
    D2D1_COLOR_F langEnSoft{};

    D2D1_COLOR_F langRuFill{};
    D2D1_COLOR_F langRuSoft{};

    D2D1_COLOR_F statusActive{};
    D2D1_COLOR_F statusActiveSoft{};
    D2D1_COLOR_F statusPaused{};
    D2D1_COLOR_F statusPausedSoft{};

    D2D1_COLOR_F danger{};
    D2D1_COLOR_F dangerHoverBg{};
    D2D1_COLOR_F dangerSoft{};
};

bool IsSystemDarkMode();
bool IsTaskbarDarkMode();
FluentPalette GetFluentPalette(bool dark);
bool ApplyWindowBackdrop(HWND hWnd, BackdropKind kind, bool dark);
UINT GetWindowDpiSafe(HWND hWnd);

float Clamp01(float v);
float Lerp(float a, float b, float t);
float EaseOutCubic(float t);
float EaseOutQuint(float t);
float ExpDecay(float current, float target, float speed, float dt);
D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t);
D2D1_COLOR_F WithAlpha(const D2D1_COLOR_F& c, float alpha);
inline D2D1_COLOR_F FadeColor(const D2D1_COLOR_F& c, float alpha) {
    return D2D1::ColorF(c.r, c.g, c.b, Clamp01(c.a * alpha));
}

inline D2D1_COLOR_F Rgba(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f) {
    return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

class FluentGraphics {
public:
    static FluentGraphics& Instance();

    bool EnsureInitialized();
    bool EnsureGpuInitialized();
    bool IsGpuAvailable();
    void InvalidateGpuDevice();
    void Shutdown();

    ID2D1Factory1* D2DFactory() const { return d2dFactory_; }
    ID2D1Device* D2DDevice() const { return d2dDevice_; }
    IDXGIDevice* DXGIDevice() const { return dxgiDevice_; }
    IDXGIFactory2* DXGIFactory() const { return dxgiFactory_; }
    IDCompositionDevice* DCompDevice() const { return dcompDevice_; }
    IDWriteFactory* DWriteFactory() const { return dwriteFactory_; }
    ID2D1StrokeStyle* RoundStrokeStyle() const { return roundStroke_; }

    ID2D1PathGeometry* ShieldGeometry() const { return shieldGeo_; }
    ID2D1PathGeometry* PlayGeometry() const { return playGeo_; }
    ID2D1PathGeometry* PowerGeometry() const { return powerGeo_; }
    ID2D1PathGeometry* FolderGeometry() const { return folderGeo_; }
    ID2D1PathGeometry* MoonGeometry() const { return moonGeo_; }

    IDWriteTextFormat* CreateTextFormat(
        float dipSize,
        DWRITE_FONT_WEIGHT weight,
        bool display = false,
        DWRITE_TEXT_ALIGNMENT hAlign = DWRITE_TEXT_ALIGNMENT_LEADING,
        DWRITE_PARAGRAPH_ALIGNMENT vAlign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL
    );

private:
    FluentGraphics() = default;
    ~FluentGraphics();

    bool HasFontFamily(const wchar_t* familyName);
    void BuildCachedGeometries();
    void ReleaseCachedGeometries();
    void ReleaseGpuDevices();

    ID2D1Factory1* d2dFactory_ = nullptr;
    IDWriteFactory* dwriteFactory_ = nullptr;
    ID2D1StrokeStyle* roundStroke_ = nullptr;

    ID3D11Device* d3dDevice_ = nullptr;
    IDXGIDevice* dxgiDevice_ = nullptr;
    IDXGIFactory2* dxgiFactory_ = nullptr;
    ID2D1Device* d2dDevice_ = nullptr;
    IDCompositionDevice* dcompDevice_ = nullptr;

    ID2D1PathGeometry* shieldGeo_ = nullptr;
    ID2D1PathGeometry* playGeo_ = nullptr;
    ID2D1PathGeometry* powerGeo_ = nullptr;
    ID2D1PathGeometry* folderGeo_ = nullptr;
    ID2D1PathGeometry* moonGeo_ = nullptr;

    bool initialized_ = false;
    bool gpuAttempted_ = false;
    bool gpuAvailable_ = false;
    bool hasVariableText_ = false;
    bool hasVariableDisplay_ = false;
};

class D2DSurface {
public:
    D2DSurface() = default;
    ~D2DSurface();

    D2DSurface(const D2DSurface&) = delete;
    D2DSurface& operator=(const D2DSurface&) = delete;

    bool EnsureSize(HDC refDC, int width, int height);
    bool BeginDraw(float dpiScale);
    HRESULT EndDrawAndBlit(HDC dstDC);
    void Release();

    ID2D1DCRenderTarget* RT() const { return rt_; }
    ID2D1SolidColorBrush* Brush() const { return brush_; }
    int Width() const { return width_; }
    int Height() const { return height_; }

private:
    void ReleaseBitmap();
    void ReleaseRenderTarget();

    HDC hdcMem_ = nullptr;
    HBITMAP hbm_ = nullptr;
    HBITMAP hbmOld_ = nullptr;
    ID2D1DCRenderTarget* rt_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool drawing_ = false;
};

class GpuSurface {
public:
    GpuSurface() = default;
    ~GpuSurface();

    GpuSurface(const GpuSurface&) = delete;
    GpuSurface& operator=(const GpuSurface&) = delete;

    bool EnsureSize(HWND hWnd, int width, int height);
    ID2D1RenderTarget* BeginDraw(float dpiScale, HDC fallbackRefDC = nullptr);
    HRESULT EndDrawAndPresent(HDC fallbackDstDC = nullptr);
    void Release();

    bool IsGpu() const { return usingGpu_; }
    ID2D1SolidColorBrush* Brush() const { return usingGpu_ ? brush_ : fallback_.Brush(); }
    int Width() const { return width_; }
    int Height() const { return height_; }

private:
    bool InitSwapChain(HWND hWnd, int width, int height);
    bool BindTargetBitmap();
    void ReleaseTargetBitmap();
    void ReleaseGpu();

    HWND hWnd_ = nullptr;
    IDXGISwapChain1* swapChain_ = nullptr;
    ID2D1DeviceContext* dc_ = nullptr;
    ID2D1Bitmap1* targetBmp_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    IDCompositionTarget* dcompTarget_ = nullptr;
    IDCompositionVisual* dcompVisual_ = nullptr;

    D2DSurface fallback_;
    int width_ = 0;
    int height_ = 0;
    bool usingGpu_ = false;
    bool drawing_ = false;
};

void DrawFluentIcon(
    ID2D1RenderTarget* rt,
    FluentIcon icon,
    const D2D1_RECT_F& rect,
    const D2D1_COLOR_F& color,
    float strokeWidth = 1.45f,
    ID2D1SolidColorBrush* reuseBrush = nullptr
);

} // namespace UI
