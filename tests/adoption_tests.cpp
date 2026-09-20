#include "adoption.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

std::filesystem::path MakeTempRoot(
    std::wstring_view label) {
    wchar_t temp[MAX_PATH + 1]{};

    const DWORD length =
        GetTempPathW(
            MAX_PATH,
            temp);

    if (length == 0 ||
        length > MAX_PATH) {
        return {};
    }

    const auto root =
        std::filesystem::path(temp) /
        (
            L"TocPilotAdoptionTest-" +
            std::wstring(label) +
            L"-" +
            std::to_wstring(
                GetCurrentProcessId()) +
            L"-" +
            std::to_wstring(
                GetTickCount64()));

    std::error_code ec;
    std::filesystem::create_directories(
        root,
        ec);

    return ec
        ? std::filesystem::path{}
        : root;
}

bool WriteAll(
    const std::filesystem::path& path,
    std::string_view content) {
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

    stream.write(
        content.data(),
        static_cast<std::streamsize>(
            content.size()));

    return stream.good();
}

std::filesystem::path MakeSimpleClone(
    std::wstring_view label) {
    const auto root =
        MakeTempRoot(label);

    if (root.empty()) {
        return {};
    }

    const auto addon =
        root /
        L"Interface" /
        L"AddOns" /
        L"ExampleAddon";

    if (!WriteAll(
            addon /
                L"ExampleAddon.toc",
            "## Interface: 11200\n"
            "## Title: ExampleAddon\n"
            "main.lua\n") ||
        !WriteAll(
            addon /
                L"main.lua",
            "ExampleAddon = true\n") ||
        !WriteAll(
            addon /
                L".git" /
                L"HEAD",
            "ref: refs/heads/main\n") ||
        !WriteAll(
            addon /
                L".git" /
                L"refs" /
                L"heads" /
                L"main",
            "0123456789abcdef0123456789abcdef01234567\n") ||
        !WriteAll(
            addon /
                L".git" /
                L"config",
            "[core]\n"
            "    repositoryformatversion = 0\n"
            "[remote \"origin\"]\n"
            "    url = https://github.com/Owner/ExampleAddon.git\n"
            "    fetch = +refs/heads/*:refs/remotes/origin/*\n"
            "[branch \"main\"]\n"
            "    remote = origin\n"
            "    merge = refs/heads/main\n")) {
        std::error_code ec;
        std::filesystem::remove_all(
            root,
            ec);
        return {};
    }

    return root;
}

