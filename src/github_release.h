#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

struct GitHubReleaseAsset {
    std::wstring name;
    std::wstring downloadUrl;
    std::wstring digest;
    std::uint64_t size = 0;
};

struct GitHubReleaseInfo {
    std::wstring tag;
    std::wstring name;
    bool prerelease = false;
    bool draft = false;
    std::vector<GitHubReleaseAsset> assets;
};

bool ParseGitHubReleaseJson(
    std::string_view json,
    GitHubReleaseInfo& release,
    std::wstring& error);

bool FindExactGitHubReleaseAsset(
    const GitHubReleaseInfo& release,
    std::wstring_view assetName,
    GitHubReleaseAsset& asset,
    std::wstring& error);

bool FetchLatestStableGitHubRelease(
    std::wstring_view repository,
    GitHubReleaseInfo& release,
    std::wstring& error);

} // namespace tp
