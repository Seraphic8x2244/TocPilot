#include "toolbar_icons.h"

#include "resource.h"

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace tp {
namespace {

constexpr int kIconSize = 16;

struct FontCandidate {
    const wchar_t* face;
    int weight;
};

constexpr std::array<FontCandidate, 2> kFontAwesomeFaces{{
    {L"Font Awesome 6 Free Solid", FW_DONTCARE},
    {L"Font Awesome 6 Free", FW_BLACK}
}};

bool HasToolbarGlyphs(HFONT font) {
    if (!font) {
        return false;
    }

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        return false;
    }

    const HGDIOBJ oldFont = SelectObject(dc, font);

    constexpr std::array<wchar_t, 10> glyphs{{
        L'\uf0ed',
        L'\uf021',
        L'\uf65e',
        L'\uf07b',
        L'\uf363',
        L'\uf65d',
        L'\uf002',
        L'\uf07c',
        L'\uf05a',
        L'\uf7d9'
    }};
    std::array<WORD, glyphs.size()> indices{};

    const DWORD result = GetGlyphIndicesW(
        dc,
        glyphs.data(),
        static_cast<int>(glyphs.size()),
        indices.data(),
        GGI_MARK_NONEXISTING_GLYPHS);

    SelectObject(dc, oldFont);
    DeleteDC(dc);

    if (result == GDI_ERROR) {
        return false;
    }

    return std::all_of(
        indices.begin(),
        indices.end(),
        [](WORD index) {
            return index != 0xFFFF;
        });
}

HFONT CreateFontAwesomeFont(int pixelHeight) {
    for (const auto& candidate : kFontAwesomeFaces) {
        HFONT font = CreateFontW(
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
            DEFAULT_PITCH | FF_DONTCARE,
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

void DrawGlyph(HDC dc, HFONT font, wchar_t glyph, COLORREF colour) {
    const HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colour);
    RECT rect{0, 0, kIconSize, kIconSize};
    DrawTextW(
        dc, &glyph, 1, &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

HICON CreateGlyphIcon(
    HFONT baseFont,
    wchar_t baseGlyph,
    HFONT cutoutFont = nullptr,
    wchar_t cutoutGlyph = L'\0') {
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = kIconSize;
    bitmapInfo.bmiHeader.biHeight = -kIconSize;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* rawBits = nullptr;
    HBITMAP colourBitmap = CreateDIBSection(
        nullptr, &bitmapInfo, DIB_RGB_COLORS, &rawBits, nullptr, 0);
    if (!colourBitmap || !rawBits) {
        if (colourBitmap) {
            DeleteObject(colourBitmap);
        }
        return nullptr;
    }

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        DeleteObject(colourBitmap);
        return nullptr;
    }

    const HGDIOBJ oldBitmap = SelectObject(dc, colourBitmap);
    auto* pixels = static_cast<std::uint32_t*>(rawBits);
    std::fill_n(pixels, kIconSize * kIconSize, 0u);

    DrawGlyph(dc, baseFont, baseGlyph, RGB(255, 255, 255));
    if (cutoutFont && cutoutGlyph != L'\0') {
        DrawGlyph(dc, cutoutFont, cutoutGlyph, RGB(0, 0, 0));
    }

    auto* bytes = static_cast<unsigned char*>(rawBits);
    for (int pixel = 0; pixel < kIconSize * kIconSize; ++pixel) {
        unsigned char* value = bytes + pixel * 4;
        const unsigned char alpha =
            std::max({value[0], value[1], value[2]});
        value[0] = 0;
        value[1] = 0;
        value[2] = 0;
        value[3] = alpha;
    }

    SelectObject(dc, oldBitmap);
    DeleteDC(dc);

    std::array<std::uint16_t, kIconSize> maskBits{};
    HBITMAP maskBitmap = CreateBitmap(
        kIconSize, kIconSize, 1, 1, maskBits.data());
    if (!maskBitmap) {
        DeleteObject(colourBitmap);
        return nullptr;
    }

    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmMask = maskBitmap;
    info.hbmColor = colourBitmap;
    HICON icon = CreateIconIndirect(&info);

    DeleteObject(maskBitmap);
    DeleteObject(colourBitmap);
    return icon;
}

void DestroyIconHandle(HICON& icon) {
    if (icon) {
        DestroyIcon(icon);
        icon = nullptr;
    }
}

}  // namespace

bool InitializeToolbarIcons(HINSTANCE instance, ToolbarIcons& icons) {
    DestroyToolbarIcons(icons);

    const HRSRC fontResource = FindResourceW(
        instance, MAKEINTRESOURCEW(IDR_FA_SOLID_FONT), RT_RCDATA);
    if (!fontResource) {
        return false;
    }

    const HGLOBAL loadedResource = LoadResource(instance, fontResource);
    if (!loadedResource) {
        return false;
    }

    const void* fontBytes = LockResource(loadedResource);
    const DWORD fontSize = SizeofResource(instance, fontResource);
    if (!fontBytes || fontSize == 0) {
        return false;
    }

    DWORD fontCount = 0;
    icons.fontResource = AddFontMemResourceEx(
        const_cast<void*>(fontBytes), fontSize, nullptr, &fontCount);
    if (!icons.fontResource || fontCount == 0) {
        DestroyToolbarIcons(icons);
        return false;
    }

    HFONT iconFont = CreateFontAwesomeFont(kIconSize);
    HFONT smallIconFont = CreateFontAwesomeFont(8);
    if (!iconFont || !smallIconFont) {
        if (iconFont) {
            DeleteObject(iconFont);
        }
        if (smallIconFont) {
            DeleteObject(smallIconFont);
        }
        DestroyToolbarIcons(icons);
        return false;
    }

    icons.update = CreateGlyphIcon(iconFont, L'\uf0ed');
    icons.refresh = CreateGlyphIcon(iconFont, L'\uf021');
    icons.addRepository = CreateGlyphIcon(iconFont, L'\uf65e');
    icons.reinstallRepository =
        CreateGlyphIcon(iconFont, L'\uf07b', smallIconFont, L'\uf363');
    icons.removeRepository = CreateGlyphIcon(iconFont, L'\uf65d');
    icons.inspect = CreateGlyphIcon(iconFont, L'\uf002');
    icons.scan = CreateGlyphIcon(iconFont, L'\uf07c');
    icons.tocPilot = CreateGlyphIcon(iconFont, L'\uf05a');
    icons.advanced = CreateGlyphIcon(iconFont, L'\uf7d9');

    DeleteObject(smallIconFont);
    DeleteObject(iconFont);

    return
        icons.update &&
        icons.refresh &&
        icons.addRepository &&
        icons.reinstallRepository &&
        icons.removeRepository &&
        icons.inspect &&
        icons.scan &&
        icons.tocPilot &&
        icons.advanced;
}

void DestroyToolbarIcons(ToolbarIcons& icons) {
    DestroyIconHandle(icons.update);
    DestroyIconHandle(icons.refresh);
    DestroyIconHandle(icons.addRepository);
    DestroyIconHandle(icons.reinstallRepository);
    DestroyIconHandle(icons.removeRepository);
    DestroyIconHandle(icons.inspect);
    DestroyIconHandle(icons.scan);
    DestroyIconHandle(icons.tocPilot);
    DestroyIconHandle(icons.advanced);

    if (icons.fontResource) {
        RemoveFontMemResourceEx(icons.fontResource);
        icons.fontResource = nullptr;
    }
}

void ApplyToolbarIcon(
    HWND button,
    HICON icon,
    const wchar_t* accessibleName) {
    if (!button) {
        return;
    }

    SetWindowTextW(button, accessibleName);
    if (icon) {
        SendMessageW(
            button, BM_SETIMAGE, IMAGE_ICON,
            reinterpret_cast<LPARAM>(icon));
    }
}

HWND CreateToolbarTooltip(HWND owner) {
    HWND tooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (tooltip) {
        SetWindowPos(
            tooltip, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return tooltip;
}

void AddToolbarTooltip(
    HWND tooltip,
    HWND owner,
    HWND control,
    const wchar_t* text) {
    if (!tooltip || !control || !text) {
        return;
    }

    TTTOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.hwnd = owner;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<LPWSTR>(text);

    SendMessageW(
        tooltip, TTM_ADDTOOLW, 0,
        reinterpret_cast<LPARAM>(&tool));
}

}  // namespace tp
