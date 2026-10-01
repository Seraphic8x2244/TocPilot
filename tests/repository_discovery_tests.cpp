#include "repository_discovery.h"

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
    std::wstring installFolder) {
    tp::AddonCandidate addon;
    addon.repositoryRelativePath =
        std::move(path);
    addon.installFolder =
        std::move(installFolder);
    addon.tocFiles.push_back(
        addon.installFolder +
        L".toc");
    return addon;
}

tp::RepositoryAddonLayout Layout(
    tp::RepositoryAddonLayoutKind kind,
    std::vector<tp::AddonCandidate> addons) {
    tp::RepositoryAddonLayout layout;
    layout.kind = kind;
    layout.candidates =
        std::move(addons);
    return layout;
}

tp::GitHubReleaseAsset Asset(
    std::wstring name) {
    tp::GitHubReleaseAsset asset;
    asset.name =
        std::move(name);
    asset.downloadUrl =
        L"https://github.com/Owner/Repo/releases/download/v1.2.3/" +
        asset.name;
    asset.size = 1234;
    return asset;
}

tp::GitHubReleaseInfo Release(
    std::vector<std::wstring> names) {
    tp::GitHubReleaseInfo release;
    release.tag =
        L"v1.2.3";

    for (auto& name :
         names) {
        release.assets.push_back(
            Asset(std::move(name)));
    }

    return release;
}

std::size_t Count(
    const std::vector<tp::RepositoryCandidate>& candidates,
    tp::RepositoryCandidateKind kind) {
    std::size_t count = 0;

    for (const auto& candidate :
         candidates) {
        if (candidate.kind == kind) {
            ++count;
        }
    }

    return count;
}

void ExpectCounts(
    const std::vector<tp::RepositoryCandidate>& candidates,
    std::size_t addons,
    std::size_t dlls,
    std::size_t mpqs,
    const char* message) {
    if (Count(
            candidates,
            tp::RepositoryCandidateKind::Addon) !=
            addons ||
        Count(
            candidates,
            tp::RepositoryCandidateKind::Dll) !=
            dlls ||
        Count(
            candidates,
            tp::RepositoryCandidateKind::Mpq) !=
            mpqs) {
        Fail(message);
    }
}

void TestRequiredCombinations() {
    std::vector<tp::RepositoryCandidate>
        candidates;

    const auto root =
        Layout(
            tp::RepositoryAddonLayoutKind::RootAddon,
            {Addon(L".", L"Repo")});

    const auto library =
        Layout(
            tp::RepositoryAddonLayoutKind::RepositoryLibrary,
            {
                Addon(L"Alpha", L"Alpha"),
                Addon(L"Beta", L"Beta")
            });

    const auto none =
        Layout(
            tp::RepositoryAddonLayoutKind::None,
            {});

    const auto dll =
        Release({L"ClassicAPI.dll"});
    const auto mpq =
        Release({L"WideLoadScreens.mpq"});
    const auto dllMpq =
        Release({
            L"ClassicAPI.dll",
            L"WideLoadScreens.mpq"
        });

    tp::AggregateRepositoryCandidates(
        L"github",
        &root,
        nullptr,
        candidates);
    ExpectCounts(
        candidates,
        1,
        0,
        0,
        "addon-only discovery did not retain the addon candidate");

    tp::AggregateRepositoryCandidates(
        L"github",
        &library,
        nullptr,
        candidates);
    ExpectCounts(
        candidates,
        2,
        0,
        0,
        "library-only discovery did not retain every shallow addon candidate");

    tp::AggregateRepositoryCandidates(
        L"github",
        &none,
        &dll,
        candidates);
    ExpectCounts(
        candidates,
        0,
        1,
        0,
        "DLL-only discovery did not retain the exact DLL asset");

    tp::AggregateRepositoryCandidates(
        L"github",
        &none,
        &mpq,
        candidates);
    ExpectCounts(
        candidates,
        0,
        0,
        1,
        "MPQ-only discovery did not retain the exact MPQ asset");

    tp::AggregateRepositoryCandidates(
        L"github",
        &root,
        &dll,
        candidates);
    ExpectCounts(
        candidates,
        1,
        1,
        0,
        "addon+DLL discovery suppressed one candidate class");

    tp::AggregateRepositoryCandidates(
        L"github",
        &root,
        &mpq,
        candidates);
    ExpectCounts(
        candidates,
        1,
        0,
        1,
        "addon+MPQ discovery suppressed one candidate class");

    tp::AggregateRepositoryCandidates(
        L"github",
        &root,
        &dllMpq,
        candidates);
    ExpectCounts(
        candidates,
        1,
        1,
        1,
        "addon+DLL+MPQ discovery did not aggregate all valid classes");
}

