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
    std::wstring status;
    std::wstring action;
    std::wstring error;
};

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
