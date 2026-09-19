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
            if (!tp::BeginAddonInstallTransaction(
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
            if (!tp::BeginAddonInstallTransaction(
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
            if (tp::BeginAddonInstallTransaction(
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
            if (!tp::BeginAddonInstallTransaction(
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
                        if (!tp::BeginAddonInstallTransaction(
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
                        if (!tp::BeginAddonInstallTransaction(
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
            if (tp::BeginAddonInstallTransaction(
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
