#include "ui_theme.h"
#include <cstdio>
int main() {
    using namespace tp::ui;
    if (!(StatusFill(RowState::AlreadyGood) == Hex(0x3F6F94))) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(StatusFill(RowState::GoodNow) == Hex(0x4F8A57))) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(StatusFill(RowState::Ignored) == Hex(0x5A5E64))) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(StatusFill(RowState::NeedsAttention) == Hex(0xB7842E))) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(StatusFill(RowState::Error) == Hex(0xA14F4F))) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(Classify(true,true,true,true) == RowState::Error)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(Classify(false,true,true,false) == RowState::NeedsAttention)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(Classify(false,false,true,false) == RowState::GoodNow)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(Classify(false,false,false,false) == RowState::AlreadyGood)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(Classify(false,true,true,true) == RowState::Ignored)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    const auto base = ResolveRow(RowState::AlreadyGood,false,false,false);
    const auto selected = ResolveRow(RowState::AlreadyGood,false,true,true);
    const auto hover = ResolveRow(RowState::AlreadyGood,true,false,false);
    const auto selectedHover = ResolveRow(RowState::AlreadyGood,true,true,true);
    if (!(base.fill == selected.fill)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(hover.fill == selectedHover.fill)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(hover.fill == Lift(base.fill))) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(selected.showSelectionBorder && selected.showFocus)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(selectedHover.showSelectionBorder)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(!base.showSelectionBorder && !base.showFocus)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(KindFromTarget(L"addons") == PackageKind::Addon)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(KindFromTarget(L"dll") == PackageKind::Dll)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(KindFromTarget(L"dlls") == PackageKind::Dll)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(KindFromTarget(L"mpq") == PackageKind::Mpq)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(KindFromTarget(L"mpqs") == PackageKind::Mpq)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(Lift(Palette::goodNow) != Palette::goodNow)) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(ResolveRow(RowState::Error, false, true, false).fill ==
           StatusFill(RowState::Error))) { std::fputs("UI theme regression failed\n", stderr); return 1; }
    if (!(Palette::accountSync == Palette::toolbar)) { std::fputs("UI theme regression failed\n", stderr); return 1; }

    if (Palette::packageDetails == Palette::window) return 1;
    const auto navInactive = ResolveChrome(ChromeRole::Navigation, false, false, false, false);
    const auto navActive = ResolveChrome(ChromeRole::Navigation, true, false, false, false);
    const auto navHot = ResolveChrome(ChromeRole::Navigation, false, true, false, false);
    const auto primaryNormal = ResolveChrome(ChromeRole::Primary, false, false, false, false);
    const auto primaryHot = ResolveChrome(ChromeRole::Primary, false, true, false, false);
    const auto secondary = ResolveChrome(ChromeRole::Secondary, false, false, false, false);
    const auto disabledButton = ResolveChrome(ChromeRole::Primary, false, true, false, true);
    if (!(navActive.activeUnderline && !navInactive.activeUnderline)) return 1;
    if (!(navActive.fill != navInactive.fill && navInactive.fill != navHot.fill)) return 1;
    if (!(primaryNormal.fill == Palette::alreadyGood &&
          primaryHot.fill == Lift(Palette::alreadyGood))) return 1;
    if (!(secondary.fill == Palette::button &&
          ResolveChrome(ChromeRole::Secondary, false, false, true, false).fill ==
              Palette::buttonPressed)) return 1;
    if (!(disabledButton.fill == Palette::title &&
          disabledButton.text != primaryNormal.text)) return 1;
    return 0;
}
