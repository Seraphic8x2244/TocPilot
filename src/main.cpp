#include "add_package_dialog.h"
#include "archive.h"
#include "branch_dialog.h"
#include "github_api.h"
#include "install.h"
#include "state.h"
#include "update.h"
#include "version.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace {

constexpr wchar_t kWindowClass[] = L"TocPilotMainWindow";
constexpr UINT WM_TP_CHECK_COMPLETE = WM_APP + 1;
constexpr UINT WM_TP_UPDATE_COMPLETE = WM_APP + 2;
constexpr UINT WM_TP_PACKAGE_REFRESH_COMPLETE = WM_APP + 3;
constexpr UINT WM_TP_PACKAGE_INSPECT_COMPLETE = WM_APP + 4;
constexpr UINT WM_TP_PACKAGE_INSTALL_COMPLETE = WM_APP + 5;

constexpr int IDC_UPDATE = 1001;
constexpr int IDC_WOW_STATUS = 1002;
constexpr int IDC_GITHUB_STATUS = 1003;
constexpr int IDC_RELEASE_STATUS = 1004;
constexpr int IDC_STATE_STATUS = 1005;
constexpr int IDC_PACKAGE_LIST = 1006;
constexpr int IDC_TEXT_SCALE = 1007;
constexpr int IDC_UPDATE_ALL = 1008;
constexpr int IDC_REFRESH_PACKAGES = 1009;
constexpr int IDC_ADD_PACKAGE = 1010;
constexpr int IDC_SET_BRANCH = 1011;
constexpr int IDC_INSPECT_PACKAGE = 1012;
constexpr int IDC_INSTALL_PACKAGE = 1013;
constexpr int IDC_REMOVE_PACKAGE = 1014;

constexpr std::array<double, 5> kTextScales{
    0.90,
    1.00,
    1.10,
    1.25,
    1.50
};

constexpr std::array<const wchar_t*, 5> kTextScaleLabels{
    L"90%",
    L"100%",
    L"110%",
    L"125%",
    L"150%"
};

HWND g_wowStatus = nullptr;
HWND g_githubStatus = nullptr;
HWND g_releaseStatus = nullptr;
HWND g_stateStatus = nullptr;
HWND g_updateButton = nullptr;
HWND g_updateAllButton = nullptr;
HWND g_refreshPackagesButton = nullptr;
HWND g_setBranchButton = nullptr;
HWND g_inspectPackageButton = nullptr;
HWND g_installPackageButton = nullptr;
HWND g_removePackageButton = nullptr;
HWND g_addPackageButton = nullptr;
HWND g_packageList = nullptr;
HWND g_packageHint = nullptr;
HWND g_textScaleLabel = nullptr;
HWND g_textScaleCombo = nullptr;
HWND g_rootLabel = nullptr;
HWND g_versionLabel = nullptr;

HFONT g_uiFont = nullptr;

tp::ReleaseInfo g_release;
tp::AppState g_state;
std::filesystem::path g_root;
bool g_stateReady = false;
bool g_stateCreated = false;
bool g_packageRefreshInProgress = false;
bool g_packageInspectInProgress = false;
bool g_packageInstallInProgress = false;
bool g_appUpdateInProgress = false;
std::wstring g_stateError;

struct CheckResult {
    bool ok = false;
    tp::ReleaseCheckState state = tp::ReleaseCheckState::NoRelease;
    tp::ReleaseInfo release;
    std::wstring error;
};

struct UpdateResult {
    bool ok = false;
    std::wstring error;
};

struct PackageRefreshResult {
    bool ok = false;
    std::size_t index = 0;
    std::wstring packageId;
    std::wstring branch;
    std::wstring remoteSha;
    std::wstring error;
};

struct PackageInspectResult {
    bool ok = false;
    std::size_t index = 0;
    std::wstring packageId;
    std::wstring branch;
    std::wstring remoteSha;
    std::uint64_t downloadedBytes = 0;
    tp::ArchiveInspection inspection;
    std::wstring error;
};

struct PackageInstallResult {
    bool ok = false;
    std::size_t index = 0;
    std::wstring packageId;
    std::wstring packageName;
    std::wstring branch;
    std::wstring remoteSha;
    std::uint64_t downloadedBytes = 0;
    tp::ArchiveInspection inspection;
    tp::AddonInstallTransaction transaction;
    std::wstring error;

    ~PackageInstallResult() {
        if (transaction.prepared) {
            std::wstring ignored;
            tp::RollbackAddonInstallTransaction(
                transaction,
                ignored);
        }
    }
};

void SetIndicator(HWND control, const std::wstring& text) {
    if (control) {
        SetWindowTextW(control, text.c_str());
        InvalidateRect(control, nullptr, TRUE);
    }
}

std::wstring StateStatusText() {
    if (!g_stateReady) {
        return L"State: Error - " + g_stateError;
    }

    std::wstring text = L"State: TocPilot.json ready";
    if (g_stateCreated) {
        text += L" (created)";
    }
    text += L" - ";
    const std::size_t packageCount = g_state.packages.size();
    text += std::to_wstring(packageCount);
    text += packageCount == 1
        ? L" package record"
        : L" package records";
    return text;
}

std::wstring PackageHintText() {
    if (!g_stateReady) {
        return L"Package state is read-only until TocPilot.json is fixed.";
    }

    if (g_state.packages.empty()) {
        return
            L"No managed packages yet. Add Package saves a repository source; "
            L"no addon files are installed.";
    }

    return
        std::to_wstring(g_state.packages.size()) +
        L" package record(s). Set Branch changes tracking; Refresh checks "
        L"the saved branch head; Inspect previews without live changes; "
        L"Install/Update uses a staged rollback transaction.";
}

BOOL CALLBACK ApplyFontToChild(HWND child, LPARAM fontValue) {
    SendMessageW(
        child,
        WM_SETFONT,
        static_cast<WPARAM>(fontValue),
        TRUE);
    return TRUE;
}

void ApplyUiFont(HWND hwnd) {
    HDC dc = GetDC(hwnd);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) {
        ReleaseDC(hwnd, dc);
    }

    const double scale = g_stateReady
        ? g_state.settings.textScale
        : 1.0;

    const int pointTimes100 =
        static_cast<int>(std::lround(1000.0 * scale));
    const int height = -MulDiv(
        pointTimes100,
        dpi,
        72 * 100);

    HFONT font = CreateFontW(
        height,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    if (!font) {
        return;
    }

    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    EnumChildWindows(
        hwnd,
        ApplyFontToChild,
        reinterpret_cast<LPARAM>(font));

    if (g_uiFont) {
        DeleteObject(g_uiFont);
    }
    g_uiFont = font;
}

