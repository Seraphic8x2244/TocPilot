#include "install.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

bool WriteText(
    const std::filesystem::path& path,
    const std::string& text) {
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
    stream << text;
    return static_cast<bool>(stream);
}

std::string ReadText(
    const std::filesystem::path& path) {
    std::ifstream stream(
        path,
        std::ios::binary);
    std::string text(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    return text;
}

tp::AddonCandidate Candidate(
    std::wstring relative,
    std::wstring folder) {
    tp::AddonCandidate candidate;
    candidate.sourceRelativePath =
        std::filesystem::path(relative);
    candidate.installFolder =
        std::move(folder);
    candidate.tocFiles.push_back(
        candidate.sourceRelativePath /
        (candidate.installFolder + L".toc"));
    return candidate;
}

bool Exists(
    const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) &&
        !ec;
}


tp::AddonTransactionStateMarker Marker(
    std::wstring packageId,
    std::wstring transactionId,
    bool present = true) {
    tp::AddonTransactionStateMarker marker;
    marker.packageId = std::move(packageId);
    marker.transactionId =
        std::move(transactionId);
    marker.present = present;
    return marker;
}

bool BeginForTest(
    const tp::AddonInstallPlan& plan,
    tp::AddonInstallTransaction& transaction,
    std::wstring& error,
    const tp::AddonInstallOptions& options = {}) {
    if (!tp::PrepareAddonInstallTransaction(
            plan,
            transaction,
            error)) {
        return false;
    }

    if (!tp::ArmAddonInstallTransaction(
            transaction,
            Marker(
                plan.packageId,
                L"test-pre-state"),
            Marker(
                plan.packageId,
                transaction.transactionId),
            error)) {
        std::wstring cleanupError;
        tp::RollbackAddonInstallTransaction(
            transaction,
            cleanupError);
        return false;
    }

    if (!tp::CommitAddonInstallTransaction(
            transaction,
            error,
            options)) {
        if (transaction.prepared) {
            std::wstring cleanupError;
            tp::RollbackAddonInstallTransaction(
                transaction,
                cleanupError);
        }
        return false;
    }

    return true;
}


bool BuildRecoveryUpdateFixture(
    const std::filesystem::path& wowRoot,
    tp::AddonInstallPlan& plan,
    std::wstring& error) {
    const auto addOns =
        wowRoot /
        L"Interface" /
        L"AddOns";
    const auto extracted =
        wowRoot /
        L"recovery-extracted";

    if (!WriteText(
            addOns /
                L"Recovery/Recovery.toc",
            "old toc") ||
        !WriteText(
            addOns /
                L"Recovery/old.lua",
            "old file") ||
        !WriteText(
            extracted /
                L"repo/Recovery/Recovery.toc",
            "new toc") ||
        !WriteText(
            extracted /
                L"repo/Recovery/new.lua",
            "new file")) {
        error =
            L"Could not create restart-recovery fixture.";
        return false;
    }

    return tp::BuildAddonInstallPlan(
        wowRoot,
        L"github:Owner/Recovery",
        extracted,
        {Candidate(
            L"repo/Recovery",
            L"Recovery")},
        {
            L"Interface/AddOns/Recovery/Recovery.toc",
            L"Interface/AddOns/Recovery/old.lua"
        },
        {},
        plan,
        error);
}

bool RecoveryOldLive(
    const std::filesystem::path& wowRoot) {
    const auto root =
        wowRoot /
        L"Interface" /
        L"AddOns" /
        L"Recovery";
    return
        ReadText(root / L"Recovery.toc") ==
            "old toc" &&
        Exists(root / L"old.lua") &&
        !Exists(root / L"new.lua");
}

bool RecoveryNewLive(
    const std::filesystem::path& wowRoot) {
    const auto root =
        wowRoot /
        L"Interface" /
        L"AddOns" /
        L"Recovery";
    return
        ReadText(root / L"Recovery.toc") ==
            "new toc" &&
        !Exists(root / L"old.lua") &&
        Exists(root / L"new.lua");
}

