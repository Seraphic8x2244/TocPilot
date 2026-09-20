#include "adoption.h"
#include "provider.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <utility>

namespace tp {
namespace {

std::string Trim(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string Lower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](char ch) {
            return static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch)));
        });
    return value;
}

bool EqualsInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.size(); ++i) {
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

    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::towlower(value[i]) !=
            std::towlower(prefix[i])) {
            return false;
        }
    }
    return true;
}

bool IsGitMetadataName(std::wstring_view value) {
    return EqualsInsensitive(value, L".git");
}

bool ReadTextFile(
    const std::filesystem::path& path,
    std::string& content,
    std::wstring& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error =
            L"Could not read Git metadata: " +
            path.wstring();
        return false;
    }

    content.assign(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
    return true;
}

bool AsciiToWide(
    std::string_view value,
    std::wstring& result,
    std::wstring& error) {
    result.clear();
    result.reserve(value.size());

    for (const unsigned char ch : value) {
        if (ch >= 0x80) {
            error =
                L"Git metadata contains non-ASCII text that this adoption pass "
                L"does not handle safely.";
            return false;
        }
        result.push_back(static_cast<wchar_t>(ch));
    }

    return true;
}

bool IsHexRevision(std::string_view value) {
    if (value.size() != 40) {
        return false;
    }

    for (const unsigned char ch : value) {
        if (!std::isxdigit(ch)) {
            return false;
        }
    }
    return true;
}

bool ParseSection(
    std::string_view section,
    std::string_view kind,
    std::string& name) {
    const std::string value = Trim(section);
    const std::string prefix =
        std::string(kind) + " \"";

    if (value.size() <= prefix.size() ||
        value.compare(0, prefix.size(), prefix) != 0 ||
        value.back() != '"') {
        return false;
    }

    name = value.substr(
        prefix.size(),
        value.size() - prefix.size() - 1);
    return !name.empty();
}

struct GitConfigInfo {
    std::string remoteUrl;
    std::string branchRemote;
    std::string branchMerge;
};

bool ReadGitConfig(
    const std::filesystem::path& path,
    std::string_view branch,
    GitConfigInfo& info,
    std::wstring& error) {
    info = {};

    std::string content;
    if (!ReadTextFile(path, content, error)) {
        return false;
    }

    std::string section;
    std::size_t start = 0;

    while (start <= content.size()) {
        const std::size_t newline =
            content.find('\n', start);
        const std::size_t end =
            newline == std::string::npos
                ? content.size()
                : newline;

        std::string line =
            Trim(
                std::string_view(content).substr(
                    start,
                    end - start));

        if (!line.empty() &&
            line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty() &&
            line.front() != '#' &&
            line.front() != ';') {
            if (line.size() >= 2 &&
                line.front() == '[' &&
                line.back() == ']') {
                section =
                    line.substr(
                        1,
                        line.size() - 2);
            } else {
                const std::size_t equals =
                    line.find('=');

                if (equals !=
                    std::string::npos) {
                    const std::string key =
                        Lower(
                            Trim(
                                std::string_view(line).substr(
                                    0,
                                    equals)));

                    const std::string value =
                        Trim(
                            std::string_view(line).substr(
                                equals + 1));

                    std::string name;
                    if (ParseSection(
                            section,
                            "remote",
                            name) &&
                        name == "origin" &&
                        key == "url") {
                        info.remoteUrl = value;
                    } else if (
                        ParseSection(
                            section,
                            "branch",
                            name) &&
                        name == branch) {
                        if (key == "remote") {
                            info.branchRemote = value;
                        } else if (key == "merge") {
                            info.branchMerge = value;
                        }
                    }
                }
            }
        }

        if (newline ==
            std::string::npos) {
            break;
        }

        start = newline + 1;
    }

    if (info.remoteUrl.empty()) {
        error =
            L"Git repository has no origin URL.";
        return false;
    }

    if (info.branchRemote != "origin") {
        error =
            L"Git branch is not configured to track the origin remote.";
        return false;
    }

    const std::string expectedMerge =
        "refs/heads/" +
        std::string(branch);

    if (info.branchMerge !=
        expectedMerge) {
        error =
            L"Git branch upstream metadata does not match the checked-out branch.";
        return false;
    }

    return true;
}

bool ReadHead(
    const std::filesystem::path& gitDir,
    std::string& ref,
    std::string& branch,
    std::wstring& error) {
    std::string content;
    if (!ReadTextFile(
            gitDir / "HEAD",
            content,
            error)) {
        return false;
    }

    content = Trim(content);

    constexpr std::string_view prefix =
        "ref: refs/heads/";

    if (content.rfind(prefix, 0) != 0 ||
        content.size() <= prefix.size()) {
        error =
            L"Git repository has a detached or unsupported HEAD; adoption "
            L"requires an attached local branch.";
        return false;
    }

    ref = content.substr(5);
    branch =
        content.substr(
            prefix.size());

    return true;
}

