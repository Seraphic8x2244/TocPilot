#include "addon_scan.h"
#include "adoption.h"
#include "add_package_dialog.h"
#include "archive.h"
#include "branch_dialog.h"
#include "github_api.h"
#include "install.h"
#include "refresh_freshness.h"
#include "state.h"
#include "update.h"
#include "update_all.h"
#include "ui_dialog.h"
#include "version.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

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
constexpr int IDC_UNINSTALL_PACKAGE = 1015;
constexpr int IDC_ADOPT_GIT = 1016;

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
HWND g_uninstallPackageButton = nullptr;
HWND g_removePackageButton = nullptr;
HWND g_addPackageButton = nullptr;
HWND g_adoptGitButton = nullptr;
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
bool g_updateAllInProgress = false;
bool g_appUpdateInProgress = false;
bool g_autoStatusRefreshInProgress = false;
tp::UpdateAllProgress g_updateAllProgress;
std::vector<tp::PackageRefreshStamp> g_packageRefreshStamps;
std::vector<std::wstring> g_autoStatusPackageIds;
std::size_t g_autoStatusPosition = 0;
std::size_t g_autoStatusCurrent = 0;
std::size_t g_autoStatusUpdates = 0;
std::size_t g_autoStatusFailed = 0;
bool g_autoStatusRateLimited = false;
std::vector<std::size_t> g_packageViewOrder;
int g_packageSortColumn = -1;
bool g_packageSortAscending = true;
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
        L"Install/Update and Uninstall use staged rollback transactions; "
        L"Update All checks installed tracked packages and updates only changed branches; "
        L"Forget removes only the TocPilot record.";
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
    if (g_uninstallPackageButton) {
        MoveWindow(g_uninstallPackageButton, x, buttonY, 75, 32, TRUE);
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
    if (g_adoptGitButton) {
        MoveWindow(g_adoptGitButton, x, buttonY, 75, 32, TRUE);
        x += 80;
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

bool PackageBranchMode(
    const tp::PackageRecord& package) {
    return
        package.mode == L"branch" &&
        !package.ref.empty();
}

std::wstring PackageSourceText(
    const tp::PackageRecord& package) {
    return
        ProviderLabel(package.provider) +
        (PackageBranchMode(package)
            ? L" / " + package.ref
            : L" / source only");
}

std::wstring PackageRevisionText(
    const std::wstring& revision) {
    return revision.empty()
        ? L"—"
        : revision.substr(
            0,
            std::min<std::size_t>(
                7,
                revision.size()));
}

std::wstring PackageStatusText(
    const tp::PackageRecord& package) {
    if (!PackageBranchMode(package)) {
        return L"Not configured";
    }

    if (package.installedRevision.empty()) {
        return L"Not installed";
    }

    return package.installedRevision ==
            package.latestRevision
        ? L"Current"
        : L"Update available";
}

int CompareInsensitive(
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
            return -1;
        }
        if (a > b) {
            return 1;
        }
    }

    if (left.size() < right.size()) {
        return -1;
    }
    if (left.size() > right.size()) {
        return 1;
    }

    return 0;
}

std::wstring SingleOwnedAddonRoot(
    const tp::PackageRecord& package) {
    constexpr std::wstring_view prefix =
        L"Interface/AddOns/";

    std::wstring root;

    for (auto owned :
         package.installedFiles) {
        std::replace(
            owned.begin(),
            owned.end(),
            L'\\',
            L'/');

        if (owned.size() <=
                prefix.size() ||
            CompareInsensitive(
                std::wstring_view(owned).substr(
                    0,
                    prefix.size()),
                prefix) != 0) {
            return {};
        }

        const std::size_t slash =
            owned.find(
                L'/',
                prefix.size());

        if (slash ==
                std::wstring::npos ||
            slash ==
                prefix.size()) {
            return {};
        }

        const std::wstring candidate =
            owned.substr(
                prefix.size(),
                slash -
                    prefix.size());

        if (root.empty()) {
            root =
                candidate;
        } else if (
            CompareInsensitive(
                root,
                candidate) != 0) {
            return {};
        }
    }

    return root;
}

std::wstring PackageDisplayName(
    const tp::PackageRecord& package) {
    if (!PackageBranchMode(package) ||
        package.ref.empty() ||
        CompareInsensitive(package.ref, L"main") == 0 ||
        CompareInsensitive(package.ref, L"master") == 0) {
        return package.name;
    }

    return
        package.name +
        L" (" +
        package.ref +
        L")";
}

bool PackageNeedsAttention(
    const tp::PackageRecord& package) {
    return
        !PackageBranchMode(package) ||
        package.installedRevision.empty() ||
        package.latestRevision.empty() ||
        package.installedRevision !=
            package.latestRevision;
}

std::wstring PackageColumnText(
    const tp::PackageRecord& package,
    int column) {
    switch (column) {
    case 0:
        return PackageDisplayName(package);
    case 1:
        return PackageSourceText(package);
    case 2:
        return package.installedRevision;
    case 3:
        return package.latestRevision;
    case 4:
        return PackageStatusText(package);
    default:
        return {};
    }
}

void RebuildPackageViewOrder() {
    g_packageViewOrder.clear();
    g_packageViewOrder.reserve(
        g_state.packages.size());

    for (std::size_t index = 0;
         index < g_state.packages.size();
         ++index) {
        g_packageViewOrder.push_back(
            index);
    }

    std::stable_sort(
        g_packageViewOrder.begin(),
        g_packageViewOrder.end(),
        [](std::size_t leftIndex,
           std::size_t rightIndex) {
            const auto& left =
                g_state.packages[leftIndex];
            const auto& right =
                g_state.packages[rightIndex];

            const bool leftAttention =
                PackageNeedsAttention(left);
            const bool rightAttention =
                PackageNeedsAttention(right);

            if (leftAttention != rightAttention) {
                return leftAttention;
            }

            if (g_packageSortColumn < 0) {
                return false;
            }

            int comparison =
                CompareInsensitive(
                    PackageColumnText(
                        left,
                        g_packageSortColumn),
                    PackageColumnText(
                        right,
                        g_packageSortColumn));

            if (comparison == 0) {
                comparison =
                    CompareInsensitive(
                        left.name,
                        right.name);
            }

            if (comparison == 0) {
                comparison =
                    CompareInsensitive(
                        left.id,
                        right.id);
            }

            return g_packageSortAscending
                ? comparison < 0
                : comparison > 0;
        });
}

