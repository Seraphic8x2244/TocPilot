#include "account_sync.h"
#include "state.h"

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(
    bool condition,
    const char* message) {
    if (condition) {
        return;
    }

    std::cerr <<
        "FAIL: " <<
        message <<
        "\n";
    ++g_failures;
}

std::filesystem::path TestRoot(
    const wchar_t* name) {
    return
        std::filesystem::temp_directory_path() /
        (std::wstring(
             L"TocPilotAccountSyncTests-") +
         std::to_wstring(
             GetCurrentProcessId()) +
         L"-" +
         name);
}

void ResetRoot(
    const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    ec.clear();
    std::filesystem::create_directories(
        root,
        ec);
    Check(
        !ec,
        "test root should be creatable");
}

void WriteText(
    const std::filesystem::path& path,
    const std::string& content) {
    std::error_code ec;
    std::filesystem::create_directories(
        path.parent_path(),
        ec);
    Check(
        !ec,
        "parent folder should be creatable");

    std::ofstream stream(
        path,
        std::ios::binary |
            std::ios::trunc);
    stream.write(
        content.data(),
        static_cast<std::streamsize>(
            content.size()));
    Check(
        static_cast<bool>(stream),
        "test file should be writable");
}

std::string ReadText(
    const std::filesystem::path& path) {
    std::ifstream stream(
        path,
        std::ios::binary);

    return std::string(
        std::istreambuf_iterator<char>(
            stream),
        std::istreambuf_iterator<char>());
}

std::vector<std::filesystem::path>
BackupRunFolders(
    const std::filesystem::path& root) {
    std::vector<std::filesystem::path>
        folders;
    const auto backupRoot =
        root /
        L"WTF" /
        L"tocpilot";

    std::error_code ec;
    std::filesystem::directory_iterator it(
        backupRoot,
        ec);
    const std::filesystem::directory_iterator end;

    if (ec) {
        return folders;
    }

    for (;
         it != end;
         it.increment(ec)) {
        if (ec) {
            break;
        }

        std::error_code entryError;
        if (it->is_directory(entryError) &&
            !entryError) {
            folders.push_back(
                it->path());
        }
    }

    return folders;
}

std::wstring BackupStampAtMinuteOffset(
    int minuteOffset) {
    SYSTEMTIME local{};
    GetLocalTime(&local);

    FILETIME fileTime{};
    if (!SystemTimeToFileTime(
            &local,
            &fileTime)) {
        return {};
    }

    ULARGE_INTEGER value{};
    value.LowPart =
        fileTime.dwLowDateTime;
    value.HighPart =
        fileTime.dwHighDateTime;

    const LONGLONG adjusted =
        static_cast<LONGLONG>(
            value.QuadPart) +
        static_cast<LONGLONG>(
            minuteOffset) *
            60LL *
            10000000LL;

    if (adjusted < 0) {
        return {};
    }

    value.QuadPart =
        static_cast<ULONGLONG>(
            adjusted);
    fileTime.dwLowDateTime =
        value.LowPart;
    fileTime.dwHighDateTime =
        value.HighPart;

    SYSTEMTIME shifted{};
    if (!FileTimeToSystemTime(
            &fileTime,
            &shifted)) {
        return {};
    }

    wchar_t timestamp[32]{};
    swprintf_s(
        timestamp,
        L"%02u-%02u-%04u-%02u%02u",
        static_cast<unsigned>(
            shifted.wDay),
        static_cast<unsigned>(
            shifted.wMonth),
        static_cast<unsigned>(
            shifted.wYear),
        static_cast<unsigned>(
            shifted.wHour),
        static_cast<unsigned>(
            shifted.wMinute));

    return timestamp;
}

std::filesystem::path AccountFile(
    const std::filesystem::path& root,
    const wchar_t* account,
    const wchar_t* relative) {
    return
        root /
        L"WTF" /
        L"Account" /
        account /
        relative;
}

void MakeNewer(
    const std::filesystem::path& newer,
    const std::filesystem::path& older) {
    std::error_code ec;
    const auto base =
        std::filesystem::file_time_type::
            clock::now();

    std::filesystem::last_write_time(
        older,
        base -
            std::chrono::seconds(30),
        ec);
    Check(
        !ec,
        "older timestamp should be set");

    ec.clear();

    std::filesystem::last_write_time(
        newer,
        base,
        ec);
    Check(
        !ec,
        "newer timestamp should be set");
}

