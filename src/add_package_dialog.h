#pragma once

#include "state.h"

#include <windows.h>

#include <string>
#include <string_view>

namespace tp {

bool ShowAddPackageDialog(
    HWND owner,
    PackageRecord& package);

bool ShowDirectDllFallbackDialog(
    HWND owner,
    std::wstring_view repository,
    PackageRecord& package,
    bool& supportedDllFound,
    std::wstring& error);

} // namespace tp
