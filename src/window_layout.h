#pragma once

#include <algorithm>

namespace tp {

inline int ExpandedWindowWidth(
    int compactWidth,
    int compactMinimum,
    int advancedMinimum) {
    return std::max(
        advancedMinimum,
        compactWidth +
            (advancedMinimum - compactMinimum));
}

inline int RestoredCompactWindowWidth(
    int compactWidthBeforeAdvanced,
    int currentAdvancedWidth,
    int compactMinimum,
    int advancedMinimum) {
    if (compactWidthBeforeAdvanced > 0) {
        return std::max(
            compactMinimum,
            compactWidthBeforeAdvanced);
    }

    return std::max(
        compactMinimum,
        currentAdvancedWidth -
            (advancedMinimum - compactMinimum));
}

} // namespace tp
