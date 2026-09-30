#pragma once

#include "github_release.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

inline constexpr std::uint64_t kMaxSelfUpdateChecksumBytes =
    64 * 1024;

struct ReleaseInfo {
    std::wstring tag;
    std::wstring assetUrl;
    std::wstring assetDigest;
    std::wstring checksumUrl;
    std::uint64_t assetSize = 0;
    std::uint64_t checksumSize = 0;
};

enum class ReleaseCheckState {
    NoRelease,
    UpToDate,
    UpdateAvailable
};

std::filesystem::path ExecutablePath();

bool ParseLatestReleaseTagFromUrl(
    std::wstring_view url,
    std::wstring& tag,
    std::wstring& error);

bool ResolveLatestReleaseTag(
    std::wstring& tag,
    std::wstring& error);

bool CompareSelfUpdateVersionTags(
    std::wstring_view left,
    std::wstring_view right,
    int& comparison);

bool SelectSelfUpdateReleaseForChannel(
    const std::vector<GitHubReleaseInfo>& releases,
    bool receiveDevelopmentBuilds,
    GitHubReleaseInfo& release,
    std::wstring& error);

bool ShouldInstallSelfUpdateVersion(
    std::wstring_view candidateTag,
    std::wstring_view currentTag,
    bool receiveDevelopmentBuilds,
    bool& shouldInstall,
    std::wstring& error);

bool CheckLatestRelease(
    ReleaseInfo& release,
    ReleaseCheckState& state,
    std::wstring& error);

bool CheckLatestRelease(
    bool receiveDevelopmentBuilds,
    ReleaseInfo& release,
    ReleaseCheckState& state,
    std::wstring& error);

bool ValidateSelfUpdateRelease(
    const ReleaseInfo& release,
    std::wstring& error);

bool ValidateSelfUpdateDownloadSize(
    std::uint64_t expectedSize,
    std::uint64_t actualSize,
    std::wstring& error);

bool DownloadVerifyAndLaunchUpdater(
    const ReleaseInfo& release,
    DWORD parentPid,
    const std::filesystem::path& targetExe,
    const std::filesystem::path& workingDirectory,
    std::wstring& error);

int RunUpdaterMode(
    DWORD parentPid,
    const std::filesystem::path& targetExe,
    const std::filesystem::path& workingDirectory);

void CleanupAfterUpdate(
    const std::filesystem::path& backupExe,
    const std::filesystem::path& stagedExe);

} // namespace tp
