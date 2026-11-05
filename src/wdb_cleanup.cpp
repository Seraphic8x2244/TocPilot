#include "wdb_cleanup.h"

#include <windows.h>
#include <filesystem>
#include <string>
#include <system_error>

namespace tp {
namespace {
namespace fs = std::filesystem;

bool VerifySafeEntry(const fs::path& path, std::wstring& error) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error = L"Could not inspect WDB entry: " + path.wstring() +
            L" (Windows error " + std::to_wstring(GetLastError()) + L").";
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = L"WDB contains a link or junction. Cleanup stopped: " +
            path.wstring();
        return false;
    }
    return true;
}
} // namespace

bool ClearWdbBeforeLaunch(const std::filesystem::path& wowRoot,
                          std::wstring& error) {
    error.clear();
    if (wowRoot.empty()) {
        error = L"The WoW installation path is unavailable.";
        return false;
    }
    // No user-provided target path: only the WDB child of the client root.
    const fs::path wdb = wowRoot / L"WDB";
    const DWORD attributes = GetFileAttributesW(wdb.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_FILE_NOT_FOUND ||
            lastError == ERROR_PATH_NOT_FOUND)
            return true;
        error = L"Could not inspect WDB (Windows error " +
            std::to_wstring(lastError) + L").";
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        error = L"The WDB path is not a folder: " + wdb.wstring();
        return false;
    }
    if (!VerifySafeEntry(wdb, error)) return false;

    // Reject nested links/junctions before removing any cache content.
    std::error_code ec;
    fs::recursive_directory_iterator it(wdb, ec), end;
    if (ec) {
        error = L"Could not inspect WDB contents (error " +
            std::to_wstring(ec.value()) + L").";
        return false;
    }
    while (it != end) {
        if (!VerifySafeEntry(it->path(), error)) return false;
        it.increment(ec);
        if (ec) {
            error = L"Could not inspect WDB contents (error " +
                std::to_wstring(ec.value()) + L").";
            return false;
        }
    }

    // std::filesystem::remove_all does not follow symlinks. Any failure
    // prevents launch, so the user can resolve locked or protected files.
    fs::remove_all(wdb, ec);
    if (ec) {
        error = L"Could not delete WDB folder: " + wdb.wstring() +
            L" (error " + std::to_wstring(ec.value()) + L").";
        return false;
    }
    return true;
}
} // namespace tp
