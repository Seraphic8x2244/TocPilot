#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

struct AddonCandidate {
    std::filesystem::path sourceRelativePath;
    std::filesystem::path repositoryRelativePath;
    std::wstring installFolder;
    std::vector<std::filesystem::path> tocFiles;
};

enum class RepositoryAddonLayoutKind {
    None,
    RootAddon,
    SingleNestedAddon,
    RepositoryLibrary,
    MixedAmbiguous
};

struct RepositoryAddonLayout {
    RepositoryAddonLayoutKind kind = RepositoryAddonLayoutKind::None;
    std::vector<AddonCandidate> candidates;
};

struct ArchiveInspection {
    std::filesystem::path stagingDirectory;
    std::filesystem::path archivePath;
    std::filesystem::path extractedRoot;
    std::size_t entryCount = 0;
    std::uint64_t totalUncompressedBytes = 0;
    std::vector<AddonCandidate> candidates;
};

std::filesystem::path ProviderPackageStagingDirectory(
    const std::filesystem::path& wowRoot,
    std::wstring_view provider,
    std::wstring_view repository);

bool ResetProviderPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view provider,
    std::wstring_view repository,
    std::filesystem::path& stagingDirectory,
    std::wstring& error);

bool CleanupProviderPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view provider,
    std::wstring_view repository,
    std::wstring& error);

std::filesystem::path GitHubPackageStagingDirectory(
    const std::filesystem::path& wowRoot,
    std::wstring_view repository);

bool ResetGitHubPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view repository,
    std::filesystem::path& stagingDirectory,
    std::wstring& error);

bool CleanupGitHubPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view repository,
    std::wstring& error);

bool SafeArchiveRelativePath(
    std::string_view archiveName,
    std::filesystem::path& relativePath,
    std::wstring& error);

bool ExtractZipSecure(
    const std::filesystem::path& archivePath,
    const std::filesystem::path& extractedRoot,
    std::size_t& entryCount,
    std::uint64_t& totalUncompressedBytes,
    std::wstring& error);

bool DetectAddonCandidates(
    const std::filesystem::path& extractedRoot,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error);

bool DetectShallowRepositoryAddonLayout(
    const std::filesystem::path& extractedRoot,
    std::wstring_view providerName,
    std::wstring_view repository,
    RepositoryAddonLayout& layout,
    std::wstring& error);

bool DetectRepositoryAddonCandidates(
    const std::filesystem::path& extractedRoot,
    std::wstring_view providerName,
    std::wstring_view repository,
    std::wstring_view existingInstallFolder,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error);

bool DetectGitHubAddonCandidates(
    const std::filesystem::path& extractedRoot,
    std::wstring_view repository,
    std::wstring_view existingInstallFolder,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error);

bool IsRepositoryLibrary(
    const std::vector<AddonCandidate>& candidates);

bool SelectRepositoryAddonCandidate(
    std::wstring_view sourcePath,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error);

} // namespace tp
