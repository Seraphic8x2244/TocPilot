#include "account_sync.h"

#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <regex>
#include <system_error>
#include <utility>

namespace tp {
namespace {

struct PfUiNormalizedData {
    std::vector<std::string> settings;
    std::vector<std::string> cache;
};

struct AccountCopy {
    std::wstring account;
    std::size_t index = 0;
    std::filesystem::path path;
    bool exists = false;
    std::filesystem::file_time_type modified{};
};

struct ItemAnalysis {
    AccountSyncItem item = AccountSyncItem::Macros;
    bool noSource = false;
    bool comparerFallback = false;
    AccountCopy source;
    std::vector<AccountCopy> automaticTargets;
    std::vector<AccountCopy> confirmationTargets;
};

std::size_t ItemIndex(AccountSyncItem item) {
    return static_cast<std::size_t>(item);
}

std::wstring FilesystemError(
    const std::error_code& error) {
    if (!error) {
        return L"Unknown filesystem error.";
    }

    const std::string message =
        error.message();

    if (message.empty()) {
        return
            L"Filesystem error " +
            std::to_wstring(error.value()) +
            L".";
    }

    const int required =
        MultiByteToWideChar(
            CP_UTF8,
            0,
            message.c_str(),
            static_cast<int>(message.size()),
            nullptr,
            0);

    if (required <= 0) {
        return
            L"Filesystem error " +
            std::to_wstring(error.value()) +
            L".";
    }

    std::wstring wide(
        static_cast<std::size_t>(required),
        L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        message.c_str(),
        static_cast<int>(message.size()),
        wide.data(),
        required);

    return wide;
}

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

bool LessInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    const std::size_t count =
        std::min(
            left.size(),
            right.size());

    for (std::size_t i = 0;
         i < count;
         ++i) {
        const wchar_t a =
            static_cast<wchar_t>(
                std::towlower(left[i]));
        const wchar_t b =
            static_cast<wchar_t>(
                std::towlower(right[i]));

        if (a < b) {
            return true;
        }
        if (a > b) {
            return false;
        }
    }

    return left.size() < right.size();
}

bool SafeAccountName(
    std::wstring_view account) {
    if (account.empty() ||
        account == L"." ||
        account == L".." ||
        account.find(L'\\') !=
            std::wstring::npos ||
        account.find(L'/') !=
            std::wstring::npos ||
        account.find(L':') !=
            std::wstring::npos) {
        return false;
    }

    return
        std::filesystem::path(
            std::wstring(account))
            .filename()
            .wstring() ==
        account;
}

bool NormalizeAccounts(
    const std::vector<std::wstring>& input,
    std::vector<std::wstring>& output,
    std::wstring& error) {
    output.clear();

    for (const auto& account :
         input) {
        if (!SafeAccountName(account)) {
            error =
                L"Account Sync contains an unsafe account name: " +
                account;
            return false;
        }

        const bool duplicate =
            std::any_of(
                output.begin(),
                output.end(),
                [&](const std::wstring& value) {
                    return
                        EqualsInsensitive(
                            value,
                            account);
                });

        if (!duplicate) {
            output.push_back(account);
        }
    }

    return true;
}

std::filesystem::path ItemRelativePath(
    AccountSyncItem item) {
    switch (item) {
    case AccountSyncItem::Macros:
        return L"macros-cache.txt";
    case AccountSyncItem::Keybindings:
        return L"bindings-cache.wtf";
    case AccountSyncItem::PfUi:
        return
            std::filesystem::path(
                L"SavedVariables") /
            L"pfUI.lua";
    }

    return {};
}

std::filesystem::path AccountItemPath(
    const std::filesystem::path& wowRoot,
    std::wstring_view account,
    AccountSyncItem item) {
    return
        wowRoot /
        L"WTF" /
        L"Account" /
        std::wstring(account) /
        ItemRelativePath(item);
}

std::string Trim(
    std::string value) {
    const auto whitespace =
        [](unsigned char value) {
            return
                value == ' ' ||
                value == '\t' ||
                value == '\r' ||
                value == '\n' ||
                value == '\f' ||
                value == '\v';
        };

    while (!value.empty() &&
           whitespace(
               static_cast<unsigned char>(
                   value.front()))) {
        value.erase(value.begin());
    }

    while (!value.empty() &&
           whitespace(
               static_cast<unsigned char>(
                   value.back()))) {
        value.pop_back();
    }

    return value;
}

std::string LowerAscii(
    std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            if (ch >= 'A' &&
                ch <= 'Z') {
                return
                    static_cast<char>(
                        ch - 'A' + 'a');
            }
            return
                static_cast<char>(ch);
        });

    return value;
}

