#pragma once

#include <string>
#include <string_view>

namespace tp {

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
    std::wstring& error);

} // namespace tp
