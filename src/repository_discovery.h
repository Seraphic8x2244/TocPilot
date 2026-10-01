#pragma once

#include "archive.h"
#include "github_release.h"
#include "state.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

enum class RepositoryCandidateKind {
    Addon,
    Dll,
    Mpq
};

struct RepositoryCandidate {
    RepositoryCandidateKind kind =
        RepositoryCandidateKind::Addon;
    AddonCandidate addon;
    GitHubReleaseAsset releaseAsset;
    std::wstring releaseTag;
};

struct RepositoryDiscoveryResult {
    std::wstring provider;
    std::wstring repository;
    std::wstring branch;
    std::wstring branchRevision;

    bool branchAttempted = false;
    bool branchSucceeded = false;
    RepositoryAddonLayout addonLayout;
    std::wstring branchError;

    bool releaseAttempted = false;
    bool releaseSucceeded = false;
    std::wstring releaseTag;
    std::wstring releaseError;

    std::vector<RepositoryCandidate> candidates;
};

void AggregateRepositoryCandidates(
    std::wstring_view provider,
    const RepositoryAddonLayout* addonLayout,
    const GitHubReleaseInfo* release,
    std::vector<RepositoryCandidate>& candidates);

bool DiscoverRepositoryCandidates(
    const PackageRecord& branchPackage,
    const std::filesystem::path& wowRoot,
    RepositoryDiscoveryResult& result,
    std::wstring& error);

} // namespace tp
