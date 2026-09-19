#include "install.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <iomanip>
#include <set>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace tp {
namespace {

constexpr int kFilesystemRetries = 5;
constexpr auto kFilesystemRetryDelay = std::chrono::milliseconds(50);

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
                return std::towlower(a) < std::towlower(b);
            });
    }
};

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

bool IsReservedDeviceName(std::wstring_view segment) {
    const std::size_t dot = segment.find(L'.');
    std::wstring stem(
        segment.substr(
            0,
            dot == std::wstring_view::npos
                ? segment.size()
                : dot));

    std::transform(
        stem.begin(),
        stem.end(),
        stem.begin(),
        [](wchar_t ch) {
            return std::towupper(ch);
        });

    if (stem == L"CON" ||
        stem == L"PRN" ||
        stem == L"AUX" ||
        stem == L"NUL") {
        return true;
    }

    if (stem.size() == 4 &&
        (stem.rfind(L"COM", 0) == 0 ||
         stem.rfind(L"LPT", 0) == 0) &&
        stem[3] >= L'1' &&
        stem[3] <= L'9') {
        return true;
    }

    return false;
}

bool SafeInstallFolderName(
    std::wstring_view folder,
    std::wstring& error) {
    if (folder.empty() ||
        folder == L"." ||
        folder == L"..") {
        error = L"Addon install folder is empty or unsafe.";
        return false;
    }

    if (folder.back() == L'.' ||
        folder.back() == L' ') {
        error =
            L"Addon install folder ends with a dot or space.";
        return false;
    }

    for (const wchar_t ch : folder) {
        if (ch < 0x20 ||
            ch == L'<' ||
            ch == L'>' ||
            ch == L':' ||
            ch == L'"' ||
            ch == L'/' ||
            ch == L'\\' ||
            ch == L'|' ||
            ch == L'?' ||
            ch == L'*') {
            error =
                L"Addon install folder contains an unsafe Windows character.";
            return false;
        }
    }

    if (IsReservedDeviceName(folder)) {
        error =
            L"Addon install folder uses a reserved Windows device name.";
        return false;
    }

    return true;
}

bool SafeSourceRelativePath(
    const std::filesystem::path& path) {
    if (path.empty() ||
        path.is_absolute() ||
        path.has_root_name() ||
        path.has_root_directory()) {
        return false;
    }

    for (const auto& part : path) {
        if (part.empty() ||
            part == L"." ||
            part == L"..") {
            return false;
        }
    }

    return true;
}

std::wstring TransactionKey(std::wstring_view packageId) {
    std::wstring key;
    key.reserve(std::min<std::size_t>(packageId.size(), 48) + 17);

    for (const wchar_t ch : packageId) {
        if (key.size() >= 48) {
            break;
        }

        if ((ch >= L'a' && ch <= L'z') ||
            (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'0' && ch <= L'9') ||
            ch == L'-' ||
            ch == L'_' ||
            ch == L'.') {
            key.push_back(ch);
        } else {
            key.push_back(L'-');
        }
    }

    if (key.empty()) {
        key = L"package";
    }

    std::uint64_t hash = 1469598103934665603ULL;
    for (const wchar_t ch : packageId) {
        hash ^= static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(ch));
        hash *= 1099511628211ULL;
    }

    std::wostringstream stream;
    stream
        << key
        << L"-"
        << std::hex
        << std::setw(16)
        << std::setfill(L'0')
        << hash;
    return stream.str();
}

bool ParseOwnedAddonRoot(
    std::wstring_view ownedFile,
    std::wstring& root,
    std::wstring& error) {
    std::wstring normalized(ownedFile);
    std::replace(
        normalized.begin(),
        normalized.end(),
        L'\\',
        L'/');

    if (normalized.empty() ||
        normalized.front() == L'/' ||
        normalized.find(L':') != std::wstring::npos) {
        error =
            L"Installed-file ownership contains an unsafe path.";
        return false;
    }

    std::vector<std::wstring> parts;
    std::size_t start = 0;
    while (start <= normalized.size()) {
        const std::size_t slash =
            normalized.find(L'/', start);
        const std::size_t end =
            slash == std::wstring::npos
                ? normalized.size()
                : slash;

        if (end == start) {
            error =
                L"Installed-file ownership contains an empty path segment.";
            return false;
        }

        std::wstring part =
            normalized.substr(start, end - start);
        if (part == L"." || part == L"..") {
            error =
                L"Installed-file ownership contains traversal.";
            return false;
        }

        parts.push_back(std::move(part));
        if (slash == std::wstring::npos) {
            break;
        }
        start = slash + 1;
    }

    if (parts.size() < 4 ||
        !EqualsInsensitive(parts[0], L"Interface") ||
        !EqualsInsensitive(parts[1], L"AddOns")) {
        error =
            L"Installed-file ownership is outside Interface\\AddOns.";
        return false;
    }

    if (!SafeInstallFolderName(parts[2], error)) {
        return false;
    }

    root = std::move(parts[2]);
    return true;
}