bool ReadRevision(
    const std::filesystem::path& gitDir,
    std::string_view ref,
    std::string& revision,
    std::wstring& error) {
    const auto loosePath =
        gitDir /
        std::filesystem::path(
            std::string(ref));

    std::error_code ec;

    if (std::filesystem::is_regular_file(
            loosePath,
            ec) &&
        !ec) {
        std::string content;
        if (!ReadTextFile(
                loosePath,
                content,
                error)) {
            return false;
        }

        revision =
            Trim(content);
    } else {
        ec.clear();

        const auto packedPath =
            gitDir /
            "packed-refs";

        if (!std::filesystem::is_regular_file(
                packedPath,
                ec) ||
            ec) {
            error =
                L"Git branch revision could not be resolved from local metadata.";
            return false;
        }

        std::string content;
        if (!ReadTextFile(
                packedPath,
                content,
                error)) {
            return false;
        }

        const std::string wanted(ref);
        std::size_t start = 0;

        while (start <=
               content.size()) {
            const std::size_t newline =
                content.find(
                    '\n',
                    start);

            const std::size_t end =
                newline ==
                        std::string::npos
                    ? content.size()
                    : newline;

            const std::string line =
                Trim(
                    std::string_view(content).substr(
                        start,
                        end - start));

            if (!line.empty() &&
                line.front() != '#' &&
                line.front() != '^') {
                const std::size_t space =
                    line.find(' ');

                if (space !=
                        std::string::npos &&
                    line.substr(
                        space + 1) ==
                        wanted) {
                    revision =
                        line.substr(
                            0,
                            space);
                    break;
                }
            }

            if (newline ==
                std::string::npos) {
                break;
            }

            start =
                newline + 1;
        }
    }

    if (!IsHexRevision(revision)) {
        error =
            L"Git branch revision is not a valid 40-character commit SHA.";
        return false;
    }

    return true;
}

bool HasRootToc(
    const std::filesystem::path& addonRoot,
    std::wstring& error) {
    std::error_code ec;

    for (std::filesystem::directory_iterator it(
             addonRoot,
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

        if (std::filesystem::is_symlink(
                status)) {
            continue;
        }

        if (std::filesystem::is_regular_file(
                status)) {
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

            if (extension ==
                L".toc") {
                return true;
            }
        }
    }

    if (ec) {
        error =
            L"Could not inspect the addon root for a .toc file.";
    } else {
        error =
            L"Git repository has no root-level .toc file; complex/unpacked "
            L"GitAddonsManager layouts are not adopted automatically.";
    }

    return false;
}

bool CollectInstalledFiles(
    const std::filesystem::path& addonRoot,
    std::wstring_view installFolder,
    std::vector<std::wstring>& files,
    std::wstring& error) {
    files.clear();

    std::error_code ec;

    std::filesystem::recursive_directory_iterator it(
        addonRoot,
        std::filesystem::directory_options::none,
        ec);

    const std::filesystem::recursive_directory_iterator end;

    for (;
         !ec &&
         it != end;
         it.increment(ec)) {
        const auto relative =
            it->path()
                .lexically_relative(
                    addonRoot);

        if (relative.empty()) {
            continue;
        }

        auto first =
            relative.begin();

        if (first !=
                relative.end() &&
            IsGitMetadataName(
                first->wstring())) {
            if (it->is_directory(
                    ec)) {
                it.disable_recursion_pending();
            }
            ec.clear();
            continue;
        }

        const auto status =
            it->symlink_status(ec);

        if (ec) {
            break;
        }

        if (std::filesystem::is_symlink(
                status)) {
            error =
                L"Addon root contains a symbolic link; adoption refuses "
                L"ambiguous filesystem ownership.";
            return false;
        }

        if (std::filesystem::is_regular_file(
                status)) {
            files.push_back(
                (
                    std::filesystem::path(
                        L"Interface") /
                    L"AddOns" /
                    installFolder /
                    relative)
                    .generic_wstring());
        }
    }

    if (ec) {
        error =
            L"Could not enumerate all addon files for adoption.";
        return false;
    }

    if (files.empty()) {
        error =
            L"Git addon root contains no installable files.";
        return false;
    }

    std::sort(
        files.begin(),
        files.end(),
        [](const auto& left,
           const auto& right) {
            std::wstring a = left;
            std::wstring b = right;

            std::transform(
                a.begin(),
                a.end(),
                a.begin(),
                [](wchar_t ch) {
                    return static_cast<wchar_t>(
                        std::towlower(ch));
                });

            std::transform(
                b.begin(),
                b.end(),
                b.begin(),
                [](wchar_t ch) {
                    return static_cast<wchar_t>(
                        std::towlower(ch));
                });

            return a < b;
        });

    return true;
}

bool ResolveStateTarget(
    const AppState& state,
    std::wstring_view packageId,
    std::wstring_view installFolder,
    std::optional<std::size_t>& existingPackageIndex,
    std::wstring& error) {
    existingPackageIndex.reset();

    const std::wstring rootPrefix =
        (
            std::filesystem::path(
                L"Interface") /
            L"AddOns" /
            installFolder)
            .generic_wstring() +
        L"/";

    for (std::size_t index = 0;
         index < state.packages.size();
         ++index) {
        const auto& existing =
            state.packages[index];

        if (EqualsInsensitive(
                existing.id,
                packageId)) {
            if (!existing.installedRevision.empty() ||
                !existing.installedFiles.empty()) {
                error =
                    L"This repository is already managed by TocPilot.";
                return false;
            }

            if (existing.target != L"addons") {
                error =
                    L"The matching TocPilot package does not target addons and cannot "
                    L"adopt this folder in place.";
                return false;
            }

            existingPackageIndex =
                index;
        }

        for (auto owned :
             existing.installedFiles) {
            std::replace(
                owned.begin(),
                owned.end(),
                L'\\',
                L'/');

            if (StartsWithInsensitive(
                    owned,
                    rootPrefix)) {
                error =
                    L"This addon root is already owned by another TocPilot package.";
                return false;
            }
        }
    }

    return true;
}

} // namespace

