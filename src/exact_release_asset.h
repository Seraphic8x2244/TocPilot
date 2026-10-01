#pragma once

#include "github_release.h"
#include "state.h"

#include <string>
#include <string_view>

namespace tp {

enum class ExactReleaseAssetKind {
    Dll,
    Mpq
};

struct ExactReleaseAssetRelease {
    ExactReleaseAssetKind kind =
        ExactReleaseAssetKind::Dll;
    std::wstring tag;
    GitHubReleaseAsset asset;
    std::wstring expectedSha256;
};

std::wstring_view ExactReleaseAssetExtension(
    ExactReleaseAssetKind kind);

bool IsExactReleaseAssetName(
    std::wstring_view name,
    ExactReleaseAssetKind kind);

bool ValidateLatestStableExactReleaseAssetPackage(
    const PackageRecord& package,
    ExactReleaseAssetKind kind,
    std::wstring& error);

bool ResolveExactReleaseAssetMetadata(
    const PackageRecord& package,
    ExactReleaseAssetKind kind,
    const GitHubReleaseInfo& metadata,
    ExactReleaseAssetRelease& release,
    std::wstring& error);

bool ResolveLatestStableExactReleaseAsset(
    const PackageRecord& package,
    ExactReleaseAssetKind kind,
    ExactReleaseAssetRelease& release,
    std::wstring& error);

} // namespace tp