std::string JoinStack(
    const std::vector<std::string>& stack) {
    std::string result;

    for (std::size_t i = 0;
         i < stack.size();
         ++i) {
        if (i != 0) {
            result.push_back(
                static_cast<char>(31));
        }
        result += stack[i];
    }

    return result;
}

enum class PfUiDataClass {
    Settings,
    Cache,
    Junk
};

PfUiDataClass GetPfUiDataClass(
    const std::vector<std::string>& stack) {
    if (stack.size() >= 2 &&
        stack[0] ==
            "root:pfUI_cache") {
        const std::string key =
            LowerAscii(stack[1]);

        if (key ==
            "string:chathistory") {
            return PfUiDataClass::Junk;
        }

        if (key ==
                "string:libhealth" ||
            key ==
                "string:prediction" ||
            key ==
                "string:gold") {
            return PfUiDataClass::Cache;
        }
    }

    return PfUiDataClass::Settings;
}

bool ReadTextFileLines(
    const std::filesystem::path& path,
    std::vector<std::string>& lines,
    std::wstring& error) {
    lines.clear();

    std::ifstream stream(
        path,
        std::ios::binary);

    if (!stream) {
        error =
            L"Could not open pfUI file for semantic comparison: " +
            path.wstring();
        return false;
    }

    std::string line;
    while (std::getline(
               stream,
               line)) {
        if (!line.empty() &&
            line.back() == '\r') {
            line.pop_back();
        }

        lines.push_back(
            std::move(line));
    }

    if (!stream.eof()) {
        error =
            L"Could not read pfUI file for semantic comparison: " +
            path.wstring();
        return false;
    }

    if (!lines.empty() &&
        lines[0].size() >= 3 &&
        static_cast<unsigned char>(
            lines[0][0]) == 0xEF &&
        static_cast<unsigned char>(
            lines[0][1]) == 0xBB &&
        static_cast<unsigned char>(
            lines[0][2]) == 0xBF) {
        lines[0].erase(0, 3);
    }

    return true;
}

