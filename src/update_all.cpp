#include "update_all.h"

#include <utility>

namespace tp {

bool IsUpdateAllCandidate(const PackageRecord& package) {
    return
        package.provider == L"github" &&
        package.mode == L"branch" &&
        !package.ref.empty() &&
        package.target == L"addons" &&
        !package.installedRevision.empty() &&
        !package.installedFiles.empty();
}

UpdateAllProgress MakeUpdateAllProgress(const AppState& state) {
    UpdateAllProgress progress;

    for (const auto& package : state.packages) {
        if (IsUpdateAllCandidate(package)) {
            progress.packageIds.push_back(package.id);
        }
    }

    return progress;
}

bool UpdateAllHasCurrent(const UpdateAllProgress& progress) {
    return progress.position < progress.packageIds.size();
}

std::wstring_view UpdateAllCurrentPackageId(
    const UpdateAllProgress& progress) {
    if (!UpdateAllHasCurrent(progress)) {
        return {};
    }

    return progress.packageIds[progress.position];
}

bool CompleteUpdateAllItem(
    UpdateAllProgress& progress,
    UpdateAllOutcome outcome,
    std::wstring failureMessage,
    std::wstring& error) {
    if (!UpdateAllHasCurrent(progress)) {
        error = L"Update All has no active package to complete.";
        return false;
    }

    switch (outcome) {
    case UpdateAllOutcome::Current:
        ++progress.current;
        break;

    case UpdateAllOutcome::Updated:
        ++progress.updated;
        break;

    case UpdateAllOutcome::Failed:
        ++progress.failed;
        if (!failureMessage.empty()) {
            progress.failures.push_back(
                std::move(failureMessage));
        }
        break;
    }

    ++progress.position;
    error.clear();
    return true;
}

} // namespace tp