bool CollectOwnedRoots(
    const std::vector<std::wstring>& files,
    std::set<std::wstring, InsensitiveLess>& roots,
    std::wstring& error) {
    roots.clear();

    for (const auto& file : files) {
        std::wstring root;
        if (!ParseOwnedAddonRoot(file, root, error)) {
            return false;
        }
        roots.insert(std::move(root));
    }

    return true;
}

std::wstring OwnedFilePath(
    std::wstring_view installFolder,
    const std::filesystem::path& relative) {
    return (
        std::filesystem::path(L"Interface") /
        L"AddOns" /
        installFolder /
        relative)
        .generic_wstring();
}

std::wstring ErrorMessage(
    const std::error_code& ec) {
    const std::string message = ec.message();
    return std::wstring(
        message.begin(),
        message.end());
}

bool RemoveAllWithRetries(
    const std::filesystem::path& path,
    std::wstring& error) {
    std::error_code last;

    for (int attempt = 0;
         attempt < kFilesystemRetries;
         ++attempt) {
        last.clear();
        std::filesystem::remove_all(path, last);

        std::error_code existsError;
        const bool exists =
            std::filesystem::exists(path, existsError);

        if (!last && !existsError && !exists) {
            return true;
        }

        if (attempt + 1 < kFilesystemRetries) {
            std::this_thread::sleep_for(
                kFilesystemRetryDelay);
        }
    }

    error =
        L"Could not remove " +
        path.wstring() +
        L": " +
        ErrorMessage(last);
    return false;
}

bool RenameWithRetries(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::wstring& error) {
    std::error_code last;

    for (int attempt = 0;
         attempt < kFilesystemRetries;
         ++attempt) {
        last.clear();
        std::filesystem::rename(from, to, last);
        if (!last) {
            return true;
        }

        if (attempt + 1 < kFilesystemRetries) {
            std::this_thread::sleep_for(
                kFilesystemRetryDelay);
        }
    }

    error =
        L"Could not move " +
        from.wstring() +
        L" to " +
        to.wstring() +
        L": " +
        ErrorMessage(last);
    return false;
}

bool CopyTree(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::wstring& error) {
    std::error_code ec;
    const auto sourceStatus =
        std::filesystem::symlink_status(source, ec);

    if (ec ||
        std::filesystem::is_symlink(sourceStatus) ||
        !std::filesystem::is_directory(sourceStatus)) {
        error =
            L"Staged addon root is not a normal directory: " +
            source.wstring();
        return false;
    }

    std::filesystem::create_directories(
        destination,
        ec);
    if (ec) {
        error =
            L"Could not prepare transaction directory: " +
            destination.wstring();
        return false;
    }

    std::filesystem::recursive_directory_iterator it(
        source,
        std::filesystem::directory_options::none,
        ec);
    const std::filesystem::recursive_directory_iterator end;

    if (ec) {
        error =
            L"Could not enumerate staged addon root: " +
            source.wstring();
        return false;
    }

    for (; it != end; it.increment(ec)) {
        if (ec) {
            error =
                L"Could not enumerate staged addon files: " +
                source.wstring();
            return false;
        }

        const auto status =
            std::filesystem::symlink_status(
                it->path(),
                ec);
        if (ec) {
            error =
                L"Could not inspect staged addon path: " +
                it->path().wstring();
            return false;
        }

        if (std::filesystem::is_symlink(status)) {
            error =
                L"Staged addon contains a symbolic link: " +
                it->path().wstring();
            return false;
        }

        const auto relative =
            it->path().lexically_relative(source);
        if (relative.empty() ||
            !SafeSourceRelativePath(relative)) {
            error =
                L"Staged addon contains an unsafe relative path.";
            return false;
        }

        const auto target =
            destination / relative;

        if (std::filesystem::is_directory(status)) {
            std::filesystem::create_directories(
                target,
                ec);
            if (ec) {
                error =
                    L"Could not create prepared addon directory: " +
                    target.wstring();
                return false;
            }
            continue;
        }

        if (!std::filesystem::is_regular_file(status)) {
            error =
                L"Staged addon contains an unsupported filesystem object: " +
                it->path().wstring();
            return false;
        }

        std::filesystem::create_directories(
            target.parent_path(),
            ec);
        if (ec) {
            error =
                L"Could not create prepared addon parent directory.";
            return false;
        }

        std::filesystem::copy_file(
            it->path(),
            target,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        if (ec) {
            error =
                L"Could not copy staged addon file into the transaction: " +
                it->path().wstring();
            return false;
        }
    }

    return true;
}

bool RootExists(
    const std::filesystem::path& path,
    bool& exists,
    std::wstring& error) {
    std::error_code ec;
    const auto status =
        std::filesystem::symlink_status(path, ec);

    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            exists = false;
            return true;
        }

        error =
            L"Could not inspect live addon path: " +
            path.wstring();
        return false;
    }

    if (!std::filesystem::exists(status)) {
        exists = false;
        return true;
    }

    if (std::filesystem::is_symlink(status) ||
        !std::filesystem::is_directory(status)) {
        error =
            L"Live addon destination is not a normal directory: " +
            path.wstring();
        return false;
    }

    exists = true;
    return true;
}

