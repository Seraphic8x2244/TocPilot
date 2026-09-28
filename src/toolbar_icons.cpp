#include "toolbar_icons.h"

#include "resource.h"

#include <commctrl.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tp {
namespace {

struct FontCandidate {
    const wchar_t* face;
    int weight;
};

constexpr FontCandidate kFontAwesomeFaces[]{
    {L"Font Awesome 6 Free Solid", FW_DONTCARE},
    {L"Font Awesome 6 Free", FW_BLACK}
};

bool HasToolbarGlyphs(HFONT font) {
    if (!font) {
        return false;
    }

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        return false;
    }

    const HGDIOBJ oldFont =
        SelectObject(dc, font);

    constexpr wchar_t glyphs[]{
        L'\uf0ed',
        L'\uf021',
        L'\uf65e',
        L'\uf07b',
        L'\uf363',
        L'\uf65d',
        L'\uf002',
        L'\uf07c',
        L'\uf362',
        L'\uf05a',
        L'\uf7d9'
    };
    constexpr int glyphCount =
        static_cast<int>(
            sizeof(glyphs) /
            sizeof(glyphs[0]));
    WORD indices[glyphCount]{};

    const DWORD result =
        GetGlyphIndicesW(
            dc,
            glyphs,
            glyphCount,
            indices,
            GGI_MARK_NONEXISTING_GLYPHS);

    SelectObject(dc, oldFont);
    DeleteDC(dc);

    if (result == GDI_ERROR) {
        return false;
    }

    for (const WORD index : indices) {
        if (index == 0xFFFF) {
            return false;
        }
    }
    return true;
}

HFONT CreateFontAwesomeFont(
    int pixelHeight) {
    for (const auto& candidate :
         kFontAwesomeFaces) {
        HFONT font =
            CreateFontW(
                -pixelHeight,
                0,
                0,
                0,
                candidate.weight,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_TT_ONLY_PRECIS,
                CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY,
                DEFAULT_PITCH |
                    FF_DONTCARE,
                candidate.face);

        if (HasToolbarGlyphs(font)) {
            return font;
        }

        if (font) {
            DeleteObject(font);
        }
    }

    return nullptr;
}

void DrawGlyph(
    HDC dc,
    HFONT font,
    wchar_t glyph,
    int iconSize,
    COLORREF colour) {
    const HGDIOBJ oldFont =
        SelectObject(dc, font);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colour);

    RECT rect{
        0,
        0,
        iconSize,
        iconSize};

    DrawTextW(
        dc,
        &glyph,
        1,
        &rect,
        DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_NOPREFIX);

    SelectObject(dc, oldFont);
}

