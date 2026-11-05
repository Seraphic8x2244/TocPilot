#include "wdb_cleanup.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
int main() {
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD n = GetTempPathW(MAX_PATH, temp);
    if (n == 0 || n > MAX_PATH) return 1;
    const fs::path root = fs::path(temp) /
        (L"TocPilotWdbTest-" + std::to_wstring(GetCurrentProcessId()) +
         L"-" + std::to_wstring(GetTickCount64()));
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) return 1;
    auto cleanup = [&]() { std::error_code ignored; fs::remove_all(root, ignored); };
    auto expect = [&](bool ok, const char* message) {
        if (!ok) std::cerr << message << '\n';
        return ok;
    };
    bool passed = true;
    std::wstring error;
    passed &= expect(tp::ClearWdbBeforeLaunch(root, error),
                     "Missing WDB should be a no-op");
    fs::create_directories(root / L"WDB" / L"enUS", ec);
    fs::create_directories(root / L"WTF", ec);
    std::ofstream(root / L"WDB" / L"enUS" / L"creaturecache.wdb") << "cache";
    std::ofstream(root / L"WTF" / L"Config.wtf") << "keep this";
    passed &= expect(tp::ClearWdbBeforeLaunch(root, error),
                     "Existing WDB should be removed recursively");
    passed &= expect(!fs::exists(root / L"WDB") &&
                     fs::exists(root / L"WTF" / L"Config.wtf"),
                     "WDB cleanup must leave WTF and other folders intact");
    passed &= expect(tp::ClearWdbBeforeLaunch(root, error),
                     "Repeated cleanup should succeed");
    std::ofstream(root / L"WDB") << "not a directory";
    passed &= expect(!tp::ClearWdbBeforeLaunch(root, error) &&
                     fs::exists(root / L"WDB"),
                     "Non-directory WDB must not be removed");
    fs::remove(root / L"WDB", ec);
    fs::create_directories(root / L"outside", ec);
    std::ofstream(root / L"outside" / L"untouched.txt") << "keep";
    // Windows symlink privileges vary; exercise reparse protection if allowed.
    ec.clear();
    fs::create_directory_symlink(root / L"outside", root / L"WDB", ec);
    if (!ec) {
        passed &= expect(!tp::ClearWdbBeforeLaunch(root, error) &&
                         fs::exists(root / L"outside" / L"untouched.txt"),
                         "WDB symlink must be rejected");
        fs::remove(root / L"WDB", ec);
    }
    fs::create_directories(root / L"WDB", ec);
    ec.clear();
    fs::create_directory_symlink(root / L"outside",
                                 root / L"WDB" / L"linked", ec);
    if (!ec) {
        passed &= expect(!tp::ClearWdbBeforeLaunch(root, error) &&
                         fs::exists(root / L"outside" / L"untouched.txt"),
                         "Nested WDB symlink must be rejected");
        fs::remove(root / L"WDB" / L"linked", ec);
    }
    cleanup();
    return passed ? 0 : 1;
}