std::filesystem::path BackupPath(
    const AddonInstallPlan& plan,
    std::wstring_view folder) {
    return
        plan.transactionRoot /
        L"backup" /
        folder;
}

bool RestoreBackups(
    AddonInstallTransaction& transaction,
    std::wstring& error) {
    bool ok = true;
    std::wstring firstError;

    for (auto it = transaction.installedRoots.rbegin();
         it != transaction.installedRoots.rend();
         ++it) {
        std::wstring removeError;
        if (!RemoveAllWithRetries(
                transaction.plan.addOnsRoot / *it,
                removeError) &&
            firstError.empty()) {
            firstError = std::move(removeError);
            ok = false;
        }
    }

    for (auto it = transaction.backedUpRoots.rbegin();
         it != transaction.backedUpRoots.rend();
         ++it) {
        const auto live =
            transaction.plan.addOnsRoot / *it;
        const auto backup =
            BackupPath(transaction.plan, *it);

        std::error_code existsError;
        const bool backupExists =
            std::filesystem::exists(
                backup,
                existsError);
        if (existsError) {
            if (firstError.empty()) {
                firstError =
                    L"Could not inspect rollback backup for " +
                    *it +
                    L".";
            }
            ok = false;
            continue;
        }

        if (!backupExists) {
            bool liveExists = false;
            std::wstring liveError;
            if (RootExists(
                    live,
                    liveExists,
                    liveError) &&
                liveExists) {
                // A prior rollback attempt already restored this root.
                continue;
            }

            if (firstError.empty()) {
                firstError =
                    liveError.empty()
                        ? L"Rollback backup is missing for " +
                            *it +
                            L"."
                        : std::move(liveError);
            }
            ok = false;
            continue;
        }

        std::wstring removeError;
        if (!RemoveAllWithRetries(
                live,
                removeError)) {
            if (firstError.empty()) {
                firstError = std::move(removeError);
            }
            ok = false;
            continue;
        }

        std::wstring moveError;
        if (!RenameWithRetries(
                backup,
                live,
                moveError)) {
            if (firstError.empty()) {
                firstError = std::move(moveError);
            }
            ok = false;
        }
    }

    if (!ok) {
        error = std::move(firstError);
    }
    return ok;
}

} // namespace