void SetTextScaleSelection() {
    if (!g_textScaleCombo) {
        return;
    }

    const double current = g_stateReady
        ? g_state.settings.textScale
        : 1.0;

    int bestIndex = 0;
    double bestDistance = std::abs(current - kTextScales[0]);

    for (int index = 1;
         index < static_cast<int>(kTextScales.size());
         ++index) {
        const double distance =
            std::abs(current - kTextScales[static_cast<std::size_t>(index)]);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    ComboBox_SetCurSel(g_textScaleCombo, bestIndex);
}

void ResizeListColumns() {
    if (!g_packageList) {
        return;
    }

    RECT rect{};
    GetClientRect(g_packageList, &rect);
    const int width = std::max(720, static_cast<int>(rect.right - rect.left - 4));

    const int nameWidth = 180;
    const int sourceWidth = 250;
    const int installedWidth = 115;
    const int latestWidth = 115;
    const int statusWidth = std::max(
        120,
        width - nameWidth - sourceWidth - installedWidth - latestWidth);

    ListView_SetColumnWidth(g_packageList, 0, nameWidth);
    ListView_SetColumnWidth(g_packageList, 1, sourceWidth);
    ListView_SetColumnWidth(g_packageList, 2, installedWidth);
    ListView_SetColumnWidth(g_packageList, 3, latestWidth);
    ListView_SetColumnWidth(g_packageList, 4, statusWidth);
}

void LayoutControls(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int contentWidth = std::max(720, width - 40);

    if (g_rootLabel) {
        MoveWindow(g_rootLabel, 20, 48, contentWidth, 22, TRUE);
    }

    if (g_wowStatus) {
        MoveWindow(g_wowStatus, 20, 80, 205, 24, TRUE);
    }
    if (g_githubStatus) {
        MoveWindow(g_githubStatus, 235, 80, 220, 24, TRUE);
    }
    if (g_releaseStatus) {
        MoveWindow(
            g_releaseStatus,
            465,
            80,
            std::max(260, width - 485),
            24,
            TRUE);
    }

    if (g_stateStatus) {
        MoveWindow(g_stateStatus, 20, 108, contentWidth, 24, TRUE);
    }

    int x = 20;
    const int buttonY = 145;

    if (g_updateAllButton) {
        MoveWindow(g_updateAllButton, x, buttonY, 80, 32, TRUE);
        x += 85;
    }
    if (g_refreshPackagesButton) {
        MoveWindow(g_refreshPackagesButton, x, buttonY, 65, 32, TRUE);
        x += 70;
    }
    if (g_setBranchButton) {
        MoveWindow(g_setBranchButton, x, buttonY, 80, 32, TRUE);
        x += 85;
    }
    if (g_inspectPackageButton) {
        MoveWindow(g_inspectPackageButton, x, buttonY, 65, 32, TRUE);
        x += 70;
    }
    if (g_installPackageButton) {
        MoveWindow(g_installPackageButton, x, buttonY, 75, 32, TRUE);
        x += 80;
    }
    if (g_addPackageButton) {
        MoveWindow(g_addPackageButton, x, buttonY, 80, 32, TRUE);
        x += 85;
    }
    if (g_removePackageButton) {
        MoveWindow(g_removePackageButton, x, buttonY, 55, 32, TRUE);
        x += 60;
    }
    if (g_updateButton) {
        MoveWindow(g_updateButton, x, buttonY, 100, 32, TRUE);
    }

    if (g_textScaleLabel) {
        MoveWindow(
            g_textScaleLabel,
            std::max(620, width - 205),
            151,
            70,
            22,
            TRUE);
    }
    if (g_textScaleCombo) {
        MoveWindow(
            g_textScaleCombo,
            std::max(690, width - 130),
            145,
            110,
            180,
            TRUE);
    }

    if (g_packageHint) {
        MoveWindow(g_packageHint, 20, 188, contentWidth, 22, TRUE);
    }

    const int listTop = 215;
    const int listBottomPadding = 24;
    const int listHeight = std::max(
        190,
        height - listTop - listBottomPadding);

    if (g_packageList) {
        MoveWindow(
            g_packageList,
            20,
            listTop,
            contentWidth,
            listHeight,
            TRUE);
        ResizeListColumns();
    }
}

void AddPackageListColumns() {
    struct ColumnSpec {
        const wchar_t* name;
        int width;
    };

    constexpr std::array<ColumnSpec, 5> columns{{
        {L"Name", 180},
        {L"Source / Track", 250},
        {L"Installed", 115},
        {L"Latest", 115},
        {L"Status", 140}
    }};

    for (int index = 0;
         index < static_cast<int>(columns.size());
         ++index) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<LPWSTR>(
            columns[static_cast<std::size_t>(index)].name);
        column.cx = columns[static_cast<std::size_t>(index)].width;
        column.iSubItem = index;
        ListView_InsertColumn(g_packageList, index, &column);
    }
}

std::wstring ProviderLabel(const std::wstring& provider) {
    if (provider == L"github") {
        return L"GitHub";
    }
    if (provider == L"gitlab") {
        return L"GitLab";
    }
    return provider;
}

void PopulatePackageList() {
    if (!g_packageList) {
        return;
    }

    ListView_DeleteAllItems(g_packageList);

    for (std::size_t i = 0; i < g_state.packages.size(); ++i) {
        const auto& package = g_state.packages[i];

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(package.name.c_str());

        const int row = ListView_InsertItem(g_packageList, &item);
        if (row < 0) {
            continue;
        }

        const bool branchMode =
            package.mode == L"branch" &&
            !package.ref.empty();

        const std::wstring source =
            ProviderLabel(package.provider) +
            (branchMode
                ? L" / " + package.ref
                : L" / source only");

        const std::wstring installed =
            package.installedRevision.empty()
                ? L"—"
                : package.installedRevision.substr(
                    0,
                    std::min<std::size_t>(
                        7,
                        package.installedRevision.size()));

        const std::wstring latest =
            package.latestRevision.empty()
                ? L"—"
                : package.latestRevision.substr(
                    0,
                    std::min<std::size_t>(
                        7,
                        package.latestRevision.size()));

        const wchar_t* status = branchMode
            ? (package.installedRevision.empty()
                ? L"Not installed"
                : (package.installedRevision ==
                       package.latestRevision
                    ? L"Current"
                    : L"Update available"))
            : L"Not configured";

        ListView_SetItemText(
            g_packageList,
            row,
            1,
            const_cast<LPWSTR>(source.c_str()));
        ListView_SetItemText(
            g_packageList,
            row,
            2,
            const_cast<LPWSTR>(installed.c_str()));
        ListView_SetItemText(
            g_packageList,
            row,
            3,
            const_cast<LPWSTR>(latest.c_str()));
        ListView_SetItemText(
            g_packageList,
            row,
            4,
            const_cast<LPWSTR>(status));
    }

    if (!g_state.packages.empty()) {
        ListView_SetItemState(
            g_packageList,
            0,
            LVIS_SELECTED | LVIS_FOCUSED,
            LVIS_SELECTED | LVIS_FOCUSED);
    }
}

int SelectedPackageRow() {
    if (!g_packageList) {
        return -1;
    }

    return ListView_GetNextItem(
        g_packageList,
        -1,
        LVNI_SELECTED);
}

