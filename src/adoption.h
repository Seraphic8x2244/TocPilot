#pragma once

#include "state.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace tp {

struct GitAddonAdoptionPlan {
    std::filesystem::path addonRoot;
    PackageRecord package;
    std::optional<std::size_t> existingPackageIndex;
};

bool PlanGitAddonAdoption(
    const std::filesystem::path& wowRoot,
    const std::filesystem::path& addonRoot,
    const AppState& state,
    GitAddonAdoptionPlan& plan,
    std::wstring& error);

} // namespace tp