bool BuildAddonInstallPlan(
    const std::filesystem::path& wowRoot,
    std::wstring_view packageId,
    const std::filesystem::path& extractedRoot,
    const std::vector<AddonCandidate>& candidates,
    const std::vector<std::wstring>& priorInstalledFiles,
    const std::vector<std::wstring>& otherInstalledFiles,
    AddonInstallPlan& plan,
    std::wstring& error) {
    plan = {};
    error.clear();

    if (wowRoot.empty() ||
        packageId.empty() ||
        extractedRoot.empty()) {
        error =
            L"Install planning requires WoW root, package id, and extracted archive root.";
        return false;
    }

    if (candidates.empty()) {
        error =
            L"The staged archive contains no addon roots to install.";
        return false;
    }

    std::set<std::wstring, InsensitiveLess> priorRoots;
    std::set<std::wstring, InsensitiveLess> otherRoots;
    if (!CollectOwnedRoots(
            priorInstalledFiles,
            priorRoots,
            error) ||
        !CollectOwnedRoots(
            otherInstalledFiles,
            otherRoots,
            error)) {
        return false;
    }

    for (const auto& root : priorRoots) {
        if (otherRoots.contains(root)) {
            error =
                L"Package ownership overlaps another managed package at addon root " +
                root +
                L".";
            return false;
        }
    }

    plan.wowRoot = wowRoot;
    plan.addOnsRoot =
        wowRoot /
        L"Interface" /
        L"AddOns";
    plan.transactionRoot =
        wowRoot /
        L"Interface" /
        L"TocPilot" /
        L"transactions" /
        TransactionKey(packageId);

    std::set<std::wstring, InsensitiveLess> desiredRoots;
    std::set<std::wstring, InsensitiveLess> desiredFiles;
    const auto preparedRoot =
        plan.transactionRoot /
        L"prepared";

    for (const auto& candidate : candidates) {
        if (!SafeInstallFolderName(
                candidate.installFolder,
                error)) {
            return false;
        }

        if (!desiredRoots.insert(
                candidate.installFolder).second) {
            error =
                L"The archive maps more than one addon root to " +
                candidate.installFolder +
                L".";
            return false;
        }

        if (otherRoots.contains(
                candidate.installFolder)) {
            error =
                L"Addon root " +
                candidate.installFolder +
                L" is owned by another TocPilot package.";
            return false;
        }

        if (!SafeSourceRelativePath(
                candidate.sourceRelativePath)) {
            error =
                L"Addon candidate has an unsafe staged source path.";
            return false;
        }

        const auto source =
            extractedRoot /
            candidate.sourceRelativePath;

        std::error_code ec;
        const auto sourceStatus =
            std::filesystem::symlink_status(
                source,
                ec);
        if (ec ||
            std::filesystem::is_symlink(sourceStatus) ||
            !std::filesystem::is_directory(sourceStatus)) {
            error =
                L"Addon candidate source is missing or unsafe: " +
                source.wstring();
            return false;
        }

        bool hasToc = false;
        std::size_t regularFiles = 0;

        std::filesystem::recursive_directory_iterator it(
            source,
            std::filesystem::directory_options::none,
            ec);
        const std::filesystem::recursive_directory_iterator end;
        if (ec) {
            error =
                L"Could not enumerate addon candidate " +
                candidate.installFolder +
                L".";
            return false;
        }

        for (; it != end; it.increment(ec)) {
            if (ec) {
                error =
                    L"Could not enumerate addon candidate files.";
                return false;
            }

            const auto status =
                std::filesystem::symlink_status(
                    it->path(),
                    ec);
            if (ec) {
                error =
                    L"Could not inspect staged addon file.";
                return false;
            }

            if (std::filesystem::is_symlink(status)) {
                error =
                    L"Addon candidate contains a symbolic link.";
                return false;
            }

            if (std::filesystem::is_directory(status)) {
                continue;
            }

            if (!std::filesystem::is_regular_file(status)) {
                error =
                    L"Addon candidate contains an unsupported filesystem object.";
                return false;
            }

            const auto relative =
                it->path().lexically_relative(source);
            if (relative.empty() ||
                !SafeSourceRelativePath(relative)) {
                error =
                    L"Addon candidate contains an unsafe relative file path.";
                return false;
            }

            if (EqualsInsensitive(
                    relative.extension().wstring(),
                    L".toc")) {
                hasToc = true;
            }

            const std::wstring owned =
                OwnedFilePath(
                    candidate.installFolder,
                    relative);

            if (!desiredFiles.insert(owned).second) {
                error =
                    L"Addon install plan contains a case-insensitive file collision.";
                return false;
            }

            ++regularFiles;
        }

        if (!hasToc || regularFiles == 0) {
            error =
                L"Addon root " +
                candidate.installFolder +
                L" has no installable .toc package.";
            return false;
        }

        const auto live =
            plan.addOnsRoot /
            candidate.installFolder;

        bool liveExists = false;
        if (!RootExists(
                live,
                liveExists,
                error)) {
            return false;
        }

        if (liveExists &&
            !priorRoots.contains(
                candidate.installFolder)) {
            error =
                L"Refusing to replace existing unowned addon folder " +
                candidate.installFolder +
                L".";
            return false;
        }

        AddonInstallRoot root;
        root.sourceDirectory = source;
        root.installFolder =
            candidate.installFolder;
        root.liveDirectory = live;
        root.preparedDirectory =
            preparedRoot /
            candidate.installFolder;
        root.backupDirectory =
            BackupPath(
                plan,
                candidate.installFolder);
        plan.roots.push_back(std::move(root));
    }

    for (const auto& prior : priorRoots) {
        if (!desiredRoots.contains(prior)) {
            plan.obsoleteInstallFolders.push_back(prior);
        }
    }

    plan.desiredInstalledFiles.assign(
        desiredFiles.begin(),
        desiredFiles.end());

    std::sort(
        plan.roots.begin(),
        plan.roots.end(),
        [](const AddonInstallRoot& left,
           const AddonInstallRoot& right) {
            return InsensitiveLess{}(
                left.installFolder,
                right.installFolder);
        });
    std::sort(
        plan.obsoleteInstallFolders.begin(),
        plan.obsoleteInstallFolders.end(),
        InsensitiveLess{});

    return true;
}