std::string PfUiText(
    const char* width,
    const char* gold,
    const char* history,
    bool unknown = false,
    bool reverse = false) {
    std::string cache =
        "pfUI_cache = {\n"
        "[\"gold\"] = {\n"
        "[\"value\"] = \"" +
        std::string(gold) +
        "\",\n"
        "},\n"
        "[\"chathistory\"] = {\n"
        "[1] = \"" +
        std::string(history) +
        "\",\n"
        "},\n";

    if (unknown) {
        cache +=
            "[\"future_section\"] = {\n"
            "[\"value\"] = \"new\",\n"
            "},\n";
    }

    cache += "}\n";

    const std::string settings =
        "pfUI_profiles = {\n"
        "[\"default\"] = {\n"
        "[\"width\"] = \"" +
        std::string(width) +
        "\",\n"
        "},\n"
        "}\n";

    return
        reverse
            ? settings + cache
            : cache + settings;
}

void TestPfUiComparison() {
    const auto root =
        TestRoot(L"pfui");
    ResetRoot(root);

    const auto a =
        root / L"a.lua";
    const auto b =
        root / L"b.lua";

    WriteText(
        a,
        PfUiText(
            "160",
            "100",
            "alpha",
            false,
            false));
    WriteText(
        b,
        PfUiText(
            "160",
            "100",
            "different history",
            false,
            true));

    tp::PfUiComparison comparison;
    std::wstring error;

    Check(
        tp::ComparePfUiFiles(
            a,
            b,
            comparison,
            error),
        "pfUI equivalent comparison should parse");
    Check(
        comparison.equivalent,
        "table order and chathistory should be ignored");

    WriteText(
        b,
        PfUiText(
            "160",
            "250",
            "different history",
            false,
            true));

    Check(
        tp::ComparePfUiFiles(
            a,
            b,
            comparison,
            error),
        "pfUI cache comparison should parse");
    Check(
        comparison.cacheOnly,
        "gold-only difference should be cache-only");

    WriteText(
        b,
        PfUiText(
            "161",
            "100",
            "alpha",
            false,
            false));

    Check(
        tp::ComparePfUiFiles(
            a,
            b,
            comparison,
            error),
        "pfUI settings comparison should parse");
    Check(
        comparison.settingsChanged,
        "profile setting difference should be meaningful");

    WriteText(
        b,
        PfUiText(
            "160",
            "100",
            "alpha",
            true,
            false));

    Check(
        tp::ComparePfUiFiles(
            a,
            b,
            comparison,
            error),
        "unknown pfUI section should still parse");
    Check(
        comparison.settingsChanged,
        "unknown pfUI section should default to settings");

    WriteText(
        b,
        "pfUI_profiles = {\n"
        "this is not supported\n"
        "}\n");

    Check(
        !tp::ComparePfUiFiles(
            a,
            b,
            comparison,
            error),
        "unexpected pfUI syntax should fail safe");

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestConfirmedCopyAndBackup() {
    const auto root =
        TestRoot(L"backup");
    ResetRoot(root);

    const auto source =
        AccountFile(
            root,
            L"ALPHA",
            L"macros-cache.txt");
    const auto older =
        AccountFile(
            root,
            L"BETA",
            L"macros-cache.txt");

    WriteText(
        source,
        "new macros");
    WriteText(
        older,
        "old macros");
    MakeNewer(
        source,
        older);

    std::error_code ec;
    std::filesystem::create_directories(
        root /
            L"WTF" /
            L"Account" /
            L"GAMMA",
        ec);
    Check(
        !ec,
        "missing-target account should exist");

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA",
        L"GAMMA"
    };
    config.macros = true;

    tp::AccountSyncRunResult result;
    std::wstring error;
    int confirmations = 0;

    const bool ran =
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem item,
                std::wstring_view sourceAccount,
                const std::vector<std::wstring>& targets,
                bool fallback) {
                ++confirmations;
                Check(
                    item ==
                        tp::AccountSyncItem::Macros,
                    "macro copy should prompt as macros");
                Check(
                    sourceAccount == L"ALPHA",
                    "newest account should be source");
                Check(
                    targets.size() == 2,
                    "older and missing targets should share one prompt");
                Check(
                    !fallback,
                    "macros should not use pfUI fallback");
                return true;
            },
            result,
            error);

    Check(
        ran,
        "macro sync should run");
    Check(
        !result.fatal,
        "confirmed macro sync should not be fatal");
    Check(
        confirmations == 1,
        "macro sync should prompt once per item");
    Check(
        ReadText(older) ==
            "new macros",
        "older macro target should be replaced");

    const auto missing =
        AccountFile(
            root,
            L"GAMMA",
            L"macros-cache.txt");

    Check(
        ReadText(missing) ==
            "new macros",
        "missing macro target should be created");

    const auto backupRoot =
        root /
        L"WTF" /
        L"tocpilot";

    std::vector<std::filesystem::path>
        runFolders;

    for (const auto& entry :
         std::filesystem::directory_iterator(
             backupRoot)) {
        if (entry.is_directory()) {
            runFolders.push_back(
                entry.path());
        }
    }

    Check(
        runFolders.size() == 1,
        "one backup run folder should be created");

    if (runFolders.size() == 1) {
        const auto backup =
            runFolders.front() /
            L"BETA" /
            L"macros-cache.txt";

        Check(
            ReadText(backup) ==
                "old macros",
            "existing destination must be backed up before overwrite");

        const auto noBackup =
            runFolders.front() /
            L"GAMMA" /
            L"macros-cache.txt";

        Check(
            !std::filesystem::exists(
                noBackup),
            "missing destination must not create a fake backup");
    }

    std::filesystem::remove_all(
        root,
        ec);
}

