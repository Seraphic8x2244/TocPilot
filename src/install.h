#pragma once

#include "archive.h"

#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

struct AddonInstallRoot {
    std::filesystem::path sourceDirectory;
    std::wstring installFolder;
    std::filesystem::path liveDirectory;
    std::filesystem::path preparedDirectory;
    std::filesystem::path backupDirectory;
};

struct AddonInstallPlan {
    std::wstring packageId;
    std::filesystem::path wowRoot;
    std::filesystem::path addOnsRoot;
    std::filesystem::path transactionRoot;
    std::vector<AddonInstallRoot> roots;
    std::vector<std::wstring> obsoleteInstallFolders;
    std::vector<std::wstring> desiredInstalledFiles;
};

struct AddonInstallOptions {
    // Test hooks. Production callers leave these at the defaults.
    std::size_t failAfterRootBackups =
        std::numeric_limits<std::size_t>::max();
    std::size_t failAfterNewRootCommits =
        std::numeric_limits<std::size_t>::max();
    bool simulateCrashAfterRootBackupRename = false;
    bool simulateCrashAfterAllRootBackups = false;
    bool simulateCrashAfterNewRootCommitRename = false;
};

struct AddonTransactionStateMarker {
    std::wstring packageId;
    std::wstring transactionId;
    bool present = false;
};

enum class AddonTransactionRecoveryDecision {
    Rollback,
    Finalize,
    Conflict
};

struct AddonInstallTransaction {
    AddonInstallPlan plan;
    std::wstring transactionId;
    AddonTransactionStateMarker preState;
    AddonTransactionStateMarker postState;
    std::vector<std::wstring> preExistingRoots;
    std::vector<std::wstring> postRoots;
    std::vector<std::wstring> backedUpRoots;
    std::vector<std::wstring> installedRoots;
    bool prepared = false;
    bool journalArmed = false;
    bool active = false;
};

bool BuildAddonInstallPlan(
    const std::filesystem::path& wowRoot,
    std::wstring_view packageId,
    const std::filesystem::path& extractedRoot,
    const std::vector<AddonCandidate>& candidates,
    const std::vector<std::wstring>& priorInstalledFiles,
    const std::vector<std::wstring>& otherInstalledFiles,
    AddonInstallPlan& plan,
    std::wstring& error);

bool BuildAddonRemovalPlan(
    const std::filesystem::path& wowRoot,
    std::wstring_view packageId,
    const std::vector<std::wstring>& installedFiles,
    const std::vector<std::wstring>& otherInstalledFiles,
    AddonInstallPlan& plan,
    std::wstring& error);

bool PrepareAddonInstallTransaction(
    const AddonInstallPlan& plan,
    AddonInstallTransaction& transaction,
    std::wstring& error);

bool ArmAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    AddonTransactionStateMarker preState,
    AddonTransactionStateMarker postState,
    std::wstring& error);

AddonTransactionRecoveryDecision DecideAddonTransactionRecovery(
    const AddonTransactionStateMarker& preState,
    const AddonTransactionStateMarker& postState,
    const std::vector<AddonTransactionStateMarker>& currentState);

bool RecoverAddonInstallTransactions(
    const std::filesystem::path& wowRoot,
    const std::vector<AddonTransactionStateMarker>& currentState,
    std::wstring& error);

bool CommitAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error,
    const AddonInstallOptions& options = {});

bool RollbackAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error);

bool FinalizeAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error);

} // namespace tp
