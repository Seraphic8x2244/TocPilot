#include "window_layout.h"

#include <iostream>
#include <array>

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
    // Chunk 1: unchanged toolbar and full-width readable status band above
    // two native side-by-side panes, at minimum, ordinary, maximized and DPI.
    for (const auto [scale, clientWidth, clientHeight] :
         {std::array<int, 3>{1, 690, 680}, {1, 790, 760},
          {1, 1560, 1024}, {2, 690, 680}, {2, 1560, 1024}}) {
        const int margin = 20 * scale;
        const int pageLeft = margin + 12 * scale;
        const int pageWidth = tp::UnifiedPageWidth(
            (clientWidth - 2 * 20) * scale, 12 * scale);
        const auto shell = tp::UnifiedShellLayout(
            clientHeight * scale, 16 * scale, 42 * scale,
            78 * scale, 12 * scale);
        const int toolbarBottom = shell.contentTop + (14 + 40) * scale;
        const auto panes = tp::LayoutPackageTwoPanes({
            pageLeft, pageWidth, toolbarBottom, shell.contentBottom,
            8 * scale, 56 * scale, 10 * scale, 12 * scale,
            6 * scale, 270 * scale, 270 * scale});
        const auto yes = [](bool condition) { return condition ? 1 : 0; };
        ExpectEqual(panes.statusTop, toolbarBottom + 8 * scale,
                    "Status band directly follows toolbar");
        ExpectEqual(panes.listTop, panes.statusTop + panes.statusHeight +
                    10 * scale, "List begins below status text");
        ExpectEqual(panes.listTop, panes.detailsTop,
                    "Both panes share a top edge");
        ExpectEqual(panes.listHeight, panes.detailsHeight,
                    "Both panes use remaining vertical space");
        ExpectEqual(panes.detailsLeft - (panes.listLeft + panes.listWidth),
                    12 * scale, "Horizontal panes have a consistent gap");
        ExpectEqual(panes.detailsLeft + panes.detailsWidth,
                    pageLeft + pageWidth, "Right pane stops at page edge");
        ExpectEqual(panes.statusLeft + panes.statusWidth,
                    pageLeft + pageWidth, "Status spans the whole page");
        ExpectEqual(panes.listTop + panes.listHeight,
                    shell.contentBottom - 6 * scale,
                    "Both panes stop above footer");
        ExpectEqual(yes(panes.listWidth >= 270 * scale), 1,
                    "List minimum is respected at supported widths");
        ExpectEqual(yes(panes.detailsWidth >= 270 * scale), 1,
                    "Details minimum is respected at supported widths");
        ExpectEqual(yes(panes.listHeight > 320 * scale), 1,
                    "Controls fit vertically at supported heights");
        ExpectEqual(yes(panes.listLeft >= margin), 1,
                    "List stays inside the content frame");
        // Provisional five actions: 2+2+1, each inside the details pane.
        const int inset = 12 * scale;
        const int gap = 8 * scale;
        const int actionWidth = (panes.detailsWidth - 2 * inset - gap) / 2;
        for (int index = 0; index < 5; ++index) {
            const int x = panes.detailsLeft + inset +
                (index % 2) * (actionWidth + gap);
            const int y = panes.detailsTop + 198 * scale +
                (index / 2) * 42 * scale;
            ExpectEqual(yes(x >= panes.detailsLeft && x + actionWidth <=
                            panes.detailsLeft + panes.detailsWidth), 1,
                        "Context action does not escape right pane");
            ExpectEqual(yes(y + 34 * scale <=
                            panes.detailsTop + panes.detailsHeight), 1,
                        "Context action does not collide with footer");
        }
    }
    {
        const auto footer = tp::LayoutRightAnchoredLaunchFooter(
            790, 20, 118, 350, 14);
        ExpectEqual(footer.buttonLeft, 288, "Footer group is right anchored");
        ExpectEqual(footer.buttonWidth, 118, "Launch button keeps compact width");
        ExpectEqual(footer.textLeft, 420, "Client details follow Launch button");
        ExpectEqual(footer.textWidth, 350, "Client details retain preferred width");
        ExpectEqual(footer.textLeft + footer.textWidth, 770,
                    "Client details end at right margin");
        const auto wide = tp::LayoutRightAnchoredLaunchFooter(
            1560, 20, 118, 350, 14);
        ExpectEqual(wide.textLeft + wide.textWidth, 1540,
                    "Right alignment survives maximize");
        ExpectEqual(wide.buttonLeft, 1058,
                    "Launch button travels with footer details");
        const auto narrow = tp::LayoutRightAnchoredLaunchFooter(
            480, 20, 118, 350, 14);
        ExpectEqual(narrow.buttonLeft, 20,
                    "Compact footer stays within left margin");
        ExpectEqual(narrow.textWidth, 308,
                    "Client path shrinks instead of clipping the button");
        ExpectEqual(narrow.textLeft + narrow.textWidth, 460,
                    "Compact footer stays within right margin");
        const auto dpi = tp::LayoutRightAnchoredLaunchFooter(
            1185, 30, 177, 525, 21);
        ExpectEqual(dpi.textLeft + dpi.textWidth, 1155,
                    "DPI-scaled footer remains right anchored");
    }
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