void TestPfUiAutomaticAndFallback() {
    const auto root =
        TestRoot(L"pfui-run");
    ResetRoot(root);

    const auto source =
        AccountFile(
            root,
            L"ALPHA",
            L"SavedVariables\\pfUI.lua");
    const auto target =
        AccountFile(
            root,
            L"BETA",
            L"SavedVariables\\pfUI.lua");

    WriteText(
        source,
        PfUiText(
            "160",
            "500",
            "source",
            false,
            false));
    WriteText(
        target,
        PfUiText(
            "160",
            "100",
            "target",
            false,
            true));
    MakeNewer(
        source,
        target);

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA"
    };
    config.pfUi = true;

    tp::AccountSyncRunResult result;
    std::wstring error;
    int confirmations = 0;

    Check(
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem,
                std::wstring_view,
                const std::vector<std::wstring>&,
                bool) {
                ++confirmations;
                return false;
            },
            result,
            error),
        "pfUI cache-only sync should run");

    Check(
        confirmations == 0,
        "pfUI cache-only difference should not prompt");
    Check(
        !result.fatal,
        "pfUI cache-only auto sync should not be fatal");
    Check(
        ReadText(target) ==
            ReadText(source),
        "pfUI cache-only target should receive complete newest file");

    WriteText(
        target,
        "pfUI_profiles = {\n"
        "unsupported line\n"
        "}\n");
    MakeNewer(
        source,
        target);

    std::array<
        tp::AccountSyncItemResult,
        tp::kAccountSyncItemCount>
        preview{};

    Check(
        tp::InspectAccountSync(
            root,
            config,
            preview,
            error),
        "pfUI fallback preview should succeed");

    const auto& pfui =
        preview[
            static_cast<std::size_t>(
                tp::AccountSyncItem::PfUi)];

    Check(
        pfui.confirmationRequired,
        "pfUI parser failure should fall back to confirmation");
    Check(
        pfui.comparerFallback,
        "pfUI parser failure should be reported as safe fallback");

    confirmations = 0;
    bool fallbackPrompt = false;
    result = {};

    Check(
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem item,
                std::wstring_view sourceAccount,
                const std::vector<std::wstring>& targets,
                bool fallback) {
                ++confirmations;
                fallbackPrompt = fallback;
                Check(
                    item ==
                        tp::AccountSyncItem::PfUi,
                    "pfUI parser fallback should prompt as pfUI");
                Check(
                    sourceAccount == L"ALPHA",
                    "pfUI parser fallback should keep newest source");
                Check(
                    targets.size() == 1 &&
                        targets.front() == L"BETA",
                    "pfUI parser fallback should prompt for the older target");
                return true;
            },
            result,
            error),
        "pfUI parser fallback execution should run");
    Check(
        confirmations == 1 &&
            fallbackPrompt,
        "pfUI parser fallback execution should require one safe confirmation");
    Check(
        !result.fatal,
        "confirmed pfUI parser fallback should not be fatal");
    Check(
        ReadText(target) ==
            ReadText(source),
        "confirmed pfUI parser fallback should copy only after confirmation");

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}


