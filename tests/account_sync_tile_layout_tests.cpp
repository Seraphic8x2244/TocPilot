#include "account_sync_tile_layout.h"

#include <iostream>
#include <limits>

namespace {
int failures = 0;
void Check(bool condition, const char* label) {
    if (!condition) {
        std::cerr << label << "\n";
        ++failures;
    }
}
} // namespace

int main() {
    using namespace tp;
    for (const unsigned dpi : {96U, 120U, 144U, 192U}) {
        const int tile = ScaleSyncTile(kSyncTileWidth, dpi);
        const int gap = ScaleSyncTile(kSyncTileGap, dpi);
        for (const std::size_t count : {2U, 3U, 5U, 12U}) {
            const int content = SyncTileContentWidth(count, dpi);
            Check(content == SyncTileX(count - 1, dpi) + tile +
                ScaleSyncTile(kSyncTileMargin, dpi),
                "content extent must include every peer tile");
            for (std::size_t i = 1; i < count; ++i) {
                Check(SyncTileX(i, dpi) - SyncTileX(i - 1, dpi) ==
                    tile + gap,
                    "tiles must share a fixed width and a fixed gap");
            }
            for (int row = 1; row < kSyncTileVisibleRowCount; ++row) {
                Check(SyncTileRowY(row, dpi) - SyncTileRowY(row - 1, dpi) ==
                    ScaleSyncTile(kSyncTileRowStep, dpi),
                    "row baselines must align across accounts");
            }
        }
        const int two = SyncTileContentWidth(2, dpi);
        const int three = SyncTileContentWidth(3, dpi);
        Check(SyncTileMaximumScroll(two, two) == 0,
            "two tiles fitting viewport have no horizontal scroll");
        Check(SyncTileMaximumScroll(three, two) > 0,
            "a third tile enables scrolling without resizing");
        Check(ClampSyncTileScroll(-10, three, two) == 0,
            "scroll cannot move left of first tile");
        Check(ClampSyncTileScroll(std::numeric_limits<int>::max(), three, two) ==
            three - two, "scroll reaches the last tile");
        Check(ClampSyncTileScroll(200, two, three) == 0,
            "scroll resets when all tiles fit");
    }

    Check(SyncTileContentWidth(0, 96) == 0,
        "no-account projection produces no tile strip");
    Check(SyncTileContentWidth(
        std::numeric_limits<std::size_t>::max(), 96) ==
        std::numeric_limits<int>::max(),
        "very large account counts saturate native scroll coordinates");

    // A -> C makes one marker beside C, not A -> B -> C.
    Check(SyncTileTargetGap(0, 2) == 1 &&
        SyncTileArrowPointsRight(0, 2),
        "distant right target gets one incoming marker");
    Check(SyncTileTargetGap(2, 0) == 0 &&
        !SyncTileArrowPointsRight(2, 0),
        "distant left target gets one incoming marker");
    Check(SyncTileTargetGap(1, 0) == 0 &&
        !SyncTileArrowPointsRight(1, 0),
        "adjacent left source direction");
    Check(SyncTileTargetGap(1, 2) == 1 &&
        SyncTileArrowPointsRight(1, 2),
        "adjacent right source direction");

    if (failures) {
        std::cerr << failures << " tile layout checks failed\n";
        return 1;
    }
    std::cout << "account sync tile layout checks passed\n";
    return 0;
}
