#pragma once

#include <algorithm>
#include <array>

namespace tp {


inline std::array<int, 2> FitCompactColumnWidths(
    int firstWidth,
    int secondWidth,
    int totalWidth,
    int firstMinimumWidth,
    int secondMinimumWidth) {
    const int total =
        std::max(
            0,
            totalWidth);
    const int firstMinimum =
        std::max(
            0,
            firstMinimumWidth);
    const int secondMinimum =
        std::max(
            0,
            secondMinimumWidth);

    if (total <=
        firstMinimum +
            secondMinimum) {
        const int first =
            std::min(
                total,
                firstMinimum);
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
            firstMinimum,
            total - secondMinimum);

    return {
        first,
        total - first};
}

inline std::array<int, 2> CompactDividerWidths(
    int proposedLeftWidth,
    int totalWidth,
    int leftMinimumWidth,
    int rightMinimumWidth) {
    const int total =
        std::max(
            0,
            totalWidth);
    const int leftMinimum =
        std::max(
            0,
            leftMinimumWidth);
    const int rightMinimum =
        std::max(
            0,
            rightMinimumWidth);

    if (total <=
        leftMinimum +
            rightMinimum) {
        const int left =
            std::min(
                total,
                leftMinimum);
        return {
            left,
            total - left};
    }

    const int left =
        std::clamp(
            proposedLeftWidth,
            leftMinimum,
            total - rightMinimum);

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