void TestEqualTimestampAndDecline() {
    const auto root =
        TestRoot(L"equal-decline");
    ResetRoot(root);

    const auto source =
        AccountFile(
            root,
            L"ALPHA",
            L"bindings-cache.wtf");
    const auto target =
        AccountFile(
            root,
            L"BETA",
            L"bindings-cache.wtf");

    WriteText(source, "alpha");
    WriteText(target, "beta");

    std::error_code ec;
    const auto equalTime =
        std::filesystem::file_time_type::
            clock::now();

    std::filesystem::last_write_time(
        source,
        equalTime,
        ec);
    Check(!ec, "source equal timestamp should be set");
    ec.clear();
    std::filesystem::last_write_time(
        target,
        equalTime,
        ec);
    Check(!ec, "target equal timestamp should be set");

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA"
    };
    config.keybindings = true;

    tp::AccountSyncRunResult result;
    std::wstring error;
    int confirmations = 0;

    Check(
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem,
                std::wstring_view,
                const std::vector<std::wstring>&,
                bool) {
                ++confirmations;
                return true;
            },
            result,
            error),
        "equal timestamp sync should run");
    Check(
        confirmations == 0,
        "equal timestamps must remain untouched without confirmation");
    Check(
        ReadText(target) == "beta",
        "equal timestamp target must remain unchanged");

    MakeNewer(source, target);
    confirmations = 0;
    result = {};

    Check(
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem,
                std::wstring_view,
                const std::vector<std::wstring>&,
                bool) {
                ++confirmations;
                return false;
            },
            result,
            error),
        "declined confirmation sync should run");
    Check(
        confirmations == 1,
        "older keybindings should require one confirmation");
    Check(
        !result.fatal,
        "declined confirmation must not be fatal");
    Check(
        ReadText(target) == "beta",
        "declined target must remain unchanged");

    std::filesystem::remove_all(
        root,
        ec);
}

