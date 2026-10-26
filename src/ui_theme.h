#pragma once
// One palette for TocPilot native UI. Use semantic names rather than local RGB literals.
#include <windows.h>
#include <string_view>
namespace tp::ui {
constexpr COLORREF Hex(unsigned rgb) {
    return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}
enum class RowState { AlreadyGood, GoodNow, Ignored, NeedsAttention, Error };
enum class PackageKind { Addon, Dll, Mpq };
struct Palette {
    static constexpr COLORREF window = Hex(0x1F2125);
    static constexpr COLORREF title = Hex(0x2B2E33);
    static constexpr COLORREF toolbar = Hex(0x3A3D42);
    static constexpr COLORREF list = Hex(0x2A2D31);
    static constexpr COLORREF accountSync = toolbar;
    static constexpr COLORREF button = Hex(0x4A4E54);
    static constexpr COLORREF buttonHover = Hex(0x5A5F66);
    static constexpr COLORREF buttonPressed = Hex(0x676D75);
    static constexpr COLORREF primaryText = Hex(0xF1F3F5);
    static constexpr COLORREF secondaryText = Hex(0xC1C6CD);
    static constexpr COLORREF divider = Hex(0x555A61);
    static constexpr COLORREF selection = secondaryText;
    static constexpr COLORREF alreadyGood = Hex(0x3F6F94);
    static constexpr COLORREF goodNow = Hex(0x4F8A57);
    static constexpr COLORREF ignored = Hex(0x5A5E64);
    static constexpr COLORREF attention = Hex(0xB7842E);
    static constexpr COLORREF error = Hex(0xA14F4F);
};
// Derive hover shades from one algorithm, never per-status constants.
constexpr COLORREF Lift(COLORREF colour) {
    auto channel = [](unsigned char value) -> unsigned char {
        return static_cast<unsigned char>(value + (255 - value) / 12);
    };
    return RGB(channel(GetRValue(colour)), channel(GetGValue(colour)), channel(GetBValue(colour)));
}

enum class ChromeRole { Navigation, Primary, Secondary };
struct ChromeVisual {
    COLORREF fill;
    COLORREF border;
    COLORREF text;
    bool activeUnderline;
};
// Colour and emphasis belong to the presentation layer; the underlying
// Win32 button retains focus, keyboard activation, enabled state and commands.
constexpr ChromeVisual ResolveChrome(ChromeRole role, bool active, bool hovered,
                                      bool pressed, bool disabled) {
    const bool navigation = role == ChromeRole::Navigation;
    const COLORREF base = navigation
        ? (active ? Palette::title : Palette::toolbar)
        : role == ChromeRole::Primary ? Palette::alreadyGood : Palette::button;
    const COLORREF surface = pressed ? Palette::buttonPressed :
        hovered && !disabled ? Lift(base) : base;
    return {
        disabled ? Palette::title : surface,
        navigation ? (active ? Palette::alreadyGood : Palette::toolbar)
                   : (role == ChromeRole::Primary ? Palette::alreadyGood : Palette::divider),
        disabled ? Hex(0x858B93) :
            navigation && !active ? Palette::secondaryText : Palette::primaryText,
        navigation && active
    };
}

constexpr COLORREF StatusFill(RowState state) {
    switch (state) {
    case RowState::AlreadyGood: return Palette::alreadyGood;
    case RowState::GoodNow: return Palette::goodNow;
    case RowState::Ignored: return Palette::ignored;
    case RowState::NeedsAttention: return Palette::attention;
    case RowState::Error: return Palette::error;
    }
    return Palette::alreadyGood;
}
constexpr RowState Classify(bool error, bool needsAttention, bool changedThisSession,
                            bool ignored) {
    // A real failure is never concealed by a persisted ignore preference.
    if (error) return RowState::Error;
    if (ignored) return RowState::Ignored;
    if (needsAttention) return RowState::NeedsAttention;
    if (changedThisSession) return RowState::GoodNow;
    return RowState::AlreadyGood;
}
constexpr PackageKind KindFromTarget(std::wstring_view target) {
    if (target == L"dll" || target == L"dlls") return PackageKind::Dll;
    if (target == L"mpq" || target == L"mpqs") return PackageKind::Mpq;
    return PackageKind::Addon;
}
struct RowVisual {
    COLORREF fill;
    COLORREF text;
    COLORREF border;
    bool showSelectionBorder;
    bool showFocus;
};
constexpr RowVisual ResolveRow(RowState status, bool hovered, bool selected, bool focused) {
    const COLORREF base = StatusFill(status);
    return { hovered ? Lift(base) : base, Palette::primaryText,
             Palette::selection, selected, focused };
}
inline bool HighContrast() {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
           (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}
} // namespace tp::ui
