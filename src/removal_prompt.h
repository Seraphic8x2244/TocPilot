#pragma once

#include "install.h"

#include <string>
#include <string_view>
#include <vector>

namespace tp {

enum class RemovalAction {
    Uninstall,
    Remove
};

struct RemovalPrompt {
    std::wstring title;
    std::wstring instruction;
    std::wstring content;
    std::wstring details;
};

RemovalPrompt BuildRemovalPrompt(
    std::wstring_view packageName,
    RemovalAction action,
    const AddonInstallPlan& plan,
    const std::vector<std::wstring>& installedFiles);

} // namespace tp