bool PrepareArmedRecoveryUpdate(
    const std::filesystem::path& wowRoot,
    tp::AddonInstallPlan& plan,
    tp::AddonInstallTransaction& transaction,
    tp::AddonTransactionStateMarker& pre,
    tp::AddonTransactionStateMarker& post,
    std::wstring& error) {
    if (!BuildRecoveryUpdateFixture(
            wowRoot,
            plan,
            error) ||
        !tp::PrepareAddonInstallTransaction(
            plan,
            transaction,
            error)) {
        return false;
    }

    pre =
        Marker(
            plan.packageId,
            L"durable-pre");
    post =
        Marker(
            plan.packageId,
            transaction.transactionId);

    if (!tp::ArmAddonInstallTransaction(
            transaction,
            pre,
            post,
            error)) {
        std::wstring cleanupError;
        tp::RollbackAddonInstallTransaction(
            transaction,
            cleanupError);
        return false;
    }

    return true;
}

void TestRestartRecovery(
    const std::filesystem::path& temp) {
    const auto recoveryRoot =
        temp /
        L"RestartRecovery";

    std::error_code ec;
    std::filesystem::remove_all(
        recoveryRoot,
        ec);

    {
        const auto pre =
            Marker(
                L"github:Owner/Recovery",
                L"durable-pre");
        const auto post =
            Marker(
                L"github:Owner/Recovery",
                L"durable-post");

        if (tp::DecideAddonTransactionRecovery(
                pre,
                post,
                {pre}) !=
                tp::AddonTransactionRecoveryDecision::Rollback ||
            tp::DecideAddonTransactionRecovery(
                pre,
                post,
                {post}) !=
                tp::AddonTransactionRecoveryDecision::Finalize ||
            tp::DecideAddonTransactionRecovery(
                pre,
                post,
                {Marker(
                    pre.packageId,
                    L"neither")}) !=
                tp::AddonTransactionRecoveryDecision::Conflict) {
            Fail("restart recovery state decision was not deterministic");
        }

        const auto replacedPre =
            Marker(
                L"github:Old/Recovery",
                L"old-marker");
        const auto replacementPost =
            Marker(
                L"github:New/Recovery",
                L"replacement-marker");

        if (tp::DecideAddonTransactionRecovery(
                replacedPre,
                replacementPost,
                {replacedPre}) !=
                tp::AddonTransactionRecoveryDecision::Rollback ||
            tp::DecideAddonTransactionRecovery(
                replacedPre,
                replacementPost,
                {replacementPost}) !=
                tp::AddonTransactionRecoveryDecision::Finalize ||
            tp::DecideAddonTransactionRecovery(
                replacedPre,
                replacementPost,
                {replacedPre, replacementPost}) !=
                tp::AddonTransactionRecoveryDecision::Conflict) {
            Fail("replacement recovery state decision was not deterministic");
        }
    }

    // Crash during prepare after the durable prepared journal exists.
    {
        const auto wowRoot =
            recoveryRoot / L"Prepared";
        tp::AddonInstallPlan plan;
        std::wstring error;
        if (!BuildRecoveryUpdateFixture(
                wowRoot,
                plan,
                error)) {
            Fail("prepared-crash fixture plan failed");
        } else {
            tp::AddonInstallTransaction transaction;
            if (!tp::PrepareAddonInstallTransaction(
                    plan,
                    transaction,
                    error)) {
                Fail("prepared-crash transaction preparation failed");
            } else if (!tp::RecoverAddonInstallTransactions(
                           wowRoot,
                           {Marker(
                               plan.packageId,
                               L"durable-pre")},
                           error) ||
                       Exists(plan.transactionRoot) ||
                       !RecoveryOldLive(wowRoot)) {
                Fail("prepared-crash restart recovery failed");
            }
        }
    }

    // Crash after a successful live->backup rename but before bookkeeping.
    {
        const auto wowRoot =
            recoveryRoot / L"BackupRename";
        tp::AddonInstallPlan plan;
        tp::AddonInstallTransaction transaction;
        tp::AddonTransactionStateMarker pre;
        tp::AddonTransactionStateMarker post;
        std::wstring error;

        if (!PrepareArmedRecoveryUpdate(
                wowRoot,
                plan,
                transaction,
                pre,
                post,
                error)) {
            Fail("backup-rename crash fixture failed");
        } else {
            tp::AddonInstallOptions options;
            options.simulateCrashAfterRootBackupRename =
                true;

            if (tp::CommitAddonInstallTransaction(
                    transaction,
                    error,
                    options) ||
                !transaction.backedUpRoots.empty() ||
                !Exists(
                    plan.transactionRoot /
                    L"backup/Recovery") ||
                Exists(
                    plan.addOnsRoot /
                    L"Recovery")) {
                Fail("backup-rename crash window was not constructed");
            } else if (!tp::RecoverAddonInstallTransactions(
                           wowRoot,
                           {pre},
                           error) ||
                       Exists(plan.transactionRoot) ||
                       !RecoveryOldLive(wowRoot)) {
                Fail("backup-rename restart rollback failed");
            }
        }
    }

    // Crash after all old roots are backed up, before any new root is live.
    {
        const auto wowRoot =
            recoveryRoot / L"AllBackups";
        tp::AddonInstallPlan plan;
        tp::AddonInstallTransaction transaction;
        tp::AddonTransactionStateMarker pre;
        tp::AddonTransactionStateMarker post;
        std::wstring error;

        if (!PrepareArmedRecoveryUpdate(
                wowRoot,
                plan,
                transaction,
                pre,
                post,
                error)) {
            Fail("all-backups crash fixture failed");
        } else {
            tp::AddonInstallOptions options;
            options.simulateCrashAfterAllRootBackups =
                true;

            if (tp::CommitAddonInstallTransaction(
                    transaction,
                    error,
                    options) ||
                transaction.backedUpRoots.size() != 1 ||
                Exists(
                    plan.addOnsRoot /
                    L"Recovery")) {
                Fail("all-backups crash window was not constructed");
            } else if (!tp::RecoverAddonInstallTransactions(
                           wowRoot,
                           {pre},
                           error) ||
                       !RecoveryOldLive(wowRoot) ||
                       Exists(plan.transactionRoot)) {
                Fail("all-backups restart rollback failed");
            }
        }
    }

    // Crash after prepared->live rename but before installedRoots bookkeeping.
    {
        const auto wowRoot =
            recoveryRoot / L"NewRename";
        tp::AddonInstallPlan plan;
        tp::AddonInstallTransaction transaction;
        tp::AddonTransactionStateMarker pre;
        tp::AddonTransactionStateMarker post;
        std::wstring error;

        if (!PrepareArmedRecoveryUpdate(
                wowRoot,
                plan,
                transaction,
                pre,
                post,
                error)) {
            Fail("new-rename crash fixture failed");
        } else {
            tp::AddonInstallOptions options;
            options.simulateCrashAfterNewRootCommitRename =
                true;

            if (tp::CommitAddonInstallTransaction(
                    transaction,
                    error,
                    options) ||
                !transaction.installedRoots.empty() ||
                !RecoveryNewLive(wowRoot) ||
                !Exists(
                    plan.transactionRoot /
                    L"backup/Recovery")) {
                Fail("new-rename crash window was not constructed");
            } else if (!tp::RecoverAddonInstallTransactions(
                           wowRoot,
                           {pre},
                           error) ||
                       !RecoveryOldLive(wowRoot) ||
                       Exists(plan.transactionRoot)) {
                Fail("new-rename restart rollback failed");
            }
        }
    }

    // Filesystem commit completed but durable state is still pre-state.
    {
        const auto wowRoot =
            recoveryRoot / L"BeforeStateSave";
        tp::AddonInstallPlan plan;
        tp::AddonInstallTransaction transaction;
        tp::AddonTransactionStateMarker pre;
        tp::AddonTransactionStateMarker post;
        std::wstring error;

        if (!PrepareArmedRecoveryUpdate(
                wowRoot,
                plan,
                transaction,
                pre,
                post,
                error) ||
            !tp::CommitAddonInstallTransaction(
                transaction,
                error)) {
            Fail("pre-state committed fixture failed");
        } else if (!RecoveryNewLive(wowRoot) ||
                   !tp::RecoverAddonInstallTransactions(
                       wowRoot,
                       {pre},
                       error) ||
                   !RecoveryOldLive(wowRoot) ||
                   Exists(plan.transactionRoot)) {
            Fail("pre-state restart rollback after filesystem commit failed");
        }
    }

    // State save committed before finalize: new live state must be preserved.
    {
        const auto wowRoot =
            recoveryRoot / L"AfterStateSave";
        tp::AddonInstallPlan plan;
        tp::AddonInstallTransaction transaction;
        tp::AddonTransactionStateMarker pre;
        tp::AddonTransactionStateMarker post;
        std::wstring error;

        if (!PrepareArmedRecoveryUpdate(
                wowRoot,
                plan,
                transaction,
                pre,
                post,
                error) ||
            !tp::CommitAddonInstallTransaction(
                transaction,
                error)) {
            Fail("post-state committed fixture failed");
        } else if (!tp::RecoverAddonInstallTransactions(
                       wowRoot,
                       {post},
                       error) ||
                   !RecoveryNewLive(wowRoot) ||
                   Exists(plan.transactionRoot)) {
            Fail("post-state restart finalize failed");
        }
    }

    // Finalize cleanup may be partially complete when the process stops.
    {
        const auto wowRoot =
            recoveryRoot / L"PartialFinalize";
        tp::AddonInstallPlan plan;
        tp::AddonInstallTransaction transaction;
        tp::AddonTransactionStateMarker pre;
        tp::AddonTransactionStateMarker post;
        std::wstring error;

        if (!PrepareArmedRecoveryUpdate(
                wowRoot,
                plan,
                transaction,
                pre,
                post,
                error) ||
            !tp::CommitAddonInstallTransaction(
                transaction,
                error)) {
            Fail("partial-finalize fixture failed");
        } else {
            std::filesystem::remove_all(
                plan.transactionRoot /
                    L"backup",
                ec);

            if (!tp::RecoverAddonInstallTransactions(
                    wowRoot,
                    {post},
                    error) ||
                !RecoveryNewLive(wowRoot) ||
                Exists(plan.transactionRoot)) {
                Fail("partial-finalize restart cleanup failed");
            }
        }
    }

    // A remove transaction uses an absent package as its durable post-state.
    {
        const auto wowRoot =
            recoveryRoot / L"RemovedPostState";
        const auto addOns =
            wowRoot /
            L"Interface" /
            L"AddOns";
        WriteText(
            addOns /
                L"Owned/Owned.toc",
            "owned");

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (!tp::BuildAddonRemovalPlan(
                wowRoot,
                L"github:Owner/Removed",
                {L"Interface/AddOns/Owned/Owned.toc"},
                {},
                plan,
                error)) {
            Fail("removed-post-state plan failed");
        } else {
            tp::AddonInstallTransaction transaction;
            if (!tp::PrepareAddonInstallTransaction(
                    plan,
                    transaction,
                    error)) {
                Fail("removed-post-state prepare failed");
            } else {
                const auto pre =
                    Marker(
                        plan.packageId,
                        L"durable-pre");
                const auto post =
                    Marker(
                        plan.packageId,
                        L"",
                        false);

                if (!tp::ArmAddonInstallTransaction(
                        transaction,
                        pre,
                        post,
                        error) ||
                    !tp::CommitAddonInstallTransaction(
                        transaction,
                        error)) {
                    Fail("removed-post-state commit failed");
                } else if (
                    Exists(addOns / L"Owned") ||
                    !tp::RecoverAddonInstallTransactions(
                        wowRoot,
                        {},
                        error) ||
                    Exists(addOns / L"Owned") ||
                    Exists(plan.transactionRoot)) {
                    Fail("removed-post-state restart finalize failed");
                }
            }
        }
    }

    // Unknown durable state must preserve evidence instead of guessing.
    {
        const auto wowRoot =
            recoveryRoot / L"Conflict";
        tp::AddonInstallPlan plan;
        tp::AddonInstallTransaction transaction;
        tp::AddonTransactionStateMarker pre;
        tp::AddonTransactionStateMarker post;
        std::wstring error;

        if (!PrepareArmedRecoveryUpdate(
                wowRoot,
                plan,
                transaction,
                pre,
                post,
                error) ||
            !tp::CommitAddonInstallTransaction(
                transaction,
                error)) {
            Fail("conflict recovery fixture failed");
        } else if (tp::RecoverAddonInstallTransactions(
                       wowRoot,
                       {Marker(
                           plan.packageId,
                           L"neither")},
                       error) ||
                   !Exists(plan.transactionRoot)) {
            Fail("conflict recovery did not preserve evidence");
        } else {
            error.clear();
            if (!tp::RecoverAddonInstallTransactions(
                    wowRoot,
                    {pre},
                    error) ||
                !RecoveryOldLive(wowRoot) ||
                Exists(plan.transactionRoot)) {
                Fail("conflict fixture cleanup rollback failed");
            }
        }
    }
}

} // namespace