void UpdatePackageButtons() {
    const int row = SelectedPackageRow();
    const bool validSelection =
        g_stateReady &&
        row >= 0 &&
        row < static_cast<int>(
            g_state.packages.size());

    bool canSetBranch = false;
    bool canRefresh = false;
    bool canInspect = false;
    bool canInstall = false;
    const tp::PackageRecord* selectedPackage = nullptr;

    if (validSelection) {
        selectedPackage =
            &g_state.packages[
                static_cast<std::size_t>(row)];

        canSetBranch =
            selectedPackage->provider == L"github";

        canRefresh =
            canSetBranch &&
            selectedPackage->mode == L"branch" &&
            !selectedPackage->ref.empty();

        canInspect = canRefresh;
        canInstall = canRefresh;
    }

    const bool packageBusy =
        g_packageRefreshInProgress ||
        g_packageInspectInProgress ||
        g_packageInstallInProgress ||
        g_appUpdateInProgress;

    if (g_refreshPackagesButton) {
        EnableWindow(
            g_refreshPackagesButton,
            canRefresh && !packageBusy
                ? TRUE
                : FALSE);
    }

    if (g_inspectPackageButton) {
        EnableWindow(
            g_inspectPackageButton,
            canInspect && !packageBusy
                ? TRUE
                : FALSE);
    }

    if (g_installPackageButton) {
        const wchar_t* label = L"Install";
        if (selectedPackage &&
            !selectedPackage->installedRevision.empty()) {
            label =
                selectedPackage->installedRevision ==
                        selectedPackage->latestRevision
                    ? L"Reinstall"
                    : L"Update";
        }

        SetWindowTextW(
            g_installPackageButton,
            label);
        EnableWindow(
            g_installPackageButton,
            canInstall && !packageBusy
                ? TRUE
                : FALSE);
    }

    if (g_setBranchButton) {
        EnableWindow(
            g_setBranchButton,
            canSetBranch && !packageBusy
                ? TRUE
                : FALSE);
    }

    if (g_removePackageButton) {
        EnableWindow(
            g_removePackageButton,
            validSelection && !packageBusy
                ? TRUE
                : FALSE);
    }

    if (g_addPackageButton) {
        EnableWindow(
            g_addPackageButton,
            g_stateReady && !packageBusy
                ? TRUE
                : FALSE);
    }
}

void SelectPackageRow(std::size_t index) {
    if (!g_packageList ||
        index >= g_state.packages.size()) {
        return;
    }

    ListView_SetItemState(
        g_packageList,
        static_cast<int>(index),
        LVIS_SELECTED | LVIS_FOCUSED,
        LVIS_SELECTED | LVIS_FOCUSED);

    ListView_EnsureVisible(
        g_packageList,
        static_cast<int>(index),
        FALSE);
}

void SetPackageRowStatus(
    std::size_t index,
    const std::wstring& text) {
    if (!g_packageList ||
        index >= g_state.packages.size()) {
        return;
    }

    ListView_SetItemText(
        g_packageList,
        static_cast<int>(index),
        4,
        const_cast<LPWSTR>(text.c_str()));
}

void RefreshPackageStateUi() {
    SetIndicator(
        g_stateStatus,
        StateStatusText());

    if (g_packageHint) {
        SetWindowTextW(
            g_packageHint,
            PackageHintText().c_str());
    }

    PopulatePackageList();
    UpdatePackageButtons();
}