void TestBackupFailureBlocksOverwrite() {
    const auto root =
        TestRoot(L"backup-failure");
    ResetRoot(root);

    const auto source =
        AccountFile(
            root,
            L"ALPHA",
            L"macros-cache.txt");
    const auto target =
        AccountFile(
            root,
            L"BETA",
            L"macros-cache.txt");

    WriteText(source, "new");
    WriteText(target, "old");
    MakeNewer(source, target);

    WriteText(
        root /
            L"WTF" /
            L"tocpilot",
        "not a directory");

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA"
    };
    config.macros = true;

    tp::AccountSyncRunResult result;
    std::wstring error;

    Check(
        tp::RunAccountSync(
            root,
            config,
            [](tp::AccountSyncItem,
               std::wstring_view,
               const std::vector<std::wstring>&,
               bool) {
                return true;
            },
            result,
            error),
        "backup failure should be reported through run result");
    Check(
        result.fatal,
        "backup failure must be fatal to Launch");
    Check(
        ReadText(target) == "old",
        "backup failure must prevent destination overwrite");

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestCopyFailureKeepsBackup() {
    const auto root =
        TestRoot(L"copy-failure");
    ResetRoot(root);

    const auto source =
        AccountFile(
            root,
            L"ALPHA",
            L"macros-cache.txt");
    const auto target =
        AccountFile(
            root,
            L"BETA",
            L"macros-cache.txt");

    WriteText(source, "new");
    WriteText(target, "old");
    MakeNewer(source, target);

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA"
    };
    config.macros = true;

    tp::AccountSyncRunResult result;
    std::wstring error;
    bool sourceRemoved = false;

    Check(
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem,
                std::wstring_view,
                const std::vector<std::wstring>&,
                bool) {
                std::error_code removeError;
                sourceRemoved =
                    std::filesystem::remove(
                        source,
                        removeError);
                Check(
                    !removeError &&
                        sourceRemoved,
                    "copy-failure test should remove source after analysis");
                return true;
            },
            result,
            error),
        "copy failure should be reported through run result");
    Check(
        result.fatal,
        "copy failure after backup must be fatal");
    Check(
        ReadText(target) == "old",
        "failed source copy must leave destination content unchanged");

    bool foundBackup = false;
    for (const auto& folder :
         BackupRunFolders(root)) {
        const auto backup =
            folder /
            L"BETA" /
            L"macros-cache.txt";

        if (std::filesystem::is_regular_file(
                backup)) {
            foundBackup = true;
            Check(
                ReadText(backup) == "old",
                "copy failure must leave the successful backup intact");
        }
    }

    Check(
        foundBackup,
        "copy failure after successful backup should retain a backup file");

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestBackupRunFolderCollision() {
    const auto root =
        TestRoot(L"backup-collision");
    ResetRoot(root);

    const auto source =
        AccountFile(
            root,
            L"ALPHA",
            L"macros-cache.txt");
    const auto target =
        AccountFile(
            root,
            L"BETA",
            L"macros-cache.txt");

    WriteText(source, "new");
    WriteText(target, "old");
    MakeNewer(source, target);

    const auto backupRoot =
        root /
        L"WTF" /
        L"tocpilot";

    std::error_code ec;
    for (int offset = -1;
         offset <= 1;
         ++offset) {
        const auto stamp =
            BackupStampAtMinuteOffset(
                offset);
        Check(
            !stamp.empty(),
            "backup collision timestamp should be available");

        if (!stamp.empty()) {
            std::filesystem::create_directories(
                backupRoot /
                    stamp,
                ec);
            Check(
                !ec,
                "backup collision base folder should be creatable");
            ec.clear();
        }
    }

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA"
    };
    config.macros = true;

    tp::AccountSyncRunResult result;
    std::wstring error;

    Check(
        tp::RunAccountSync(
            root,
            config,
            [](tp::AccountSyncItem,
               std::wstring_view,
               const std::vector<std::wstring>&,
               bool) {
                return true;
            },
            result,
            error),
        "backup collision sync should run");
    Check(
        !result.fatal,
        "backup collision should resolve without a fatal error");

    bool foundSuffix = false;
    for (const auto& folder :
         BackupRunFolders(root)) {
        const std::wstring name =
            folder.filename().wstring();

        if (name.size() < 3 ||
            name.substr(
                name.size() - 3) !=
                L"-02") {
            continue;
        }

        const auto backup =
            folder /
            L"BETA" /
            L"macros-cache.txt";

        if (std::filesystem::is_regular_file(
                backup)) {
            foundSuffix = true;
            Check(
                ReadText(backup) == "old",
                "collision-suffixed backup should contain the old destination");
        }
    }

    Check(
        foundSuffix,
        "existing backup run folder should force a -02 collision suffix");

    std::filesystem::remove_all(
        root,
        ec);
}

void TestNoSourceItem() {
    const auto root =
        TestRoot(L"no-source");
    ResetRoot(root);

    std::error_code ec;
    std::filesystem::create_directories(
        root /
            L"WTF" /
            L"Account" /
            L"ALPHA",
        ec);
    Check(
        !ec,
        "first no-source account folder should be creatable");
    ec.clear();
    std::filesystem::create_directories(
        root /
            L"WTF" /
            L"Account" /
            L"BETA",
        ec);
    Check(
        !ec,
        "second no-source account folder should be creatable");

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA"
    };
    config.macros = true;

    tp::AccountSyncRunResult result;
    std::wstring error;
    int confirmations = 0;

    Check(
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem,
                std::wstring_view,
                const std::vector<std::wstring>&,
                bool) {
                ++confirmations;
                return true;
            },
            result,
            error),
        "no-source sync should run");

    const auto& macros =
        result.items[
            static_cast<std::size_t>(
                tp::AccountSyncItem::Macros)];

    Check(
        !result.fatal,
        "no-source item should not be fatal");
    Check(
        confirmations == 0,
        "no-source item should not request confirmation");
    Check(
        macros.status == L"No source" &&
            macros.action == L"Skipped",
        "no-source item should be reported as skipped");

    std::filesystem::remove_all(
        root,
        ec);
}

