#include "archive.h"
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

void TestRootAddonIdentity(
    const std::filesystem::path& root) {
    const auto extractedRoot =
        root /
        L"root-addon-discovery";
    const auto wrapper =
        extractedRoot /
        L"Performante-main";

    std::error_code ec;
    std::filesystem::remove_all(
        extractedRoot,
        ec);
    ec.clear();
    std::filesystem::create_directories(
        wrapper,
        ec);

    if (ec) {
        Fail(
            "could not create root-addon discovery fixture");
        return;
    }

    {
        std::ofstream toc(
            wrapper /
                L"Performante.toc",
            std::ios::binary);
        toc << "## Interface: 11200\n";
    }

    tp::RepositoryAddonLayout layout;
    std::wstring error;

    if (!tp::DetectShallowRepositoryAddonLayout(
            extractedRoot,
            L"GitHub",
            L"Seraphic8x2244/Performante",
            layout,
            error) ||
        layout.kind !=
            tp::RepositoryAddonLayoutKind::RootAddon ||
        layout.candidates.size() != 1 ||
        !layout.candidates.front()
             .repositoryRelativePath.empty()) {
        Fail(
            "real shallow root discovery did not emit the expected empty repository-relative path");
        return;
    }

    tp::RepositoryDiscoveryResult discovery;
    discovery.provider =
        L"github";
    discovery.repository =
        L"Seraphic8x2244/Performante";
    discovery.branch =
        L"main";
    discovery.branchRevision =
        L"0123456789abcdef";
    discovery.addonLayout =
        layout;

    tp::RepositoryCandidate candidate;
    candidate.kind =
        tp::RepositoryCandidateKind::Addon;
    candidate.addon =
        layout.candidates.front();
    discovery.candidates.push_back(
        candidate);

    std::vector<tp::RepositoryInstallItem>
        items;

    if (!tp::BuildRepositoryInstallItems(
            discovery,
            {0},
            root,
            {},
            items,
            error) ||
        items.size() != 1) {
        Fail(
            "real empty root discovery path could not build a package");
        return;
    }

    const auto& rootPackage =
        items.front().package;

    if (rootPackage.id !=
            L"github:Seraphic8x2244/Performante" ||
        rootPackage.sourcePath !=
            L"." ||
        rootPackage.name !=
            L"Performante" ||
        rootPackage.ref !=
            L"main" ||
        rootPackage.latestRevision !=
            L"0123456789abcdef") {
        Fail(
            "real empty root discovery path did not normalize to repository identity");
        return;
    }

    tp::AppState state;
    if (!tp::AppendPackage(
            state,
            rootPackage,
            error)) {
        Fail(
            "normalized root package did not pass strict semantic validation");
        return;
    }

    if (!tp::SaveState(
            root,
            state,
            error)) {
        Fail(
            "normalized root package could not be saved");
        return;
    }

    if (!tp::SetPackageInstalledState(
            state.packages.front(),
            L"0123456789abcdef",
            {L"Interface/AddOns/Performante/Performante.toc"},
            error) ||
        !tp::SaveState(
            root,
            state,
            error)) {
        Fail(
            "normalized root package could not commit valid installed state");
        return;
    }

    tp::PackageRecord malformed =
        rootPackage;
    malformed.id =
        L"github:Seraphic8x2244/Performante:addon:";

    tp::AppState malformedState;
    error.clear();

    if (tp::AppendPackage(
            malformedState,
            std::move(malformed),
            error) ||
        error.find(
            L"id that does not match") ==
            std::wstring::npos) {
        Fail(
            "strict package identity validation no longer rejects malformed root ids");
        return;
    }

    tp::RepositoryDiscoveryResult
        dotDiscovery =
            discovery;
    dotDiscovery.candidates.clear();
    dotDiscovery.candidates.push_back(
        AddonCandidate(
            L".",
            L"Performante"));

    items.clear();
    error.clear();

    if (!tp::BuildRepositoryInstallItems(
            dotDiscovery,
            {0},
            root,
            {},
            items,
            error) ||
        items.size() != 1 ||
        items.front().package.id !=
            L"github:Seraphic8x2244/Performante" ||
        items.front().package.sourcePath !=
            L".") {
        Fail(
            "literal dot root compatibility did not normalize to repository identity");
    }
}

void TestReservedDllDestination(
    const std::filesystem::path& root) {
    auto discovery =
        Discovery();

    tp::PackageRecord existing =
        tp::MakeRepositoryPackage(
            L"github",
            L"Other/ClassicAPI");
    existing.id =
        L"github:Other/ClassicAPI:release:ClassicAPI.dll";
    existing.name =
        L"ClassicAPI.dll";
    existing.mode =
        L"release";
    existing.releasePolicy =
        L"latest_stable";
    existing.asset =
        L"ClassicAPI.dll";
    existing.target =
        L"wow_root";
    existing.targetPath =
        L"ClassicAPI.dll";

    std::vector<tp::RepositoryInstallItem>
        items;
    std::wstring error;

    if (tp::BuildRepositoryInstallItems(
            discovery,
            {1},
            root,
            {existing},
            items,
            error) ||
        error.find(
            L"already reserved") ==
            std::wstring::npos) {
        Fail(
            "existing direct DLL destination reservation was not preserved");
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
        TestRootAddonIdentity(
            root);
        TestMixedPackageCreationAndQueue(
            root);
        TestReservedDllDestination(
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
