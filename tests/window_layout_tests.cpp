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

    {
        const auto widths =
            tp::FitCompactColumnWidths(
                240,
                160,
                500,
                40);
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
                40);
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
                40,
                300,
                40);
        ExpectEqual(
            widths[0],
            260,
            "Compact fitting should preserve the minimum width of the second column");
        ExpectEqual(
            widths[1],
            40,
            "Compact fitting should clamp only when required by the existing minimum");
    }

    {
        const auto widths =
            tp::CompactDividerWidths(
                175,
                500,
                40);
        ExpectEqual(
            widths[0],
            175,
            "middle-divider drag should keep the proposed left width");
        ExpectEqual(
            widths[1],
            325,
            "middle-divider drag should resize the right column inversely");
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
                40);
        ExpectEqual(
            widths[0],
            460,
            "middle-divider drag should preserve the right column minimum");
        ExpectEqual(
            widths[1],
            40,
            "middle-divider drag should keep the total width fixed");
    }

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
