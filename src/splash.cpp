#include "splash.h"
#include "resource.h"

#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kSplashClassName[] =
    L"TocPilotStartupSplash";
constexpr UINT_PTR kSplashTimerId = 1;
constexpr UINT kSplashTimerMs = 420;
constexpr int kSplashWidth = 820;
constexpr int kSplashHeight = 575;

constexpr char kPlaqueBase64[] =
#include "splash_plaque_00.inc"
#include "splash_plaque_01.inc"
#include "splash_plaque_02.inc"
;

HWND g_splashWindow = nullptr;
HWND g_splashMainWindow = nullptr;
ULONG_PTR g_gdiplusToken = 0;
std::unique_ptr<Gdiplus::Bitmap> g_logo;
std::unique_ptr<Gdiplus::Bitmap> g_plaque;
StartupSplashPhase g_phase =
    StartupSplashPhase::CheckingAppUpdate;
int g_dotCount = 1;

int Base64Value(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 26;
    }
    if (ch >= '0' && ch <= '9') {
        return ch - '0' + 52;
    }
    if (ch == '+') {
        return 62;
    }
    if (ch == '/') {
        return 63;
    }
    return -1;
}

bool DecodeBase64(
    std::string_view encoded,
    std::vector<BYTE>& bytes) {
    bytes.clear();
    bytes.reserve(
        encoded.size() / 4 * 3);

    std::uint32_t accumulator = 0;
    int bits = 0;

    for (const char ch : encoded) {
        if (ch == '=') {
            break;
        }

        const int value =
            Base64Value(ch);

        if (value < 0) {
            continue;
        }

        accumulator =
            (accumulator << 6) |
            static_cast<std::uint32_t>(
                value);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(
                static_cast<BYTE>(
                    (accumulator >> bits) &
                    0xffu));
        }
    }

    return !bytes.empty();
}

std::unique_ptr<Gdiplus::Bitmap>
LoadBitmapFromBytes(
    const BYTE* bytes,
    std::size_t byteCount) {
    if (!bytes ||
        byteCount == 0) {
        return {};
    }

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            byteCount);

    if (!memory) {
        return {};
    }

    void* data =
        GlobalLock(memory);

    if (!data) {
        GlobalFree(memory);
        return {};
    }

    std::copy(
        bytes,
        bytes + byteCount,
        static_cast<BYTE*>(data));
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(
            CreateStreamOnHGlobal(
                memory,
                TRUE,
                &stream))) {
        GlobalFree(memory);
        return {};
    }

    Gdiplus::Bitmap source(stream);

    if (source.GetLastStatus() !=
            Gdiplus::Ok ||
        source.GetWidth() == 0 ||
        source.GetHeight() == 0) {
        stream->Release();
        return {};
    }

    auto copy =
        std::make_unique<Gdiplus::Bitmap>(
            static_cast<INT>(
                source.GetWidth()),
            static_cast<INT>(
                source.GetHeight()),
            PixelFormat32bppPARGB);

    if (!copy ||
        copy->GetLastStatus() !=
            Gdiplus::Ok) {
        stream->Release();
        return {};
    }

    {
        Gdiplus::Graphics graphics(
            copy.get());

        graphics.SetCompositingMode(
            Gdiplus::CompositingModeSourceCopy);
        graphics.Clear(
            Gdiplus::Color(
                0,
                0,
                0,
                0));
        graphics.DrawImage(
            &source,
            0,
            0,
            static_cast<INT>(
                source.GetWidth()),
            static_cast<INT>(
                source.GetHeight()));
    }

    stream->Release();
    return copy;
}

std::unique_ptr<Gdiplus::Bitmap>
LoadBitmapFromBase64(
    std::string_view encoded) {
    std::vector<BYTE> bytes;
    if (!DecodeBase64(
            encoded,
            bytes)) {
        return {};
    }

    return LoadBitmapFromBytes(
        bytes.data(),
        bytes.size());
}

