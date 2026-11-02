#include "package_list_status.h"
#include "window_layout.h"
#include "state.h"
#include <array>
#include <iostream>
#include <string>
#include <utility>

namespace {
int failures = 0;
void Check(bool value, const char* label) {
    if (!value) { std::cerr << label << "\n"; ++failures; }
}
}
int main() {
    using namespace tp::ui;
    const std::array cases{
        std::pair{L"Checking...", PackageStatusIcon::Busy},
        std::pair{L"Inspecting...", PackageStatusIcon::Busy},
        std::pair{L"Updating...", PackageStatusIcon::Busy},
        std::pair{L"Installing DLL...", PackageStatusIcon::Busy},
        std::pair{L"Updating DLL...", PackageStatusIcon::Busy},
        std::pair{L"Installing MPQ...", PackageStatusIcon::Busy},
        std::pair{L"Updating MPQ...", PackageStatusIcon::Busy},
        std::pair{L"Preparing install...", PackageStatusIcon::Busy},
        std::pair{L"Preparing update...", PackageStatusIcon::Busy},
        std::pair{L"Preparing replacement...", PackageStatusIcon::Busy},
        std::pair{L"Uninstalling...", PackageStatusIcon::Busy},
        std::pair{L"Removing...", PackageStatusIcon::Busy},
        std::pair{L"Committing...", PackageStatusIcon::Busy},
        std::pair{L"Up To Date", PackageStatusIcon::Current},
        std::pair{L"Update Available", PackageStatusIcon::UpdateAvailable},
        std::pair{L"Not Configured", PackageStatusIcon::NotConfigured},
        std::pair{L"Not Installed", PackageStatusIcon::NotInstalled},
        std::pair{L"Needs Attention", PackageStatusIcon::NeedsAttention},
        std::pair{L"Refresh failed", PackageStatusIcon::Error},
        std::pair{L"Branch lookup failed", PackageStatusIcon::Error},
        std::pair{L"State save failed", PackageStatusIcon::Error},
        std::pair{L"Save failed", PackageStatusIcon::Error},
        std::pair{L"Rollback failed", PackageStatusIcon::Error},
        std::pair{L"Uninstall rolled back", PackageStatusIcon::Error},
        std::pair{L"Remove rolled back", PackageStatusIcon::Error},
        std::pair{L"Install rolled back", PackageStatusIcon::Error},
        std::pair{L"DLL update failed", PackageStatusIcon::Error},
        std::pair{L"MPQ update failed", PackageStatusIcon::Error},
        std::pair{L"Inspect failed", PackageStatusIcon::Error},
        std::pair{L"Install failed", PackageStatusIcon::Error},
        std::pair{L"Unexpected new state", PackageStatusIcon::NeedsAttention},
        std::pair{L"", PackageStatusIcon::NeedsAttention}
    };
    for (const auto& item : cases)
        Check(ResolvePackageStatusIcon(item.first) == item.second,
              "Real operation status category");
    // All glyphs resolve to distinct bundled Font Awesome symbols rather than
    // improvised GDI polygons; preserve the status/type semantic mapping.
    constexpr std::array statuses{
        PackageStatusIcon::Busy, PackageStatusIcon::UpdateAvailable,
        PackageStatusIcon::Current, PackageStatusIcon::NotConfigured,
        PackageStatusIcon::NotInstalled, PackageStatusIcon::NeedsAttention,
        PackageStatusIcon::Error};
    constexpr std::array types{PackageKind::Addon, PackageKind::Dll, PackageKind::Mpq};
    for (std::size_t i = 0; i < statuses.size(); ++i)
        for (std::size_t j = i+1; j < statuses.size(); ++j)
            Check(PackageStatusGlyph(statuses[i]) != PackageStatusGlyph(statuses[j]),
                  "Distinct readable status font glyphs");
    for (std::size_t i = 0; i < types.size(); ++i)
        for (std::size_t j = i+1; j < types.size(); ++j)
            Check(PackageKindGlyph(types[i]) != PackageKindGlyph(types[j]),
                  "Distinct readable package type font glyphs");
    Check(PackageStatusGlyph(PackageStatusIcon::Current) == L'\uf00c',
          "Font Awesome check icon for current");
    Check(PackageKindGlyph(PackageKind::Addon) == L'\uf1b2',
          "Font Awesome cube icon for addons");
    Check(PackageKindLabel(PackageKind::Addon) == L"Addon", "Addon type");
    Check(PackageKindLabel(PackageKind::Dll) == L"DLL", "DLL type");
    Check(PackageKindLabel(PackageKind::Mpq) == L"MPQ", "MPQ type");
    Check(KindFromTarget(L"mpq") == PackageKind::Mpq, "MPQ glyph selection");
    Check(KindFromTarget(L"dll") == PackageKind::Dll, "DLL glyph selection");
    Check(AccessiblePackageCell(L"Project (dev)", L"Checking...", PackageKind::Addon) ==
          L"Project (dev), Addon, Checking...", "Native accessible name");
    Check(PackageCellTooltip(L"Project (dev)", L"Refresh failed", PackageKind::Dll) ==
          L"Project (dev)\r\nType: DLL\r\nStatus: Refresh failed",
          "Full hover tooltip and actual error status");
    Check(CompactGlyphInk(true, RGB(12, 34, 56)) == RGB(12, 34, 56),
          "High-contrast status and type glyphs use system text ink");
    Check(CompactGlyphInk(false, RGB(12, 34, 56)) == Palette::primaryText,
          "Normal glyphs use theme ink");
    const auto offsets = PackageCellOffsets();
    for (int scale : {1, 2, 3}) {
        Check((offsets.typeLeft - offsets.statusLeft)*scale >= 12*scale,
              "Status and type don't overlap");
        Check((offsets.nameLeft - offsets.typeLeft)*scale >= 22*scale,
              "Type and Name don't overlap");
    }
    // Status updates can be repeated while the row stays selected/focused;
    // the category must follow the latest live status, not old row colours.
    for (const auto [label, kind] : std::array{
        std::pair{L"Checking...", PackageStatusIcon::Busy},
        std::pair{L"Update Available", PackageStatusIcon::UpdateAvailable},
        std::pair{L"Updating...", PackageStatusIcon::Busy},
        std::pair{L"Up To Date", PackageStatusIcon::Current},
        std::pair{L"Refresh failed", PackageStatusIcon::Error}}) {
        Check(ResolvePackageStatusIcon(label) == kind,
              "Repeated status updates resolve the latest icon");
    }
    Check(tp::CompactListColumnVisible(0), "Name visible");
    for (int index = 1; index < static_cast<int>(tp::kPackageColumnCount); ++index)
        Check(!tp::CompactListColumnVisible(index),
              "Status/metadata hidden but retained for legacy sorting");
    Check(tp::CompactPackageNameWidth(340, 320, 220) == 340,
          "Saved Name wider than viewport scrolls");
    Check(tp::CompactPackageNameWidth(240, 550, 220) == 550,
          "Viewport fit does not modify saved Name");
    Check(tp::CompactPackageNameWidth(100, 300, 220) == 300,
          "Legacy Name reaches minimum and viewport");
    Check(tp::CompactPackageNameWidth(425, 1125, 275) == 1125,
          "DPI-scaled viewport geometry");
    if (failures) { std::cerr << failures << " compact list failures\n"; return 1; }
    return 0;
}
