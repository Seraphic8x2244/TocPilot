#pragma once

#include "state.h"

#include <filesystem>
#include <string>

namespace tp {

struct GitAddonAdoptionPlan {
    std::filesystem::path addonRoot;
    PackageRecord package;
};

bool PlanGitAddonAdoption(
    const std::filesystem::path& wowRoot,
    const std::filesystem::path& addonRoot,
    const AppState& state,
    GitAddonAdoptionPlan& plan,
    std::wstring& error);

} // namespace tp
