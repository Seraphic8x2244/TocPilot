#include "addon_scan.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

std::filesystem::path MakeTempRoot() {
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD length =
        GetTempPathW(MAX_PATH, temp);

    if (length == 0 ||
        length > MAX_PATH) {
        return {};
    }

    const auto root =
        std::filesystem::path(temp) /
        (
            L"TocPilotAddonScanTest-" +
            std::to_wstring(GetCurrentProcessId()) +
            L"-" +
            std::to_wstring(GetTickCount64()));

    std::error_code ec;
    std::filesystem::create_directories(
        root /
            L"Interface" /
            L"AddOns",
        ec);

    return ec
        ? std::filesystem::path{}
        : root;
}

bool WriteAll(
    const std::filesystem::path& path,
    std::string_view content) {
    std::error_code ec;
    std::filesystem::create_directories(
        path.parent_path(),
        ec);

    if (ec) {
        return false;
    }

    std::ofstream stream(
        path,
        std::ios::binary |
            std::ios::trunc);

    stream.write(
        content.data(),
        static_cast<std::streamsize>(
            content.size()));

    return stream.good();
}

const tp::AddonFolderInfo* Find(
    const std::vector<tp::AddonFolderInfo>& folders,
    std::wstring_view name) {
    for (const auto& folder :
         folders) {
        if (folder.name == name) {
            return &folder;
        }
    }

    return nullptr;
}

void TestClassification() {
    const auto root =
        MakeTempRoot();

    if (root.empty()) {
        Fail("could not create scan fixture");
        return;
    }

    const auto addons =
        root /
        L"Interface" /
        L"AddOns";

    if (!WriteAll(
            addons /
                L"Managed" /
                L"Managed.toc",
            "## Interface: 11200\n") ||
        !WriteAll(
            addons /
                L"LocalAddon" /
                L"LocalAddon.toc",
            "## Interface: 11200\n") ||
        !WriteAll(
            addons /
                L"Blizzard_AuctionUI" /
                L"Blizzard_AuctionUI.toc",
            "## Interface: 11200\n") ||
        !WriteAll(
            addons /
                L"Atlas.repo" /
                L".git" /
                L"HEAD",
            "ref: refs/heads/master\n") ||
        !WriteAll(
            addons /
                L"Atlas.repo" /
                L"Atlas" /
                L"Atlas.toc",
            "## Interface: 11200\n") ||
        !WriteAll(
            addons /
                L"Atlas.repo" /
                L"AtlasLoot" /
                L"AtlasLoot.toc",
            "## Interface: 11200\n") ||
        !WriteAll(
            addons /
                L"Notes" /
                L"README.txt",
            "not an addon\n")) {
        Fail("could not create scan files");
        return;
    }

    tp::PackageRecord package;
    package.installedFiles = {
        L"Interface/AddOns/Managed/Managed.toc"
    };

    tp::AppState state;
    state.packages.push_back(
        std::move(package));

    std::vector<tp::AddonFolderInfo>
        folders;
    std::wstring error;

    if (!tp::ScanAddonFolders(
            root,
            state,
            folders,
            error)) {
        Fail("ScanAddonFolders rejected valid fixture");
    } else {
        const auto* managed =
            Find(folders, L"Managed");
        const auto* local =
            Find(folders, L"LocalAddon");
        const auto* blizzard =
            Find(folders, L"Blizzard_AuctionUI");
        const auto* atlas =
            Find(folders, L"Atlas.repo");
        const auto* notes =
            Find(folders, L"Notes");

        if (!managed ||
            managed->kind !=
                tp::AddonFolderKind::ManagedAddon) {
            Fail("managed addon was not classified");
        }

        if (!local ||
            local->kind !=
                tp::AddonFolderKind::UnmanagedAddon) {
            Fail("unmanaged addon was not classified");
        }

        if (!blizzard ||
            blizzard->kind !=
                tp::AddonFolderKind::BlizzardSystemAddon) {
            Fail("Blizzard addon was not classified");
        }

        if (!atlas ||
            atlas->kind !=
                tp::AddonFolderKind::GitContainer ||
            atlas->oneLevelAddonRoots.size() != 2 ||
            atlas->oneLevelAddonRoots[0] != L"Atlas" ||
            atlas->oneLevelAddonRoots[1] != L"AtlasLoot") {
            Fail("Git container one-level roots were not classified");
        }

        if (!notes ||
            notes->kind !=
                tp::AddonFolderKind::NonAddon) {
            Fail("non-addon folder was not classified");
        }
    }

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

} // namespace

int main() {
    TestClassification();

    if (failures != 0) {
        std::cerr
            << failures
            << " addon scan test(s) failed\n";
        return 1;
    }

    std::cout
        << "addon scan tests passed\n";
    return 0;
}
