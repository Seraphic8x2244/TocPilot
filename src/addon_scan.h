#pragma once

#include "state.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tp {

enum class AddonFolderKind {
    ManagedAddon,
    UnmanagedAddon,
    GitContainer,
    BlizzardSystemAddon,
    NonAddon
};

struct AddonFolderInfo {
    std::filesystem::path path;
    std::wstring name;
    AddonFolderKind kind = AddonFolderKind::NonAddon;
    bool hasGitMetadata = false;
    bool hasRootToc = false;
    std::vector<std::wstring> oneLevelAddonRoots;
};

bool ScanAddonFolders(
    const std::filesystem::path& wowRoot,
    const AppState& state,
    std::vector<AddonFolderInfo>& folders,
    std::wstring& error);

const wchar_t* AddonFolderKindLabel(AddonFolderKind kind);

} // namespace tp