HICON CreateGlyphIcon(
    HFONT baseFont,
    wchar_t baseGlyph,
    int iconSize,
    COLORREF outputColour,
    HFONT cutoutFont = nullptr,
    wchar_t cutoutGlyph = L'\0') {
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth =
        iconSize;
    bitmapInfo.bmiHeader.biHeight =
        -iconSize;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression =
        BI_RGB;

    void* rawBits = nullptr;
    HBITMAP colourBitmap =
        CreateDIBSection(
            nullptr,
            &bitmapInfo,
            DIB_RGB_COLORS,
            &rawBits,
            nullptr,
            0);

    if (!colourBitmap ||
        !rawBits) {
        if (colourBitmap) {
            DeleteObject(colourBitmap);
        }
        return nullptr;
    }

    HDC dc =
        CreateCompatibleDC(nullptr);
    if (!dc) {
        DeleteObject(colourBitmap);
        return nullptr;
    }

    const HGDIOBJ oldBitmap =
        SelectObject(
            dc,
            colourBitmap);

    auto* pixels =
        static_cast<std::uint32_t*>(
            rawBits);
    std::fill_n(
        pixels,
        iconSize * iconSize,
        0u);

    DrawGlyph(
        dc,
        baseFont,
        baseGlyph,
        iconSize,
        RGB(255, 255, 255));

    if (cutoutFont &&
        cutoutGlyph != L'\0') {
        DrawGlyph(
            dc,
            cutoutFont,
            cutoutGlyph,
            iconSize,
            RGB(0, 0, 0));
    }

    auto* bytes =
        static_cast<unsigned char*>(
            rawBits);

    for (int pixel = 0;
         pixel <
             iconSize * iconSize;
         ++pixel) {
        unsigned char* value =
            bytes + pixel * 4;

        const unsigned char alpha =
            std::max({
                value[0],
                value[1],
                value[2]});

        value[0] =
            GetBValue(outputColour);
        value[1] =
            GetGValue(outputColour);
        value[2] =
            GetRValue(outputColour);
        value[3] =
            alpha;
    }

    SelectObject(dc, oldBitmap);
    DeleteDC(dc);

    const int maskStride =
        ((iconSize + 15) / 16) * 2;
    std::vector<unsigned char>
        maskBits(
            static_cast<std::size_t>(
                maskStride *
                iconSize),
            0);

    HBITMAP maskBitmap =
        CreateBitmap(
            iconSize,
            iconSize,
            1,
            1,
            maskBits.data());

    if (!maskBitmap) {
        DeleteObject(colourBitmap);
        return nullptr;
    }

    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmMask =
        maskBitmap;
    info.hbmColor =
        colourBitmap;

    HICON icon =
        CreateIconIndirect(
            &info);

    DeleteObject(maskBitmap);
    DeleteObject(colourBitmap);
    return icon;
}

void DestroyIconHandle(
    HICON& icon) {
    if (!icon) {
        return;
    }

    DestroyIcon(icon);
    icon = nullptr;
}

void DestroyPair(
    ToolbarIcon& icon) {
    DestroyIconHandle(icon.normal);
    DestroyIconHandle(icon.disabled);
}

ToolbarIcon CreatePair(
    HFONT iconFont,
    wchar_t glyph,
    int iconSize,
    HFONT cutoutFont = nullptr,
    wchar_t cutoutGlyph = L'\0') {
    ToolbarIcon icon;

    icon.normal =
        CreateGlyphIcon(
            iconFont,
            glyph,
            iconSize,
            RGB(0, 0, 0),
            cutoutFont,
            cutoutGlyph);

    icon.disabled =
        CreateGlyphIcon(
            iconFont,
            glyph,
            iconSize,
            GetSysColor(
                COLOR_GRAYTEXT),
            cutoutFont,
            cutoutGlyph);

    return icon;
}

bool PairReady(
    const ToolbarIcon& icon) {
    return
        icon.normal &&
        icon.disabled;
}

}  // namespace