bool PlanGitAddonAdoption(
    const std::filesystem::path& wowRoot,
    const std::filesystem::path& addonRoot,
    const AppState& state,
    GitAddonAdoptionPlan& plan,
    std::wstring& error) {
    plan = {};
    error.clear();

    std::error_code ec;

    const auto rootStatus =
        std::filesystem::symlink_status(
            addonRoot,
            ec);

    if (ec ||
        !std::filesystem::is_directory(
            rootStatus) ||
        std::filesystem::is_symlink(
            rootStatus)) {
        error =
            L"Adoption target is not a normal addon directory.";
        return false;
    }

    const auto addOnsRoot =
        wowRoot /
        L"Interface" /
        L"AddOns";

    if (!std::filesystem::equivalent(
            addonRoot.parent_path(),
            addOnsRoot,
            ec) ||
        ec) {
        error =
            L"Adoption target must be a direct child of Interface\\AddOns.";
        return false;
    }

    const std::wstring installFolder =
        addonRoot
            .filename()
            .wstring();

    if (installFolder.empty() ||
        installFolder == L"." ||
        installFolder == L"..") {
        error =
            L"Addon folder name is invalid.";
        return false;
    }

    const auto gitDir =
        addonRoot /
        L".git";

    const auto gitStatus =
        std::filesystem::symlink_status(
            gitDir,
            ec);

    if (ec ||
        !std::filesystem::is_directory(
            gitStatus) ||
        std::filesystem::is_symlink(
            gitStatus)) {
        error =
            L"Addon does not contain a normal .git directory; linked "
            L"worktrees and submodules are not adopted automatically.";
        return false;
    }

    if (!HasRootToc(
            addonRoot,
            error)) {
        return false;
    }

    std::string ref;
    std::string branch;

    if (!ReadHead(
            gitDir,
            ref,
            branch,
            error)) {
        return false;
    }

    GitConfigInfo config;

    if (!ReadGitConfig(
            gitDir /
                L"config",
            branch,
            config,
            error)) {
        return false;
    }

    std::wstring remoteUrl;

    if (!AsciiToWide(
            config.remoteUrl,
            remoteUrl,
            error)) {
        return false;
    }

    RepositoryIdentity identity;

    if (!NormalizeRepositoryUrl(
            remoteUrl,
            identity,
            error)) {
        error =
            L"Git origin is not a supported public repository: " +
            error;
        return false;
    }

    if (identity.provider !=
        ProviderKind::GitHub) {
        error =
            L"Only GitHub branch installs can be adopted in this TocPilot "
            L"version; GitLab adoption remains deferred with GitLab package support.";
        return false;
    }

    std::string revisionAscii;

    if (!ReadRevision(
            gitDir,
            ref,
            revisionAscii,
            error)) {
        return false;
    }

    std::wstring branchWide;
    std::wstring revisionWide;

    if (!AsciiToWide(
            branch,
            branchWide,
            error) ||
        !AsciiToWide(
            revisionAscii,
            revisionWide,
            error)) {
        return false;
    }

    const std::wstring packageId =
        L"github:" +
        identity.repository;

    std::optional<std::size_t>
        existingPackageIndex;

    if (!ResolveStateTarget(
            state,
            packageId,
            installFolder,
            existingPackageIndex,
            error)) {
        return false;
    }

    std::vector<std::wstring>
        installedFiles;

    if (!CollectInstalledFiles(
            addonRoot,
            installFolder,
            installedFiles,
            error)) {
        return false;
    }

    PackageRecord package;

    if (existingPackageIndex) {
        package =
            state.packages[
                *existingPackageIndex];
    } else {
        package.id =
            packageId;
        package.name =
            installFolder;
        package.provider =
            L"github";
        package.repository =
            identity.repository;
        package.target =
            L"addons";
    }

    package.mode =
        L"branch";
    package.ref =
        std::move(branchWide);
    package.installedRevision =
        revisionWide;
    package.latestRevision =
        std::move(revisionWide);
    package.installedFiles =
        std::move(installedFiles);

    plan.addonRoot =
        addonRoot;
    plan.package =
        std::move(package);
    plan.existingPackageIndex =
        existingPackageIndex;

    return true;
}

} // namespace tp
