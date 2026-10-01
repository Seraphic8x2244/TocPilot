#pragma once

#include "repository_discovery.h"
#include "state.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace tp {

bool ShowRepositorySelectionDialog(
    HWND owner,
    const RepositoryDiscoveryResult& discovery,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& existingPackages,
    std::vector<std::size_t>& selectedIndices,
    std::wstring& error);

} // namespace tp
