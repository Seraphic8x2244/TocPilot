#pragma once
#include <filesystem>
#include <string>

namespace tp {
// Deletes the WDB cache immediately before client launch. Only wowRoot/WDB
// is targeted; missing WDB is an acceptable, already-clean state.
bool ClearWdbBeforeLaunch(const std::filesystem::path& wowRoot,
                          std::wstring& error);
} // namespace tp