bool BuildAddonRemovalPlan(
    const std::filesystem::path& wowRoot,
    std::wstring_view packageId,
    const std::vector<std::wstring>& installedFiles,
    const std::vector<std::wstring>& otherInstalledFiles,
    AddonInstallPlan& plan,
    std::wstring& error) {
    plan = {};
    error.clear();

    if (wowRoot.empty() || packageId.empty()) {
        error =
            L"Removal planning requires WoW root and package id.";
        return false;
    }

    if (installedFiles.empty()) {
        error =
            L"The selected package has no recorded installed files to remove.";
        return false;
    }

    std::set<std::wstring, InsensitiveLess> ownedRoots;
    std::set<std::wstring, InsensitiveLess> otherRoots;
    if (!CollectOwnedRoots(
            installedFiles,
            ownedRoots,
            error) ||
        !CollectOwnedRoots(
            otherInstalledFiles,
            otherRoots,
            error)) {
        return false;
    }

    if (ownedRoots.empty()) {
        error =
            L"The selected package has no owned addon roots to remove.";
        return false;
    }

    for (const auto& root : ownedRoots) {
        if (otherRoots.contains(root)) {
            error =
                L"Refusing to remove addon root " +
                root +
                L" because another TocPilot package also records ownership there.";
            return false;
        }
    }

    plan.wowRoot = wowRoot;
    plan.addOnsRoot =
        wowRoot /
        L"Interface" /
        L"AddOns";
    plan.transactionRoot =
        wowRoot /
        L"Interface" /
        L"TocPilot" /
        L"transactions" /
        TransactionKey(packageId);
    plan.obsoleteInstallFolders.assign(
        ownedRoots.begin(),
        ownedRoots.end());

    return true;
}

bool PrepareAddonInstallTransaction(
    const AddonInstallPlan& plan,
    AddonInstallTransaction& transaction,
    std::wstring& error) {
    transaction = {};
    error.clear();

    const bool hasInstallRoots =
        !plan.roots.empty();
    const bool hasRemovalRoots =
        !plan.obsoleteInstallFolders.empty();

    if ((!hasInstallRoots && !hasRemovalRoots) ||
        (hasInstallRoots &&
         plan.desiredInstalledFiles.empty()) ||
        plan.transactionRoot.empty()) {
        error =
            L"Install transaction plan is incomplete.";
        return false;
    }

    std::error_code ec;
    const bool transactionExists =
        std::filesystem::exists(
            plan.transactionRoot,
            ec);
    if (ec) {
        error =
            L"Could not inspect install transaction directory.";
        return false;
    }
    if (transactionExists) {
        error =
            L"An unfinished TocPilot install transaction already exists for this package.";
        return false;
    }

    const auto preparedRoot =
        plan.transactionRoot /
        L"prepared";
    const auto backupRoot =
        plan.transactionRoot /
        L"backup";

    std::filesystem::create_directories(
        preparedRoot,
        ec);
    if (ec) {
        error =
            L"Could not create install transaction staging directory.";
        return false;
    }

    std::filesystem::create_directories(
        backupRoot,
        ec);
    if (ec) {
        std::wstring cleanupError;
        RemoveAllWithRetries(
            plan.transactionRoot,
            cleanupError);
        error =
            L"Could not create install transaction backup directory.";
        return false;
    }

    for (const auto& root : plan.roots) {
        if (!CopyTree(
                root.sourceDirectory,
                root.preparedDirectory,
                error)) {
            std::wstring cleanupError;
            RemoveAllWithRetries(
                plan.transactionRoot,
                cleanupError);
            return false;
        }
    }

    transaction.plan = plan;
    transaction.prepared = true;
    return true;
}

