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