void UpdatePackageSortIndicator() {
    if (!g_packageList) {
        return;
    }

    const HWND header =
        ListView_GetHeader(
            g_packageList);

    if (!header) {
        return;
    }

    const int count =
        Header_GetItemCount(
            header);

    for (int column = 0;
         column < count;
         ++column) {
        HDITEMW item{};
        item.mask =
            HDI_FORMAT;

        if (!Header_GetItem(
                header,
                column,
                &item)) {
            continue;
        }

        item.fmt &=
            ~(HDF_SORTUP |
              HDF_SORTDOWN);

        if (column ==
            g_packageSortColumn) {
            item.fmt |=
                g_packageSortAscending
                    ? HDF_SORTUP
                    : HDF_SORTDOWN;
        }

        Header_SetItem(
            header,
            column,
            &item);
    }
}

int PackageDisplayRow(
    std::size_t packageIndex) {
    for (std::size_t row = 0;
         row < g_packageViewOrder.size();
         ++row) {
        if (g_packageViewOrder[row] ==
            packageIndex) {
            return static_cast<int>(
                row);
        }
    }

    return -1;
}

void PopulatePackageList() {
    if (!g_packageList) {
        return;
    }

    RebuildPackageViewOrder();
    ListView_DeleteAllItems(
        g_packageList);

    for (std::size_t displayIndex = 0;
         displayIndex <
            g_packageViewOrder.size();
         ++displayIndex) {
        const std::size_t packageIndex =
            g_packageViewOrder[
                displayIndex];

        const auto& package =
            g_state.packages[
                packageIndex];

        LVITEMW item{};
        item.mask =
            LVIF_TEXT;
        item.iItem =
            static_cast<int>(
                displayIndex);
        item.iSubItem = 0;
        const std::wstring displayName =
            PackageDisplayName(package);
        item.pszText =
            const_cast<LPWSTR>(
                displayName.c_str());

        const int row =
            ListView_InsertItem(
                g_packageList,
                &item);

        if (row < 0) {
            continue;
        }

        const std::wstring source =
            PackageSourceText(
                package);
        const std::wstring installed =
            PackageRevisionText(
                package.installedRevision);
        const std::wstring latest =
            PackageRevisionText(
                package.latestRevision);
        const std::wstring status =
            PackageStatusText(
                package);

        ListView_SetItemText(
            g_packageList,
            row,
            1,
            const_cast<LPWSTR>(
                source.c_str()));
        ListView_SetItemText(
            g_packageList,
            row,
            2,
            const_cast<LPWSTR>(
                installed.c_str()));
        ListView_SetItemText(
            g_packageList,
            row,
            3,
            const_cast<LPWSTR>(
                latest.c_str()));
        ListView_SetItemText(
            g_packageList,
            row,
            4,
            const_cast<LPWSTR>(
                status.c_str()));
    }

    UpdatePackageSortIndicator();
}

void SelectPackageRow(std::size_t index);

int SelectedPackageRow() {
    if (!g_packageList) {
        return -1;
    }

    const int displayRow =
        ListView_GetNextItem(
            g_packageList,
            -1,
            LVNI_SELECTED);

    if (displayRow < 0 ||
        displayRow >=
            static_cast<int>(
                g_packageViewOrder.size())) {
        return -1;
    }

    return static_cast<int>(
        g_packageViewOrder[
            static_cast<std::size_t>(
                displayRow)]);
}

