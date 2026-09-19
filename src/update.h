#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

namespace tp {

struct ReleaseInfo {
    std::wstring tag;
    std::wstring assetUrl;
    std::wstring assetDigest;
    std::wstring checksumUrl;
};

std::filesystem::path ExecutablePath();

bool CheckLatestRelease(
    ReleaseInfo& release,
    bool& updateAvailable,
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
