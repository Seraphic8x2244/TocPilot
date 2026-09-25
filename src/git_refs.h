#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tp {

struct GitRemoteBranch {
    std::wstring name;
    std::wstring sha;
};

struct GitRemoteRepositoryInfo {
    std::wstring defaultBranch;
    std::vector<GitRemoteBranch> branches;
};

bool ParseGitSmartHttpRepositoryAdvertisement(
    std::string_view advertisement,
    GitRemoteRepositoryInfo& info,
    std::wstring& error);

bool FetchPublicGitRepositoryInfo(
    std::wstring_view host,
    std::wstring_view repository,
    GitRemoteRepositoryInfo& info,
    std::wstring& error);

bool ParseGitSmartHttpBranchAdvertisement(
    std::string_view advertisement,
    std::string_view branch,
    std::string& remoteSha,
    std::wstring& error);

bool ResolvePublicGitBranchHead(
    std::wstring_view host,
    std::wstring_view repository,
    std::wstring_view branch,
    std::wstring& remoteSha,
    std::wstring& error,
    GitRemoteRepositoryInfo*
        repositoryInfo = nullptr);

} // namespace tp
