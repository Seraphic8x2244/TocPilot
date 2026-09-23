#include "addon_scan.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

namespace tp {
namespace {

struct InsensitiveLess {
    bool operator()(
        const std::wstring& left,
        const std::wstring& right) const {
        return std::lexicographical_compare(
            left.begin(),
            left.end(),
            right.begin(),
            right.end(),
            [](wchar_t a, wchar_t b) {
                return std::towlower(a) <
                    std::towlower(b);
            });
    }
};

bool EqualsInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < left.size();
         ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }

    return true;
}

bool StartsWithInsensitive(
    std::wstring_view value,
    std::wstring_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < prefix.size();
         ++i) {
        if (std::towlower(value[i]) !=
            std::towlower(prefix[i])) {
            return false;
        }
    }

    return true;
}

bool HasDirectToc(
    const std::filesystem::path& folder,
    bool& hasToc,
    std::wstring& error) {
    hasToc = false;

    std::error_code ec;
    for (std::filesystem::directory_iterator it(
             folder,
             ec),
         end;
         !ec &&
         it != end;
         it.increment(ec)) {
        const auto status =
            it->symlink_status(ec);

        if (ec) {
            break;
        }

        if (!std::filesystem::is_regular_file(
                status) ||
            std::filesystem::is_symlink(
                status)) {
            continue;
        }

        std::wstring extension =
            it->path()
                .extension()
                .wstring();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](wchar_t ch) {
                return static_cast<wchar_t>(
                    std::towlower(ch));
            });

        if (extension == L".toc") {
            hasToc = true;
            return true;
        }
    }

    if (ec) {
        error =
            L"Could not inspect addon folder: " +
            folder.wstring();
        return false;
    }

    return true;
}

bool CollectManagedRoots(
    const AppState& state,
    std::set<std::wstring, InsensitiveLess>& roots,
    std::wstring& error) {
    roots.clear();

    for (const auto& package :
         state.packages) {
        for (auto owned :
             package.installedFiles) {
            std::replace(
                owned.begin(),
                owned.end(),
                L'\\',
                L'/');

            constexpr std::wstring_view prefix =
                L"Interface/AddOns/";

            if (!StartsWithInsensitive(
                    owned,
                    prefix)) {
                error =
                    L"TocPilot ownership contains a path outside Interface\\AddOns.";
                return false;
            }

            const std::size_t slash =
                owned.find(
                    L'/',
                    prefix.size());

            if (slash ==
                    std::wstring::npos ||
                slash ==
                    prefix.size()) {
                error =
                    L"TocPilot ownership contains an invalid addon-root path.";
                return false;
            }

            roots.insert(
                owned.substr(
                    prefix.size(),
                    slash -
                        prefix.size()));
        }
    }

    return true;
}

bool HasGitMetadata(
    const std::filesystem::path& folder,
    bool& hasGit,
    std::wstring& error) {
    hasGit = false;

    std::error_code ec;
    const auto status =
        std::filesystem::symlink_status(
            folder / L".git",
            ec);

    if (ec) {
        if (ec ==
            std::errc::no_such_file_or_directory) {
            ec.clear();
            return true;
        }

        error =
            L"Could not inspect Git metadata in: " +
            folder.wstring();
        return false;
    }

    hasGit =
        std::filesystem::is_directory(
            status) ||
        std::filesystem::is_regular_file(
            status);

    return true;
}

bool CollectOneLevelAddonRoots(
    const std::filesystem::path& folder,
    std::vector<std::wstring>& roots,
    std::wstring& error) {
    roots.clear();

    std::error_code ec;
    for (std::filesystem::directory_iterator it(
             folder,
             ec),
         end;
         !ec &&
         it != end;
         it.increment(ec)) {
        const auto status =
            it->symlink_status(ec);

        if (ec) {
            break;
        }

        if (!std::filesystem::is_directory(
                status) ||
            std::filesystem::is_symlink(
                status) ||
            EqualsInsensitive(
                it->path()
                    .filename()
                    .wstring(),
                L".git")) {
            continue;
        }

        bool hasToc = false;
        if (!HasDirectToc(
                it->path(),
                hasToc,
                error)) {
            return false;
        }

        if (hasToc) {
            roots.push_back(
                it->path()
                    .filename()
                    .wstring());
        }
    }

    if (ec) {
        error =
            L"Could not inspect one-level addon roots in: " +
            folder.wstring();
        return false;
    }

    std::sort(
        roots.begin(),
        roots.end(),
        InsensitiveLess{});

    return true;
}

std::string_view TrimAscii(
    std::string_view value) {
    while (!value.empty() &&
           (value.front() == ' ' ||
            value.front() == '\t' ||
            value.front() == '\r' ||
            value.front() == '\n')) {
        value.remove_prefix(1);
    }

    while (!value.empty() &&
           (value.back() == ' ' ||
            value.back() == '\t' ||
            value.back() == '\r' ||
            value.back() == '\n')) {
        value.remove_suffix(1);
    }

    return value;
}

