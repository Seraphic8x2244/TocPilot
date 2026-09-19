#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

struct AddonCandidate {
    std::filesystem::path sourceRelativePath;
    std::wstring installFolder;
    std::vector<std::filesystem::path> tocFiles;
};

struct ArchiveInspection {
    std::filesystem::path stagingDirectory;
    std::filesystem::path archivePath;
    std::filesystem::path extractedRoot;
    std::size_t entryCount = 0;
    std::uint64_t totalUncompressedBytes = 0;
    std::vector<AddonCandidate> candidates;
};

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

bool DetectGitHubAddonCandidates(
    const std::filesystem::path& extractedRoot,
    std::wstring_view repository,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error);

} // namespace tp
