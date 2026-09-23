#pragma once

#include "archive.h"

#include <windows.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace tp {

bool ShowRepositoryLibraryDialog(
    HWND owner,
    std::wstring_view repository,
    std::wstring_view branch,
    const std::vector<AddonCandidate>& candidates,
    std::vector<std::size_t>& selectedIndices);

} // namespace tp
