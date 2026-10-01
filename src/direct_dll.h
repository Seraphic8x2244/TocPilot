#pragma once

#include "exact_release_asset.h"
#include "state.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace tp {

using DirectDllRelease =
    ExactReleaseAssetRelease;

bool IsDirectDllPackage(
    const PackageRecord& package);

bool ValidateDirectDllPackage(
    const PackageRecord& package,
    std::wstring& error);

bool DirectDllTargetPath(
    const std::filesystem::path& wowRoot,
    const PackageRecord& package,
    std::filesystem::path& target,
    std::wstring& error);

bool ResolveLatestDirectDllRelease(
    const PackageRecord& package,
    DirectDllRelease& release,
    std::wstring& error);

bool DownloadAndVerifyDirectDll(
    const PackageRecord& package,
    const DirectDllRelease& release,
    const std::filesystem::path& wowRoot,
    std::uint64_t& downloadedBytes,
    std::wstring& actualSha256,
    std::wstring& error);

} // namespace tp
