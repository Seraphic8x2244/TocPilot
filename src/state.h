#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace tp {

struct AppSettings {
    double textScale = 1.0;
    bool checkAppUpdates = true;
};

struct AppState {
    int schema = 1;
    AppSettings settings;
    std::size_t packageCount = 0;

    // P1 initially preserves package JSON verbatim until package editing lands.
    std::string sourceJson;
};

std::filesystem::path StatePath(const std::filesystem::path& wowRoot);

bool LoadOrCreateState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    bool& created,
    std::wstring& error);

bool SaveState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    std::wstring& error);

} // namespace tp