std::unique_ptr<Gdiplus::Bitmap>
LoadBitmapFromResource(
    HINSTANCE instance,
    int resourceId) {
    const HRSRC resource =
        FindResourceW(
            instance,
            MAKEINTRESOURCEW(
                resourceId),
            RT_RCDATA);

    if (!resource) {
        return {};
    }

    const DWORD byteCount =
        SizeofResource(
            instance,
            resource);

    if (byteCount == 0) {
        return {};
    }

    const HGLOBAL loaded =
        LoadResource(
            instance,
            resource);

    if (!loaded) {
        return {};
    }

    const void* data =
        LockResource(loaded);

    if (!data) {
        return {};
    }

    return LoadBitmapFromBytes(
        static_cast<const BYTE*>(
            data),
        static_cast<std::size_t>(
            byteCount));
}

std::wstring SplashStatusText() {
    switch (g_phase) {
    case StartupSplashPhase::CheckingAppUpdate:
    case StartupSplashPhase::ScanningAddonUpdates: {
        std::wstring text =
            g_phase ==
                    StartupSplashPhase::
                        CheckingAppUpdate
                ? L"Checking for TocPilot Update"
                : L"Scanning for Addon Updates";

        text.append(
            static_cast<std::size_t>(
                std::clamp(
                    g_dotCount,
                    1,
                    3)),
            L'.');
        return text;
    }

    case StartupSplashPhase::AwaitingContinue:
        return L"Click to continue!";
    }

    return {};
}