void TestValidSimpleClone() {
    const auto root =
        MakeSimpleClone(
            L"valid");

    if (root.empty()) {
        Fail("could not create valid adoption fixture");
        return;
    }

    tp::AppState state;
    tp::GitAddonAdoptionPlan plan;
    std::wstring error;

    const bool ok =
        tp::PlanGitAddonAdoption(
            root,
            root /
                L"Interface" /
                L"AddOns" /
                L"ExampleAddon",
            state,
            plan,
            error);

    if (!ok) {
        Fail("valid Git addon adoption was rejected");
    } else if (
        plan.package.id !=
            L"github:Owner/ExampleAddon" ||
        plan.package.name !=
            L"ExampleAddon" ||
        plan.package.repository !=
            L"Owner/ExampleAddon" ||
        plan.package.mode !=
            L"branch" ||
        plan.package.ref !=
            L"main" ||
        plan.package.installedRevision !=
            L"0123456789abcdef0123456789abcdef01234567" ||
        plan.package.latestRevision !=
            plan.package.installedRevision ||
        plan.package.installedFiles.size() !=
            2) {
        Fail("valid adoption plan had incorrect package state");
    } else {
        for (const auto& file :
             plan.package.installedFiles) {
            if (file.find(
                    L"/.git/") !=
                std::wstring::npos) {
                Fail("adoption ownership included Git metadata");
                break;
            }
        }
    }

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestPackedBranchRef() {
    const auto root =
        MakeSimpleClone(
            L"packed");

    if (root.empty()) {
        Fail("could not create packed-ref fixture");
        return;
    }

    const auto git =
        root /
        L"Interface" /
        L"AddOns" /
        L"ExampleAddon" /
        L".git";

    std::error_code ec;
    std::filesystem::remove(
        git /
            L"refs" /
            L"heads" /
            L"main",
        ec);

    if (!WriteAll(
            git /
                L"packed-refs",
            "# pack-refs with: peeled fully-peeled sorted\n"
            "89abcdef0123456789abcdef0123456789abcdef refs/heads/main\n")) {
        Fail("could not write packed-ref fixture");
        std::filesystem::remove_all(
            root,
            ec);
        return;
    }

    tp::AppState state;
    tp::GitAddonAdoptionPlan plan;
    std::wstring error;

    if (!tp::PlanGitAddonAdoption(
            root,
            root /
                L"Interface" /
                L"AddOns" /
                L"ExampleAddon",
            state,
            plan,
            error) ||
        plan.package.installedRevision !=
            L"89abcdef0123456789abcdef0123456789abcdef") {
        Fail("packed branch ref was not resolved");
    }

    std::filesystem::remove_all(
        root,
        ec);
}

void TestDetachedHeadRejected() {
    const auto root =
        MakeSimpleClone(
            L"detached");

    if (root.empty()) {
        Fail("could not create detached-head fixture");
        return;
    }

    const auto addon =
        root /
        L"Interface" /
        L"AddOns" /
        L"ExampleAddon";

    WriteAll(
        addon /
            L".git" /
            L"HEAD",
        "0123456789abcdef0123456789abcdef01234567\n");

    tp::AppState state;
    tp::GitAddonAdoptionPlan plan;
    std::wstring error;

    if (tp::PlanGitAddonAdoption(
            root,
            addon,
            state,
            plan,
            error) ||
        error.empty()) {
        Fail("detached Git HEAD was accepted");
    }

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestComplexLayoutRejected() {
    const auto root =
        MakeSimpleClone(
            L"complex");

    if (root.empty()) {
        Fail("could not create complex-layout fixture");
        return;
    }

    const auto addon =
        root /
        L"Interface" /
        L"AddOns" /
        L"ExampleAddon";

    std::error_code ec;
    std::filesystem::remove(
        addon /
            L"ExampleAddon.toc",
        ec);

    WriteAll(
        addon /
            L"SubAddon" /
            L"SubAddon.toc",
        "## Interface: 11200\n");

    tp::AppState state;
    tp::GitAddonAdoptionPlan plan;
    std::wstring error;

    if (tp::PlanGitAddonAdoption(
            root,
            addon,
            state,
            plan,
            error) ||
        error.find(
            L"root-level .toc") ==
            std::wstring::npos) {
        Fail("complex/unpacked Git layout was accepted");
    }

    std::filesystem::remove_all(
        root,
        ec);
}

void TestGitLabRejectedForNow() {
    const auto root =
        MakeSimpleClone(
            L"gitlab");

    if (root.empty()) {
        Fail("could not create GitLab fixture");
        return;
    }

    const auto addon =
        root /
        L"Interface" /
        L"AddOns" /
        L"ExampleAddon";

    WriteAll(
        addon /
            L".git" /
            L"config",
        "[remote \"origin\"]\n"
        "    url = https://gitlab.com/Owner/ExampleAddon.git\n"
        "[branch \"main\"]\n"
        "    remote = origin\n"
        "    merge = refs/heads/main\n");

    tp::AppState state;
    tp::GitAddonAdoptionPlan plan;
    std::wstring error;

    if (tp::PlanGitAddonAdoption(
            root,
            addon,
            state,
            plan,
            error) ||
        error.find(
            L"Only GitHub") ==
            std::wstring::npos) {
        Fail("GitLab clone was adopted before GitLab package support");
    }

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestExistingRootOwnershipRejected() {
    const auto root =
        MakeSimpleClone(
            L"owned");

    if (root.empty()) {
        Fail("could not create ownership fixture");
        return;
    }

    tp::PackageRecord existing;
    existing.id =
        L"github:Other/Repo";
    existing.installedFiles = {
        L"Interface/AddOns/ExampleAddon/old.lua"
    };

    tp::AppState state;
    state.packages.push_back(
        std::move(existing));

    tp::GitAddonAdoptionPlan plan;
    std::wstring error;

    if (tp::PlanGitAddonAdoption(
            root,
            root /
                L"Interface" /
                L"AddOns" /
                L"ExampleAddon",
            state,
            plan,
            error) ||
        error.find(
            L"already owned") ==
            std::wstring::npos) {
        Fail("existing TocPilot root ownership was ignored");
    }

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

void TestDuplicateRepositoryRejected() {
    const auto root =
        MakeSimpleClone(
            L"duplicate");

    if (root.empty()) {
        Fail("could not create duplicate fixture");
        return;
    }

    tp::PackageRecord existing;
    existing.id =
        L"github:owner/exampleaddon";

    tp::AppState state;
    state.packages.push_back(
        std::move(existing));

    tp::GitAddonAdoptionPlan plan;
    std::wstring error;

    if (tp::PlanGitAddonAdoption(
            root,
            root /
                L"Interface" /
                L"AddOns" /
                L"ExampleAddon",
            state,
            plan,
            error) ||
        error.find(
            L"already managed") ==
            std::wstring::npos) {
        Fail("duplicate repository adoption was accepted");
    }

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
}

} // namespace

int main() {
    TestValidSimpleClone();
    TestPackedBranchRef();
    TestDetachedHeadRejected();
    TestComplexLayoutRejected();
    TestGitLabRejectedForNow();
    TestExistingRootOwnershipRejected();
    TestDuplicateRepositoryRejected();

    if (failures != 0) {
        std::cerr
            << failures
            << " adoption test(s) failed\n";
        return 1;
    }

    std::cout
        << "adoption tests passed\n";
    return 0;
}