void StartPackageRefresh(
    HWND hwnd,
    std::size_t index) {
    if (index >= g_state.packages.size()) {
        return;
    }

    const auto package =
        g_state.packages[index];

    if (package.provider != L"github" ||
        package.mode != L"branch" ||
        package.ref.empty()) {
        return;
    }

    g_packageRefreshInProgress = true;
    UpdatePackageButtons();
    SetPackageRowStatus(
        index,
        L"Checking...");

    if (g_packageHint) {
        const std::wstring message =
            package.name +
            L": checking " +
            package.ref +
            L" on GitHub. No addon files will be changed.";

        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    std::thread(
        [hwnd, index, package]() {
            auto result =
                std::make_unique<PackageRefreshResult>();

            result->index = index;
            result->packageId = package.id;
            result->branch = package.ref;
            result->ok =
                tp::ResolveGitHubBranchHead(
                    package.repository,
                    package.ref,
                    result->remoteSha,
                    result->error);

            if (!PostMessageW(
                    hwnd,
                    WM_TP_PACKAGE_REFRESH_COMPLETE,
                    0,
                    reinterpret_cast<LPARAM>(
                        result.get()))) {
                return;
            }

            result.release();
        })
        .detach();
}

void StartPackageInspection(
    HWND hwnd,
    std::size_t index) {
    if (index >= g_state.packages.size()) {
        return;
    }

    const auto package =
        g_state.packages[index];

    if (package.provider != L"github" ||
        package.mode != L"branch" ||
        package.ref.empty()) {
        return;
    }

    g_packageInspectInProgress = true;
    UpdatePackageButtons();
    SetPackageRowStatus(
        index,
        L"Inspecting...");

    if (g_packageHint) {
        const std::wstring message =
            package.name +
            L": resolving and staging " +
            package.ref +
            L". Interface\\AddOns will not be changed.";

        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    const auto wowRoot = g_root;

    std::thread(
        [hwnd, index, package, wowRoot]() {
            auto result =
                std::make_unique<PackageInspectResult>();

            result->index = index;
            result->packageId = package.id;
            result->branch = package.ref;

            if (!tp::ResolveGitHubBranchHead(
                    package.repository,
                    package.ref,
                    result->remoteSha,
                    result->error)) {
                result->ok = false;
            } else if (!tp::ResetGitHubPackageStaging(
                    wowRoot,
                    package.repository,
                    result->inspection.stagingDirectory,
                    result->error)) {
                result->ok = false;
            } else {
                result->inspection.archivePath =
                    result->inspection.stagingDirectory /
                    L"archive.zip";

                result->inspection.extractedRoot =
                    result->inspection.stagingDirectory /
                    L"extracted";

                if (!tp::DownloadGitHubArchive(
                        package.repository,
                        result->remoteSha,
                        result->inspection.archivePath,
                        result->downloadedBytes,
                        result->error)) {
                    result->ok = false;
                } else if (!tp::ExtractZipSecure(
                        result->inspection.archivePath,
                        result->inspection.extractedRoot,
                        result->inspection.entryCount,
                        result->inspection.totalUncompressedBytes,
                        result->error)) {
                    result->ok = false;
                } else if (!tp::DetectGitHubAddonCandidates(
                        result->inspection.extractedRoot,
                        package.repository,
                        result->inspection.candidates,
                        result->error)) {
                    result->ok = false;
                } else {
                    result->ok = true;
                }
            }

            if (!PostMessageW(
                    hwnd,
                    WM_TP_PACKAGE_INSPECT_COMPLETE,
                    0,
                    reinterpret_cast<LPARAM>(
                        result.get()))) {
                return;
            }

            result.release();
        })
        .detach();
}

void StartPackageInstall(
    HWND hwnd,
    std::size_t index) {
    if (index >= g_state.packages.size()) {
        return;
    }

    const auto package =
        g_state.packages[index];

    if (package.provider != L"github" ||
        package.mode != L"branch" ||
        package.ref.empty()) {
        return;
    }

    std::vector<std::wstring> otherInstalledFiles;
    for (std::size_t i = 0;
         i < g_state.packages.size();
         ++i) {
        if (i == index ||
            g_state.packages[i].target != L"addons") {
            continue;
        }

        const auto& files =
            g_state.packages[i].installedFiles;
        otherInstalledFiles.insert(
            otherInstalledFiles.end(),
            files.begin(),
            files.end());
    }

    g_packageInstallInProgress = true;
    UpdatePackageButtons();
    SetPackageRowStatus(
        index,
        package.installedRevision.empty()
            ? L"Preparing install..."
            : L"Preparing update...");

    if (g_packageHint) {
        const std::wstring message =
            package.name +
            L": downloading, validating, and preparing " +
            package.ref +
            L". Live addons are unchanged until the final commit.";

        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    const auto wowRoot = g_root;

    std::thread(
        [hwnd,
         index,
         package,
         otherInstalledFiles = std::move(otherInstalledFiles),
         wowRoot]() mutable {
            auto result =
                std::make_unique<PackageInstallResult>();

            result->index = index;
            result->packageId = package.id;
            result->packageName = package.name;
            result->branch = package.ref;

            if (!tp::ResolveGitHubBranchHead(
                    package.repository,
                    package.ref,
                    result->remoteSha,
                    result->error)) {
                result->ok = false;
            } else if (!tp::ResetGitHubPackageStaging(
                    wowRoot,
                    package.repository,
                    result->inspection.stagingDirectory,
                    result->error)) {
                result->ok = false;
            } else {
                result->inspection.archivePath =
                    result->inspection.stagingDirectory /
                    L"archive.zip";

                result->inspection.extractedRoot =
                    result->inspection.stagingDirectory /
                    L"extracted";

                if (!tp::DownloadGitHubArchive(
                        package.repository,
                        result->remoteSha,
                        result->inspection.archivePath,
                        result->downloadedBytes,
                        result->error)) {
                    result->ok = false;
                } else if (!tp::ExtractZipSecure(
                        result->inspection.archivePath,
                        result->inspection.extractedRoot,
                        result->inspection.entryCount,
                        result->inspection.totalUncompressedBytes,
                        result->error)) {
                    result->ok = false;
                } else if (!tp::DetectGitHubAddonCandidates(
                        result->inspection.extractedRoot,
                        package.repository,
                        result->inspection.candidates,
                        result->error)) {
                    result->ok = false;
                } else {
                    tp::AddonInstallPlan plan;
                    if (!tp::BuildAddonInstallPlan(
                            wowRoot,
                            package.id,
                            result->inspection.extractedRoot,
                            result->inspection.candidates,
                            package.installedFiles,
                            otherInstalledFiles,
                            plan,
                            result->error)) {
                        result->ok = false;
                    } else if (!tp::PrepareAddonInstallTransaction(
                            plan,
                            result->transaction,
                            result->error)) {
                        result->ok = false;
                    } else {
                        result->ok = true;
                    }
                }
            }

            if (!PostMessageW(
                    hwnd,
                    WM_TP_PACKAGE_INSTALL_COMPLETE,
                    0,
                    reinterpret_cast<LPARAM>(
                        result.get()))) {
                return;
            }

            result.release();
        })
        .detach();
}

void StartUpdateCheck(HWND hwnd) {
    EnableWindow(g_updateButton, FALSE);
    SetWindowTextW(g_updateButton, L"Checking app...");
    SetIndicator(g_githubStatus, L"GitHub Comms: Checking...");
    SetIndicator(g_releaseStatus, L"Release: Checking...");

    std::thread([hwnd]() {
        auto result = std::make_unique<CheckResult>();
        result->ok = tp::CheckLatestRelease(
            result->release,
            result->state,
            result->error);

        if (!PostMessageW(
                hwnd,
                WM_TP_CHECK_COMPLETE,
                0,
                reinterpret_cast<LPARAM>(result.get()))) {
            return;
        }
        result.release();
    }).detach();
}

void StartUpdate(HWND hwnd) {
    if (g_packageInstallInProgress) {
        MessageBoxW(
            hwnd,
            L"Finish the active package install before replacing TocPilot itself.",
            L"TocPilot - Package Install Active",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    g_appUpdateInProgress = true;
    UpdatePackageButtons();
    EnableWindow(g_updateButton, FALSE);
    SetWindowTextW(g_updateButton, L"Updating app...");
    SetIndicator(g_releaseStatus, L"Release: Downloading and verifying...");

    const auto target = tp::ExecutablePath();
    const auto workingDirectory = target.parent_path();
    const auto release = g_release;
    const DWORD pid = GetCurrentProcessId();

    std::thread([hwnd, release, target, workingDirectory, pid]() {
        auto result = std::make_unique<UpdateResult>();
        result->ok = tp::DownloadVerifyAndLaunchUpdater(
            release,
            pid,
            target,
            workingDirectory,
            result->error);

        if (!PostMessageW(
                hwnd,
                WM_TP_UPDATE_COMPLETE,
                0,
                reinterpret_cast<LPARAM>(result.get()))) {
            return;
        }
        result.release();
    }).detach();
}

LRESULT HandleStatusColour(WPARAM wParam, LPARAM lParam) {
    const HWND control = reinterpret_cast<HWND>(lParam);
    const int id = GetDlgCtrlID(control);
    const std::wstring text = [&]() {
        const int length = GetWindowTextLengthW(control);
        std::wstring value(static_cast<size_t>(length) + 1, L'\0');
        if (length > 0) {
            GetWindowTextW(control, value.data(), length + 1);
        }
        value.resize(static_cast<size_t>(length));
        return value;
    }();

    COLORREF colour = GetSysColor(COLOR_WINDOWTEXT);

    if (id == IDC_WOW_STATUS) {
        colour = RGB(0, 128, 0);
    } else if (id == IDC_GITHUB_STATUS) {
        if (text.find(L"Good") != std::wstring::npos) {
            colour = RGB(0, 128, 0);
        } else if (text.find(L"Failed") != std::wstring::npos) {
            colour = RGB(180, 0, 0);
        } else {
            colour = GetSysColor(COLOR_GRAYTEXT);
        }
    } else if (id == IDC_RELEASE_STATUS) {
        if (text.find(L"Update available") != std::wstring::npos) {
            colour = RGB(0, 110, 180);
        } else if (text.find(L"Current") != std::wstring::npos) {
            colour = RGB(0, 128, 0);
        } else if (text.find(L"Not found") != std::wstring::npos) {
            colour = RGB(180, 100, 0);
        } else if (text.find(L"failed") != std::wstring::npos ||
                   text.find(L"Failed") != std::wstring::npos ||
                   text.find(L"Unavailable") != std::wstring::npos) {
            colour = RGB(180, 0, 0);
        } else {
            colour = GetSysColor(COLOR_GRAYTEXT);
        }
    } else if (id == IDC_STATE_STATUS) {
        if (text.find(L"ready") != std::wstring::npos) {
            colour = RGB(0, 128, 0);
        } else {
            colour = RGB(180, 0, 0);
        }
    } else {
        return 0;
    }

    HDC dc = reinterpret_cast<HDC>(wParam);
    SetTextColor(dc, colour);
    SetBkColor(dc, GetSysColor(COLOR_WINDOW));
    return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
}

void HandleTextScaleChange(HWND hwnd) {
    if (!g_stateReady || !g_textScaleCombo) {
        return;
    }

    const int selection = ComboBox_GetCurSel(g_textScaleCombo);
    if (selection < 0 ||
        selection >= static_cast<int>(kTextScales.size())) {
        return;
    }

    g_state.settings.textScale =
        kTextScales[static_cast<std::size_t>(selection)];

    std::wstring error;
    if (!tp::SaveState(g_root, g_state, error)) {
        g_stateError = error;
        SetIndicator(
            g_stateStatus,
            L"State: Save failed - " + error);
        return;
    }

    g_stateCreated = false;
    SetIndicator(g_stateStatus, StateStatusText());
    ApplyUiFont(hwnd);
    LayoutControls(hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowExW(
            0,
            L"STATIC",
            L"TocPilot",
            WS_CHILD | WS_VISIBLE,
            20,
            16,
            220,
            28,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        std::wstring versionText = L"Version ";
        versionText += TOCPILOT_VERSION_TAG_W;
        g_versionLabel = CreateWindowExW(
            0,
            L"STATIC",
            versionText.c_str(),
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            620,
            18,
            240,
            24,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        std::wstring rootText = L"World of Warcraft: ";
        rootText += g_root.wstring();
        g_rootLabel = CreateWindowExW(
            0,
            L"STATIC",
            rootText.c_str(),
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            20,
            48,
            820,
            22,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        g_wowStatus = CreateWindowExW(
            0,
            L"STATIC",
            L"WoW Install: Found",
            WS_CHILD | WS_VISIBLE,
            20,
            80,
            205,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WOW_STATUS)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_githubStatus = CreateWindowExW(
            0,
            L"STATIC",
            L"GitHub Comms: Not checked",
            WS_CHILD | WS_VISIBLE,
            235,
            80,
            220,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GITHUB_STATUS)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_releaseStatus = CreateWindowExW(
            0,
            L"STATIC",
            L"Release: Not checked",
            WS_CHILD | WS_VISIBLE,
            465,
            80,
            380,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RELEASE_STATUS)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_stateStatus = CreateWindowExW(
            0,
            L"STATIC",
            StateStatusText().c_str(),
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            20,
            108,
            820,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATE_STATUS)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_updateAllButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Update All",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            20,
            145,
            105,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_UPDATE_ALL)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_refreshPackagesButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Refresh",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            130,
            145,
            85,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REFRESH_PACKAGES)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_setBranchButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Set Branch",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            220,
            145,
            100,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_BRANCH)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_inspectPackageButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Inspect",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            325,
            145,
            75,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INSPECT_PACKAGE)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_installPackageButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Install",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            405,
            145,
            80,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INSTALL_PACKAGE)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_addPackageButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Add Package",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            490,
            145,
            100,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ADD_PACKAGE)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_removePackageButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Forget",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            595,
            145,
            65,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REMOVE_PACKAGE)),
            GetModuleHandleW(nullptr),
            nullptr);

        EnableWindow(g_updateAllButton, FALSE);
        EnableWindow(g_refreshPackagesButton, FALSE);
        EnableWindow(g_setBranchButton, FALSE);
        EnableWindow(g_inspectPackageButton, FALSE);
        EnableWindow(g_installPackageButton, FALSE);
        EnableWindow(g_removePackageButton, FALSE);
        EnableWindow(
            g_addPackageButton,
            g_stateReady ? TRUE : FALSE);

        g_updateButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Check app update",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            435,
            145,
            145,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_UPDATE)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_textScaleLabel = CreateWindowExW(
            0,
            L"STATIC",
            L"Text size",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            650,
            151,
            70,
            22,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        g_textScaleCombo = CreateWindowExW(
            0,
            L"COMBOBOX",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                CBS_DROPDOWNLIST | WS_VSCROLL,
            730,
            145,
            110,
            180,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TEXT_SCALE)),
            GetModuleHandleW(nullptr),
            nullptr);

        for (const wchar_t* label : kTextScaleLabels) {
            ComboBox_AddString(g_textScaleCombo, label);
        }
        SetTextScaleSelection();
        EnableWindow(g_textScaleCombo, g_stateReady ? TRUE : FALSE);

        g_packageHint = CreateWindowExW(
            0,
            L"STATIC",
            PackageHintText().c_str(),
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            20,
            188,
            820,
            22,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        g_packageList = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEWW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
            20,
            215,
            820,
            280,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PACKAGE_LIST)),
            GetModuleHandleW(nullptr),
            nullptr);

        ListView_SetExtendedListViewStyle(
            g_packageList,
            LVS_EX_FULLROWSELECT |
                LVS_EX_DOUBLEBUFFER |
                LVS_EX_LABELTIP);

        AddPackageListColumns();
        PopulatePackageList();
        UpdatePackageButtons();
        ApplyUiFont(hwnd);
        LayoutControls(hwnd);

        if (!g_stateReady || g_state.settings.checkAppUpdates) {
            StartUpdateCheck(hwnd);
        } else {
            EnableWindow(g_updateButton, TRUE);
        }

        return 0;
    }

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 820;
        info->ptMinTrackSize.y = 500;
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        const LRESULT colourResult = HandleStatusColour(wParam, lParam);
        if (colourResult != 0) {
            return colourResult;
        }
        break;
    }

    case WM_NOTIFY: {
        const auto* header =
            reinterpret_cast<NMHDR*>(lParam);

        if (header &&
            header->idFrom == IDC_PACKAGE_LIST &&
            header->code == LVN_ITEMCHANGED) {
            UpdatePackageButtons();
            return 0;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_UPDATE && HIWORD(wParam) == BN_CLICKED) {
            if (!g_release.assetUrl.empty()) {
                StartUpdate(hwnd);
            } else {
                StartUpdateCheck(hwnd);
            }
            return 0;
        }

        if (LOWORD(wParam) == IDC_REFRESH_PACKAGES &&
            HIWORD(wParam) == BN_CLICKED) {
            const int row = SelectedPackageRow();

            if (row >= 0 &&
                row < static_cast<int>(
                    g_state.packages.size())) {
                StartPackageRefresh(
                    hwnd,
                    static_cast<std::size_t>(row));
            }
            return 0;
        }

        if (LOWORD(wParam) == IDC_INSPECT_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            const int row = SelectedPackageRow();

            if (row >= 0 &&
                row < static_cast<int>(
                    g_state.packages.size())) {
                StartPackageInspection(
                    hwnd,
                    static_cast<std::size_t>(row));
            }
            return 0;
        }

        if (LOWORD(wParam) == IDC_INSTALL_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            const int row = SelectedPackageRow();

            if (row >= 0 &&
                row < static_cast<int>(
                    g_state.packages.size())) {
                const auto index =
                    static_cast<std::size_t>(row);
                const auto& package =
                    g_state.packages[index];

                std::wstring prompt;
                if (package.installedRevision.empty()) {
                    prompt =
                        L"Install " +
                        package.name +
                        L" from " +
                        package.ref +
                        L"?";
                } else {
                    prompt =
                        L"Update/reinstall " +
                        package.name +
                        L" from " +
                        package.ref +
                        L"?";
                }

                prompt +=
                    L"\r\n\r\nTocPilot will fully stage and validate the package first, "
                    L"then replace only addon roots owned by this package. Existing "
                    L"unowned addon folders will not be overwritten.";

                if (MessageBoxW(
                        hwnd,
                        prompt.c_str(),
                        L"TocPilot - Install Package",
                        MB_YESNO |
                            MB_ICONQUESTION |
                            MB_DEFBUTTON2) == IDYES) {
                    StartPackageInstall(
                        hwnd,
                        index);
                }
            }
            return 0;
        }

        if (LOWORD(wParam) == IDC_SET_BRANCH &&
            HIWORD(wParam) == BN_CLICKED) {
            const int row = SelectedPackageRow();

            if (row < 0 ||
                row >= static_cast<int>(
                    g_state.packages.size())) {
                MessageBoxW(
                    hwnd,
                    L"Select a package row first.",
                    L"TocPilot - Set Branch",
                    MB_OK | MB_ICONINFORMATION);
                return 0;
            }

            const auto index =
                static_cast<std::size_t>(row);

            tp::BranchSelection selection;
            if (!tp::ShowBranchDialog(
                    hwnd,
                    g_state.packages[index],
                    selection)) {
                return 0;
            }

            tp::AppState updatedState = g_state;
            std::wstring error;

            if (!tp::SetPackageBranch(
                    updatedState.packages[index],
                    std::move(selection.name),
                    std::move(selection.sha),
                    error) ||
                !tp::SaveState(
                    g_root,
                    updatedState,
                    error)) {
                SetIndicator(
                    g_stateStatus,
                    L"State: Save failed - " +
                        error);

                MessageBoxW(
                    hwnd,
                    error.c_str(),
                    L"TocPilot - Set Branch",
                    MB_OK | MB_ICONERROR);
                return 0;
            }

            g_state =
                std::move(updatedState);
            g_stateCreated = false;
            g_stateError.clear();

            RefreshPackageStateUi();
            SelectPackageRow(index);
            UpdatePackageButtons();
            return 0;
        }

        if (LOWORD(wParam) == IDC_REMOVE_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            const int row = SelectedPackageRow();

            if (row < 0 ||
                row >= static_cast<int>(
                    g_state.packages.size())) {
                return 0;
            }

            const auto index =
                static_cast<std::size_t>(row);
            const auto package =
                g_state.packages[index];

            std::wstring prompt =
                L"Forget " +
                package.name +
                L" from TocPilot?\r\n\r\n"
                L"This removes only the TocPilot package record. "
                L"No files or folders under Interface\\AddOns will be deleted.";

            if (!package.installedFiles.empty()) {
                prompt +=
                    L"\r\n\r\nTocPilot will also forget its recorded ownership "
                    L"of this package's installed files.";
            }

            if (MessageBoxW(
                    hwnd,
                    prompt.c_str(),
                    L"TocPilot - Forget Package",
                    MB_YESNO |
                        MB_ICONWARNING |
                        MB_DEFBUTTON2) != IDYES) {
                return 0;
            }

            tp::AppState updatedState = g_state;
            std::wstring error;
            if (!tp::RemovePackageRecord(
                    updatedState,
                    package.id,
                    error) ||
                !tp::SaveState(
                    g_root,
                    updatedState,
                    error)) {
                MessageBoxW(
                    hwnd,
                    error.c_str(),
                    L"TocPilot - Forget Package",
                    MB_OK | MB_ICONERROR);
                return 0;
            }

            g_state = std::move(updatedState);
            g_stateCreated = false;
            g_stateError.clear();
            RefreshPackageStateUi();
            return 0;
        }

        if (LOWORD(wParam) == IDC_ADD_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            tp::PackageRecord package;
            if (!tp::ShowAddPackageDialog(hwnd, package)) {
                return 0;
            }

            tp::AppState updatedState = g_state;
            std::wstring error;
            if (!tp::AppendPackage(
                    updatedState,
                    std::move(package),
                    error)) {
                MessageBoxW(
                    hwnd,
                    error.c_str(),
                    L"TocPilot - Add Package",
                    MB_OK | MB_ICONWARNING);
                return 0;
            }

            if (!tp::SaveState(
                    g_root,
                    updatedState,
                    error)) {
                SetIndicator(
                    g_stateStatus,
                    L"State: Save failed - " + error);
                MessageBoxW(
                    hwnd,
                    error.c_str(),
                    L"TocPilot - Add Package",
                    MB_OK | MB_ICONERROR);
                return 0;
            }

            g_state = std::move(updatedState);
            g_stateCreated = false;
            g_stateError.clear();
            RefreshPackageStateUi();
            return 0;
        }

        if (LOWORD(wParam) == IDC_TEXT_SCALE &&
            HIWORD(wParam) == CBN_SELCHANGE) {
            HandleTextScaleChange(hwnd);
            return 0;
        }
        break;

    case WM_TP_CHECK_COMPLETE: {
        std::unique_ptr<CheckResult> result(
            reinterpret_cast<CheckResult*>(lParam));

        if (!result->ok) {
            g_release = {};
            SetIndicator(g_githubStatus, L"GitHub Comms: Failed");
            SetIndicator(g_releaseStatus, L"Release: Unavailable");
            SetWindowTextW(g_updateButton, L"Retry app check");
            EnableWindow(g_updateButton, TRUE);
            return 0;
        }

        SetIndicator(g_githubStatus, L"GitHub Comms: Good");

        switch (result->state) {
        case tp::ReleaseCheckState::NoRelease:
            g_release = {};
            SetIndicator(g_releaseStatus, L"Release: Not found");
            SetWindowTextW(g_updateButton, L"Check app update");
            EnableWindow(g_updateButton, TRUE);
            break;

        case tp::ReleaseCheckState::UpdateAvailable:
            g_release = result->release;
            SetIndicator(
                g_releaseStatus,
                L"Release: Update available - " + result->release.tag);
            SetWindowTextW(g_updateButton, L"Update app");
            EnableWindow(g_updateButton, TRUE);
            break;

        case tp::ReleaseCheckState::UpToDate:
            g_release = {};
            SetIndicator(
                g_releaseStatus,
                L"Release: Current - " + result->release.tag);
            SetWindowTextW(g_updateButton, L"Check app update");
            EnableWindow(g_updateButton, TRUE);
            break;
        }

        return 0;
    }

    case WM_TP_PACKAGE_REFRESH_COMPLETE: {
        std::unique_ptr<PackageRefreshResult> result(
            reinterpret_cast<PackageRefreshResult*>(
                lParam));

        g_packageRefreshInProgress = false;

        if (result->index >=
                g_state.packages.size() ||
            g_state.packages[result->index].id !=
                result->packageId ||
            g_state.packages[result->index].ref !=
                result->branch) {
            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    L"Refresh result was ignored because the package tracking changed.");
            }
            UpdatePackageButtons();
            return 0;
        }

        const auto index = result->index;
        const std::wstring packageName =
            g_state.packages[index].name;

        if (!result->ok) {
            SetPackageRowStatus(
                index,
                L"Refresh failed");

            if (g_packageHint) {
                const std::wstring message =
                    packageName +
                    L": refresh failed - " +
                    result->error;

                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            SelectPackageRow(index);
            UpdatePackageButtons();
            return 0;
        }

        tp::AppState updatedState = g_state;
        std::wstring error;

        if (!tp::SetPackageLatestRevision(
                updatedState.packages[index],
                std::move(result->remoteSha),
                error) ||
            !tp::SaveState(
                g_root,
                updatedState,
                error)) {
            SetPackageRowStatus(
                index,
                L"Save failed");

            if (g_packageHint) {
                const std::wstring message =
                    packageName +
                    L": refresh result could not be saved - " +
                    error;

                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            SelectPackageRow(index);
            UpdatePackageButtons();
            return 0;
        }

        g_state = std::move(updatedState);
        g_stateCreated = false;
        g_stateError.clear();

        RefreshPackageStateUi();
        SelectPackageRow(index);
        UpdatePackageButtons();

        if (g_packageHint) {
            const auto& package =
                g_state.packages[index];

            const std::wstring shortSha =
                package.latestRevision.substr(
                    0,
                    std::min<std::size_t>(
                        7,
                        package.latestRevision.size()));

            const std::wstring message =
                package.name +
                L": " +
                package.ref +
                L" remote head is " +
                shortSha +
                L". No addon files were changed.";

            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        return 0;
    }

    case WM_TP_PACKAGE_INSPECT_COMPLETE: {
        std::unique_ptr<PackageInspectResult> result(
            reinterpret_cast<PackageInspectResult*>(
                lParam));

        g_packageInspectInProgress = false;

        if (result->index >=
                g_state.packages.size() ||
            g_state.packages[result->index].id !=
                result->packageId ||
            g_state.packages[result->index].ref !=
                result->branch) {
            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    L"Inspection result was ignored because the package tracking changed.");
            }
            UpdatePackageButtons();
            return 0;
        }

        const auto index = result->index;
        const auto& package =
            g_state.packages[index];

        if (!result->ok) {
            SetPackageRowStatus(
                index,
                L"Inspect failed");

            if (g_packageHint) {
                const std::wstring message =
                    package.name +
                    L": inspection failed - " +
                    result->error;

                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            SelectPackageRow(index);
            UpdatePackageButtons();
            return 0;
        }

        RefreshPackageStateUi();
        SelectPackageRow(index);
        UpdatePackageButtons();

        const std::wstring shortSha =
            result->remoteSha.substr(
                0,
                std::min<std::size_t>(
                    7,
                    result->remoteSha.size()));

        std::wstring hint =
            package.name +
            L": staged " +
            std::to_wstring(
                result->inspection.entryCount) +
            L" ZIP entries at " +
            shortSha +
            L"; detected " +
            std::to_wstring(
                result->inspection.candidates.size()) +
            L" addon root(s). Interface\\AddOns was not changed.";

        if (g_packageHint) {
            SetWindowTextW(
                g_packageHint,
                hint.c_str());
        }

        std::wstring summary =
            package.name +
            L" / " +
            package.ref +
            L" @ " +
            shortSha +
            L"\r\n\r\nStaging: " +
            result->inspection.stagingDirectory.wstring() +
            L"\r\nDownloaded: " +
            std::to_wstring(
                result->downloadedBytes) +
            L" bytes\r\nArchive entries: " +
            std::to_wstring(
                result->inspection.entryCount) +
            L"\r\nExtracted bytes: " +
            std::to_wstring(
                result->inspection.totalUncompressedBytes) +
            L"\r\n\r\n";

        if (result->inspection.candidates.empty()) {
            summary +=
                L"No addon roots containing .toc files were detected.";
        } else {
            summary +=
                L"Candidate addon folders (preview only):\r\n";

            constexpr std::size_t maxShown = 30;
            const std::size_t shown =
                std::min<std::size_t>(
                    maxShown,
                    result->inspection.candidates.size());

            for (std::size_t i = 0;
                 i < shown;
                 ++i) {
                const auto& candidate =
                    result->inspection.candidates[i];

                summary +=
                    L"\r\n- " +
                    candidate.installFolder +
                    L"  <-  " +
                    candidate.sourceRelativePath
                        .generic_wstring();

                if (!candidate.tocFiles.empty()) {
                    summary += L"  [";
                    for (std::size_t toc = 0;
                         toc < candidate.tocFiles.size();
                         ++toc) {
                        if (toc != 0) {
                            summary += L", ";
                        }

                        summary +=
                            candidate.tocFiles[toc]
                                .filename()
                                .wstring();
                    }
                    summary += L"]";
                }
            }

            if (shown <
                result->inspection.candidates.size()) {
                summary +=
                    L"\r\n\r\n...and " +
                    std::to_wstring(
                        result->inspection.candidates.size() -
                        shown) +
                    L" more.";
            }
        }

        summary +=
            L"\r\n\r\nNothing was copied, moved, deleted, or overwritten under Interface\\AddOns.";

        if (!package.latestRevision.empty() &&
            package.latestRevision !=
                result->remoteSha) {
            summary +=
                L"\r\n\r\nThe branch moved since the saved Refresh result. "
                L"Use Refresh to persist this newer remote head.";
        }

        MessageBoxW(
            hwnd,
            summary.c_str(),
            L"TocPilot - Archive Inspection",
            MB_OK | MB_ICONINFORMATION);

        return 0;
    }

    case WM_TP_PACKAGE_INSTALL_COMPLETE: {
        std::unique_ptr<PackageInstallResult> result(
            reinterpret_cast<PackageInstallResult*>(
                lParam));

        g_packageInstallInProgress = false;

        if (result->index >=
                g_state.packages.size() ||
            g_state.packages[result->index].id !=
                result->packageId ||
            g_state.packages[result->index].ref !=
                result->branch) {
            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    L"Install result was discarded because package tracking changed. Live addons were not committed.");
            }
            UpdatePackageButtons();
            return 0;
        }

        const auto index = result->index;
        const std::wstring packageName =
            g_state.packages[index].name;

        if (!result->ok) {
            SetPackageRowStatus(
                index,
                L"Install failed");

            if (g_packageHint) {
                const std::wstring message =
                    packageName +
                    L": install preparation failed - " +
                    result->error;

                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            SelectPackageRow(index);
            UpdatePackageButtons();
            return 0;
        }

        SetPackageRowStatus(
            index,
            L"Committing...");

        std::wstring error;
        if (!tp::CommitAddonInstallTransaction(
                result->transaction,
                error)) {
            SetPackageRowStatus(
                index,
                L"Install failed");

            if (g_packageHint) {
                const std::wstring message =
                    packageName +
                    L": live commit failed and was rolled back - " +
                    error;

                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            SelectPackageRow(index);
            UpdatePackageButtons();
            return 0;
        }

        tp::AppState updatedState = g_state;
        if (!tp::SetPackageInstalledState(
                updatedState.packages[index],
                result->remoteSha,
                result->transaction.plan.desiredInstalledFiles,
                error) ||
            !tp::SaveState(
                g_root,
                updatedState,
                error)) {
            const std::wstring saveError = error;
            std::wstring rollbackError;
            const bool rolledBack =
                tp::RollbackAddonInstallTransaction(
                    result->transaction,
                    rollbackError);

            SetPackageRowStatus(
                index,
                rolledBack
                    ? L"Install rolled back"
                    : L"Rollback failed");

            if (g_packageHint) {
                std::wstring message =
                    packageName +
                    L": package state could not be saved - " +
                    saveError;

                if (rolledBack) {
                    message +=
                        L". The previous live addon state was restored.";
                } else {
                    message +=
                        L". Filesystem rollback also failed: " +
                        rollbackError;
                }

                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            if (!rolledBack) {
                MessageBoxW(
                    hwnd,
                    rollbackError.c_str(),
                    L"TocPilot - Rollback Failed",
                    MB_OK | MB_ICONERROR);
            }

            SelectPackageRow(index);
            UpdatePackageButtons();
            return 0;
        }

        g_state = std::move(updatedState);
        g_stateCreated = false;
        g_stateError.clear();

        std::wstring cleanupError;
        const bool cleanupOk =
            tp::FinalizeAddonInstallTransaction(
                result->transaction,
                cleanupError);

        const auto& installedPackage =
            g_state.packages[index];

        std::wstring stagingCleanupError;
        const bool stagingCleanupOk =
            tp::CleanupGitHubPackageStaging(
                g_root,
                installedPackage.repository,
                stagingCleanupError);

        RefreshPackageStateUi();
        SelectPackageRow(index);
        UpdatePackageButtons();
        const std::wstring shortSha =
            installedPackage.installedRevision.substr(
                0,
                std::min<std::size_t>(
                    7,
                    installedPackage.installedRevision.size()));

        std::wstring hint =
            installedPackage.name +
            L": installed " +
            std::to_wstring(
                result->transaction.plan.desiredInstalledFiles.size()) +
            L" file(s) across " +
            std::to_wstring(
                result->transaction.plan.roots.size()) +
            L" addon root(s) at " +
            shortSha +
            L".";

        if (!cleanupOk) {
            hint +=
                L" Install succeeded, but transaction cleanup needs attention: " +
                cleanupError;
        }
        if (!stagingCleanupOk) {
            hint +=
                L" Staging cleanup needs attention: " +
                stagingCleanupError;
        }

        if (g_packageHint) {
            SetWindowTextW(
                g_packageHint,
                hint.c_str());
        }

        std::wstring summary =
            installedPackage.name +
            L" installed successfully.\r\n\r\nRevision: " +
            shortSha +
            L"\r\nAddon roots: " +
            std::to_wstring(
                result->transaction.plan.roots.size()) +
            L"\r\nOwned files: " +
            std::to_wstring(
                result->transaction.plan.desiredInstalledFiles.size());

        if (!result->transaction.plan.obsoleteInstallFolders.empty()) {
            summary +=
                L"\r\nObsolete roots removed: " +
                std::to_wstring(
                    result->transaction.plan.obsoleteInstallFolders.size());
        }

        if (!cleanupOk) {
            summary +=
                L"\r\n\r\nThe install and state save succeeded, but old transaction backup cleanup failed:\r\n" +
                cleanupError;
        }
        if (!stagingCleanupOk) {
            summary +=
                L"\r\n\r\nPackage staging cleanup failed:\r\n" +
                stagingCleanupError;
        }

        MessageBoxW(
            hwnd,
            summary.c_str(),
            L"TocPilot - Package Installed",
            MB_OK |
                (cleanupOk && stagingCleanupOk
                    ? MB_ICONINFORMATION
                    : MB_ICONWARNING));

        return 0;
    }

    case WM_TP_UPDATE_COMPLETE: {
        std::unique_ptr<UpdateResult> result(
            reinterpret_cast<UpdateResult*>(lParam));

        if (!result->ok) {
            g_appUpdateInProgress = false;
            UpdatePackageButtons();
            SetIndicator(
                g_releaseStatus,
                L"Release: Update failed - " + result->error);
            SetWindowTextW(g_updateButton, L"Retry app update");
            EnableWindow(g_updateButton, TRUE);
            return 0;
        }

        SetIndicator(g_releaseStatus, L"Release: Verified - restarting...");
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
    }

    case WM_CLOSE:
        if (g_packageInstallInProgress) {
            MessageBoxW(
                hwnd,
                L"A package install is currently being prepared. TocPilot will stay open until the install transaction reaches a safe completion point.",
                L"TocPilot - Install In Progress",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;

    case WM_DESTROY:
        if (g_uiFont) {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ValidateWowRoot(const std::filesystem::path& root, std::wstring& error) {
    std::error_code ec;
    const auto wowExe = root / L"WoW.exe";

    if (!std::filesystem::is_regular_file(wowExe, ec)) {
        error =
            L"TocPilot.exe must be placed in the World of Warcraft folder "
            L"beside WoW.exe.";
        return false;
    }

    const auto addons = root / L"Interface" / L"AddOns";
    std::filesystem::create_directories(addons, ec);
    if (ec) {
        error = L"Could not create or access Interface\\AddOns.";
        return false;
    }

    return true;
}

int RunMainWindow(HINSTANCE instance) {
    const auto exe = tp::ExecutablePath();
    if (exe.empty()) {
        MessageBoxW(
            nullptr,
            L"Could not determine the TocPilot executable path.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 2;
    }

    g_root = exe.parent_path();

    std::wstring error;
    if (!ValidateWowRoot(g_root, error)) {
        MessageBoxW(
            nullptr,
            error.c_str(),
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 2;
    }

    SetCurrentDirectoryW(g_root.c_str());

    g_stateReady = tp::LoadOrCreateState(
        g_root,
        g_state,
        g_stateCreated,
        g_stateError);

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(
            nullptr,
            L"Could not register the TocPilot window class.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 3;
    }

    std::wstring title = L"TocPilot ";
    title += TOCPILOT_VERSION_TAG_W;

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClass,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        560,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd) {
        MessageBoxW(
            nullptr,
            L"Could not create the TocPilot window.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 4;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return 1;
    }

    if (argc >= 5 && std::wstring_view(argv[1]) == L"--apply-update") {
        const DWORD parentPid =
            static_cast<DWORD>(std::wcstoul(argv[2], nullptr, 10));
        const std::filesystem::path target = argv[3];
        const std::filesystem::path workingDirectory = argv[4];
        LocalFree(argv);
        return tp::RunUpdaterMode(parentPid, target, workingDirectory);
    }

    if (argc >= 4 && std::wstring_view(argv[1]) == L"--post-update-cleanup") {
        const std::filesystem::path backup = argv[2];
        const std::filesystem::path staged = argv[3];
        tp::CleanupAfterUpdate(backup, staged);
    }

    LocalFree(argv);
    return RunMainWindow(instance);
}
