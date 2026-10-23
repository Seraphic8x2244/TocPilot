#include "ui_theme.h"
#include <cassert>
int main() {
    using namespace tp::ui;
    assert(StatusFill(RowState::AlreadyGood) == Hex(0x3F6F94));
    assert(StatusFill(RowState::GoodNow) == Hex(0x4F8A57));
    assert(StatusFill(RowState::Ignored) == Hex(0x5A5E64));
    assert(StatusFill(RowState::NeedsAttention) == Hex(0xB7842E));
    assert(StatusFill(RowState::Error) == Hex(0xA14F4F));
    assert(Classify(true,true,true,true) == RowState::Error);
    assert(Classify(false,true,true,false) == RowState::NeedsAttention);
    assert(Classify(false,false,true,false) == RowState::GoodNow);
    assert(Classify(false,false,false,false) == RowState::AlreadyGood);
    assert(Classify(false,true,true,true) == RowState::Ignored);
    const auto base = ResolveRow(RowState::AlreadyGood,false,false,false);
    const auto selected = ResolveRow(RowState::AlreadyGood,false,true,true);
    const auto hover = ResolveRow(RowState::AlreadyGood,true,false,false);
    const auto selectedHover = ResolveRow(RowState::AlreadyGood,true,true,true);
    assert(base.fill == selected.fill);
    assert(hover.fill == selectedHover.fill);
    assert(hover.fill == Lift(base.fill));
    assert(selected.showSelectionBorder && selected.showFocus);
    assert(selectedHover.showSelectionBorder);
    assert(!base.showSelectionBorder && !base.showFocus);
    assert(KindFromTarget(L"addons") == PackageKind::Addon);
    assert(KindFromTarget(L"dll") == PackageKind::Dll);
    assert(KindFromTarget(L"dlls") == PackageKind::Dll);
    assert(KindFromTarget(L"mpq") == PackageKind::Mpq);
    assert(KindFromTarget(L"mpqs") == PackageKind::Mpq);
    assert(Lift(Palette::goodNow) != Palette::goodNow);
    assert(ResolveRow(RowState::Error, false, true, false).fill ==
           StatusFill(RowState::Error));
    assert(Palette::accountSync == Palette::toolbar);
    return 0;
}
