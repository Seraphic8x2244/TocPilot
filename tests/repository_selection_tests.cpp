#include "repository_selection.h"

#include <iostream>
#include <string>
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

tp::AddonCandidate Addon(
    std::wstring path,
    std::wstring name) {
    tp::AddonCandidate addon;
    addon.repositoryRelativePath =
        std::move(path);
    addon.installFolder =
        std::move(name);
    return addon;
}

tp::RepositoryCandidate AddonCandidate(
    std::wstring path,
    std::wstring name) {
    tp::RepositoryCandidate candidate;
    candidate.kind =
        tp::RepositoryCandidateKind::Addon;
    candidate.addon =
        Addon(
            std::move(path),
            std::move(name));
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
    tp::RepositoryDiscoveryResult result;
    result.provider =
        L"github";
    result.repository =
        L"Owner/Repo";
    result.branch =
        L"dev";
    result.addonLayout.kind =
        tp::RepositoryAddonLayoutKind::RepositoryLibrary;

    result.candidates.push_back(
        AddonCandidate(
            L"Alpha",
            L"Alpha"));
    result.candidates.push_back(
        AddonCandidate(
            L"Beta",
            L"Beta"));
    result.candidates.push_back(
        ReleaseCandidate(
            tp::RepositoryCandidateKind::Dll,
            L"ClassicAPI.dll",
            L"v1.2.3"));
    result.candidates.push_back(
        ReleaseCandidate(
            tp::RepositoryCandidateKind::Mpq,
            L"WideLoadScreens.mpq",
            L"v1.2.3"));
    return result;
}

void TestRows() {
    const auto discovery =
        Discovery();

    std::vector<std::wstring>
        destinations(
            discovery.candidates.size());
    std::vector<std::wstring>
        errors(
            discovery.candidates.size());

    destinations[3] =
        L"Data/patch-C.mpq";

    std::vector<tp::RepositorySelectionRow>
        rows;
    std::wstring error;

    if (!tp::BuildRepositorySelectionRows(
            discovery,
            destinations,
            errors,
            rows,
            error)) {
        Fail(
            "selection rows failed to build");
        return;
    }

    if (rows.size() != 4 ||
        rows[0].type !=
            L"Library addon" ||
        rows[0].source !=
            L"Branch: dev" ||
        rows[0].destination !=
            L"Interface\\AddOns\\Alpha" ||
        rows[2].type !=
            L"DLL" ||
        rows[2].source !=
            L"Latest stable release: v1.2.3" ||
        rows[2].destination !=
            L"WoW root\\ClassicAPI.dll" ||
        !rows[2].requiresDllTrust ||
        rows[3].type !=
            L"MPQ" ||
        rows[3].destination !=
            L"Data/patch-C.mpq") {
        Fail(
            "selection row labels did not preserve type/source/destination context");
    }
}

void TestUnavailableMpq() {
    const auto discovery =
        Discovery();

    std::vector<std::wstring>
        destinations(
            discovery.candidates.size());
    std::vector<std::wstring>
        errors(
            discovery.candidates.size());

    errors[3] =
        L"No free slot";

    std::vector<tp::RepositorySelectionRow>
        rows;
    std::wstring error;

    if (!tp::BuildRepositorySelectionRows(
            discovery,
            destinations,
            errors,
            rows,
            error) ||
        rows[3].selectable ||
        rows[3].destination !=
            L"Unavailable" ||
        rows[3].unavailableReason !=
            L"No free slot") {
        Fail(
            "unavailable MPQ destination was not represented without hiding other candidates");
    }
}

void TestExecutionBoundary() {
    const auto discovery =
        Discovery();
    std::wstring error;

    if (tp::ClassifyRepositorySelection(
            discovery,
            {0, 1},
            error) !=
        tp::RepositorySelectionExecution::AddonsOnly) {
        Fail(
            "addon-only multi-selection did not preserve the existing addon queue path");
    }

    if (tp::ClassifyRepositorySelection(
            discovery,
            {2},
            error) !=
        tp::RepositorySelectionExecution::SingleDll) {
        Fail(
            "single DLL selection did not preserve the existing DLL path");
    }

    if (tp::ClassifyRepositorySelection(
            discovery,
            {3},
            error) !=
        tp::RepositorySelectionExecution::Deferred) {
        Fail(
            "MPQ selection crossed into Phase 5 orchestration");
    }

    if (tp::ClassifyRepositorySelection(
            discovery,
            {0, 2, 3},
            error) !=
        tp::RepositorySelectionExecution::Deferred) {
        Fail(
            "mixed selection crossed into Phase 5 orchestration");
    }

    error.clear();
    if (tp::ClassifyRepositorySelection(
            discovery,
            {2, 2},
            error) !=
            tp::RepositorySelectionExecution::Deferred ||
        error.empty()) {
        Fail(
            "duplicate selection was not rejected deterministically");
    }
}

} // namespace

int main() {
    TestRows();
    TestUnavailableMpq();
    TestExecutionBoundary();

    if (failures != 0) {
        std::cerr
            << failures
            << " repository selection test(s) failed\n";
        return 1;
    }

    std::cout
        << "Repository selection tests passed\n";
    return 0;
}
