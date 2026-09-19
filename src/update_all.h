#pragma once

#include "state.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

enum class UpdateAllOutcome {
    Current,
    Updated,
    Failed
};

struct UpdateAllProgress {
    std::vector<std::wstring> packageIds;
    std::size_t position = 0;
    std::size_t current = 0;
    std::size_t updated = 0;
    std::size_t failed = 0;
    std::vector<std::wstring> failures;
};

bool IsUpdateAllCandidate(const PackageRecord& package);

UpdateAllProgress MakeUpdateAllProgress(const AppState& state);

bool UpdateAllHasCurrent(const UpdateAllProgress& progress);

std::wstring_view UpdateAllCurrentPackageId(
    const UpdateAllProgress& progress);

bool CompleteUpdateAllItem(
    UpdateAllProgress& progress,
    UpdateAllOutcome outcome,
    std::wstring failureMessage,
    std::wstring& error);

} // namespace tp