bool NormalizePfUi(
    const std::filesystem::path& path,
    PfUiNormalizedData& data,
    std::wstring& error) {
    data = {};

    std::vector<std::string> lines;

    if (!ReadTextFileLines(
            path,
            lines,
            error)) {
        return false;
    }

    static const std::regex rootPattern(
        R"(^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\{$)");
    static const std::regex tablePattern(
        R"tp(^\[(?:"((?:[^"\\]|\\.)*)"|(\d+))\]\s*=\s*\{$)tp");
    static const std::regex valuePattern(
        R"tp(^\[(?:"((?:[^"\\]|\\.)*)"|(\d+))\]\s*=\s*(.+),$)tp");

    std::vector<std::string> stack;

    for (const auto& rawLine :
         lines) {
        const std::string line =
            Trim(rawLine);

        if (line.empty()) {
            continue;
        }

        std::smatch match;

        if (std::regex_match(
                line,
                match,
                rootPattern)) {
            stack.push_back(
                "root:" +
                match[1].str());
            continue;
        }

        if (std::regex_match(
                line,
                match,
                tablePattern)) {
            const std::string key =
                match[1].matched
                    ? "string:" +
                        match[1].str()
                    : "number:" +
                        match[2].str();

            stack.push_back(key);

            const PfUiDataClass dataClass =
                GetPfUiDataClass(stack);

            if (dataClass !=
                PfUiDataClass::Junk) {
                const std::string token =
                    "table:" +
                    JoinStack(stack);

                if (dataClass ==
                    PfUiDataClass::Cache) {
                    data.cache.push_back(
                        token);
                } else {
                    data.settings.push_back(
                        token);
                }
            }

            continue;
        }

        if (std::regex_match(
                line,
                match,
                valuePattern)) {
            const std::string key =
                match[1].matched
                    ? "string:" +
                        match[1].str()
                    : "number:" +
                        match[2].str();

            const PfUiDataClass dataClass =
                GetPfUiDataClass(stack);

            if (dataClass !=
                PfUiDataClass::Junk) {
                auto pathStack =
                    stack;
                pathStack.push_back(key);

                const std::string token =
                    "value:" +
                    JoinStack(pathStack) +
                    "=" +
                    Trim(match[3].str());

                if (dataClass ==
                    PfUiDataClass::Cache) {
                    data.cache.push_back(
                        token);
                } else {
                    data.settings.push_back(
                        token);
                }
            }

            continue;
        }

        if (line == "}," ||
            line == "}") {
            if (!stack.empty()) {
                stack.pop_back();
            }
            continue;
        }

        error =
            L"pfUI comparison encountered an unexpected line format.";
        return false;
    }

    std::sort(
        data.settings.begin(),
        data.settings.end());
    std::sort(
        data.cache.begin(),
        data.cache.end());

    return true;
}

bool InspectCopy(
    const std::filesystem::path& path,
    bool& exists,
    std::filesystem::file_time_type& modified,
    std::wstring& error) {
    std::error_code ec;

    exists =
        std::filesystem::is_regular_file(
            path,
            ec);

    if (ec ==
        std::errc::no_such_file_or_directory) {
        ec.clear();
        exists = false;
        return true;
    }

    if (ec) {
        error =
            L"Could not inspect Account Sync file '" +
            path.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    if (!exists) {
        return true;
    }

    modified =
        std::filesystem::last_write_time(
            path,
            ec);

    if (ec) {
        error =
            L"Could not read Account Sync modified time for '" +
            path.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    return true;
}

void SetDisabledResult(
    AccountSyncItem item,
    AccountSyncItemResult& result) {
    result = {};
    result.item = item;
    result.status = L"Disabled";
    result.action = L"—";
}

void SetNotConfiguredResult(
    AccountSyncItem item,
    AccountSyncItemResult& result) {
    result = {};
    result.item = item;
    result.status = L"Not configured";
    result.action =
        L"Select 2+ accounts";
}

bool AnalyzeItem(
    const std::filesystem::path& wowRoot,
    const std::vector<std::wstring>& accounts,
    AccountSyncItem item,
    ItemAnalysis& analysis,
    std::wstring& error) {
    analysis = {};
    analysis.item = item;

    std::vector<AccountCopy> copies;
    copies.reserve(accounts.size());

    for (std::size_t i = 0;
         i < accounts.size();
         ++i) {
        AccountCopy copy;
        copy.account = accounts[i];
        copy.index = i;
        copy.path =
            AccountItemPath(
                wowRoot,
                copy.account,
                item);

        if (!InspectCopy(
                copy.path,
                copy.exists,
                copy.modified,
                error)) {
            return false;
        }

        copies.push_back(
            std::move(copy));
    }

    std::vector<AccountCopy> existing;

    for (const auto& copy :
         copies) {
        if (copy.exists) {
            existing.push_back(copy);
        }
    }

    if (existing.empty()) {
        analysis.noSource = true;
        return true;
    }

    std::sort(
        existing.begin(),
        existing.end(),
        [](const AccountCopy& left,
           const AccountCopy& right) {
            if (left.modified !=
                right.modified) {
                return
                    left.modified >
                    right.modified;
            }

            return
                left.index <
                right.index;
        });

    analysis.source =
        existing.front();

    for (std::size_t i = 1;
         i < existing.size();
         ++i) {
        const auto& candidate =
            existing[i];

        if (!(candidate.modified <
              analysis.source.modified)) {
            continue;
        }

        if (item ==
            AccountSyncItem::PfUi) {
            PfUiComparison comparison;
            std::wstring compareError;

            if (ComparePfUiFiles(
                    analysis.source.path,
                    candidate.path,
                    comparison,
                    compareError)) {
                if (comparison.equivalent) {
                    continue;
                }

                if (comparison.cacheOnly) {
                    analysis.automaticTargets
                        .push_back(candidate);
                    continue;
                }
            } else {
                analysis.comparerFallback =
                    true;
            }
        }

        analysis.confirmationTargets
            .push_back(candidate);
    }

    for (const auto& copy :
         copies) {
        if (!copy.exists) {
            analysis.confirmationTargets
                .push_back(copy);
        }
    }

    return true;
}

void FillPreviewResult(
    const ItemAnalysis& analysis,
    AccountSyncItemResult& result) {
    result = {};
    result.item = analysis.item;

    if (analysis.noSource) {
        result.status = L"No source";
        result.action = L"Skipped";
        return;
    }

    result.sourceAccount =
        analysis.source.account;
    result.comparerFallback =
        analysis.comparerFallback;

    for (const auto& target :
         analysis.automaticTargets) {
        result.automaticTargets.push_back(
            target.account);
    }

    for (const auto& target :
         analysis.confirmationTargets) {
        result.confirmationTargets
            .push_back(
                target.account);
    }

    result.confirmationRequired =
        !analysis.confirmationTargets.empty();

    const std::size_t automatic =
        analysis.automaticTargets.size();
    const std::size_t confirmation =
        analysis.confirmationTargets.size();

    if (confirmation != 0) {
        result.status =
            L"Confirmation required";

        if (analysis.comparerFallback) {
            result.action =
                L"pfUI safe fallback; ";
        }

        result.action +=
            std::to_wstring(confirmation) +
            (confirmation == 1
                ? L" target"
                : L" targets");

        if (automatic != 0) {
            result.action +=
                L"; " +
                std::to_wstring(automatic) +
                L" cache auto";
        }

        return;
    }

    if (automatic != 0) {
        result.status = L"Cache differs";
        result.action =
            std::to_wstring(automatic) +
            (automatic == 1
                ? L" automatic target"
                : L" automatic targets");
        return;
    }

    result.status = L"Up to date";
    result.action = L"No changes";
}

bool EnsureBackupRunFolder(
    const std::filesystem::path& wowRoot,
    std::filesystem::path& runFolder,
    std::wstring& error) {
    if (!runFolder.empty()) {
        return true;
    }

    const std::filesystem::path backupRoot =
        wowRoot /
        L"WTF" /
        L"tocpilot";

    std::error_code ec;

    std::filesystem::create_directories(
        backupRoot,
        ec);

    if (ec) {
        error =
            L"Could not create Account Sync backup root '" +
            backupRoot.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);

    wchar_t timestamp[32]{};

    swprintf_s(
        timestamp,
        L"%02u-%02u-%04u-%02u%02u",
        static_cast<unsigned>(now.wDay),
        static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wYear),
        static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute));

    std::filesystem::path candidate =
        backupRoot /
        timestamp;

    int suffix = 2;

    for (;;) {
        ec.clear();

        const bool exists =
            std::filesystem::exists(
                candidate,
                ec);

        if (ec) {
            error =
                L"Could not inspect Account Sync backup path '" +
                candidate.wstring() +
                L"': " +
                FilesystemError(ec);
            return false;
        }

        if (!exists) {
            break;
        }

        wchar_t suffixed[40]{};

        swprintf_s(
            suffixed,
            L"%s-%02d",
            timestamp,
            suffix++);

        candidate =
            backupRoot /
            suffixed;
    }

    std::filesystem::create_directory(
        candidate,
        ec);

    if (ec) {
        error =
            L"Could not create Account Sync backup run folder '" +
            candidate.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    runFolder =
        std::move(candidate);
    return true;
}

bool CopyTarget(
    const std::filesystem::path& wowRoot,
    AccountSyncItem item,
    const AccountCopy& source,
    const AccountCopy& target,
    std::filesystem::path& backupRunFolder,
    std::wstring& error) {
    std::error_code ec;

    std::filesystem::create_directories(
        target.path.parent_path(),
        ec);

    if (ec) {
        error =
            L"Could not create destination folder for account '" +
            target.account +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    if (target.exists) {
        if (!EnsureBackupRunFolder(
                wowRoot,
                backupRunFolder,
                error)) {
            return false;
        }

        const auto backupPath =
            backupRunFolder /
            target.account /
            ItemRelativePath(item);

        std::filesystem::create_directories(
            backupPath.parent_path(),
            ec);

        if (ec) {
            error =
                L"Could not create backup folder for account '" +
                target.account +
                L"': " +
                FilesystemError(ec);
            return false;
        }

        ec.clear();

        const bool backupCopied =
            std::filesystem::copy_file(
                target.path,
                backupPath,
                std::filesystem::copy_options::
                    overwrite_existing,
                ec);

        if (ec ||
            !backupCopied) {
            error =
                L"Backup failed for account '" +
                target.account +
                L"'. The destination was not overwritten.";

            if (ec) {
                error +=
                    L" " +
                    FilesystemError(ec);
            }

            return false;
        }
    }

    ec.clear();

    const bool copied =
        std::filesystem::copy_file(
            source.path,
            target.path,
            std::filesystem::copy_options::
                overwrite_existing,
            ec);

    if (ec ||
        !copied) {
        error =
            L"Copy failed for account '" +
            target.account +
            L"'.";

        if (target.exists) {
            error +=
                L" The backup remains intact.";
        }

        if (ec) {
            error +=
                L" " +
                FilesystemError(ec);
        }

        return false;
    }

    return true;
}

void FinalizeRunItem(
    const ItemAnalysis& analysis,
    std::size_t automaticCopied,
    std::size_t confirmedCopied,
    bool confirmationDeclined,
    AccountSyncItemResult& result) {
    if (result.fatal) {
        result.status = L"Error";
        result.action =
            L"Sync failed";
        return;
    }

    const std::size_t totalCopied =
        automaticCopied +
        confirmedCopied;

    result.copiedTargets =
        totalCopied;
    result.confirmationDeclined =
        confirmationDeclined;

    if (confirmationDeclined) {
        if (automaticCopied != 0) {
            result.status =
                L"Partially synced";
            result.action =
                L"Cache auto; confirmation declined";
        } else {
            result.status = L"Skipped";
            result.action =
                L"Confirmation declined";
        }
        return;
    }

    if (totalCopied != 0) {
        result.status = L"Synced";

        if (automaticCopied != 0 &&
            confirmedCopied != 0) {
            result.action =
                L"Confirmed + automatic cache";
        } else if (confirmedCopied != 0) {
            result.action = L"Confirmed";
        } else {
            result.action =
                L"Automatic cache";
        }
        return;
    }

    FillPreviewResult(
        analysis,
        result);
}

} // namespace

const wchar_t* AccountSyncItemLabel(
    AccountSyncItem item) {
    switch (item) {
    case AccountSyncItem::Macros:
        return L"Macros";
    case AccountSyncItem::Keybindings:
        return L"Keybindings";
    case AccountSyncItem::PfUi:
        return L"pfUI";
    }

    return L"";
}

bool AccountSyncItemEnabled(
    const AccountSyncConfig& config,
    AccountSyncItem item) {
    switch (item) {
    case AccountSyncItem::Macros:
        return config.macros;
    case AccountSyncItem::Keybindings:
        return config.keybindings;
    case AccountSyncItem::PfUi:
        return config.pfUi;
    }

    return false;
}

bool AccountSyncConfigured(
    const AccountSyncConfig& config) {
    return
        config.accounts.size() >= 2 &&
        (config.macros ||
         config.keybindings ||
         config.pfUi);
}

bool RunAccountSyncLaunchFlow(
    bool syncBeforeLaunch,
    bool console,
    const AccountSyncPreLaunchFn& sync,
    const AccountSyncLaunchFn& launch) {
    if (syncBeforeLaunch &&
        (!sync ||
         !sync())) {
        return false;
    }

    if (!launch) {
        return false;
    }

    launch(console);
    return true;
}

bool DiscoverAccountNames(
    const std::filesystem::path& wowRoot,
    std::vector<std::wstring>& accounts,
    std::wstring& error) {
    accounts.clear();
    error.clear();

    const auto accountRoot =
        wowRoot /
        L"WTF" /
        L"Account";

    std::error_code ec;

    if (!std::filesystem::exists(
            accountRoot,
            ec)) {
        if (ec) {
            error =
                L"Could not inspect WTF\\Account: " +
                FilesystemError(ec);
            return false;
        }

        return true;
    }

    std::filesystem::directory_iterator it(
        accountRoot,
        ec);
    const std::filesystem::directory_iterator end;

    if (ec) {
        error =
            L"Could not enumerate WTF\\Account: " +
            FilesystemError(ec);
        return false;
    }

    for (;
         it != end;
         it.increment(ec)) {
        if (ec) {
            error =
                L"Could not enumerate WTF\\Account: " +
                FilesystemError(ec);
            return false;
        }

        const auto status =
            it->symlink_status(ec);

        if (ec) {
            error =
                L"Could not inspect an account directory: " +
                FilesystemError(ec);
            return false;
        }

        if (!std::filesystem::is_directory(
                status) ||
            std::filesystem::is_symlink(
                status)) {
            continue;
        }

        const std::wstring name =
            it->path()
                .filename()
                .wstring();

        if (SafeAccountName(name)) {
            accounts.push_back(name);
        }
    }

    std::sort(
        accounts.begin(),
        accounts.end(),
        [](const std::wstring& left,
           const std::wstring& right) {
            return
                LessInsensitive(
                    left,
                    right);
        });

    return true;
}

bool ComparePfUiFiles(
    const std::filesystem::path& pathA,
    const std::filesystem::path& pathB,
    PfUiComparison& comparison,
    std::wstring& error) {
    comparison = {};
    error.clear();

    PfUiNormalizedData dataA;
    PfUiNormalizedData dataB;

    if (!NormalizePfUi(
            pathA,
            dataA,
            error) ||
        !NormalizePfUi(
            pathB,
            dataB,
            error)) {
        return false;
    }

    const bool settingsEqual =
        dataA.settings ==
        dataB.settings;
    const bool cacheEqual =
        dataA.cache ==
        dataB.cache;

    comparison.equivalent =
        settingsEqual &&
        cacheEqual;
    comparison.cacheOnly =
        settingsEqual &&
        !cacheEqual;
    comparison.settingsChanged =
        !settingsEqual;
    comparison.cacheChanged =
        !cacheEqual;

    return true;
}

bool InspectAccountSync(
    const std::filesystem::path& wowRoot,
    const AccountSyncConfig& config,
    std::array<AccountSyncItemResult, kAccountSyncItemCount>& items,
    std::wstring& error) {
    error.clear();

    const std::array<
        AccountSyncItem,
        kAccountSyncItemCount>
        displayOrder{
            AccountSyncItem::Macros,
            AccountSyncItem::Keybindings,
            AccountSyncItem::PfUi
        };

    std::vector<std::wstring> accounts;

    if (!NormalizeAccounts(
            config.accounts,
            accounts,
            error)) {
        return false;
    }

    for (const auto item :
         displayOrder) {
        auto& result =
            items[ItemIndex(item)];

        if (!AccountSyncItemEnabled(
                config,
                item)) {
            SetDisabledResult(
                item,
                result);
            continue;
        }

        if (accounts.size() < 2) {
            SetNotConfiguredResult(
                item,
                result);
            continue;
        }

        ItemAnalysis analysis;

        if (!AnalyzeItem(
                wowRoot,
                accounts,
                item,
                analysis,
                error)) {
            return false;
        }

        FillPreviewResult(
            analysis,
            result);
    }

    return true;
}

bool RunAccountSync(
    const std::filesystem::path& wowRoot,
    const AccountSyncConfig& config,
    const AccountSyncConfirmFn& confirm,
    AccountSyncRunResult& result,
    std::wstring& error) {
    result = {};
    error.clear();

    std::vector<std::wstring> accounts;

    if (!NormalizeAccounts(
            config.accounts,
            accounts,
            error)) {
        return false;
    }

    const std::array<
        AccountSyncItem,
        kAccountSyncItemCount>
        runOrder{
            AccountSyncItem::PfUi,
            AccountSyncItem::Keybindings,
            AccountSyncItem::Macros
        };

    std::filesystem::path backupRunFolder;

    for (const auto item :
         runOrder) {
        auto& itemResult =
            result.items[
                ItemIndex(item)];

        if (!AccountSyncItemEnabled(
                config,
                item)) {
            SetDisabledResult(
                item,
                itemResult);
            continue;
        }

        if (accounts.size() < 2) {
            SetNotConfiguredResult(
                item,
                itemResult);
            continue;
        }

        ItemAnalysis analysis;
        std::wstring analysisError;

        if (!AnalyzeItem(
                wowRoot,
                accounts,
                item,
                analysis,
                analysisError)) {
            itemResult = {};
            itemResult.item = item;
            itemResult.fatal = true;
            itemResult.status = L"Error";
            itemResult.action =
                L"Sync failed";
            itemResult.error =
                analysisError;
            result.fatal = true;

            if (error.empty()) {
                error = analysisError;
            }

            continue;
        }

        FillPreviewResult(
            analysis,
            itemResult);

        std::size_t automaticCopied = 0;
        std::size_t confirmedCopied = 0;

        for (const auto& target :
             analysis.automaticTargets) {
            std::wstring copyError;

            if (CopyTarget(
                    wowRoot,
                    item,
                    analysis.source,
                    target,
                    backupRunFolder,
                    copyError)) {
                ++automaticCopied;
            } else {
                itemResult.fatal = true;
                result.fatal = true;

                if (itemResult.error.empty()) {
                    itemResult.error =
                        copyError;
                }

                if (error.empty()) {
                    error = copyError;
                }
            }
        }

        bool confirmationDeclined = false;

        if (!analysis.confirmationTargets
                 .empty()) {
            std::vector<std::wstring>
                targetAccounts;

            targetAccounts.reserve(
                analysis.confirmationTargets
                    .size());

            for (const auto& target :
                 analysis.confirmationTargets) {
                targetAccounts.push_back(
                    target.account);
            }

            const bool accepted =
                confirm &&
                confirm(
                    item,
                    analysis.source.account,
                    targetAccounts,
                    analysis.comparerFallback);

            if (accepted) {
                for (const auto& target :
                     analysis.confirmationTargets) {
                    std::wstring copyError;

                    if (CopyTarget(
                            wowRoot,
                            item,
                            analysis.source,
                            target,
                            backupRunFolder,
                            copyError)) {
                        ++confirmedCopied;
                    } else {
                        itemResult.fatal = true;
                        result.fatal = true;

                        if (itemResult.error.empty()) {
                            itemResult.error =
                                copyError;
                        }

                        if (error.empty()) {
                            error = copyError;
                        }
                    }
                }
            } else {
                confirmationDeclined = true;
            }
        }

        FinalizeRunItem(
            analysis,
            automaticCopied,
            confirmedCopied,
            confirmationDeclined,
            itemResult);
    }

    return true;
}

} // namespace tp