bool InitializeToolbarIcons(
    HINSTANCE instance,
    int pixelSize,
    ToolbarIcons& icons) {
    DestroyToolbarIcons(icons);

    if (pixelSize <= 0) {
        return false;
    }

    const HRSRC fontResource =
        FindResourceW(
            instance,
            MAKEINTRESOURCEW(
                IDR_FA_SOLID_FONT),
            RT_RCDATA);

    if (!fontResource) {
        return false;
    }

    const HGLOBAL loadedResource =
        LoadResource(
            instance,
            fontResource);
    if (!loadedResource) {
        return false;
    }

    const void* fontBytes =
        LockResource(
            loadedResource);
    const DWORD fontSize =
        SizeofResource(
            instance,
            fontResource);

    if (!fontBytes ||
        fontSize == 0) {
        return false;
    }

    DWORD fontCount = 0;
    icons.fontResource =
        AddFontMemResourceEx(
            const_cast<void*>(
                fontBytes),
            fontSize,
            nullptr,
            &fontCount);

    if (!icons.fontResource ||
        fontCount == 0) {
        DestroyToolbarIcons(icons);
        return false;
    }

    HFONT iconFont =
        CreateFontAwesomeFont(
            pixelSize);
    HFONT smallIconFont =
        CreateFontAwesomeFont(
            std::max(
                8,
                pixelSize / 2));

    if (!iconFont ||
        !smallIconFont) {
        if (iconFont) {
            DeleteObject(iconFont);
        }
        if (smallIconFont) {
            DeleteObject(
                smallIconFont);
        }
        DestroyToolbarIcons(icons);
        return false;
    }

    icons.update =
        CreatePair(
            iconFont,
            L'\uf0ed',
            pixelSize);
    icons.refresh =
        CreatePair(
            iconFont,
            L'\uf021',
            pixelSize);
    icons.addRepository =
        CreatePair(
            iconFont,
            L'\uf65e',
            pixelSize);
    icons.reinstallRepository =
        CreatePair(
            iconFont,
            L'\uf07b',
            pixelSize,
            smallIconFont,
            L'\uf363');
    icons.removeRepository =
        CreatePair(
            iconFont,
            L'\uf65d',
            pixelSize);
    icons.inspect =
        CreatePair(
            iconFont,
            L'\uf002',
            pixelSize);
    icons.scan =
        CreatePair(
            iconFont,
            L'\uf07c',
            pixelSize);
    icons.accountSync =
        CreatePair(
            iconFont,
            L'\uf362',
            pixelSize);
    icons.info =
        CreatePair(
            iconFont,
            L'\uf05a',
            pixelSize);
    icons.advanced =
        CreatePair(
            iconFont,
            L'\uf7d9',
            pixelSize);
    icons.pixelSize =
        pixelSize;

    DeleteObject(smallIconFont);
    DeleteObject(iconFont);

    const bool ready =
        PairReady(icons.update) &&
        PairReady(icons.refresh) &&
        PairReady(
            icons.addRepository) &&
        PairReady(
            icons.reinstallRepository) &&
        PairReady(
            icons.removeRepository) &&
        PairReady(icons.inspect) &&
        PairReady(icons.scan) &&
        PairReady(icons.accountSync) &&
        PairReady(icons.info) &&
        PairReady(icons.advanced);

    if (!ready) {
        DestroyToolbarIcons(icons);
    }

    return ready;
}

void DestroyToolbarIcons(
    ToolbarIcons& icons) {
    DestroyPair(icons.update);
    DestroyPair(icons.refresh);
    DestroyPair(
        icons.addRepository);
    DestroyPair(
        icons.reinstallRepository);
    DestroyPair(
        icons.removeRepository);
    DestroyPair(icons.inspect);
    DestroyPair(icons.scan);
    DestroyPair(icons.accountSync);
    DestroyPair(icons.info);
    DestroyPair(icons.advanced);

    icons.pixelSize = 0;

    if (icons.fontResource) {
        RemoveFontMemResourceEx(
            icons.fontResource);
        icons.fontResource =
            nullptr;
    }
}

HWND CreateToolbarTooltip(
    HWND owner) {
    HWND tooltip =
        CreateWindowExW(
            WS_EX_TOPMOST,
            TOOLTIPS_CLASSW,
            nullptr,
            WS_POPUP |
                TTS_ALWAYSTIP |
                TTS_NOPREFIX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            owner,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

    if (tooltip) {
        SetWindowPos(
            tooltip,
            HWND_TOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOACTIVATE);
    }

    return tooltip;
}

void AddToolbarTooltip(
    HWND tooltip,
    HWND owner,
    HWND control,
    const wchar_t* text) {
    if (!tooltip ||
        !control ||
        !text) {
        return;
    }

    TTTOOLINFOW tool{};
    tool.cbSize =
        sizeof(tool);
    tool.uFlags =
        TTF_IDISHWND |
        TTF_SUBCLASS;
    tool.hwnd =
        owner;
    tool.uId =
        reinterpret_cast<UINT_PTR>(
            control);
    tool.lpszText =
        const_cast<LPWSTR>(
            text);

    SendMessageW(
        tooltip,
        TTM_ADDTOOLW,
        0,
        reinterpret_cast<LPARAM>(
            &tool));
}

}  // namespace tp
