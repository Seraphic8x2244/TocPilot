#pragma once

#include <string>
#include <string_view>

namespace tp {

enum class ProviderKind {
    GitHub,
    GitLab
};

struct RepositoryIdentity {
    ProviderKind provider = ProviderKind::GitHub;
    std::wstring host;
    std::wstring repository;
    std::wstring canonicalUrl;
};

const wchar_t* ProviderName(ProviderKind provider);

bool NormalizeRepositoryUrl(
    std::wstring_view input,
    RepositoryIdentity& identity,
    std::wstring& error);

} // namespace tp
