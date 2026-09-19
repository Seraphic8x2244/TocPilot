#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

struct GitHubBranch {
    std::wstring name;
    std::wstring sha;
};

struct GitHubRepositoryInfo {
    std::wstring defaultBranch;
    std::vector<GitHubBranch> branches;
};

bool ParseGitHubRepositoryJson(
    std::string_view json,
    std::wstring& defaultBranch,
    std::wstring& error);

bool ParseGitHubBranchesJson(
    std::string_view json,
    std::vector<GitHubBranch>& branches,
    std::wstring& error);

bool FindGitHubBranchHead(
    const std::vector<GitHubBranch>& branches,
    std::wstring_view branch,
    std::wstring& remoteSha,
    std::wstring& error);

bool FetchGitHubRepositoryInfo(
    std::wstring_view repository,
    GitHubRepositoryInfo& info,
    std::wstring& error);

bool ResolveGitHubBranchHead(
    std::wstring_view repository,
    std::wstring_view branch,
    std::wstring& remoteSha,
    std::wstring& error);

bool BuildGitHubArchiveApiPath(
    std::wstring_view repository,
    std::wstring_view ref,
    std::wstring& apiPath,
    std::wstring& error);

bool DownloadGitHubArchive(
    std::wstring_view repository,
    std::wstring_view ref,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    std::wstring& error);

} // namespace tp
