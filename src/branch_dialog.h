#pragma once

#include "state.h"

#include <windows.h>

#include <string>

namespace tp {

struct BranchSelection {
    std::wstring name;
    std::wstring sha;
};

bool ShowBranchDialog(
    HWND owner,
    const PackageRecord& package,
    BranchSelection& selection);

} // namespace tp
