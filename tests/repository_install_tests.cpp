#include "repository_install.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void Fail(
    const char* message) {
    std::cerr
        << message
        << '\n';
    ++failures;
}

tp::RepositoryCandidate AddonCandidate(
    std::wstring path,
    std::wstring name) {
    tp::RepositoryCandidate candidate;
    candidate.kind =
        tp::RepositoryCandidateKind::Addon;
    candidate.addon.repositoryRelativePath =
        std::move(path);
    candidate.addon.installFolder =
        std::move(name);
    return candidate;
}

tp::RepositoryCandidate ReleaseCandidate(
    tp::RepositoryCandidateKind kind,
    std::wstring asset,
    std::wstring tag) {
    tp::RepositoryCandidate candidate;
    candidate.kind = kind;
    candidate.releaseAsset.name =
        std::move(asset);
    candidate.releaseTag =
        std::move(tag);
    return candidate;
}

tp::RepositoryDiscoveryResult Discovery() {
    tp::RepositoryDiscoveryResult discovery;
    discovery.provider =
        L"github";
    discovery.repository =
        L"Owner/MixedRepo";
    discovery.branch =
        L"dev";
    discovery.branchRevision =
        L"0123456789abcdef";

    discovery.candidates.push_back(
        AddonCandidate(
            L"Alpha",
            L"Alpha"));
    discovery.candidates.push_back(
        ReleaseCandidate(
            tp::RepositoryCandidateKind::Dll,
            L"ClassicAPI.dll",
            L"v2.0.0"));
    discovery.candidates.push_back(
        ReleaseCandidate(
            tp::RepositoryCandidateKind::Mpq,
            L"WideLoadScreens.mpq",
            L"v2.0.0"));
    return discovery;
}

std::filesystem::path MakeRoot() {
    const auto root =
        std::filesystem::temp_directory_path() /
        L"TocPilotRepositoryInstallTests";

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    ec.clear();
    std::filesystem::create_directories(
        root / L"Data",
        ec);

    if (ec) {
        return {};
    }

    return root;
}

