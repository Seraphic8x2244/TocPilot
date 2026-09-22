#include "update_all.h"

#include <iostream>
#include <string>
#include <utility>

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


tp::PackageRecord DirectDllPackage(
    std::wstring installed,
    std::wstring latest) {
    tp::PackageRecord package;
    package.id =
        L"github:Owner/ClassicAPI:release:ClassicAPI.dll";
    package.name =
        L"ClassicAPI.dll";
    package.provider =
        L"github";
    package.repository =
        L"Owner/ClassicAPI";
    package.mode =
        L"release";
    package.releasePolicy =
        L"latest_stable";
    package.asset =
        L"ClassicAPI.dll";
    package.target =
        L"wow_root";
    package.targetPath =
        L"ClassicAPI.dll";
    package.installedRevision =
        std::move(installed);
    package.latestRevision =
        std::move(latest);
    if (!package.installedRevision.empty()) {
        package.installedFiles = {
            L"ClassicAPI.dll"
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

    auto directDll =
        DirectDllPackage(
            L"v1.0.0",
            L"v1.1.0");

    auto directDllCurrent =
        DirectDllPackage(
            L"v1.1.0",
            L"v1.1.0");
    directDllCurrent.id =
        L"github:Owner/ClassicAPI:release:Current.dll";
    directDllCurrent.asset =
        L"Current.dll";
    directDllCurrent.targetPath =
        L"Current.dll";
    directDllCurrent.installedFiles = {
        L"Current.dll"
    };

    state.packages = {
        current,
        update,
        notInstalled,
        gitlab,
        unconfigured,
        missingOwnership,
        directDll,
        directDllCurrent
    };

    auto progress =
        tp::MakeUpdateAllProgress(state);

    if (progress.packageIds.size() != 2 ||
        progress.packageIds[0] != update.id ||
        progress.packageIds[1] !=
            directDll.id) {
        Fail("Update All should queue known addon and DLL updates");
    }

    if (!tp::UpdateAllHasCurrent(progress) ||
        tp::UpdateAllCurrentPackageId(progress) != update.id) {
        Fail("Update All did not start at the known update");
    }

    std::wstring error;
    if (!tp::CompleteUpdateAllItem(
            progress,
            tp::UpdateAllOutcome::Updated,
            {},
            error)) {
        Fail("Update All could not record an updated package");
    }

    if (!tp::UpdateAllHasCurrent(progress) ||
        tp::UpdateAllCurrentPackageId(progress) !=
            directDll.id ||
        progress.updated != 1 ||
        progress.current != 0 ||
        progress.failed != 0) {
        Fail("Update All did not advance from addon to DLL update");
    }

    if (!tp::CompleteUpdateAllItem(
            progress,
            tp::UpdateAllOutcome::Updated,
            {},
            error) ||
        tp::UpdateAllHasCurrent(progress) ||
        progress.updated != 2) {
        Fail("Update All could not record the DLL update");
    }

    if (tp::CompleteUpdateAllItem(
            progress,
            tp::UpdateAllOutcome::Current,
            {},
            error) ||
        error.empty()) {
        Fail("Update All accepted completion after the queue ended");
    }

    auto noKnownUpdates =
        tp::MakeUpdateAllProgress(
            tp::AppState{
                1,
                {},
                {
                    current,
                    notInstalled
                },
                {}
            });

    if (tp::UpdateAllHasCurrent(noKnownUpdates) ||
        !noKnownUpdates.packageIds.empty()) {
        Fail("Update All queued packages without a known update");
    }

    tp::UpdateAllProgress accounting;
    accounting.packageIds = {
        L"one",
        L"two"
    };

    if (!tp::CompleteUpdateAllItem(
            accounting,
            tp::UpdateAllOutcome::Current,
            {},
            error) ||
        !tp::CompleteUpdateAllItem(
            accounting,
            tp::UpdateAllOutcome::Failed,
            L"two: simulated failure",
            error) ||
        accounting.current != 1 ||
        accounting.updated != 0 ||
        accounting.failed != 1) {
        Fail("Update All outcome accounting was incorrect");
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
