#include "repository_discovery.h"

#include "exact_release_asset.h"
#include "github_api.h"
#include "gitlab_api.h"

#include <cstdint>
#include <utility>

namespace tp {
namespace {

bool SupportedBranchProvider(
    std::wstring_view provider) {
    return
        provider == L"github" ||
        provider == L"gitlab";
}

std::wstring_view ProviderLabel(
    std::wstring_view provider) {
    if (provider == L"github") {
        return L"GitHub";
    }

    if (provider == L"gitlab") {
        return L"GitLab";
    }

    return L"Repository";
}

bool DownloadBranchArchive(
    const PackageRecord& package,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    std::wstring& error) {
    if (package.provider == L"github") {
        return DownloadGitHubArchive(
            package.repository,
            package.latestRevision,
            destination,
            downloadedBytes,
            error);
    }

    if (package.provider == L"gitlab") {
        return DownloadGitLabArchive(
            package.repository,
            package.latestRevision,
            destination,
            downloadedBytes,
            error);
    }

    error =
        L"Unsupported branch package provider.";
    return false;
}

void AppendBranchCandidates(
    const RepositoryAddonLayout* layout,
    std::vector<RepositoryCandidate>& candidates) {
    if (!layout ||
        layout->kind ==
            RepositoryAddonLayoutKind::None ||
        layout->kind ==
            RepositoryAddonLayoutKind::MixedAmbiguous) {
        return;
    }

    for (const auto& addon :
         layout->candidates) {
        RepositoryCandidate candidate;
        candidate.kind =
            RepositoryCandidateKind::Addon;
        candidate.addon =
            addon;
        candidates.push_back(
            std::move(candidate));
    }
}

void AppendReleaseCandidates(
    const GitHubReleaseInfo* release,
    std::vector<RepositoryCandidate>& candidates) {
    if (!release) {
        return;
    }

    for (const auto& asset :
         release->assets) {
        RepositoryCandidateKind kind{};

        if (IsExactReleaseAssetName(
                asset.name,
                ExactReleaseAssetKind::Dll)) {
            kind =
                RepositoryCandidateKind::Dll;
        } else if (
            IsExactReleaseAssetName(
                asset.name,
                ExactReleaseAssetKind::Mpq)) {
            kind =
                RepositoryCandidateKind::Mpq;
        } else {
            continue;
        }

        RepositoryCandidate candidate;
        candidate.kind = kind;
        candidate.releaseAsset =
            asset;
        candidate.releaseTag =
            release->tag;
        candidates.push_back(
            std::move(candidate));
    }
}

void AppendError(
    std::wstring& destination,
    std::wstring_view message) {
    if (message.empty()) {
        return;
    }

    if (!destination.empty()) {
        destination += L" ";
    }

    destination.append(message);
}

} // namespace

void AggregateRepositoryCandidates(
    std::wstring_view provider,
    const RepositoryAddonLayout* addonLayout,
    const GitHubReleaseInfo* release,
    std::vector<RepositoryCandidate>& candidates) {
    candidates.clear();

    AppendBranchCandidates(
        addonLayout,
        candidates);

    if (provider == L"github") {
        AppendReleaseCandidates(
            release,
            candidates);
    }
}

bool DiscoverRepositoryCandidates(
    const PackageRecord& branchPackage,
    const std::filesystem::path& wowRoot,
    RepositoryDiscoveryResult& result,
    std::wstring& error) {
    result = {};
    error.clear();

    if (!SupportedBranchProvider(
            branchPackage.provider) ||
        branchPackage.mode != L"branch" ||
        branchPackage.repository.empty() ||
        branchPackage.ref.empty() ||
        branchPackage.latestRevision.empty()) {
        error =
            L"Repository discovery requires a configured GitHub or GitLab branch package.";
        return false;
    }

    if (wowRoot.empty()) {
        error =
            L"Repository discovery requires the WoW root directory.";
        return false;
    }

    result.provider =
        branchPackage.provider;
    result.repository =
        branchPackage.repository;
    result.branch =
        branchPackage.ref;
    result.branchRevision =
        branchPackage.latestRevision;

    result.branchAttempted = true;

    RepositoryAddonLayout addonLayout;
    std::filesystem::path stagingDirectory;
    bool stagingReady = false;

    if (!ResetProviderPackageStaging(
            wowRoot,
            branchPackage.provider,
            branchPackage.repository,
            stagingDirectory,
            result.branchError)) {
        result.branchSucceeded = false;
    } else {
        stagingReady = true;

        const auto archivePath =
            stagingDirectory /
            L"archive.zip";
        const auto extractedRoot =
            stagingDirectory /
            L"extracted";

        std::uint64_t downloadedBytes = 0;
        std::size_t entryCount = 0;
        std::uint64_t totalUncompressedBytes = 0;

        result.branchSucceeded =
            DownloadBranchArchive(
                branchPackage,
                archivePath,
                downloadedBytes,
                result.branchError) &&
            ExtractZipSecure(
                archivePath,
                extractedRoot,
                entryCount,
                totalUncompressedBytes,
                result.branchError) &&
            DetectShallowRepositoryAddonLayout(
                extractedRoot,
                ProviderLabel(
                    branchPackage.provider),
                branchPackage.repository,
                addonLayout,
                result.branchError);
    }

    if (stagingReady) {
        std::wstring cleanupError;
        if (!CleanupProviderPackageStaging(
                wowRoot,
                branchPackage.provider,
                branchPackage.repository,
                cleanupError)) {
            const std::wstring message =
                L"Repository staging cleanup failed: " +
                cleanupError;
            AppendError(
                result.branchError,
                message);
            result.branchSucceeded = false;
        }
    }

    if (result.branchSucceeded) {
        result.addonLayout =
            std::move(addonLayout);
    } else {
        result.addonLayout = {};
    }

    GitHubReleaseInfo release;

    if (branchPackage.provider ==
        L"github") {
        result.releaseAttempted = true;

        if (FetchLatestStableGitHubRelease(
                branchPackage.repository,
                release,
                result.releaseError)) {
            result.releaseSucceeded = true;
            result.releaseTag =
                release.tag;
        }
    }

    AggregateRepositoryCandidates(
        branchPackage.provider,
        result.branchSucceeded
            ? &result.addonLayout
            : nullptr,
        result.releaseSucceeded
            ? &release
            : nullptr,
        result.candidates);

    return true;
}

} // namespace tp
