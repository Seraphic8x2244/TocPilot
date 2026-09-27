#pragma once

#include <algorithm>
#include <array>

namespace tp {


inline std::array<int, 2> FitCompactColumnWidths(
    int firstWidth,
    int secondWidth,
    int totalWidth,
    int minimumWidth) {
    const int total =
        std::max(
            0,
            totalWidth);
    const int minimum =
        std::max(
            0,
            minimumWidth);

    if (total <=
        minimum * 2) {
        const int first =
            total / 2;
        return {
            first,
            total - first};
    }

    const int delta =
        total -
        firstWidth -
        secondWidth;
    const int first =
        std::clamp(
            firstWidth +
                delta / 2,
            minimum,
            total - minimum);

    return {
        first,
        total - first};
}

inline std::array<int, 2> CompactDividerWidths(
    int proposedLeftWidth,
    int totalWidth,
    int minimumWidth) {
    const int total =
        std::max(
            0,
            totalWidth);
    const int minimum =
        std::max(
            0,
            minimumWidth);

    if (total <=
        minimum * 2) {
        const int left =
            total / 2;
        return {
            left,
            total - left};
    }

    const int left =
        std::clamp(
            proposedLeftWidth,
            minimum,
            total - minimum);

    return {
        left,
        total - left};
}

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
