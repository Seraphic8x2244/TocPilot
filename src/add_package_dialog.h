#pragma once

#include "state.h"

#include <windows.h>

namespace tp {

bool ShowAddPackageDialog(
    HWND owner,
    PackageRecord& package);

} // namespace tp
