#pragma once

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

bool FetchGitHubRepositoryInfo(
    std::wstring_view repository,
    GitHubRepositoryInfo& info,
    std::wstring& error);

} // namespace tp