void SortPackageListByColumn(
    int column) {
    if (column < 0 ||
        column >= 5) {
        return;
    }

    const int selected =
        SelectedPackageRow();

    if (g_packageSortColumn ==
        column) {
        g_packageSortAscending =
            !g_packageSortAscending;
    } else {
        g_packageSortColumn =
            column;
        g_packageSortAscending =
            true;
    }

    if (g_stateReady) {
        g_state.settings.packageSortColumn =
            g_packageSortColumn;
        g_state.settings.packageSortAscending =
            g_packageSortAscending;

        std::wstring saveError;
        if (!tp::SaveState(
                g_root,
                g_state,
                saveError)) {
            g_stateError = saveError;
        }
    }

    PopulatePackageList();

    if (selected >= 0) {
        SelectPackageRow(
            static_cast<std::size_t>(
                selected));
    }
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
    bool canUninstall = false;
    bool canUpdateAll = false;
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
        canUninstall =
            selectedPackage->target == L"addons" &&
            !selectedPackage->installedRevision.empty() &&
            !selectedPackage->installedFiles.empty();
    }

    if (g_stateReady) {
        for (const auto& package : g_state.packages) {
            if (tp::IsUpdateAllCandidate(package)) {
                canUpdateAll = true;
                break;
            }
        }
    }

    const bool packageBusy =
        g_packageRefreshInProgress ||
        g_packageInspectInProgress ||
        g_packageInstallInProgress ||
        g_updateAllInProgress ||
        g_autoStatusRefreshInProgress ||
        g_appUpdateInProgress;

    if (g_updateAllButton) {
        EnableWindow(
            g_updateAllButton,
            canUpdateAll && !packageBusy
                ? TRUE
                : FALSE);
    }

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

    if (g_uninstallPackageButton) {
        EnableWindow(
            g_uninstallPackageButton,
            canUninstall && !packageBusy
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

    if (g_adoptGitButton) {
        EnableWindow(
            g_adoptGitButton,
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

    const int row =
        PackageDisplayRow(
            index);

    if (row < 0) {
        return;
    }

    ListView_SetItemState(
        g_packageList,
        row,
        LVIS_SELECTED |
            LVIS_FOCUSED,
        LVIS_SELECTED |
            LVIS_FOCUSED);
}

void SetPackageRowStatus(
    std::size_t index,
    const std::wstring& text) {
    if (!g_packageList ||
        index >= g_state.packages.size()) {
        return;
    }

    const int row =
        PackageDisplayRow(
            index);

    if (row < 0) {
        return;
    }

    ListView_SetItemText(
        g_packageList,
        row,
        4,
        const_cast<LPWSTR>(
            text.c_str()));
}

void RefreshPackageStateUi() {
    const int selected =
        SelectedPackageRow();

    std::wstring topPackageId;
    if (g_packageList) {
        const int topRow =
            ListView_GetTopIndex(g_packageList);

        if (topRow >= 0 &&
            topRow < static_cast<int>(
                g_packageViewOrder.size())) {
            const std::size_t topIndex =
                g_packageViewOrder[
                    static_cast<std::size_t>(
                        topRow)];

            if (topIndex < g_state.packages.size()) {
                topPackageId =
                    g_state.packages[topIndex].id;
            }
        }
    }

    SetIndicator(
        g_stateStatus,
        StateStatusText());

    if (g_packageHint) {
        SetWindowTextW(
            g_packageHint,
            PackageHintText().c_str());
    }

    PopulatePackageList();

    if (!topPackageId.empty() &&
        g_packageList) {
        std::size_t topIndex =
            g_state.packages.size();

        for (std::size_t i = 0;
             i < g_state.packages.size();
             ++i) {
            if (g_state.packages[i].id ==
                topPackageId) {
                topIndex = i;
                break;
            }
        }

        if (topIndex <
            g_state.packages.size()) {
            const int topRow =
                PackageDisplayRow(topIndex);

            if (topRow > 0) {
                RECT first{};
                RECT target{};

                if (ListView_GetItemRect(
                        g_packageList,
                        0,
                        &first,
                        LVIR_BOUNDS) &&
                    ListView_GetItemRect(
                        g_packageList,
                        topRow,
                        &target,
                        LVIR_BOUNDS)) {
                    ListView_Scroll(
                        g_packageList,
                        0,
                        target.top - first.top);
                }
            }
        }
    }

    if (selected >= 0 &&
        selected <
            static_cast<int>(
                g_state.packages.size())) {
        SelectPackageRow(
            static_cast<std::size_t>(
                selected));
    }

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
                        SingleOwnedAddonRoot(package),
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
    std::size_t index,
    std::wstring knownRemoteSha = {}) {
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
         knownRemoteSha = std::move(knownRemoteSha),
         otherInstalledFiles = std::move(otherInstalledFiles),
         wowRoot]() mutable {
            auto result =
                std::make_unique<PackageInstallResult>();

            result->index = index;
            result->packageId = package.id;
            result->packageName = package.name;
            result->branch = package.ref;

            result->remoteSha =
                std::move(knownRemoteSha);

            if (result->remoteSha.empty() &&
                !tp::ResolveGitHubBranchHead(
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
                        SingleOwnedAddonRoot(package),
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

std::size_t FindPackageIndexById(
    std::wstring_view packageId) {
    for (std::size_t i = 0;
         i < g_state.packages.size();
         ++i) {
        if (g_state.packages[i].id == packageId) {
            return i;
        }
    }

    return g_state.packages.size();
}

bool IsGitHubRateLimitError(
    std::wstring_view error) {
    return
        error.find(L"rate-limited") !=
        std::wstring_view::npos;
}

bool IsAutoStatusCurrentPackage(
    std::wstring_view packageId) {
    return
        g_autoStatusRefreshInProgress &&
        g_autoStatusPosition <
            g_autoStatusPackageIds.size() &&
        g_autoStatusPackageIds[
            g_autoStatusPosition] ==
            packageId;
}

void ContinueAutoStatusRefresh(HWND hwnd);

void CompleteAutoStatusRefreshStep(
    HWND hwnd,
    bool ok,
    bool updateAvailable) {
    if (!g_autoStatusRefreshInProgress ||
        g_autoStatusPosition >=
            g_autoStatusPackageIds.size()) {
        return;
    }

    if (!ok) {
        ++g_autoStatusFailed;
    } else if (updateAvailable) {
        ++g_autoStatusUpdates;
    } else {
        ++g_autoStatusCurrent;
    }

    ++g_autoStatusPosition;
    ContinueAutoStatusRefresh(hwnd);
}

void FinishAutoStatusRefresh(HWND hwnd) {
    g_autoStatusRefreshInProgress = false;

    RefreshPackageStateUi();

    std::wstring message =
        L"Status check complete: " +
        std::to_wstring(
            g_autoStatusUpdates) +
        L" update(s) available, " +
        std::to_wstring(
            g_autoStatusCurrent) +
        L" current";

    if (g_autoStatusFailed != 0) {
        message +=
            L", " +
            std::to_wstring(
                g_autoStatusFailed) +
            L" failed";
    }

    message +=
        L".";

    if (g_autoStatusRateLimited) {
        message +=
            L" GitHub rate-limited the check; remaining packages were left at their saved status.";
    }

    if (g_packageHint) {
        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    UpdatePackageButtons();
}

void ContinueAutoStatusRefresh(HWND hwnd) {
    while (g_autoStatusPosition <
           g_autoStatusPackageIds.size()) {
        const std::wstring packageId =
            g_autoStatusPackageIds[
                g_autoStatusPosition];

        const std::size_t index =
            FindPackageIndexById(
                packageId);

        if (index >=
                g_state.packages.size() ||
            !tp::IsUpdateAllCandidate(
                g_state.packages[index])) {
            ++g_autoStatusFailed;
            ++g_autoStatusPosition;
            continue;
        }

        const auto& package =
            g_state.packages[index];

        SetPackageRowStatus(
            index,
            L"Checking...");

        if (g_packageHint) {
            const std::wstring message =
                L"Checking addon status " +
                std::to_wstring(
                    g_autoStatusPosition + 1) +
                L" of " +
                std::to_wstring(
                    g_autoStatusPackageIds.size()) +
                L": " +
                package.name +
                L". No addon files will be changed.";

            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        StartPackageRefresh(
            hwnd,
            index);
        return;
    }

    FinishAutoStatusRefresh(hwnd);
}

void StartAutoStatusRefresh(HWND hwnd) {
    if (!g_stateReady ||
        g_autoStatusRefreshInProgress ||
        g_updateAllInProgress ||
        g_packageRefreshInProgress ||
        g_packageInspectInProgress ||
        g_packageInstallInProgress ||
        g_appUpdateInProgress) {
        return;
    }

    g_autoStatusPackageIds.clear();

    for (const auto& package :
         g_state.packages) {
        if (tp::IsUpdateAllCandidate(
                package)) {
            g_autoStatusPackageIds.push_back(
                package.id);
        }
    }

    if (g_autoStatusPackageIds.empty()) {
        return;
    }

    g_autoStatusPosition = 0;
    g_autoStatusCurrent = 0;
    g_autoStatusUpdates = 0;
    g_autoStatusFailed = 0;
    g_autoStatusRateLimited = false;
    g_autoStatusRefreshInProgress = true;

    UpdatePackageButtons();
    ContinueAutoStatusRefresh(hwnd);
}

bool IsUpdateAllCurrentPackage(
    std::wstring_view packageId) {
    return
        g_updateAllInProgress &&
        tp::UpdateAllHasCurrent(g_updateAllProgress) &&
        tp::UpdateAllCurrentPackageId(g_updateAllProgress) ==
            packageId;
}

void FinishUpdateAll(HWND hwnd) {
    const std::size_t queued =
        g_updateAllProgress.packageIds.size();
    const std::size_t processed =
        g_updateAllProgress.current +
        g_updateAllProgress.updated +
        g_updateAllProgress.failed;

    g_updateAllInProgress = false;
    SetWindowTextW(
        g_updateAllButton,
        L"Update All");

    RefreshPackageStateUi();

    std::wstring summary =
        L"Update All finished.\r\n\r\nQueued: " +
        std::to_wstring(queued) +
        L"\r\nProcessed: " +
        std::to_wstring(processed) +
        L"\r\nUpdated: " +
        std::to_wstring(g_updateAllProgress.updated) +
        L"\r\nAlready current: " +
        std::to_wstring(g_updateAllProgress.current) +
        L"\r\nFailed: " +
        std::to_wstring(g_updateAllProgress.failed);

    if (processed < queued) {
        summary +=
            L"\r\nRemaining untouched: " +
            std::to_wstring(queued - processed);
    }

    if (!g_updateAllProgress.failures.empty()) {
        summary += L"\r\n\r\nFailures:";
        constexpr std::size_t maxFailuresShown = 10;
        const std::size_t shown =
            std::min<std::size_t>(
                maxFailuresShown,
                g_updateAllProgress.failures.size());

        for (std::size_t i = 0; i < shown; ++i) {
            summary +=
                L"\r\n- " +
                g_updateAllProgress.failures[i];
        }

        if (shown < g_updateAllProgress.failures.size()) {
            summary +=
                L"\r\n- ...and " +
                std::to_wstring(
                    g_updateAllProgress.failures.size() -
                    shown) +
                L" more.";
        }
    }

    if (g_packageHint) {
        SetWindowTextW(
            g_packageHint,
            summary.c_str());
    }

    MessageBoxW(
        hwnd,
        summary.c_str(),
        L"TocPilot - Update All",
        MB_OK |
            (g_updateAllProgress.failed == 0
                ? MB_ICONINFORMATION
                : MB_ICONWARNING));
}

void ContinueUpdateAll(HWND hwnd) {
    while (tp::UpdateAllHasCurrent(
            g_updateAllProgress)) {
        const std::wstring packageId(
            tp::UpdateAllCurrentPackageId(
                g_updateAllProgress));

        const std::size_t index =
            FindPackageIndexById(packageId);

        if (index >= g_state.packages.size() ||
            !tp::IsUpdateAllCandidate(
                g_state.packages[index])) {
            std::wstring ignored;
            tp::CompleteUpdateAllItem(
                g_updateAllProgress,
                tp::UpdateAllOutcome::Failed,
                packageId +
                    L": package is no longer eligible for Update All.",
                ignored);
            continue;
        }

        const auto& package =
            g_state.packages[index];

        if (tp::IsPackageRefreshFresh(
                g_packageRefreshStamps,
                package,
                GetTickCount64())) {
            if (package.installedRevision !=
                package.latestRevision) {
                SetPackageRowStatus(
                    index,
                    L"Update available");

                if (g_packageHint) {
                    const std::wstring message =
                        L"Update All: using recent status for " +
                        package.name +
                        L"; preparing the known changed revision.";

                    SetWindowTextW(
                        g_packageHint,
                        message.c_str());
                }

                StartPackageInstall(
                    hwnd,
                    index,
                    package.latestRevision);
                return;
            }

            std::wstring ignored;
            tp::CompleteUpdateAllItem(
                g_updateAllProgress,
                tp::UpdateAllOutcome::Current,
                {},
                ignored);
            continue;
        }

        SetPackageRowStatus(
            index,
            L"Checking...");

        if (g_packageHint) {
            const std::wstring message =
                L"Update All: checking " +
                std::to_wstring(
                    g_updateAllProgress.position + 1) +
                L" of " +
                std::to_wstring(
                    g_updateAllProgress.packageIds.size()) +
                L" - " +
                package.name +
                L".";
            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        StartPackageRefresh(
            hwnd,
            index);
        return;
    }

    FinishUpdateAll(hwnd);
}

void CompleteUpdateAllStep(
    HWND hwnd,
    tp::UpdateAllOutcome outcome,
    std::wstring detail,
    bool stopBatch = false) {
    std::wstring error;
    if (!tp::CompleteUpdateAllItem(
            g_updateAllProgress,
            outcome,
            std::move(detail),
            error)) {
        g_updateAllInProgress = false;
        SetWindowTextW(
            g_updateAllButton,
            L"Update All");
        UpdatePackageButtons();

        MessageBoxW(
            hwnd,
            error.c_str(),
            L"TocPilot - Update All",
            MB_OK | MB_ICONERROR);
        return;
    }

    if (stopBatch) {
        g_updateAllProgress.position =
            g_updateAllProgress.packageIds.size();
    }

    ContinueUpdateAll(hwnd);
}

void StartUpdateAll(HWND hwnd) {
    if (g_updateAllInProgress ||
        g_autoStatusRefreshInProgress ||
        g_packageRefreshInProgress ||
        g_packageInspectInProgress ||
        g_packageInstallInProgress ||
        g_appUpdateInProgress) {
        return;
    }

    auto progress =
        tp::MakeUpdateAllProgress(g_state);

    if (progress.packageIds.empty()) {
        MessageBoxW(
            hwnd,
            L"There are no installed GitHub branch packages eligible for Update All.",
            L"TocPilot - Update All",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring prompt =
        L"Check " +
        std::to_wstring(progress.packageIds.size()) +
        L" installed package(s) and update any whose tracked branch has changed?\r\n\r\n"
        L"Packages that are already current will not be reinstalled. "
        L"Packages that are not installed are ignored. "
        L"If one package fails, TocPilot will continue with the remaining packages.";

    if (MessageBoxW(
            hwnd,
            prompt.c_str(),
            L"TocPilot - Update All",
            MB_YESNO |
                MB_ICONQUESTION |
                MB_DEFBUTTON2) != IDYES) {
        return;
    }

    g_updateAllProgress = std::move(progress);
    g_updateAllInProgress = true;
    SetWindowTextW(
        g_updateAllButton,
        L"Updating...");
    UpdatePackageButtons();

    ContinueUpdateAll(hwnd);
}

void UninstallPackage(
    HWND hwnd,
    std::size_t index) {
    if (index >= g_state.packages.size()) {
        return;
    }

    const auto package =
        g_state.packages[index];

    if (package.target != L"addons" ||
        package.installedRevision.empty() ||
        package.installedFiles.empty()) {
        MessageBoxW(
            hwnd,
            L"The selected package has no TocPilot-owned addon installation to uninstall.",
            L"TocPilot - Uninstall Package",
            MB_OK | MB_ICONINFORMATION);
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

    tp::AddonInstallPlan plan;
    std::wstring error;
    if (!tp::BuildAddonRemovalPlan(
            g_root,
            package.id,
            package.installedFiles,
            otherInstalledFiles,
            plan,
            error)) {
        MessageBoxW(
            hwnd,
            error.c_str(),
            L"TocPilot - Uninstall Package",
            MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring prompt =
        L"Uninstall " +
        package.name +
        L"?\r\n\r\nTocPilot will remove " +
        std::to_wstring(
            plan.obsoleteInstallFolders.size()) +
        L" owned addon root(s) containing " +
        std::to_wstring(
            package.installedFiles.size()) +
        L" recorded file(s).\r\n\r\n"
        L"The package record and branch tracking will remain in TocPilot. "
        L"The separate Forget action stays non-destructive.";

    if (MessageBoxW(
            hwnd,
            prompt.c_str(),
            L"TocPilot - Uninstall Package",
            MB_YESNO |
                MB_ICONWARNING |
                MB_DEFBUTTON2) != IDYES) {
        return;
    }

    g_packageInstallInProgress = true;
    UpdatePackageButtons();
    SetPackageRowStatus(
        index,
        L"Uninstalling...");

    if (g_packageHint) {
        const std::wstring message =
            package.name +
            L": moving owned addon roots into rollback backup before state is changed.";
        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    tp::AddonInstallTransaction transaction;
    if (!tp::BeginAddonInstallTransaction(
            plan,
            transaction,
            error)) {
        g_packageInstallInProgress = false;
        SetPackageRowStatus(
            index,
            L"Uninstall failed");

        if (g_packageHint) {
            const std::wstring message =
                package.name +
                L": uninstall failed - " +
                error;
            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        MessageBoxW(
            hwnd,
            error.c_str(),
            L"TocPilot - Uninstall Failed",
            MB_OK | MB_ICONERROR);
        SelectPackageRow(index);
        UpdatePackageButtons();
        return;
    }

    tp::AppState updatedState = g_state;
    if (!tp::ClearPackageInstalledState(
            updatedState.packages[index],
            error) ||
        !tp::SaveState(
            g_root,
            updatedState,
            error)) {
        const std::wstring saveError = error;
        std::wstring rollbackError;
        const bool rolledBack =
            tp::RollbackAddonInstallTransaction(
                transaction,
                rollbackError);

        g_packageInstallInProgress = false;
        SetPackageRowStatus(
            index,
            rolledBack
                ? L"Uninstall rolled back"
                : L"Rollback failed");

        std::wstring message =
            package.name +
            L": package state could not be saved - " +
            saveError;

        if (rolledBack) {
            message +=
                L"\r\n\r\nThe removed addon roots were restored.";
        } else {
            message +=
                L"\r\n\r\nFilesystem rollback also failed: " +
                rollbackError;
        }

        if (g_packageHint) {
            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        MessageBoxW(
            hwnd,
            message.c_str(),
            rolledBack
                ? L"TocPilot - Uninstall Rolled Back"
                : L"TocPilot - Rollback Failed",
            MB_OK |
                (rolledBack
                    ? MB_ICONWARNING
                    : MB_ICONERROR));

        SelectPackageRow(index);
        UpdatePackageButtons();
        return;
    }

    g_state = std::move(updatedState);
    g_stateCreated = false;
    g_stateError.clear();

    std::wstring cleanupError;
    const bool cleanupOk =
        tp::FinalizeAddonInstallTransaction(
            transaction,
            cleanupError);

    g_packageInstallInProgress = false;
    RefreshPackageStateUi();
    SelectPackageRow(index);
    UpdatePackageButtons();

    std::wstring message =
        package.name +
        L" was uninstalled. The package record remains available for reinstall.";

    if (!cleanupOk) {
        message +=
            L"\r\n\r\nThe uninstall and state save succeeded, but transaction backup cleanup failed: " +
            cleanupError;
    }

    if (g_packageHint) {
        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    MessageBoxW(
        hwnd,
        message.c_str(),
        L"TocPilot - Package Uninstalled",
        MB_OK |
            (cleanupOk
                ? MB_ICONINFORMATION
                : MB_ICONWARNING));
}

void AdoptGitAddons(HWND hwnd) {
    if (!g_stateReady ||
        g_updateAllInProgress ||
        g_autoStatusRefreshInProgress ||
        g_packageRefreshInProgress ||
        g_packageInspectInProgress ||
        g_packageInstallInProgress ||
        g_appUpdateInProgress) {
        return;
    }

    const auto addOnsRoot =
        g_root /
        L"Interface" /
        L"AddOns";

    tp::AppState stagedState =
        g_state;

    std::vector<tp::GitAddonAdoptionPlan>
        plans;

    std::vector<std::wstring>
        refusals;

    std::vector<std::wstring>
        alreadyManaged;

    std::error_code ec;

    for (std::filesystem::directory_iterator it(
             addOnsRoot,
             ec),
         end;
         !ec &&
         it != end;
         it.increment(ec)) {
        const auto status =
            it->symlink_status(ec);

        if (ec) {
            break;
        }

        if (!std::filesystem::is_directory(
                status) ||
            std::filesystem::is_symlink(
                status)) {
            continue;
        }

        const auto gitPath =
            it->path() /
            L".git";

        std::error_code gitError;
        if (!std::filesystem::exists(
                gitPath,
                gitError) ||
            gitError) {
            continue;
        }

        tp::GitAddonAdoptionPlan plan;
        std::wstring error;

        if (!tp::PlanGitAddonAdoption(
                g_root,
                it->path(),
                stagedState,
                plan,
                error)) {
            const std::wstring folderName =
                it->path()
                    .filename()
                    .wstring();

            if (error ==
                L"This repository is already managed by TocPilot.") {
                alreadyManaged.push_back(
                    folderName);
            } else {
                refusals.push_back(
                    folderName +
                    L": " +
                    error);
            }
            continue;
        }

        if (plan.existingPackageIndex) {
            const std::size_t packageIndex =
                *plan.existingPackageIndex;

            if (packageIndex >=
                stagedState.packages.size()) {
                refusals.push_back(
                    it->path()
                        .filename()
                        .wstring() +
                    L": matching TocPilot package changed during adoption scan.");
                continue;
            }

            stagedState.packages[
                packageIndex] =
                plan.package;
        } else {
            stagedState.packages.push_back(
                plan.package);
        }

        plans.push_back(
            std::move(plan));
    }

    if (ec) {
        tp::ShowExpandableDialog(
            hwnd,
            L"TocPilot - Adopt Git Addons",
            L"Git addon scan failed",
            L"TocPilot could not finish scanning Interface\\AddOns.",
            {},
            tp::DialogIcon::Error,
            tp::DialogButtons::Ok);
        return;
    }

    std::vector<std::wstring>
        folderDiagnostics;

    {
        std::vector<tp::AddonFolderInfo>
            folders;
        std::wstring scanError;

        if (tp::ScanAddonFolders(
                g_root,
                g_state,
                folders,
                scanError)) {
            for (const auto& folder :
                 folders) {
                if (folder.kind ==
                    tp::AddonFolderKind::ManagedAddon) {
                    continue;
                }

                if (folder.kind ==
                        tp::AddonFolderKind::UnmanagedAddon &&
                    folder.hasGitMetadata) {
                    continue;
                }

                std::wstring line =
                    folder.name +
                    L" — " +
                    tp::AddonFolderKindLabel(
                        folder.kind);

                if (folder.kind ==
                    tp::AddonFolderKind::GitContainer) {
                    line +=
                        L"; no root .toc";

                    if (folder.oneLevelAddonRoots.empty()) {
                        line +=
                            L"; one-level addon roots: none found locally";
                    } else {
                        line +=
                            L"; one-level addon roots: ";

                        for (std::size_t i = 0;
                             i <
                                folder.oneLevelAddonRoots.size();
                             ++i) {
                            if (i != 0) {
                                line +=
                                    L", ";
                            }

                            line +=
                                folder.oneLevelAddonRoots[i];
                        }
                    }
                } else if (
                    folder.kind ==
                    tp::AddonFolderKind::UnmanagedAddon) {
                    line +=
                        L"; root .toc present; no Git source";
                } else if (
                    folder.kind ==
                    tp::AddonFolderKind::NonAddon) {
                    line +=
                        L"; no root .toc";
                }

                folderDiagnostics.push_back(
                    std::move(line));
            }
        } else {
            folderDiagnostics.push_back(
                L"Folder diagnostics unavailable: " +
                scanError);
        }
    }

    auto appendEntries = [](
        std::wstring& details,
        std::wstring_view heading,
        const std::vector<std::wstring>& entries) {
        if (entries.empty()) {
            return;
        }

        if (!details.empty()) {
            details +=
                L"\r\n\r\n";
        }

        details +=
            heading;
        details +=
            L":";

        for (const auto& entry :
             entries) {
            details +=
                L"\r\n- " +
                entry;
        }
    };

    std::vector<std::wstring>
        planDetails;
    planDetails.reserve(
        plans.size());

    for (const auto& plan :
         plans) {
        const auto& package =
            plan.package;

        const std::wstring shortSha =
            package.installedRevision.substr(
                0,
                std::min<std::size_t>(
                    7,
                    package.installedRevision.size()));

        std::wstring line =
            package.name +
            L" <- " +
            package.repository +
            L" / " +
            package.ref +
            L" @ " +
            shortSha;

        if (plan.existingPackageIndex) {
            line +=
                L" (existing TocPilot record)";
        }

        planDetails.push_back(
            std::move(line));
    }

    std::wstring details;
    appendEntries(
        details,
        L"Will adopt",
        planDetails);
    appendEntries(
        details,
        L"Refused candidates",
        refusals);
    appendEntries(
        details,
        L"Already managed by TocPilot",
        alreadyManaged);
    appendEntries(
        details,
        L"Other Interface\\AddOns folders",
        folderDiagnostics);

    if (plans.empty()) {
        std::wstring content =
            L"Already managed: " +
            std::to_wstring(
                alreadyManaged.size()) +
            L".\r\nRefused: " +
            std::to_wstring(
                refusals.size()) +
            L".\r\nOther folders in details: " +
            std::to_wstring(
                folderDiagnostics.size()) +
            L".\r\n\r\n"
            L"TocPilot adopts only direct addon folders with a normal .git "
            L"directory, root-level .toc file, GitHub origin, attached branch "
            L"with matching upstream metadata, and no conflicting ownership.";

        tp::ShowExpandableDialog(
            hwnd,
            L"TocPilot - Adopt Git Addons",
            L"No new Git installs to adopt",
            content,
            details,
            tp::DialogIcon::Information,
            tp::DialogButtons::Ok);
        return;
    }

    std::wstring content =
        L"Ready to adopt: " +
        std::to_wstring(
            plans.size()) +
        L".\r\nAlready managed: " +
        std::to_wstring(
            alreadyManaged.size()) +
        L".\r\nRefused: " +
        std::to_wstring(
            refusals.size()) +
        L".\r\nOther folders in details: " +
        std::to_wstring(
            folderDiagnostics.size()) +
        L".\r\n\r\n"
        L"Adoption records the local branch SHA and addon-file ownership in "
        L"TocPilot.json. Existing addon files and .git metadata are not changed. "
        L"GitAddonsManager leaves no unique ownership marker, so eligible normal "
        L"Git clones are indistinguishable and are listed in the details.";

    const int response =
        tp::ShowExpandableDialog(
            hwnd,
            L"TocPilot - Adopt Git Addons",
            std::to_wstring(
                plans.size()) +
                L" Git install(s) ready to adopt",
            content,
            details,
            tp::DialogIcon::Information,
            tp::DialogButtons::YesNo,
            IDNO);

    if (response != IDYES) {
        return;
    }

    std::wstring error;
    if (!tp::SaveState(
            g_root,
            stagedState,
            error)) {
        SetIndicator(
            g_stateStatus,
            L"State: Save failed - " +
                error);

        tp::ShowExpandableDialog(
            hwnd,
            L"TocPilot - Adoption Save Failed",
            L"Adoption state could not be saved",
            error,
            {},
            tp::DialogIcon::Error,
            tp::DialogButtons::Ok);
        return;
    }

    g_state =
        std::move(stagedState);
    g_stateCreated = false;
    g_stateError.clear();

    RefreshPackageStateUi();
    UpdatePackageButtons();

    const std::wstring message =
        L"Adopted " +
        std::to_wstring(
            plans.size()) +
        L" Git-managed addon install(s). No live addon files were changed.";

    if (g_packageHint) {
        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    tp::ShowExpandableDialog(
        hwnd,
        L"TocPilot - Adoption Complete",
        L"Git installs adopted",
        message,
        {},
        tp::DialogIcon::Information,
        tp::DialogButtons::Ok);
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
    if (g_updateAllInProgress ||
        g_packageRefreshInProgress ||
        g_packageInspectInProgress ||
        g_packageInstallInProgress) {
        MessageBoxW(
            hwnd,
            L"Finish the active package operation before replacing TocPilot itself.",
            L"TocPilot - Package Operation Active",
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

        g_uninstallPackageButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Uninstall",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            490,
            145,
            80,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_UNINSTALL_PACKAGE)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_addPackageButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Add Package",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            575,
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
            680,
            145,
            65,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REMOVE_PACKAGE)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_adoptGitButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Adopt Git",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            750,
            145,
            85,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ADOPT_GIT)),
            GetModuleHandleW(nullptr),
            nullptr);

        EnableWindow(g_updateAllButton, FALSE);
        EnableWindow(g_refreshPackagesButton, FALSE);
        EnableWindow(g_setBranchButton, FALSE);
        EnableWindow(g_inspectPackageButton, FALSE);
        EnableWindow(g_installPackageButton, FALSE);
        EnableWindow(g_uninstallPackageButton, FALSE);
        EnableWindow(g_removePackageButton, FALSE);
        EnableWindow(
            g_addPackageButton,
            g_stateReady ? TRUE : FALSE);
        EnableWindow(
            g_adoptGitButton,
            g_stateReady ? TRUE : FALSE);

        g_updateButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Check app update",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            750,
            145,
            100,
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

        StartAutoStatusRefresh(hwnd);

        return 0;
    }

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 1100;
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
            header->idFrom ==
                IDC_PACKAGE_LIST) {
            if (header->code ==
                LVN_ITEMCHANGED) {
                UpdatePackageButtons();
                return 0;
            }

            if (header->code ==
                LVN_COLUMNCLICK) {
                const auto* list =
                    reinterpret_cast<NMLISTVIEW*>(
                        lParam);

                SortPackageListByColumn(
                    list->iSubItem);
                UpdatePackageButtons();
                return 0;
            }
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

        if (LOWORD(wParam) == IDC_UPDATE_ALL &&
            HIWORD(wParam) == BN_CLICKED) {
            StartUpdateAll(hwnd);
            return 0;
        }

        if (LOWORD(wParam) == IDC_ADOPT_GIT &&
            HIWORD(wParam) == BN_CLICKED) {
            AdoptGitAddons(hwnd);
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

        if (LOWORD(wParam) == IDC_UNINSTALL_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            const int row = SelectedPackageRow();

            if (row >= 0 &&
                row < static_cast<int>(
                    g_state.packages.size())) {
                UninstallPackage(
                    hwnd,
                    static_cast<std::size_t>(row));
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

        const bool updateAllStep =
            IsUpdateAllCurrentPackage(
                result->packageId);

        const bool autoStatusStep =
            IsAutoStatusCurrentPackage(
                result->packageId);

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

            if (updateAllStep) {
                CompleteUpdateAllStep(
                    hwnd,
                    tp::UpdateAllOutcome::Failed,
                    result->packageId +
                        L": package tracking changed during refresh.");
            } else if (autoStatusStep) {
                CompleteAutoStatusRefreshStep(
                    hwnd,
                    false,
                    false);
            } else {
                UpdatePackageButtons();
            }
            return 0;
        }

        const auto index =
            result->index;

        const std::wstring packageName =
            g_state.packages[index].name;

        if (!result->ok) {
            SetPackageRowStatus(
                index,
                L"Refresh failed");

            const std::wstring message =
                packageName +
                L": refresh failed - " +
                result->error;

            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            if (updateAllStep) {
                CompleteUpdateAllStep(
                    hwnd,
                    tp::UpdateAllOutcome::Failed,
                    message,
                    IsGitHubRateLimitError(
                        result->error));
            } else if (autoStatusStep) {
                if (IsGitHubRateLimitError(
                        result->error)) {
                    ++g_autoStatusFailed;
                    g_autoStatusRateLimited =
                        true;
                    g_autoStatusPosition =
                        g_autoStatusPackageIds.size();
                    FinishAutoStatusRefresh(hwnd);
                } else {
                    CompleteAutoStatusRefreshStep(
                        hwnd,
                        false,
                        false);
                }
            } else {
                SelectPackageRow(index);
                UpdatePackageButtons();
            }
            return 0;
        }

        tp::AppState updatedState =
            g_state;
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

            const std::wstring message =
                packageName +
                L": refresh result could not be saved - " +
                error;

            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            if (updateAllStep) {
                CompleteUpdateAllStep(
                    hwnd,
                    tp::UpdateAllOutcome::Failed,
                    message);
            } else if (autoStatusStep) {
                CompleteAutoStatusRefreshStep(
                    hwnd,
                    false,
                    false);
            } else {
                SelectPackageRow(index);
                UpdatePackageButtons();
            }
            return 0;
        }

        g_state =
            std::move(updatedState);
        g_stateCreated = false;
        g_stateError.clear();

        tp::RecordPackageRefresh(
            g_packageRefreshStamps,
            g_state.packages[index],
            GetTickCount64());

        const auto& package =
            g_state.packages[index];

        const bool updateAvailable =
            !package.installedRevision.empty() &&
            package.installedRevision !=
                package.latestRevision;

        if (updateAllStep) {
            if (updateAvailable) {
                SetPackageRowStatus(
                    index,
                    L"Update available");
                StartPackageInstall(
                    hwnd,
                    index,
                    package.latestRevision);
                return 0;
            }

            SetPackageRowStatus(
                index,
                L"Current");
            CompleteUpdateAllStep(
                hwnd,
                tp::UpdateAllOutcome::Current,
                {});
            return 0;
        }

        if (autoStatusStep) {
            SetPackageRowStatus(
                index,
                updateAvailable
                    ? L"Update available"
                    : L"Current");
            CompleteAutoStatusRefreshStep(
                hwnd,
                true,
                updateAvailable);
            return 0;
        }

        RefreshPackageStateUi();
        SelectPackageRow(index);
        UpdatePackageButtons();

        if (g_packageHint) {
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

        const bool updateAllStep =
            IsUpdateAllCurrentPackage(
                result->packageId);

        if (result->index >=
                g_state.packages.size() ||
            g_state.packages[result->index].id !=
                result->packageId ||
            g_state.packages[result->index].ref !=
                result->branch) {
            const std::wstring message =
                result->packageId +
                L": install result was discarded because package tracking changed.";

            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    L"Install result was discarded because package tracking changed. Live addons were not committed.");
            }

            if (updateAllStep) {
                CompleteUpdateAllStep(
                    hwnd,
                    tp::UpdateAllOutcome::Failed,
                    message);
            } else {
                UpdatePackageButtons();
            }
            return 0;
        }

        const auto index = result->index;
        const std::wstring packageName =
            g_state.packages[index].name;

        if (!result->ok) {
            SetPackageRowStatus(
                index,
                L"Install failed");

            const std::wstring message =
                packageName +
                L": install preparation failed - " +
                result->error;

            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            if (updateAllStep) {
                CompleteUpdateAllStep(
                    hwnd,
                    tp::UpdateAllOutcome::Failed,
                    message);
            } else {
                const std::wstring dialog =
                    message +
                    L"\r\n\r\nNo live addon files were changed.";

                MessageBoxW(
                    hwnd,
                    dialog.c_str(),
                    L"TocPilot - Install Failed",
                    MB_OK | MB_ICONWARNING);

                SelectPackageRow(index);
                UpdatePackageButtons();
            }
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

            const std::wstring message =
                packageName +
                L": live commit failed and was rolled back - " +
                error;

            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            if (updateAllStep) {
                const bool rollbackFailed =
                    error.find(
                        L"Rollback also failed") !=
                    std::wstring::npos;

                CompleteUpdateAllStep(
                    hwnd,
                    tp::UpdateAllOutcome::Failed,
                    message,
                    rollbackFailed);
            } else {
                MessageBoxW(
                    hwnd,
                    message.c_str(),
                    L"TocPilot - Install Failed",
                    MB_OK | MB_ICONERROR);

                SelectPackageRow(index);
                UpdatePackageButtons();
            }
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

            if (g_packageHint) {
                SetWindowTextW(
                    g_packageHint,
                    message.c_str());
            }

            if (updateAllStep) {
                CompleteUpdateAllStep(
                    hwnd,
                    tp::UpdateAllOutcome::Failed,
                    message,
                    !rolledBack);
            } else {
                std::wstring dialog =
                    packageName +
                    L": package state could not be saved - " +
                    saveError;

                if (rolledBack) {
                    dialog +=
                        L"\r\n\r\nThe previous live addon state was restored.";
                } else {
                    dialog +=
                        L"\r\n\r\nFilesystem rollback also failed: " +
                        rollbackError;
                }

                MessageBoxW(
                    hwnd,
                    dialog.c_str(),
                    rolledBack
                        ? L"TocPilot - Install Rolled Back"
                        : L"TocPilot - Rollback Failed",
                    MB_OK |
                        (rolledBack
                            ? MB_ICONWARNING
                            : MB_ICONERROR));

                SelectPackageRow(index);
                UpdatePackageButtons();
            }
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

        if (updateAllStep) {
            SetPackageRowStatus(
                index,
                L"Current");
            CompleteUpdateAllStep(
                hwnd,
                tp::UpdateAllOutcome::Updated,
                {});
            return 0;
        }

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
        if (g_updateAllInProgress ||
            g_packageInstallInProgress) {
            MessageBoxW(
                hwnd,
                L"A package operation is currently active. TocPilot will stay open until it reaches a safe completion point.",
                L"TocPilot - Package Operation Active",
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

    if (g_stateReady) {
        g_packageSortColumn =
            g_state.settings.packageSortColumn;
        g_packageSortAscending =
            g_state.settings.packageSortAscending;
    }

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
        1100,
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
