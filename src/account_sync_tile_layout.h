#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>

namespace tp {

// Logical pixels (96 DPI) shared by the renderer and layout regression tests.
inline constexpr int kSyncTilePanelHeight = 164;
inline constexpr int kSyncTileMinimumPackageListHeight = 120;
inline constexpr int kSyncTileWidth = 150;
inline constexpr int kSyncTileGap = 44;
inline constexpr int kSyncTileMargin = 12;
inline constexpr int kSyncTileHeight = 139;
inline constexpr int kSyncTileHeaderY = 9;
inline constexpr int kSyncTileFirstRowY = 35;
inline constexpr int kSyncTileRowStep = 20;

// Keybinds, Macros, neutral pfUI header, indented Settings and Cache.
inline constexpr int kSyncTileVisibleRowCount = 5;

inline int ScaleSyncTile(int logical, unsigned dpi) {
    const auto scaled = static_cast<long long>(logical) *
        static_cast<long long>(dpi == 0 ? 96 : dpi);
    return static_cast<int>(std::min(
        scaled / 96, static_cast<long long>(std::numeric_limits<int>::max())));
}

inline int SyncTileContentWidth(std::size_t count, unsigned dpi) {
    if (count == 0) return 0;
    const auto width = static_cast<long long>(ScaleSyncTile(kSyncTileWidth, dpi));
    const auto gap = static_cast<long long>(ScaleSyncTile(kSyncTileGap, dpi));
    const auto margin = static_cast<long long>(ScaleSyncTile(kSyncTileMargin, dpi));
    // Account count is not capped; only the Win32 signed pixel range is.
    const auto max = static_cast<unsigned long long>(
        std::numeric_limits<int>::max());
    const auto n = static_cast<unsigned long long>(count);
    if (n > max / static_cast<unsigned long long>(width + gap)) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(std::min(
        max, 2ULL * static_cast<unsigned long long>(margin) +
            n * static_cast<unsigned long long>(width) +
            (n - 1) * static_cast<unsigned long long>(gap)));
}

inline int SyncTileX(std::size_t index, unsigned dpi) {
    const auto stride = static_cast<unsigned long long>(
        ScaleSyncTile(kSyncTileWidth + kSyncTileGap, dpi));
    const auto max = static_cast<unsigned long long>(
        std::numeric_limits<int>::max());
    if (index > max / stride) return std::numeric_limits<int>::max();
    return static_cast<int>(std::min(
        max, static_cast<unsigned long long>(
            ScaleSyncTile(kSyncTileMargin, dpi)) + index * stride));
}

inline int SyncTileRowY(int visualRow, unsigned dpi) {
    return ScaleSyncTile(
        kSyncTileFirstRowY + visualRow * kSyncTileRowStep, dpi);
}

inline int SyncTileMaximumScroll(int contentWidth, int viewportWidth) {
    return std::max(0, contentWidth - std::max(0, viewportWidth));
}

inline int ClampSyncTileScroll(int offset, int contentWidth, int viewportWidth) {
    return std::clamp(offset, 0,
        SyncTileMaximumScroll(contentWidth, viewportWidth));
}

// Place the arrow in the gutter next to its actual destination, not at
// every intermediate tile. Non-adjacent sources are named in the target row.
inline std::size_t SyncTileTargetGap(
    std::size_t sourceIndex, std::size_t targetIndex) {
    return sourceIndex < targetIndex ? targetIndex - 1 : targetIndex;
}

inline bool SyncTileArrowPointsRight(
    std::size_t sourceIndex, std::size_t targetIndex) {
    return sourceIndex < targetIndex;
}

} // namespace tp
