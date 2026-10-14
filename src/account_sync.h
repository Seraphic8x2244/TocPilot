#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

enum class AccountSyncItem {
    Macros = 0,
    Keybindings = 1,
    PfUi = 2
};

inline constexpr std::size_t kAccountSyncItemCount = 3;

struct AccountSyncConfig {
    std::vector<std::wstring> accounts;
    bool macros = false;
    bool keybindings = false;
    bool pfUi = false;
};

struct PfUiComparison {
    bool equivalent = false;
    bool cacheOnly = false;
    bool settingsChanged = false;
    bool cacheChanged = false;
};

// Recorded only at the engine's actual write/approval boundary; a fresh
// inspection must never be mistaken for a successful operation.
enum class AccountSyncTileRow {
    Keybindings = 0,
    Macros,
    PfUiSettings,
    PfUiCache
};

struct AccountSyncTargetOutcome {
    std::wstring account;
    AccountSyncTileRow row = AccountSyncTileRow::Keybindings;
    bool succeeded = false;
    bool declined = false;
    std::wstring error;
};

struct AccountSyncItemResult {
    AccountSyncItem item = AccountSyncItem::Macros;
    std::wstring sourceAccount;
    std::wstring timestampDetails;
    std::vector<std::wstring> confirmationTargets;
    std::vector<std::wstring> automaticTargets;
    bool comparerFallback = false;
    bool confirmationRequired = false;
    bool confirmationDeclined = false;
    bool fatal = false;
    std::size_t copiedTargets = 0;
    std::vector<AccountSyncTargetOutcome> targetOutcomes;
    std::wstring status;
    std::wstring action;
    std::wstring error;
};


// Read-only, snapshot-of-inspection projection for the future account tiles.
// This is not an approved/executable plan. RunAccountSync must still reanalyse
// and obtain its own confirmation and file freshness checks before writing.


inline constexpr std::size_t kAccountSyncTileRowCount = 4;

enum class AccountSyncTileState {
    Default,
    RuleSatisfied,
    Newer,
    Older
};

enum class AccountSyncTileOperationKind {
    CopyWholeFile,
    ReplaceProfiles,
    MergeSupportedCache
};

struct AccountSyncTileCell {
    AccountSyncTileRow row = AccountSyncTileRow::Keybindings;
    AccountSyncTileState state = AccountSyncTileState::Default;
    bool enabled = false;
    bool exists = false;
    // Zero denotes no content-equivalence group. Groups are 1-based.
    std::size_t matchingGroup = 0;
    // Semantic pfUI entries observed during this read-only inspection.
    std::vector<std::pair<std::string, std::string>> semanticEntries;
    // Keep full filesystem precision; formatting to seconds is a UI concern.
    std::filesystem::file_time_type modified{};
};

struct AccountSyncTileAccount {
    std::wstring account;
    std::array<AccountSyncTileCell, kAccountSyncTileRowCount> rows{};
};

struct AccountSyncTileGroup {
    AccountSyncTileRow row = AccountSyncTileRow::Keybindings;
    std::vector<std::wstring> accounts;
    bool selectedSourceGroup = false;
};

struct AccountSyncTileOperation {
    AccountSyncTileRow row = AccountSyncTileRow::Keybindings;
    AccountSyncTileOperationKind kind = AccountSyncTileOperationKind::CopyWholeFile;
    std::wstring targetAccount;
    // Copy/profile operation: the exact engine-selected account.
    // Cache merge: empty; contributors are independent group-wide inputs.
    std::wstring selectedSourceAccount;
    // Equivalent candidates for a copy/profile source, or all accounts with
    // supported cache input for a cache merge. Not a chain of operations.
    std::vector<std::wstring> sourceAccounts;
    bool requiresConfirmation = false;
    bool createsFile = false;
};

struct AccountSyncTileProjection {
    std::vector<AccountSyncTileAccount> accounts;
    std::vector<AccountSyncTileGroup> matchingGroups;
    std::vector<AccountSyncTileOperation> operations;
};

bool InspectAccountSyncTiles(
    const std::filesystem::path& wowRoot,
    const AccountSyncConfig& config,
    AccountSyncTileProjection& projection,
    std::wstring& error);

struct AccountSyncRunResult {
    std::array<AccountSyncItemResult, kAccountSyncItemCount> items;
    bool fatal = false;
};

using AccountSyncConfirmFn = std::function<bool(
    AccountSyncItem item,
    std::wstring_view sourceAccount,
    const std::vector<std::wstring>& targetAccounts,
    bool comparerFallback)>;

using AccountSyncPreLaunchFn =
    std::function<bool()>;

using AccountSyncLaunchFn =
    std::function<void(bool console)>;

const wchar_t* AccountSyncItemLabel(
    AccountSyncItem item);

bool AccountSyncItemEnabled(
    const AccountSyncConfig& config,
    AccountSyncItem item);

bool AccountSyncConfigured(
    const AccountSyncConfig& config);

bool RunAccountSyncLaunchFlow(
    bool syncBeforeLaunch,
    bool console,
    const AccountSyncPreLaunchFn& sync,
    const AccountSyncLaunchFn& launch);

bool DiscoverAccountNames(
    const std::filesystem::path& wowRoot,
    std::vector<std::wstring>& accounts,
    std::wstring& error);

bool ComparePfUiFiles(
    const std::filesystem::path& pathA,
    const std::filesystem::path& pathB,
    PfUiComparison& comparison,
    std::wstring& error);

std::wstring AccountSyncFileTime(
    const std::filesystem::path& wowRoot,
    std::wstring_view account,
    AccountSyncItem item);

bool InspectAccountSync(
    const std::filesystem::path& wowRoot,
    const AccountSyncConfig& config,
    std::array<AccountSyncItemResult, kAccountSyncItemCount>& items,
    std::wstring& error);

bool RunAccountSync(
    const std::filesystem::path& wowRoot,
    const AccountSyncConfig& config,
    const AccountSyncConfirmFn& confirm,
    AccountSyncRunResult& result,
    std::wstring& error);

} // namespace tp