bool CommitAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error,
    const AddonInstallOptions& options) {
    error.clear();

    if (!transaction.prepared ||
        transaction.active ||
        (transaction.plan.roots.empty() &&
         transaction.plan.obsoleteInstallFolders.empty())) {
        error =
            L"Install transaction is not ready to commit.";
        return false;
    }

    auto& plan = transaction.plan;
    std::error_code ec;

    std::filesystem::create_directories(
        plan.addOnsRoot,
        ec);
    if (ec) {
        error =
            L"Could not create Interface\\AddOns.";
        std::wstring cleanupError;
        RollbackAddonInstallTransaction(
            transaction,
            cleanupError);
        return false;
    }

    transaction.active = true;

    std::vector<std::wstring> rootsToBackup;
    for (const auto& root : plan.roots) {
        rootsToBackup.push_back(
            root.installFolder);
    }
    rootsToBackup.insert(
        rootsToBackup.end(),
        plan.obsoleteInstallFolders.begin(),
        plan.obsoleteInstallFolders.end());
    std::sort(
        rootsToBackup.begin(),
        rootsToBackup.end(),
        InsensitiveLess{});
    rootsToBackup.erase(
        std::unique(
            rootsToBackup.begin(),
            rootsToBackup.end(),
            [](const std::wstring& left,
               const std::wstring& right) {
                return EqualsInsensitive(
                    left,
                    right);
            }),
        rootsToBackup.end());

    for (const auto& folder : rootsToBackup) {
        const auto live =
            plan.addOnsRoot /
            folder;

        bool exists = false;
        if (!RootExists(
                live,
                exists,
                error)) {
            const std::wstring commitError = error;
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        if (!exists) {
            continue;
        }

        const auto backup =
            BackupPath(plan, folder);
        std::filesystem::create_directories(
            backup.parent_path(),
            ec);
        if (ec) {
            const std::wstring commitError =
                L"Could not create install rollback directory.";
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        if (!RenameWithRetries(
                live,
                backup,
                error)) {
            const std::wstring commitError = error;
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        transaction.backedUpRoots.push_back(
            folder);

        if (transaction.backedUpRoots.size() >=
                options.failAfterRootBackups) {
            const std::wstring commitError =
                L"Injected install failure after live root backup.";
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }
    }

    for (const auto& root : plan.roots) {
        if (!RenameWithRetries(
                root.preparedDirectory,
                root.liveDirectory,
                error)) {
            const std::wstring commitError = error;
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        transaction.installedRoots.push_back(
            root.installFolder);

        if (transaction.installedRoots.size() >=
                options.failAfterNewRootCommits) {
            const std::wstring commitError =
                L"Injected install failure after live root commit.";
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }
    }

    return true;
}

bool BeginAddonInstallTransaction(
    const AddonInstallPlan& plan,
    AddonInstallTransaction& transaction,
    std::wstring& error,
    const AddonInstallOptions& options) {
    if (!PrepareAddonInstallTransaction(
            plan,
            transaction,
            error)) {
        return false;
    }

    if (!CommitAddonInstallTransaction(
            transaction,
            error,
            options)) {
        std::wstring cleanupError;
        RollbackAddonInstallTransaction(
            transaction,
            cleanupError);
        return false;
    }

    return true;
}

bool RollbackAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.prepared) {
        return true;
    }

    if (transaction.active) {
        if (!RestoreBackups(
                transaction,
                error)) {
            return false;
        }

        transaction.active = false;
        transaction.backedUpRoots.clear();
        transaction.installedRoots.clear();
    }

    std::wstring cleanupError;
    if (!RemoveAllWithRetries(
            transaction.plan.transactionRoot,
            cleanupError)) {
        error = std::move(cleanupError);
        return false;
    }

    transaction.prepared = false;
    return true;
}

bool FinalizeAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.prepared) {
        return true;
    }

    // Once package state is saved, the new live files are authoritative.
    // Cleanup failure must never cause a later destructor/path to restore
    // the old backup over the committed installation.
    transaction.active = false;
    transaction.backedUpRoots.clear();
    transaction.installedRoots.clear();

    if (!RemoveAllWithRetries(
            transaction.plan.transactionRoot,
            error)) {
        return false;
    }

    transaction.prepared = false;
    return true;
}

} // namespace tp