void TestMixedPfUiTargets() {
    const auto root =
        TestRoot(L"mixed-pfui");
    ResetRoot(root);

    const auto alpha =
        AccountFile(
            root,
            L"ALPHA",
            L"SavedVariables\\pfUI.lua");
    const auto beta =
        AccountFile(
            root,
            L"BETA",
            L"SavedVariables\\pfUI.lua");
    const auto gamma =
        AccountFile(
            root,
            L"GAMMA",
            L"SavedVariables\\pfUI.lua");
    const auto delta =
        AccountFile(
            root,
            L"DELTA",
            L"SavedVariables\\pfUI.lua");
    const auto epsilon =
        AccountFile(
            root,
            L"EPSILON",
            L"SavedVariables\\pfUI.lua");

    const std::string sourceText =
        PfUiText(
            "160",
            "500",
            "source",
            false,
            false);
    const std::string equivalentText =
        PfUiText(
            "160",
            "500",
            "ignored history",
            false,
            true);
    const std::string cacheText =
        PfUiText(
            "160",
            "100",
            "cache",
            false,
            false);
    const std::string settingsText =
        PfUiText(
            "161",
            "500",
            "settings",
            false,
            false);
    const std::string equalTimeText =
        PfUiText(
            "999",
            "0",
            "equal",
            true,
            false);

    WriteText(alpha, sourceText);
    WriteText(beta, equivalentText);
    WriteText(gamma, cacheText);
    WriteText(delta, settingsText);
    WriteText(epsilon, equalTimeText);

    const auto base =
        std::filesystem::file_time_type::
            clock::now();
    const auto older =
        base -
        std::chrono::seconds(30);

    std::error_code ec;
    std::filesystem::last_write_time(
        alpha,
        base,
        ec);
    Check(
        !ec,
        "mixed source timestamp should be set");
    ec.clear();
    std::filesystem::last_write_time(
        epsilon,
        base,
        ec);
    Check(
        !ec,
        "mixed equal timestamp should be set");

    for (const auto& path :
         {beta, gamma, delta}) {
        ec.clear();
        std::filesystem::last_write_time(
            path,
            older,
            ec);
        Check(
            !ec,
            "mixed older timestamp should be set");
    }

    tp::AccountSyncConfig config;
    config.accounts = {
        L"ALPHA",
        L"BETA",
        L"GAMMA",
        L"DELTA",
        L"EPSILON"
    };
    config.pfUi = true;

    tp::AccountSyncRunResult result;
    std::wstring error;
    int confirmations = 0;

    Check(
        tp::RunAccountSync(
            root,
            config,
            [&](tp::AccountSyncItem item,
                std::wstring_view sourceAccount,
                const std::vector<std::wstring>& targets,
                bool fallback) {
                ++confirmations;
                Check(
                    item ==
                        tp::AccountSyncItem::PfUi,
                    "mixed pfUI prompt should be for pfUI");
                Check(
                    sourceAccount == L"ALPHA",
                    "first equal-newest account should be selected as source");
                Check(
                    targets.size() == 1 &&
                        targets.front() == L"DELTA",
                    "only settings-different older target should require confirmation");
                Check(
                    !fallback,
                    "mixed parseable pfUI set should not use fallback");
                return true;
            },
            result,
            error),
        "mixed pfUI sync should run");

    const auto& pfui =
        result.items[
            static_cast<std::size_t>(
                tp::AccountSyncItem::PfUi)];

    Check(
        !result.fatal,
        "mixed pfUI sync should not be fatal");
    Check(
        confirmations == 1,
        "mixed pfUI sync should use one confirmation");
    Check(
        pfui.sourceAccount == L"ALPHA",
        "mixed pfUI result should preserve source account");
    Check(
        pfui.automaticTargets.size() == 1 &&
            pfui.automaticTargets.front() == L"GAMMA",
        "mixed pfUI result should identify the cache-only automatic target");
    Check(
        pfui.confirmationTargets.size() == 1 &&
            pfui.confirmationTargets.front() == L"DELTA",
        "mixed pfUI result should identify the settings confirmation target");
    Check(
        pfui.copiedTargets == 2,
        "mixed pfUI sync should copy cache-only and confirmed targets");
    Check(
        ReadText(beta) ==
            equivalentText,
        "equivalent older pfUI target should remain untouched");
    Check(
        ReadText(gamma) ==
            sourceText,
        "cache-only older pfUI target should auto-sync");
    Check(
        ReadText(delta) ==
            sourceText,
        "confirmed settings-different pfUI target should sync");
    Check(
        ReadText(epsilon) ==
            equalTimeText,
        "equal-timestamp pfUI target should remain untouched");

    std::filesystem::remove_all(
        root,
        ec);
}

