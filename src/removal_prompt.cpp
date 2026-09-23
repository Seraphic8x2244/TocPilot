#include "removal_prompt.h"

namespace tp {
namespace {

std::wstring CountText(
    std::size_t count,
    std::wstring_view singular,
    std::wstring_view plural) {
    return
        std::to_wstring(count) +
        L" " +
        std::wstring(
            count == 1
                ? singular
                : plural);
}

std::wstring BuildDetails(
    const AddonInstallPlan& plan,
    const std::vector<std::wstring>& installedFiles) {
    std::wstring details =
        L"Owned addon roots:";

    for (const auto& folder :
         plan.obsoleteInstallFolders) {
        details +=
            L"\r\n  Interface\\AddOns\\" +
            folder;
    }

    details +=
        L"\r\n\r\nRecorded files:";

    for (const auto& file :
         installedFiles) {
        details +=
            L"\r\n  " +
            file;
    }

    return details;
}

} // namespace

RemovalPrompt BuildRemovalPrompt(
    std::wstring_view packageName,
    RemovalAction action,
    const AddonInstallPlan& plan,
    const std::vector<std::wstring>& installedFiles) {
    RemovalPrompt prompt;

    const std::wstring name(
        packageName);

    const std::wstring rootCount =
        CountText(
            plan.obsoleteInstallFolders.size(),
            L"owned addon root",
            L"owned addon roots");

    const std::wstring fileCount =
        CountText(
            installedFiles.size(),
            L"recorded file",
            L"recorded files");

    if (action ==
        RemovalAction::Uninstall) {
        prompt.title =
            L"TocPilot - Uninstall Package";
        prompt.instruction =
            L"Uninstall " +
            name +
            L"?";
        prompt.content =
            L"TocPilot will remove " +
            rootCount +
            L" containing " +
            fileCount +
            L".\r\n\r\n"
            L"The package record and tracking will remain in TocPilot.";
    } else {
        prompt.title =
            L"TocPilot - Remove Addon";
        prompt.instruction =
            L"Remove " +
            name +
            L"?";
        prompt.content =
            L"TocPilot will remove " +
            rootCount +
            L" containing " +
            fileCount +
            L", then delete this addon from TocPilot.";
    }

    prompt.details =
        BuildDetails(
            plan,
            installedFiles);

    return prompt;
}

} // namespace tp
