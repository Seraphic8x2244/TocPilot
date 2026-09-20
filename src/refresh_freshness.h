#pragma once

#include "state.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tp {

struct PackageRefreshStamp {
    std::wstring packageId;
    std::wstring branch;
    std::wstring latestRevision;
    std::uint64_t refreshedAtMs = 0;
};

constexpr std::uint64_t kPackageRefreshFreshnessMs =
    5ull * 60ull * 1000ull;

void RecordPackageRefresh(
    std::vector<PackageRefreshStamp>& stamps,
    const PackageRecord& package,
    std::uint64_t refreshedAtMs);

bool IsPackageRefreshFresh(
    const std::vector<PackageRefreshStamp>& stamps,
    const PackageRecord& package,
    std::uint64_t nowMs,
    std::uint64_t maxAgeMs =
        kPackageRefreshFreshnessMs);

} // namespace tp
