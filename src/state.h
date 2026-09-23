#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

inline constexpr std::array<int, 5> kDefaultPackageColumnWidths{
    240,
    300,
    115,
    115,
    150
};

inline constexpr std::array<int, 5> kDefaultPackageColumnOrder{
    0,
    1,
    2,
    3,
    4
};

struct AppSettings {
    double textScale = 1.0;
    bool checkAppUpdates = true;
    int packageSortColumn = -1;
    bool packageSortAscending = true;
    std::array<int, 5> packageColumnWidths =
        kDefaultPackageColumnWidths;
    std::array<int, 5> packageColumnOrder =
        kDefaultPackageColumnOrder;
    bool packageColumnsLocked = false;
};

struct PackageRecord {
    std::wstring id;
    std::wstring name;
    std::wstring provider;
    std::wstring repository;
    std::wstring mode = L"unconfigured";
    std::wstring ref;
    std::wstring releasePolicy;
    std::wstring asset;
    std::wstring sourcePath;
    std::wstring target = L"addons";
    std::wstring targetPath;
    std::wstring installedRevision;
    std::wstring latestRevision;
    std::vector<std::wstring> installedFiles;

    // Preserve the complete package object so fields from newer versions are
    // not discarded when this version updates unrelated state.
    std::string sourceJson;
};

struct AppState {
    int schema = 1;
    AppSettings settings;
    std::vector<PackageRecord> packages;

    // Preserve the complete top-level document so unknown/new fields survive.
    std::string sourceJson;
};

std::filesystem::path StatePath(const std::filesystem::path& wowRoot);

PackageRecord MakeRepositoryPackage(
    std::wstring provider,
    std::wstring repository);

PackageRecord MakeRepositoryAddonPackage(
    std::wstring provider,
    std::wstring repository,
    std::wstring sourcePath,
    std::wstring name);

bool AppendPackage(
    AppState& state,
    PackageRecord package,
    std::wstring& error);

std::size_t FindPackageOwningAddonRoot(
    const AppState& state,
    std::wstring_view installFolder);

bool ReplacePackageRecord(
    AppState& state,
    std::wstring_view existingPackageId,
    PackageRecord replacement,
    std::wstring& error);

bool RemovePackageRecord(
    AppState& state,
    std::wstring_view packageId,
    std::wstring& error);

bool SetPackageBranch(
    PackageRecord& package,
    std::wstring branch,
    std::wstring remoteSha,
    std::wstring& error);

bool SetPackageLatestRevision(
    PackageRecord& package,
    std::wstring remoteSha,
    std::wstring& error);

bool SetPackageInstalledState(
    PackageRecord& package,
    std::wstring installedRevision,
    std::vector<std::wstring> installedFiles,
    std::wstring& error);

bool ClearPackageInstalledState(
    PackageRecord& package,
    std::wstring& error);

bool LoadOrCreateState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    bool& created,
    std::wstring& error);

bool SaveState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    std::wstring& error);

} // namespace tp
