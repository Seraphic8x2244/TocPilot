#include "removal_prompt.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(
    bool condition,
    const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool Contains(
    const std::wstring& text,
    const std::wstring& needle) {
    return
        text.find(needle) !=
        std::wstring::npos;
}

} // namespace

int main() {
    tp::AddonInstallPlan plan;
    plan.obsoleteInstallFolders = {
        L"Atlas",
        L"AtlasLoot"
    };

    const std::vector<std::wstring>
        installedFiles{
            L"Interface\\AddOns\\Atlas\\Atlas.toc",
            L"Interface\\AddOns\\Atlas\\Atlas.lua",
            L"Interface\\AddOns\\AtlasLoot\\AtlasLoot.toc"
        };

    const auto uninstall =
        tp::BuildRemovalPrompt(
            L"Atlas",
            tp::RemovalAction::Uninstall,
            plan,
            installedFiles);

    Require(
        uninstall.title ==
            L"TocPilot - Uninstall Package",
        "uninstall title mismatch");
    Require(
        uninstall.instruction ==
            L"Uninstall Atlas?",
        "uninstall instruction mismatch");
    Require(
        Contains(
            uninstall.content,
            L"2 owned addon roots"),
        "uninstall root count missing");
    Require(
        Contains(
            uninstall.content,
            L"3 recorded files"),
        "uninstall file count missing");
    Require(
        Contains(
            uninstall.content,
            L"package record and tracking will remain"),
        "uninstall retention text missing");
    Require(
        Contains(
            uninstall.details,
            L"Interface\\AddOns\\Atlas"),
        "first addon root missing from details");
    Require(
        Contains(
            uninstall.details,
            L"Interface\\AddOns\\AtlasLoot"),
        "second addon root missing from details");
    Require(
        Contains(
            uninstall.details,
            installedFiles[1]),
        "recorded file missing from details");

    const auto remove =
        tp::BuildRemovalPrompt(
            L"Atlas",
            tp::RemovalAction::Remove,
            plan,
            installedFiles);

    Require(
        remove.title ==
            L"TocPilot - Remove Addon",
        "remove title mismatch");
    Require(
        remove.instruction ==
            L"Remove Atlas?",
        "remove instruction mismatch");
    Require(
        Contains(
            remove.content,
            L"delete this addon from TocPilot"),
        "remove destructive state text missing");
    Require(
        remove.details ==
            uninstall.details,
        "remove/uninstall detail lists diverged");

    std::cout <<
        "removal prompt tests passed\n";
    return 0;
}