void TestMixedPackageCreationAndQueue(
    const std::filesystem::path& root) {
    {
        std::ofstream unmanaged(
            root / L"Data" /
                L"patch-A.mpq",
            std::ios::binary);
        unmanaged << "existing";
    }

    tp::PackageRecord reserved =
        tp::MakeRepositoryPackage(
            L"github",
            L"Other/Reserved");
    reserved.id =
        L"github:Other/Reserved:release:Reserved.mpq";
    reserved.name =
        L"Reserved.mpq";
    reserved.mode =
        L"release";
    reserved.releasePolicy =
        L"latest_stable";
    reserved.asset =
        L"Reserved.mpq";
    reserved.target =
        L"data";
    reserved.targetPath =
        L"Data/patch-B.mpq";
    reserved.latestRevision =
        L"v1";

    const auto discovery =
        Discovery();

    std::vector<tp::RepositoryInstallItem>
        items;
    std::wstring error;

    if (!tp::BuildRepositoryInstallItems(
            discovery,
            {2, 0, 1},
            root,
            {reserved},
            items,
            error)) {
        Fail(
            "mixed repository selection did not build");
        return;
    }

    if (items.size() != 3 ||
        items[0].kind !=
            tp::RepositoryCandidateKind::Addon ||
        items[1].kind !=
            tp::RepositoryCandidateKind::Dll ||
        items[2].kind !=
            tp::RepositoryCandidateKind::Mpq) {
        Fail(
            "mixed selection order was not deterministic");
        return;
    }

    if (items[0].package.id !=
            L"github:Owner/MixedRepo:addon:Alpha" ||
        items[0].package.ref !=
            L"dev" ||
        items[0].package.latestRevision !=
            L"0123456789abcdef" ||
        items[1].package.id !=
            L"github:Owner/MixedRepo:release:ClassicAPI.dll" ||
        items[1].package.targetPath !=
            L"ClassicAPI.dll" ||
        items[2].package.id !=
            L"github:Owner/MixedRepo:release:WideLoadScreens.mpq" ||
        items[2].package.targetPath !=
            L"Data/patch-C.mpq") {
        Fail(
            "mixed package records were not independent or did not preserve destination allocation");
        return;
    }

    tp::AppState state;
    state.packages.push_back(
        reserved);

    std::vector<std::size_t>
        installIndices;

    for (auto& item : items) {
        if (!tp::AppendPackage(
                state,
                std::move(item.package),
                error)) {
            Fail(
                "independent mixed package record could not be appended");
            return;
        }

        installIndices.push_back(
            state.packages.size() - 1);
    }

    tp::RepositoryInstallQueue queue;
    if (!tp::BuildRepositoryInstallQueue(
            state,
            installIndices,
            queue,
            error)) {
        Fail(
            "mixed install queue could not be built");
        return;
    }

    if (tp::RepositoryInstallQueueCurrentPackageId(
            queue) !=
        L"github:Owner/MixedRepo:addon:Alpha") {
        Fail(
            "mixed install queue did not begin with the deterministic first package");
        return;
    }

    if (!tp::SetPackageInstalledState(
            state.packages[
                installIndices[0]],
            L"0123456789abcdef",
            {L"Interface/AddOns/Alpha/Alpha.toc"},
            error) ||
        !tp::AdvanceRepositoryInstallQueue(
            queue,
            state.packages[
                installIndices[0]].id,
            error)) {
        Fail(
            "first mixed queue item could not commit independently");
        return;
    }

    if (tp::RepositoryInstallQueueCurrentPackageId(
            queue) !=
        L"github:Owner/MixedRepo:release:ClassicAPI.dll") {
        Fail(
            "mixed install queue did not advance to DLL");
        return;
    }

    // Simulate the DLL step failing before installed state advances.
    // The already committed addon remains installed, while the later MPQ
    // record remains untouched and available for a future retry.
    if (state.packages[
            installIndices[0]]
            .installedRevision.empty() ||
        !state.packages[
            installIndices[1]]
             .installedRevision.empty() ||
        !state.packages[
            installIndices[2]]
             .installedRevision.empty() ||
        state.packages[
            installIndices[2]]
            .targetPath !=
            L"Data/patch-C.mpq") {
        Fail(
            "partial failure boundary corrupted committed or unstarted package state");
    }
}

void TestSelectionValidation(
    const std::filesystem::path& root) {
    const auto discovery =
        Discovery();
    std::vector<tp::RepositoryInstallItem>
        items;
    std::wstring error;

    if (tp::BuildRepositoryInstallItems(
            discovery,
            {1, 1},
            root,
            {},
            items,
            error) ||
        error.empty()) {
        Fail(
            "duplicate mixed selection was not rejected");
    }

    tp::AppState state;
    tp::PackageRecord package =
        tp::MakeRepositoryPackage(
            L"github",
            L"Owner/One");
    package.mode =
        L"branch";
    package.ref =
        L"main";
    package.latestRevision =
        L"abc";
    state.packages.push_back(
        package);

    tp::RepositoryInstallQueue queue;
    error.clear();

    if (tp::BuildRepositoryInstallQueue(
            state,
            {0, 0},
            queue,
            error) ||
        error.empty()) {
        Fail(
            "duplicate install queue package was not rejected");
    }
}

} // namespace

int main() {
    const auto root =
        MakeRoot();

    if (root.empty()) {
        Fail(
            "could not create repository install test root");
    } else {
        TestMixedPackageCreationAndQueue(
            root);
        TestSelectionValidation(
            root);

        std::error_code ec;
        std::filesystem::remove_all(
            root,
            ec);
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " repository install test(s) failed\n";
        return 1;
    }

    std::cout
        << "Repository install tests passed\n";
    return 0;
}
