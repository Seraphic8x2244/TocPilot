#pragma once

#include "exact_release_asset.h"
#include "state.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

using MpqRelease =
    ExactReleaseAssetRelease;

struct MpqInstallTransaction {
    std::filesystem::path targetPath;
    std::filesystem::path rollbackPath;
    bool hadPreviousTarget = false;
    bool active = false;
};

struct MpqRemovalTransaction {
    std::filesystem::path targetPath;
    std::filesystem::path rollbackPath;
    bool active = false;
};

bool IsMpqPackage(
    const PackageRecord& package);

bool ValidateMpqPackage(
    const PackageRecord& package,
    std::wstring& error);

std::wstring MpqPatchTargetForLetter(
    wchar_t letter);

bool AssignMpqTargetPath(
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    PackageRecord& package,
    std::wstring& error);

bool MpqTargetPath(
    const std::filesystem::path& wowRoot,
    const PackageRecord& package,
    std::filesystem::path& target,
    std::wstring& error);

bool ResolveLatestMpqRelease(
    const PackageRecord& package,
    MpqRelease& release,
    std::wstring& error);

bool VerifyMpqFile(
    const std::filesystem::path& path,
    std::uint64_t expectedSize,
    std::wstring_view expectedSha256,
    std::wstring& actualSha256,
    std::wstring& error);

bool VerifyInstalledMpq(
    const PackageRecord& package,
    const MpqRelease& release,
    const std::filesystem::path& wowRoot,
    std::wstring& actualSha256,
    std::wstring& error);

bool BeginMpqInstallFromFile(
    const PackageRecord& package,
    const MpqRelease& release,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    const std::filesystem::path& payloadPath,
    MpqInstallTransaction& transaction,
    std::wstring& actualSha256,
    std::wstring& error);

bool DownloadAndBeginMpqInstall(
    const PackageRecord& package,
    const MpqRelease& release,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    MpqInstallTransaction& transaction,
    std::uint64_t& downloadedBytes,
    std::wstring& actualSha256,
    std::wstring& error);

bool RollbackMpqInstall(
    MpqInstallTransaction& transaction,
    std::wstring& error);

bool FinalizeMpqInstall(
    MpqInstallTransaction& transaction,
    std::wstring& error);

bool BeginMpqRemoval(
    const PackageRecord& package,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    MpqRemovalTransaction& transaction,
    std::wstring& error);

bool RollbackMpqRemoval(
    MpqRemovalTransaction& transaction,
    std::wstring& error);

bool FinalizeMpqRemoval(
    MpqRemovalTransaction& transaction,
    std::wstring& error);

} // namespace tp
