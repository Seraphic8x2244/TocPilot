#pragma once

#include "git_refs.h"
#include "state.h"

#include <windows.h>

#include <string>

namespace tp {

struct BranchSelection {
    std::wstring name;
    std::wstring sha;
    GitRemoteRepositoryInfo repositoryInfo;
};

bool ShowBranchDialog(
    HWND owner,
    const PackageRecord& package,
    BranchSelection& selection);

} // namespace tp