int main() {
    const auto temp =
        std::filesystem::temp_directory_path() /
        L"TocPilotInstallTests";

    std::error_code ec;

    {
        const auto removalRoot =
            temp / L"RemovalWoW";
        const auto addOns =
            removalRoot /
            L"Interface" /
            L"AddOns";

        WriteText(
            addOns /
                L"Owned/Owned.toc",
            "owned toc");
        WriteText(
            addOns /
                L"Owned/main.lua",
            "owned main");
        WriteText(
            addOns /
                L"Unrelated/Unrelated.toc",
            "unrelated");

        const std::vector<std::wstring> installedFiles{
            L"Interface/AddOns/Owned/Owned.toc",
            L"Interface/AddOns/Owned/main.lua"
        };

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (!tp::BuildAddonRemovalPlan(
                removalRoot,
                L"github:Owner/Removal",
                installedFiles,
                {},
                plan,
                error)) {
            Fail("owned-root removal plan failed");
        } else if (
            plan.obsoleteInstallFolders.size() != 1 ||
            plan.obsoleteInstallFolders[0] !=
                L"Owned") {
            Fail("owned-root removal plan selected wrong roots");
        } else {
            tp::AddonInstallTransaction transaction;
            if (!BeginForTest(
                    plan,
                    transaction,
                    error)) {
                Fail("owned-root removal transaction failed");
            } else {
                if (Exists(
                        addOns /
                        L"Owned") ||
                    !Exists(
                        addOns /
                        L"Unrelated/Unrelated.toc")) {
                    Fail("owned-root removal touched the wrong live addon state");
                }

                // Simulate a state-save failure after the filesystem commit.
                if (!tp::RollbackAddonInstallTransaction(
                        transaction,
                        error) ||
                    !Exists(
                        addOns /
                        L"Owned/Owned.toc") ||
                    !Exists(
                        addOns /
                        L"Owned/main.lua")) {
                    Fail("state-save rollback did not restore removed addon roots");
                }
            }

            tp::AddonInstallTransaction committedRemoval;
            if (!BeginForTest(
                    plan,
                    committedRemoval,
                    error) ||
                !tp::FinalizeAddonInstallTransaction(
                    committedRemoval,
                    error)) {
                Fail("committed owned-root removal failed");
            } else if (
                Exists(
                    addOns /
                    L"Owned") ||
                !Exists(
                    addOns /
                    L"Unrelated/Unrelated.toc")) {
                Fail("committed removal did not preserve unrelated addons");
            }
        }
    }

    {
        const auto sharedRoot =
            temp / L"SharedRemovalWoW";
        const auto addOns =
            sharedRoot /
            L"Interface" /
            L"AddOns";

        WriteText(
            addOns /
                L"Shared/Shared.toc",
            "shared");

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (tp::BuildAddonRemovalPlan(
                sharedRoot,
                L"github:Owner/SharedRemoval",
                {L"Interface/AddOns/Shared/Shared.toc"},
                {L"Interface/AddOns/Shared/Other.toc"},
                plan,
                error)) {
            Fail("shared ownership removal was accepted");
        }

        if (!Exists(
                addOns /
                L"Shared/Shared.toc")) {
            Fail("shared ownership refusal changed live files");
        }
    }

    {
        const auto rollbackRemovalRoot =
            temp / L"RemovalRollbackWoW";
        const auto addOns =
            rollbackRemovalRoot /
            L"Interface" /
            L"AddOns";

        WriteText(
            addOns /
                L"A/A.toc",
            "A");
        WriteText(
            addOns /
                L"B/B.toc",
            "B");

        const std::vector<std::wstring> installedFiles{
            L"Interface/AddOns/A/A.toc",
            L"Interface/AddOns/B/B.toc"
        };

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (!tp::BuildAddonRemovalPlan(
                rollbackRemovalRoot,
                L"github:Owner/RemovalRollback",
                installedFiles,
                {},
                plan,
                error)) {
            Fail("removal rollback fixture plan failed");
        } else {
            tp::AddonInstallOptions options;
            options.failAfterRootBackups = 1;

            tp::AddonInstallTransaction transaction;
            if (BeginForTest(
                    plan,
                    transaction,
                    error,
                    options)) {
                Fail("injected removal failure did not fail");
            }

            if (!Exists(
                    addOns /
                    L"A/A.toc") ||
                !Exists(
                    addOns /
                    L"B/B.toc") ||
                Exists(plan.transactionRoot)) {
                Fail("injected removal failure did not restore live roots");
            }
        }
    }

    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp, ec);
    if (ec) {
        Fail("could not create install test root");
        return 1;
    }

    const auto wowRoot =
        temp / L"WoW";
    const auto extracted1 =
        temp / L"extracted1";

    if (!WriteText(
            extracted1 /
                L"repo/AddOn/AddOn.toc",
            "old toc") ||
        !WriteText(
            extracted1 /
                L"repo/AddOn/main.lua",
            "old main") ||
        !WriteText(
            extracted1 /
                L"repo/AddOn_Config/AddOn_Config.toc",
            "old config")) {
        Fail("could not create first install fixture");
    } else {
        const std::vector<tp::AddonCandidate> firstCandidates{
            Candidate(
                L"repo/AddOn",
                L"AddOn"),
            Candidate(
                L"repo/AddOn_Config",
                L"AddOn_Config")
        };

        tp::AddonInstallPlan firstPlan;
        std::wstring error;
        if (!tp::BuildAddonInstallPlan(
                wowRoot,
                L"github:Owner/Repo",
                extracted1,
                firstCandidates,
                {},
                {},
                firstPlan,
                error)) {
            Fail("first install plan failed");
        } else {
            tp::AddonInstallTransaction preparedOnly;
            if (!tp::PrepareAddonInstallTransaction(
                    firstPlan,
                    preparedOnly,
                    error)) {
                Fail("prepare-only transaction failed");
            } else {
                const auto addOns =
                    wowRoot /
                    L"Interface" /
                    L"AddOns";

                if (Exists(
                        addOns /
                        L"AddOn") ||
                    !Exists(firstPlan.transactionRoot)) {
                    Fail("prepare-only phase touched live addons or missed transaction staging");
                }

                if (!tp::RollbackAddonInstallTransaction(
                        preparedOnly,
                        error) ||
                    Exists(firstPlan.transactionRoot)) {
                    Fail("prepare-only transaction cleanup failed");
                }
            }

            tp::AddonInstallTransaction firstTransaction;
            if (!BeginForTest(
                    firstPlan,
                    firstTransaction,
                    error)) {
                Fail("first install transaction failed");
            } else {
                const auto addOns =
                    wowRoot /
                    L"Interface" /
                    L"AddOns";

                if (!Exists(
                        addOns /
                        L"AddOn/AddOn.toc") ||
                    !Exists(
                        addOns /
                        L"AddOn/main.lua") ||
                    !Exists(
                        addOns /
                        L"AddOn_Config/AddOn_Config.toc")) {
                    Fail("first install did not create expected addon roots");
                }

                if (firstPlan.desiredInstalledFiles.size() != 3) {
                    Fail("first install ownership file count was incorrect");
                }

                if (!tp::FinalizeAddonInstallTransaction(
                        firstTransaction,
                        error)) {
                    Fail("first install finalize failed");
                }

                const auto extracted2 =
                    temp / L"extracted2";
                if (!WriteText(
                        extracted2 /
                            L"repo/AddOn/AddOn.toc",
                        "new toc") ||
                    !WriteText(
                        extracted2 /
                            L"repo/AddOn/new.lua",
                        "new main")) {
                    Fail("could not create update fixture");
                } else {
                    const std::vector<tp::AddonCandidate> updateCandidates{
                        Candidate(
                            L"repo/AddOn",
                            L"AddOn")
                    };

                    tp::AddonInstallPlan updatePlan;
                    if (!tp::BuildAddonInstallPlan(
                            wowRoot,
                            L"github:Owner/Repo",
                            extracted2,
                            updateCandidates,
                            firstPlan.desiredInstalledFiles,
                            {},
                            updatePlan,
                            error)) {
                        Fail("update install plan failed");
                    } else {
                        if (updatePlan.obsoleteInstallFolders.size() != 1 ||
                            updatePlan.obsoleteInstallFolders[0] !=
                                L"AddOn_Config") {
                            Fail("obsolete addon root was not planned for removal");
                        }

                        tp::AddonInstallTransaction updateTransaction;
                        if (!BeginForTest(
                                updatePlan,
                                updateTransaction,
                                error)) {
                            Fail("update transaction failed");
                        } else {
                            if (ReadText(
                                    addOns /
                                    L"AddOn/AddOn.toc") !=
                                    "new toc" ||
                                Exists(
                                    addOns /
                                    L"AddOn/main.lua") ||
                                !Exists(
                                    addOns /
                                    L"AddOn/new.lua") ||
                                Exists(
                                    addOns /
                                    L"AddOn_Config")) {
                                Fail("update transaction live state was incorrect");
                            }

                            if (!tp::RollbackAddonInstallTransaction(
                                    updateTransaction,
                                    error)) {
                                Fail("update rollback failed");
                            } else if (
                                ReadText(
                                    addOns /
                                    L"AddOn/AddOn.toc") !=
                                    "old toc" ||
                                !Exists(
                                    addOns /
                                    L"AddOn/main.lua") ||
                                !Exists(
                                    addOns /
                                    L"AddOn_Config/AddOn_Config.toc")) {
                                Fail("rollback did not restore prior addon state");
                            }
                        }

                        tp::AddonInstallTransaction committedUpdate;
                        if (!BeginForTest(
                                updatePlan,
                                committedUpdate,
                                error) ||
                            !tp::FinalizeAddonInstallTransaction(
                                committedUpdate,
                                error)) {
                            Fail("committed update failed");
                        } else if (
                            ReadText(
                                addOns /
                                L"AddOn/AddOn.toc") !=
                                "new toc" ||
                            !Exists(
                                addOns /
                                L"AddOn/new.lua") ||
                            Exists(
                                addOns /
                                L"AddOn_Config")) {
                            Fail("committed update did not replace obsolete roots");
                        }
                    }
                }
            }
        }
    }

    {
        const auto collisionRoot =
            temp / L"CollisionWoW";
        const auto addOns =
            collisionRoot /
            L"Interface" /
            L"AddOns";
        const auto extracted =
            temp / L"collision-extracted";

        WriteText(
            addOns /
                L"Foreign/Foreign.toc",
            "existing");
        WriteText(
            extracted /
                L"repo/Foreign/Foreign.toc",
            "new");

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (tp::BuildAddonInstallPlan(
                collisionRoot,
                L"github:Owner/Foreign",
                extracted,
                {Candidate(
                    L"repo/Foreign",
                    L"Foreign")},
                {},
                {},
                plan,
                error)) {
            Fail("unowned live addon collision was accepted");
        }
    }

    {
        const auto rollbackRoot =
            temp / L"RollbackWoW";
        const auto addOns =
            rollbackRoot /
            L"Interface" /
            L"AddOns";
        const auto extracted =
            temp / L"rollback-extracted";

        WriteText(
            addOns /
                L"A/A.toc",
            "old A");
        WriteText(
            addOns /
                L"B/B.toc",
            "old B");
        WriteText(
            extracted /
                L"repo/A/A.toc",
            "new A");
        WriteText(
            extracted /
                L"repo/B/B.toc",
            "new B");

        const std::vector<std::wstring> priorFiles{
            L"Interface/AddOns/A/A.toc",
            L"Interface/AddOns/B/B.toc"
        };

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (!tp::BuildAddonInstallPlan(
                rollbackRoot,
                L"github:Owner/Rollback",
                extracted,
                {
                    Candidate(
                        L"repo/A",
                        L"A"),
                    Candidate(
                        L"repo/B",
                        L"B")
                },
                priorFiles,
                {},
                plan,
                error)) {
            Fail("rollback fixture plan failed");
        } else {
            tp::AddonInstallOptions options;
            options.failAfterNewRootCommits = 1;

            tp::AddonInstallTransaction transaction;
            if (BeginForTest(
                    plan,
                    transaction,
                    error,
                    options)) {
                Fail("injected install failure did not fail");
            }

            if (ReadText(
                    addOns /
                    L"A/A.toc") !=
                    "old A" ||
                ReadText(
                    addOns /
                    L"B/B.toc") !=
                    "old B" ||
                Exists(plan.transactionRoot)) {
                Fail("injected failure did not restore prior live state");
            }
        }
    }

    {
        const auto replacementRoot =
            temp / L"ReplacementWoW";
        const auto addOns =
            replacementRoot /
            L"Interface" /
            L"AddOns";
        const auto extracted =
            temp / L"replacement-extracted";

        WriteText(
            addOns /
                L"Shared/Shared.toc",
            "old");
        WriteText(
            addOns /
                L"Shared/old.lua",
            "old file");
        WriteText(
            extracted /
                L"repo/Shared/Shared.toc",
            "new");
        WriteText(
            extracted /
                L"repo/Shared/new.lua",
            "new file");

        const std::vector<std::wstring> priorFiles{
            L"Interface/AddOns/Shared/Shared.toc",
            L"Interface/AddOns/Shared/old.lua"
        };

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (!tp::BuildAddonInstallPlan(
                replacementRoot,
                L"github:NewOwner/Shared",
                extracted,
                {Candidate(
                    L"repo/Shared",
                    L"Shared")},
                priorFiles,
                {},
                plan,
                error)) {
            Fail("managed replacement plan was rejected");
        } else {
            tp::AddonInstallTransaction transaction;
            if (!tp::PrepareAddonInstallTransaction(
                    plan,
                    transaction,
                    error)) {
                Fail("managed replacement preparation failed");
            } else if (
                ReadText(
                    addOns /
                    L"Shared/Shared.toc") !=
                    "old" ||
                !Exists(
                    addOns /
                    L"Shared/old.lua")) {
                Fail("managed replacement preparation changed live files");
            } else if (!tp::CommitAddonInstallTransaction(
                           transaction,
                           error)) {
                Fail("managed replacement commit failed");
            } else if (
                ReadText(
                    addOns /
                    L"Shared/Shared.toc") !=
                    "new" ||
                Exists(
                    addOns /
                    L"Shared/old.lua") ||
                !Exists(
                    addOns /
                    L"Shared/new.lua")) {
                Fail("managed replacement commit did not replace the owned root");
            } else if (!tp::RollbackAddonInstallTransaction(
                           transaction,
                           error)) {
                Fail("managed replacement rollback failed");
            } else if (
                ReadText(
                    addOns /
                    L"Shared/Shared.toc") !=
                    "old" ||
                !Exists(
                    addOns /
                    L"Shared/old.lua") ||
                Exists(
                    addOns /
                    L"Shared/new.lua")) {
                Fail("managed replacement rollback did not restore the old owner files");
            }
        }
    }

    {
        const auto otherRoot =
            temp / L"OtherOwnerWoW";
        const auto extracted =
            temp / L"other-owner-extracted";
        WriteText(
            extracted /
                L"repo/Shared/Shared.toc",
            "shared");

        tp::AddonInstallPlan plan;
        std::wstring error;
        if (tp::BuildAddonInstallPlan(
                otherRoot,
                L"github:Owner/Shared",
                extracted,
                {Candidate(
                    L"repo/Shared",
                    L"Shared")},
                {},
                {L"Interface/AddOns/Shared/Shared.toc"},
                plan,
                error)) {
            Fail("other package ownership collision was accepted");
        }
    }

    TestRestartRecovery(temp);

    std::filesystem::remove_all(temp, ec);

    if (failures != 0) {
        std::cerr
            << failures
            << " install transaction test(s) failed\n";
        return 1;
    }

    std::cout
        << "Install transaction tests passed\n";
    return 0;
}