void TestPreAccountSyncStateLoad() {
    const auto root =
        TestRoot(L"pre-account-sync-state");
    ResetRoot(root);

    WriteText(
        root /
            L"TocPilot.json",
        "{\n"
        "  \"schema\": 1,\n"
        "  \"settings\": {\n"
        "    \"text_scale\": 1.0,\n"
        "    \"check_app_updates\": true,\n"
        "    \"toolbar_icons\": true,\n"
        "    \"package_sort_column\": -1,\n"
        "    \"package_sort_ascending\": true,\n"
        "    \"package_columns_locked\": false\n"
        "  },\n"
        "  \"packages\": []\n"
        "}\n");

    tp::AppState state;
    bool created = false;
    std::wstring error;

    Check(
        tp::LoadOrCreateState(
            root,
            state,
            created,
            error),
        "pre-Account-Sync state should load");
    Check(
        !created,
        "pre-Account-Sync state should not be replaced");
    Check(
        state.settings
            .accountSyncAccounts.empty(),
        "pre-Account-Sync state should default selected accounts to empty");
    Check(
        !state.settings.accountSyncMacros &&
            !state.settings.accountSyncKeybindings &&
            !state.settings.accountSyncPfUi &&
            !state.settings.accountSyncBeforeLaunch,
        "pre-Account-Sync state should default sync booleans off");

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestLaunchFlowPreservesConsoleIntent() {
    bool ctrlStillDown = true;
    bool launched = false;
    bool consoleAtLaunch = false;
    int syncCalls = 0;

    const bool launchedAfterSync =
        tp::RunAccountSyncLaunchFlow(
            true,
            ctrlStillDown,
            [&]() {
                ++syncCalls;
                ctrlStillDown = false;
                return true;
            },
            [&](bool console) {
                launched = true;
                consoleAtLaunch = console;
            });

    Check(
        launchedAfterSync &&
            launched,
        "successful Sync-before-Launch should continue to launch");
    Check(
        syncCalls == 1,
        "Sync-before-Launch should run sync exactly once");
    Check(
        consoleAtLaunch,
        "Ctrl-click console intent should survive sync confirmation flow");

    launched = false;

    const bool blocked =
        tp::RunAccountSyncLaunchFlow(
            true,
            true,
            []() {
                return false;
            },
            [&](bool) {
                launched = true;
            });

    Check(
        !blocked &&
            !launched,
        "fatal Sync-before-Launch result should block launch");
}

void TestStatePersistence() {
    const auto root =
        TestRoot(L"state");
    ResetRoot(root);

    tp::AppState state;
    state.settings.accountSyncAccounts = {
        L"ALPHA",
        L"BETA"
    };
    state.settings.accountSyncMacros = true;
    state.settings.accountSyncKeybindings = true;
    state.settings.accountSyncPfUi = true;
    state.settings.accountSyncBeforeLaunch = true;

    std::wstring error;

    Check(
        tp::SaveState(
            root,
            state,
            error),
        "Account Sync settings should save");

    tp::AppState loaded;
    bool created = false;

    Check(
        tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error),
        "Account Sync settings should load");

    Check(
        loaded.settings.accountSyncAccounts ==
            state.settings.accountSyncAccounts,
        "selected Account Sync accounts should round-trip");
    Check(
        loaded.settings.accountSyncMacros &&
        loaded.settings.accountSyncKeybindings &&
        loaded.settings.accountSyncPfUi &&
        loaded.settings.accountSyncBeforeLaunch,
        "Account Sync booleans should round-trip");

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

} // namespace

int main() {
    TestPfUiComparison();
    TestConfirmedCopyAndBackup();
    TestPfUiAutomaticAndFallback();
    TestEqualTimestampAndDecline();
    TestBackupFailureBlocksOverwrite();
    TestCopyFailureKeepsBackup();
    TestBackupRunFolderCollision();
    TestNoSourceItem();
    TestMixedPfUiTargets();
    TestPreAccountSyncStateLoad();
    TestLaunchFlowPreservesConsoleIntent();
    TestStatePersistence();

    if (g_failures != 0) {
        std::cerr <<
            g_failures <<
            " failure(s)\n";
        return 1;
    }

    std::cout <<
        "Account Sync tests passed\n";
    return 0;
}
