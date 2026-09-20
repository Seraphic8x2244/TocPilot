#include "refresh_freshness.h"

#include <utility>

namespace tp {

void RecordPackageRefresh(
    std::vector<PackageRefreshStamp>& stamps,
    const PackageRecord& package,
    std::uint64_t refreshedAtMs) {
    for (auto& stamp :
         stamps) {
        if (stamp.packageId ==
            package.id) {
            stamp.branch =
                package.ref;
            stamp.latestRevision =
                package.latestRevision;
            stamp.refreshedAtMs =
                refreshedAtMs;
            return;
        }
    }

    PackageRefreshStamp stamp;
    stamp.packageId =
        package.id;
    stamp.branch =
        package.ref;
    stamp.latestRevision =
        package.latestRevision;
    stamp.refreshedAtMs =
        refreshedAtMs;
    stamps.push_back(
        std::move(stamp));
}

bool IsPackageRefreshFresh(
    const std::vector<PackageRefreshStamp>& stamps,
    const PackageRecord& package,
    std::uint64_t nowMs,
    std::uint64_t maxAgeMs) {
    if (package.id.empty() ||
        package.ref.empty() ||
        package.latestRevision.empty()) {
        return false;
    }

    for (const auto& stamp :
         stamps) {
        if (stamp.packageId !=
                package.id ||
            stamp.branch !=
                package.ref ||
            stamp.latestRevision !=
                package.latestRevision) {
            continue;
        }

        if (nowMs <
            stamp.refreshedAtMs) {
            return false;
        }

        return nowMs -
                stamp.refreshedAtMs <=
            maxAgeMs;
    }

    return false;
}

} // namespace tp
