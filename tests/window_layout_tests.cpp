#include "window_layout.h"

#include <iostream>

namespace {

int failures = 0;

void ExpectEqual(
    int actual,
    int expected,
    const char* message) {
    if (actual == expected) {
        return;
    }

    std::cerr
        << message
        << ": expected "
        << expected
        << ", got "
        << actual
        << "\n";
    ++failures;
}

} // namespace

int main() {
    ExpectEqual(tp::UnifiedPageWidth(750, 12), 726,
                "Outer page width honours both insets");
    ExpectEqual(tp::UnifiedPageWidth(650, 15), 620,
                "Narrow page stays within viewport");
    ExpectEqual(tp::UnifiedPageWidth(1125, 18), 1089,
                "DPI-scaled page uses physical dimensions");
    ExpectEqual(tp::UnifiedPageWidth(20, 12), 1,
                "Pathological narrow window never has negative width");
    for (int h : {680, 760, 1024}) {
        const auto shell = tp::UnifiedShellLayout(h, 16, 42, 78, 12);
        if (shell.tabTop != 16 || shell.tabBottom != 58 ||
            shell.contentTop != 57 || shell.footerTop != h - 78 ||
            shell.contentBottom != shell.footerTop - 12) return 1;
    }
    // DPI-equivalent physical dimensions retain the same structure.
    const auto scaledShell = tp::UnifiedShellLayout(1020, 24, 63, 117, 18);
    if (scaledShell.tabBottom != 87 || scaledShell.contentTop != 86 ||
        scaledShell.footerTop != 903 || scaledShell.contentBottom != 885) return 1;
    const int saved = 340;
    const auto wide = tp::UnifiedPackageColumnWidths(saved, 900, 220, 150);
    const auto narrow = tp::UnifiedPackageColumnWidths(saved, 400, 220, 150);
    const auto restarted = tp::UnifiedPackageColumnWidths(saved, 900, 220, 150);
    ExpectEqual(wide[0], 340, "Name does not auto-expand");
    ExpectEqual(wide[1], 560, "Status fills wide viewport");
    ExpectEqual(narrow[0], 340, "Name survives narrow resize");
    ExpectEqual(narrow[1], 150, "Status remains at minimum for horizontal scroll");
    ExpectEqual(restarted[0], 340, "saved Name survives restart");
    const auto scaled = tp::UnifiedPackageColumnWidths(425, 1125, 275, 188);
    ExpectEqual(scaled[0], 425, "DPI-scaled Name remains authoritative");
    ExpectEqual(scaled[1], 700, "DPI-scaled Status consumes remainder");
    const auto small = tp::UnifiedPackageColumnWidths(80, 820, 220, 150);
    ExpectEqual(small[0], 220, "legacy Name reaches minimum");
    ExpectEqual(small[1], 600, "Status gets remainder after minimum");

    {
        const auto widths =
            tp::FitCompactColumnWidths(
                240,
                160,
                500,
                220,
                150);
        ExpectEqual(
            widths[0],
            290,
            "Compact growth should give half the extra width to the first column");
        ExpectEqual(
            widths[1],
            210,
            "Compact growth should give half the extra width to the second column");
    }

    {
        const auto widths =
            tp::FitCompactColumnWidths(
                290,
                210,
                401,
                220,
                150);
        ExpectEqual(
            widths[0],
            241,
            "Compact shrink should split the removed width as evenly as integer pixels allow");
        ExpectEqual(
            widths[1],
            160,
            "Compact fitted widths should exactly fill the available width");
    }

    {
        const auto widths =
            tp::FitCompactColumnWidths(
                600,
                150,
                400,
                220,
                150);
        ExpectEqual(
            widths[0],
            250,
            "Compact fitting should preserve the Status minimum when Name is oversized");
        ExpectEqual(
            widths[1],
            150,
            "Compact fitting should not squeeze Status below its Compact minimum");
    }

    {
        const auto widths =
            tp::CompactDividerWidths(
                275,
                500,
                220,
                150);
        ExpectEqual(
            widths[0],
            275,
            "middle-divider drag should keep a valid proposed Name width");
        ExpectEqual(
            widths[1],
            225,
            "middle-divider drag should resize Status inversely");
        ExpectEqual(
            widths[0] + widths[1],
            500,
            "middle-divider transaction should preserve the total Compact width");
    }

    {
        const auto widths =
            tp::CompactDividerWidths(
                490,
                500,
                220,
                150);
        ExpectEqual(
            widths[0],
            350,
            "middle-divider drag should stop Name before Status drops below its Compact minimum");
        ExpectEqual(
            widths[1],
            150,
            "middle-divider drag should preserve the Status Compact minimum");
    }

    {
        const auto widths =
            tp::CompactDividerWidths(
                400,
                500,
                150,
                220);
        ExpectEqual(
            widths[0],
            280,
            "swapped Compact order should respect the Name minimum on the right");
        ExpectEqual(
            widths[1],
            220,
            "swapped Compact order should preserve the Name Compact minimum");
    }

    ExpectEqual(
        static_cast<int>(
            tp::CompactResizeFirstColumn(
                350,
                250,
                150,
                250)),
        static_cast<int>(
            tp::CompactResizeFirst::Left),
        "reverse drag should shrink the left column before growing the right");

    ExpectEqual(
        static_cast<int>(
            tp::CompactResizeFirstColumn(
                250,
                350,
                250,
                150)),
        static_cast<int>(
            tp::CompactResizeFirst::Right),
        "forward drag should shrink the right column before growing the left");

    constexpr int compactMinimum = 600;
    constexpr int advancedMinimum = 1300;

    const int expanded =
        tp::ExpandedWindowWidth(
            900,
            compactMinimum,
            advancedMinimum);
    ExpectEqual(
        expanded,
        1600,
        "custom compact width should carry into advanced expansion");

    ExpectEqual(
        tp::RestoredCompactWindowWidth(
            900,
            expanded,
            compactMinimum,
            advancedMinimum),
        900,
        "compact width should restore after normal advanced round trip");

    ExpectEqual(
        tp::RestoredCompactWindowWidth(
            900,
            1800,
            compactMinimum,
            advancedMinimum),
        900,
        "advanced resizing should not discard the pre-expansion compact width");

    ExpectEqual(
        tp::RestoredCompactWindowWidth(
            500,
            expanded,
            compactMinimum,
            advancedMinimum),
        compactMinimum,
        "restored compact width should respect the current compact minimum");

    ExpectEqual(
        tp::RestoredCompactWindowWidth(
            540,
            1600,
            480,
            advancedMinimum),
        540,
        "compact width below the startup default should still restore when above the actual content minimum");

    ExpectEqual(
        tp::RestoredCompactWindowWidth(
            0,
            expanded,
            compactMinimum,
            advancedMinimum),
        900,
        "fallback should retain the historical delta behavior when no compact width was captured");

    if (failures != 0) {
        std::cerr
            << failures
            << " window layout test(s) failed\n";
        return 1;
    }

    std::cout << "window layout tests passed\n";
    return 0;
}