POINT SplashPosition() {
    POINT result{
        CW_USEDEFAULT,
        CW_USEDEFAULT
    };

    HMONITOR monitor =
        MonitorFromWindow(
            g_splashMainWindow,
            MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO info{};
    info.cbSize =
        sizeof(info);

    if (!GetMonitorInfoW(
            monitor,
            &info)) {
        return result;
    }

    result.x =
        info.rcWork.left +
        (info.rcWork.right -
         info.rcWork.left -
         kSplashWidth) /
            2;
    result.y =
        info.rcWork.top +
        (info.rcWork.bottom -
         info.rcWork.top -
         kSplashHeight) /
            2;

    return result;
}

void RenderSplash() {
    if (!g_splashWindow ||
        !g_logo ||
        !g_plaque) {
        return;
    }

    HDC screen =
        GetDC(nullptr);

    if (!screen) {
        return;
    }

    HDC memory =
        CreateCompatibleDC(
            screen);

    if (!memory) {
        ReleaseDC(
            nullptr,
            screen);
        return;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize =
        sizeof(
            bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth =
        kSplashWidth;
    bitmapInfo.bmiHeader.biHeight =
        -kSplashHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount =
        32;
    bitmapInfo.bmiHeader.biCompression =
        BI_RGB;

    void* pixels = nullptr;
    HBITMAP bitmap =
        CreateDIBSection(
            screen,
            &bitmapInfo,
            DIB_RGB_COLORS,
            &pixels,
            nullptr,
            0);

    if (!bitmap ||
        !pixels) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        DeleteDC(memory);
        ReleaseDC(
            nullptr,
            screen);
        return;
    }

    const HGDIOBJ oldBitmap =
        SelectObject(
            memory,
            bitmap);

    {
        Gdiplus::Bitmap target(
            kSplashWidth,
            kSplashHeight,
            kSplashWidth * 4,
            PixelFormat32bppPARGB,
            static_cast<BYTE*>(
                pixels));
        Gdiplus::Graphics graphics(
            &target);

        graphics.SetCompositingMode(
            Gdiplus::
                CompositingModeSourceCopy);
        graphics.Clear(
            Gdiplus::Color(
                0,
                0,
                0,
                0));

        graphics.SetCompositingMode(
            Gdiplus::
                CompositingModeSourceOver);
        graphics.SetCompositingQuality(
            Gdiplus::
                CompositingQualityHighQuality);
        graphics.SetInterpolationMode(
            Gdiplus::
                InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(
            Gdiplus::
                PixelOffsetModeHighQuality);
        graphics.SetSmoothingMode(
            Gdiplus::
                SmoothingModeHighQuality);

        graphics.DrawImage(
            g_plaque.get(),
            Gdiplus::Rect(
                115,
                218,
                590,
                295),
            0,
            0,
            static_cast<INT>(
                g_plaque->GetWidth()),
            static_cast<INT>(
                g_plaque->GetHeight()),
            Gdiplus::UnitPixel);

        graphics.DrawImage(
            g_logo.get(),
            Gdiplus::Rect(
                20,
                0,
                780,
                394),
            0,
            0,
            static_cast<INT>(
                g_logo->GetWidth()),
            static_cast<INT>(
                g_logo->GetHeight()),
            Gdiplus::UnitPixel);

        graphics.SetTextRenderingHint(
            Gdiplus::
                TextRenderingHintAntiAliasGridFit);

        const std::wstring text =
            SplashStatusText();

        Gdiplus::Font font(
            L"Georgia",
            27.0f,
            Gdiplus::FontStyleBold,
            Gdiplus::UnitPixel);

        Gdiplus::StringFormat format;
        format.SetLineAlignment(
            Gdiplus::
                StringAlignmentCenter);

        Gdiplus::RectF textRect(
            145.0f,
            355.0f,
            530.0f,
            66.0f);

        if (g_phase ==
                StartupSplashPhase::
                    AwaitingContinue) {
            format.SetAlignment(
                Gdiplus::
                    StringAlignmentCenter);
        } else {
            format.SetAlignment(
                Gdiplus::
                    StringAlignmentNear);

            const std::wstring longestText =
                g_phase ==
                        StartupSplashPhase::
                            CheckingAppUpdate
                    ? L"Checking for TocPilot Update..."
                    : L"Scanning for Addon Updates...";

            Gdiplus::RectF measuredText;
            if (graphics.MeasureString(
                    longestText.c_str(),
                    -1,
                    &font,
                    Gdiplus::PointF(
                        0.0f,
                        0.0f),
                    &measuredText) ==
                Gdiplus::Ok) {
                textRect.X =
                    (static_cast<Gdiplus::REAL>(
                         kSplashWidth) -
                     measuredText.Width) /
                    2.0f;
                textRect.Width =
                    measuredText.Width;
            }
        }

        Gdiplus::RectF shadowRect =
            textRect;
        shadowRect.X += 2.0f;
        shadowRect.Y += 2.0f;

        Gdiplus::SolidBrush shadow(
            Gdiplus::Color(
                190,
                32,
                18,
                8));
        Gdiplus::SolidBrush gold(
            Gdiplus::Color(
                255,
                244,
                195,
                82));

        graphics.DrawString(
            text.c_str(),
            -1,
            &font,
            shadowRect,
            &format,
            &shadow);
        graphics.DrawString(
            text.c_str(),
            -1,
            &font,
            textRect,
            &format,
            &gold);
    }

    POINT position =
        SplashPosition();
    POINT sourcePoint{0, 0};
    SIZE size{
        kSplashWidth,
        kSplashHeight
    };
    BLENDFUNCTION blend{};
    blend.BlendOp =
        AC_SRC_OVER;
    blend.SourceConstantAlpha =
        255;
    blend.AlphaFormat =
        AC_SRC_ALPHA;

    UpdateLayeredWindow(
        g_splashWindow,
        screen,
        &position,
        &size,
        memory,
        &sourcePoint,
        0,
        &blend,
        ULW_ALPHA);

    SelectObject(
        memory,
        oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(
        nullptr,
        screen);
}

void RevealMainWindow() {
    if (g_splashMainWindow &&
        IsWindow(
            g_splashMainWindow)) {
        ShowWindow(
            g_splashMainWindow,
            SW_SHOW);
        UpdateWindow(
            g_splashMainWindow);
        SetForegroundWindow(
            g_splashMainWindow);
    }
}

void CloseSplashForContinue() {
    if (g_phase !=
            StartupSplashPhase::
                AwaitingContinue) {
        return;
    }

    HWND splash =
        g_splashWindow;

    RevealMainWindow();

    if (splash &&
        IsWindow(splash)) {
        DestroyWindow(splash);
    }
}

LRESULT CALLBACK SplashWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam ==
                kSplashTimerId &&
            g_phase !=
                StartupSplashPhase::
                    AwaitingContinue) {
            g_dotCount =
                g_dotCount >= 3
                    ? 1
                    : g_dotCount + 1;
            RenderSplash();
        }
        return 0;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        CloseSplashForContinue();
        return 0;

    case WM_KEYDOWN:
        if (g_phase ==
                StartupSplashPhase::
                    AwaitingContinue &&
            (wParam == VK_RETURN ||
             wParam == VK_SPACE)) {
            CloseSplashForContinue();
        }
        return 0;

    case WM_SETCURSOR:
        if (g_phase ==
            StartupSplashPhase::
                AwaitingContinue) {
            SetCursor(
                LoadCursorW(
                    nullptr,
                    IDC_HAND));
            return TRUE;
        }
        break;

    case WM_NCHITTEST:
        return HTCLIENT;

    case WM_DESTROY:
        KillTimer(
            hwnd,
            kSplashTimerId);
        g_splashWindow =
            nullptr;
        g_splashMainWindow =
            nullptr;
        g_logo.reset();
        g_plaque.reset();

        if (g_gdiplusToken != 0) {
            Gdiplus::GdiplusShutdown(
                g_gdiplusToken);
            g_gdiplusToken = 0;
        }
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace

bool ShowStartupSplash(
    HINSTANCE instance,
    HWND mainWindow) {
    if (g_splashWindow ||
        !instance ||
        !mainWindow) {
        return false;
    }

    Gdiplus::GdiplusStartupInput
        startupInput;

    if (Gdiplus::GdiplusStartup(
            &g_gdiplusToken,
            &startupInput,
            nullptr) !=
        Gdiplus::Ok) {
        g_gdiplusToken = 0;
        return false;
    }

    g_logo =
        LoadBitmapFromResource(
            instance,
            IDR_SPLASH_LOGO);
    g_plaque =
        LoadBitmapFromBase64(
            kPlaqueBase64);

    if (!g_logo ||
        !g_plaque) {
        g_logo.reset();
        g_plaque.reset();
        Gdiplus::GdiplusShutdown(
            g_gdiplusToken);
        g_gdiplusToken = 0;
        return false;
    }

    WNDCLASSEXW wc{};
    wc.cbSize =
        sizeof(wc);
    wc.hInstance =
        instance;
    wc.lpfnWndProc =
        SplashWindowProc;
    wc.lpszClassName =
        kSplashClassName;
    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    if (!RegisterClassExW(&wc) &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        g_logo.reset();
        g_plaque.reset();
        Gdiplus::GdiplusShutdown(
            g_gdiplusToken);
        g_gdiplusToken = 0;
        return false;
    }

    g_splashMainWindow =
        mainWindow;
    g_phase =
        StartupSplashPhase::
            CheckingAppUpdate;
    g_dotCount = 1;

    g_splashWindow =
        CreateWindowExW(
            WS_EX_LAYERED |
                WS_EX_TOOLWINDOW |
                WS_EX_TOPMOST,
            kSplashClassName,
            L"TocPilot",
            WS_POPUP,
            0,
            0,
            kSplashWidth,
            kSplashHeight,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (!g_splashWindow) {
        g_splashMainWindow =
            nullptr;
        g_logo.reset();
        g_plaque.reset();
        Gdiplus::GdiplusShutdown(
            g_gdiplusToken);
        g_gdiplusToken = 0;
        return false;
    }

    SetTimer(
        g_splashWindow,
        kSplashTimerId,
        kSplashTimerMs,
        nullptr);

    RenderSplash();

    ShowWindow(
        g_splashWindow,
        SW_SHOWNORMAL);
    UpdateWindow(
        g_splashWindow);

    return true;
}

void SetStartupSplashPhase(
    StartupSplashPhase phase) {
    if (!g_splashWindow) {
        return;
    }

    g_phase = phase;
    g_dotCount = 1;

    RenderSplash();
}

bool IsStartupSplashActive() {
    return
        g_splashWindow != nullptr &&
        IsWindow(
            g_splashWindow);
}

} // namespace tp
