#include "update_all.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

tp::PackageRecord Package(
    std::wstring id,
    std::wstring installed,
    std::wstring latest) {
    tp::PackageRecord package;
    package.id = std::move(id);
    package.name = package.id;
    package.provider = L"github";
    package.repository = L"Owner/Repo";
    package.mode = L"branch";
    package.ref = L"main";
    package.target = L"addons";
    package.installedRevision = std::move(installed);
    package.latestRevision = std::move(latest);
    if (!package.installedRevision.empty()) {
        package.installedFiles = {
            L"Interface/AddOns/Test/Test.toc"
        };
    }
    return package;
}

} // namespace

int main() {
    tp::AppState state;

    auto current =
        Package(
            L"github:Owner/Current",
            L"1111111111111111111111111111111111111111",
            L"1111111111111111111111111111111111111111");

    auto update =
        Package(
            L"github:Owner/Update",
            L"2222222222222222222222222222222222222222",
            L"3333333333333333333333333333333333333333");

    auto notInstalled =
        Package(
            L"github:Owner/NotInstalled",
            L"",
            L"4444444444444444444444444444444444444444");

    auto gitlab = update;
    gitlab.id = L"gitlab:Owner/Repo";
    gitlab.provider = L"gitlab";

    auto unconfigured = update;
    unconfigured.id = L"github:Owner/Unconfigured";
    unconfigured.mode = L"unconfigured";
    unconfigured.ref.clear();

    auto missingOwnership = update;
    missingOwnership.id = L"github:Owner/MissingOwnership";
    missingOwnership.installedFiles.clear();

    state.packages = {
        current,
        update,
        notInstalled,
        gitlab,
        unconfigured,
        missingOwnership
    };

    auto progress =
        tp::MakeUpdateAllProgress(state);

    if (progress.packageIds.size() != 2 ||
        progress.packageIds[0] != current.id ||
        progress.packageIds[1] != update.id) {
        Fail("Update All candidate selection was incorrect");
    }

    if (!tp::UpdateAllHasCurrent(progress) ||
        tp::UpdateAllCurrentPackageId(progress) != current.id) {
        Fail("Update All did not start at the first candidate");
    }

    std::wstring error;
    if (!tp::CompleteUpdateAllItem(
            progress,
            tp::UpdateAllOutcome::Failed,
            L"Current: simulated failure",
            error)) {
        Fail("Update All could not record a failed package");
    }

    if (!tp::UpdateAllHasCurrent(progress) ||
        tp::UpdateAllCurrentPackageId(progress) != update.id ||
        progress.failed != 1 ||
        progress.failures.size() != 1) {
        Fail("Update All failure did not advance to the next package");
    }

    if (!tp::CompleteUpdateAllItem(
            progress,
            tp::UpdateAllOutcome::Updated,
            {},
            error)) {
        Fail("Update All could not record an updated package");
    }

    if (tp::UpdateAllHasCurrent(progress) ||
        progress.updated != 1 ||
        progress.current != 0 ||
        progress.failed != 1) {
        Fail("Update All summary counts were incorrect");
    }

    if (tp::CompleteUpdateAllItem(
            progress,
            tp::UpdateAllOutcome::Current,
            {},
            error) ||
        error.empty()) {
        Fail("Update All accepted completion after the queue ended");
    }

    auto allCurrent =
        tp::MakeUpdateAllProgress(
            tp::AppState{
                1,
                {},
                {
                    current,
                    update
                },
                {}
            });

    if (!tp::CompleteUpdateAllItem(
            allCurrent,
            tp::UpdateAllOutcome::Current,
            {},
            error) ||
        !tp::CompleteUpdateAllItem(
            allCurrent,
            tp::UpdateAllOutcome::Current,
            {},
            error) ||
        allCurrent.current != 2 ||
        allCurrent.updated != 0 ||
        allCurrent.failed != 0) {
        Fail("Update All current-item accounting was incorrect");
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " Update All test(s) failed\n";
        return 1;
    }

    std::cout << "Update All tests passed\n";
    return 0;
}