bool StartsWithAsciiInsensitive(
    std::string_view value,
    std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < prefix.size();
         ++i) {
        const unsigned char left =
            static_cast<unsigned char>(
                value[i]);
        const unsigned char right =
            static_cast<unsigned char>(
                prefix[i]);

        if (std::tolower(left) !=
            std::tolower(right)) {
            return false;
        }
    }

    return true;
}

std::wstring TocVersionFromFile(
    const std::filesystem::path& path) {
    std::ifstream stream(
        path,
        std::ios::binary);

    if (!stream) {
        return {};
    }

    std::string line;
    constexpr std::string_view prefix =
        "## Version:";

    while (std::getline(stream, line)) {
        std::string_view view =
            TrimAscii(line);

        if (!StartsWithAsciiInsensitive(
                view,
                prefix)) {
            continue;
        }

        view.remove_prefix(
            prefix.size());
        view = TrimAscii(view);

        if (view.empty()) {
            return {};
        }

        std::wstring version;
        version.reserve(view.size());

        for (const unsigned char ch :
             view) {
            version.push_back(
                static_cast<wchar_t>(
                    ch));
        }

        return version;
    }

    return {};
}

bool IsTocPath(
    const std::filesystem::path& path) {
    std::wstring extension =
        path.extension().wstring();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    return extension == L".toc";
}

} // namespace

std::wstring InstalledPackageTocVersion(
    const std::filesystem::path& wowRoot,
    const PackageRecord& package) {
    if (package.target != L"addons" ||
        package.installedFiles.empty()) {
        return L"—";
    }

    std::set<std::wstring>
        versions;

    for (const auto& owned :
         package.installedFiles) {
        const std::filesystem::path relative(
            owned);

        if (relative.empty() ||
            relative.is_absolute() ||
            !IsTocPath(relative)) {
            continue;
        }

        bool escapesRoot = false;
        for (const auto& component :
             relative) {
            if (component == L"..") {
                escapesRoot = true;
                break;
            }
        }

        if (escapesRoot) {
            continue;
        }

        const std::wstring version =
            TocVersionFromFile(
                wowRoot /
                relative);

        if (!version.empty()) {
            versions.insert(version);

            if (versions.size() > 1) {
                return L"Multiple";
            }
        }
    }

    if (versions.empty()) {
        return L"—";
    }

    return *versions.begin();
}

const wchar_t* AddonFolderKindLabel(
    AddonFolderKind kind) {
    switch (kind) {
    case AddonFolderKind::ManagedAddon:
        return L"Managed addon";
    case AddonFolderKind::UnmanagedAddon:
        return L"Unmanaged addon";
    case AddonFolderKind::GitContainer:
        return L"Git repository container";
    case AddonFolderKind::BlizzardSystemAddon:
        return L"Blizzard/system addon";
    case AddonFolderKind::NonAddon:
        return L"Non-addon folder";
    }

    return L"Unknown";
}

bool ScanAddonFolders(
    const std::filesystem::path& wowRoot,
    const AppState& state,
    std::vector<AddonFolderInfo>& folders,
    std::wstring& error) {
    folders.clear();
    error.clear();

    std::set<std::wstring, InsensitiveLess>
        managedRoots;

    if (!CollectManagedRoots(
            state,
            managedRoots,
            error)) {
        return false;
    }

    const auto addOnsRoot =
        wowRoot /
        L"Interface" /
        L"AddOns";

    std::error_code ec;

    for (std::filesystem::directory_iterator it(
             addOnsRoot,
             ec),
         end;
         !ec &&
         it != end;
         it.increment(ec)) {
        const auto status =
            it->symlink_status(ec);

        if (ec) {
            break;
        }

        if (!std::filesystem::is_directory(
                status) ||
            std::filesystem::is_symlink(
                status)) {
            continue;
        }

        AddonFolderInfo info;
        info.path =
            it->path();
        info.name =
            it->path()
                .filename()
                .wstring();

        if (!HasDirectToc(
                info.path,
                info.hasRootToc,
                error) ||
            !HasGitMetadata(
                info.path,
                info.hasGitMetadata,
                error)) {
            return false;
        }

        if (managedRoots.find(
                info.name) !=
            managedRoots.end()) {
            info.kind =
                AddonFolderKind::ManagedAddon;
        } else if (
            StartsWithInsensitive(
                info.name,
                L"Blizzard_")) {
            info.kind =
                AddonFolderKind::BlizzardSystemAddon;
        } else if (
            info.hasRootToc) {
            info.kind =
                AddonFolderKind::UnmanagedAddon;
        } else if (
            info.hasGitMetadata) {
            info.kind =
                AddonFolderKind::GitContainer;

            if (!CollectOneLevelAddonRoots(
                    info.path,
                    info.oneLevelAddonRoots,
                    error)) {
                return false;
            }
        } else {
            info.kind =
                AddonFolderKind::NonAddon;
        }

        folders.push_back(
            std::move(info));
    }

    if (ec) {
        error =
            L"Could not enumerate Interface\\AddOns.";
        return false;
    }

    std::sort(
        folders.begin(),
        folders.end(),
        [](const AddonFolderInfo& left,
           const AddonFolderInfo& right) {
            return InsensitiveLess{}(
                left.name,
                right.name);
        });

    return true;
}

} // namespace tp