void TestEmptyFailureAndAmbiguousIsolation() {
    std::vector<tp::RepositoryCandidate>
        candidates;

    const auto root =
        Layout(
            tp::RepositoryAddonLayoutKind::RootAddon,
            {Addon(L".", L"Repo")});

    const auto empty =
        Layout(
            tp::RepositoryAddonLayoutKind::None,
            {});

    const auto ambiguous =
        Layout(
            tp::RepositoryAddonLayoutKind::MixedAmbiguous,
            {
                Addon(L".", L"Repo"),
                Addon(L"Child", L"Child")
            });

    const auto releases =
        Release({
            L"ClassicAPI.dll",
            L"WideLoadScreens.mpq"
        });

    tp::AggregateRepositoryCandidates(
        L"github",
        &empty,
        nullptr,
        candidates);
    ExpectCounts(
        candidates,
        0,
        0,
        0,
        "empty repository discovery produced a candidate");

    tp::AggregateRepositoryCandidates(
        L"github",
        nullptr,
        &releases,
        candidates);
    ExpectCounts(
        candidates,
        0,
        1,
        1,
        "failed branch discovery hid valid release candidates");

    tp::AggregateRepositoryCandidates(
        L"github",
        &root,
        nullptr,
        candidates);
    ExpectCounts(
        candidates,
        1,
        0,
        0,
        "failed release discovery hid a valid addon candidate");

    tp::AggregateRepositoryCandidates(
        L"github",
        &ambiguous,
        &releases,
        candidates);
    ExpectCounts(
        candidates,
        0,
        1,
        1,
        "ambiguous addon roots either leaked as selectable addons or hid release assets");
}

void TestFilteringAndGitLabScope() {
    std::vector<tp::RepositoryCandidate>
        candidates;

    const auto root =
        Layout(
            tp::RepositoryAddonLayoutKind::RootAddon,
            {Addon(L".", L"Repo")});

    const auto releases =
        Release({
            L"ClassicAPI.DLL",
            L"WideLoadScreens.MPQ",
            L"ClassicAPI.dll.sha256",
            L"folder/Nested.dll",
            L"archive.zip"
        });

    tp::AggregateRepositoryCandidates(
        L"github",
        &root,
        &releases,
        candidates);
    ExpectCounts(
        candidates,
        1,
        1,
        1,
        "GitHub filtering did not keep only exact standalone DLL/MPQ assets");

    if (candidates.size() != 3 ||
        candidates[1].releaseTag !=
            L"v1.2.3" ||
        candidates[2].releaseTag !=
            L"v1.2.3") {
        Fail(
            "release candidates did not retain stable release identity");
    }

    tp::AggregateRepositoryCandidates(
        L"gitlab",
        &root,
        &releases,
        candidates);
    ExpectCounts(
        candidates,
        1,
        0,
        0,
        "GitLab discovery expanded beyond branch-addon-only support");
}

} // namespace

int main() {
    TestRequiredCombinations();
    TestEmptyFailureAndAmbiguousIsolation();
    TestFilteringAndGitLabScope();

    if (failures != 0) {
        std::cerr
            << failures
            << " repository discovery test(s) failed\n";
        return 1;
    }

    std::cout
        << "Repository discovery tests passed\n";
    return 0;
}
