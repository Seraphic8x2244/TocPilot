#include "addon_scan.h"
#include "adoption.h"
#include "add_package_dialog.h"
#include "archive.h"
#include "branch_dialog.h"
#include "direct_dll.h"
#include "github_api.h"
#include "git_refs.h"
#include "gitlab_api.h"
#include "install.h"
#include "refresh_freshness.h"
#include "state.h"
#include "splash.h"
#include "update.h"
#include "update_all.h"
#include "ui_dialog.h"
#include "version.h"
#include "resource.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace {

constexpr wchar_t kWindowClassBase[] = L"TocPilotMainWindow";
constexpr wchar_t kTocPilotWindowClass[] = L"TocPilotToolWindow";
constexpr wchar_t kTocPilotGitHubUrl[] =
    L"https://github.com/Seraphic8x2244/TocPilot";
constexpr wchar_t kTocPilotReleasesUrl[] =
    L"https://github.com/Seraphic8x2244/TocPilot/releases";
constexpr UINT WM_TP_CHECK_COMPLETE = WM_APP + 1;
constexpr UINT WM_TP_UPDATE_COMPLETE = WM_APP + 2;
constexpr UINT WM_TP_PACKAGE_REFRESH_COMPLETE = WM_APP + 3;
constexpr UINT WM_TP_PACKAGE_INSPECT_COMPLETE = WM_APP + 4;
constexpr UINT WM_TP_PACKAGE_INSTALL_COMPLETE = WM_APP + 5;
constexpr UINT WM_TP_BRANCHES_READY = WM_APP + 6;
constexpr UINT WM_TP_POSITION_BRANCH_SELECTOR = WM_APP + 7;
constexpr UINT WM_TP_DLL_INSTALL_COMPLETE = WM_APP + 8;

constexpr int IDC_WOW_STATUS = 1002;
constexpr int IDC_GITHUB_STATUS = 1003;
constexpr int IDC_RELEASE_STATUS = 1004;
constexpr int IDC_STATE_STATUS = 1005;
constexpr int IDC_PACKAGE_LIST = 1006;
constexpr int IDC_TEXT_SCALE = 1007;
constexpr int IDC_UPDATE_ALL = 1008;
constexpr int IDC_REFRESH_PACKAGES = 1009;
constexpr int IDC_ADD_PACKAGE = 1010;
constexpr int IDC_INSPECT_PACKAGE = 1012;
constexpr int IDC_INSTALL_PACKAGE = 1013;
constexpr int IDC_REMOVE_PACKAGE = 1014;
constexpr int IDC_UNINSTALL_PACKAGE = 1015;
constexpr int IDC_ADOPT_GIT = 1016;
constexpr int IDC_ADVANCED = 1017;
constexpr int IDC_LAUNCH_WOW = 1018;
constexpr int IDC_LAUNCH_VANILLAFIXES = 1019;
constexpr int IDC_TOCPILOT = 1020;
constexpr int IDC_TOCPILOT_UPDATE = 1021;
constexpr int IDC_TOCPILOT_GITHUB = 1022;
constexpr int IDC_TOCPILOT_RELEASES = 1023;
constexpr int IDC_BRANCH_SELECTOR = 1024;

constexpr int kCompactWindowWidth = 590;
constexpr int kDefaultWindowHeight = 480;
constexpr int kCompactButtonMinWidth = 100;
constexpr int kAdvancedButtonMinWidth = 150;
constexpr int kToolbarButtonGap = 6;
constexpr int kCompactPrimaryButtonCount = 4;
constexpr int kAdvancedButtonCount = 8;
constexpr int kCompactNameColumnMinWidth = 220;
constexpr int kCompactStatusColumnMinWidth = 150;
constexpr int kAdvancedNameColumnMinWidth = 240;
constexpr int kAdvancedBranchColumnMinWidth = 300;
constexpr int kAdvancedInstalledColumnMinWidth = 115;
constexpr int kAdvancedLatestColumnMinWidth = 115;
constexpr int kAdvancedStatusColumnMinWidth = 150;

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
HWND g_inspectPackageButton = nullptr;
HWND g_installPackageButton = nullptr;
HWND g_uninstallPackageButton = nullptr;
HWND g_removePackageButton = nullptr;
HWND g_addPackageButton = nullptr;
HWND g_adoptGitButton = nullptr;
HWND g_advancedButton = nullptr;
HWND g_tocPilotButton = nullptr;
HWND g_tocPilotWindow = nullptr;
HWND g_tocPilotUpdateStatus = nullptr;
HWND g_launchWowButton = nullptr;
HWND g_launchVanillaFixesButton = nullptr;
HWND g_packageList = nullptr;
HWND g_branchSelector = nullptr;
HWND g_packageHint = nullptr;
HWND g_textScaleLabel = nullptr;
HWND g_textScaleCombo = nullptr;
HWND g_rootLabel = nullptr;

HFONT g_uiFont = nullptr;
HFONT g_boldUiFont = nullptr;
HICON g_wowIcon = nullptr;
HICON g_vanillaFixesIcon = nullptr;

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
bool g_appUpdateCheckInProgress = false;
bool g_refreshAddonsAfterAppCheck = false;
bool g_appUpdateStartedFromStartup = false;
bool g_startupAppUpdateFailed = false;
bool g_autoStatusRefreshInProgress = false;
bool g_advancedVisible = false;
bool g_hasVanillaFixes = false;
bool g_branchSelectorUpdating = false;
bool g_branchSelectorLoadInProgress = false;
bool g_branchSelectorLoaded = false;
bool g_branchSelectorLoadFailed = false;
std::wstring g_branchSelectorOpenPackageId;
std::uint64_t g_branchSelectorGeneration = 0;
std::wstring g_branchSelectorPackageId;
std::wstring g_branchSelectorDefaultBranch;
std::vector<tp::GitRemoteBranch> g_branchSelectorBranches;
tp::UpdateAllProgress g_updateAllProgress;
std::vector<tp::PackageRefreshStamp> g_packageRefreshStamps;
std::vector<std::wstring> g_autoStatusPackageIds;
std::vector<std::wstring> g_packageAttentionIds;
std::size_t g_autoStatusPosition = 0;
std::size_t g_autoStatusCurrent = 0;
std::size_t g_autoStatusUpdates = 0;
std::size_t g_autoStatusFailed = 0;
bool g_autoStatusRateLimited = false;
std::vector<std::size_t> g_packageViewOrder;
int g_packageSortColumn = -1;
bool g_packageSortAscending = true;
std::wstring g_stateError;
std::wstring g_windowClassName;

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
    bool needsAttention = false;
    std::size_t index = 0;
    std::wstring packageId;
    std::wstring branch;
    std::wstring asset;
    std::wstring remoteSha;
    std::wstring error;
};

struct BranchLoadResult {
    bool ok = false;
    std::uint64_t generation = 0;
    std::wstring packageId;
    tp::GitRemoteRepositoryInfo info;
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

struct DirectDllInstallResult {
    bool ok = false;
    bool needsAttention = false;
    std::size_t index = 0;
    std::wstring packageId;
    std::wstring packageName;
    std::wstring asset;
    std::wstring targetPath;
    std::wstring resolvedTag;
    std::uint64_t downloadedBytes = 0;
    std::wstring actualSha256;
    std::wstring error;
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
        L" package record(s). The Advanced branch dropdown changes tracking; Refresh All checks "
        L"the saved branch head; Inspect previews without live changes; "
        L"Install/Update and Uninstall use staged rollback transactions; "
        L"Update New applies only installed packages already marked Update available; "
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

    HFONT boldFont = CreateFontW(
        height,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    EnumChildWindows(
        hwnd,
        ApplyFontToChild,
        reinterpret_cast<LPARAM>(font));

    if (g_uiFont) {
        DeleteObject(g_uiFont);
    }
    if (g_boldUiFont) {
        DeleteObject(g_boldUiFont);
    }
    g_uiFont = font;
    g_boldUiFont = boldFont;

    if (g_tocPilotWindow &&
        IsWindow(g_tocPilotWindow)) {
        SendMessageW(
            g_tocPilotWindow,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                g_uiFont),
            TRUE);
        EnumChildWindows(
            g_tocPilotWindow,
            ApplyFontToChild,
            reinterpret_cast<LPARAM>(
                g_uiFont));
    }
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

int RequiredClientWidth(
    bool advanced) {
    const int buttonCount =
        advanced
            ? kAdvancedButtonCount
            : kCompactPrimaryButtonCount;
    const int buttonMinWidth =
        advanced
            ? kAdvancedButtonMinWidth
            : kCompactButtonMinWidth;

    const int toolbarWidth =
        buttonCount *
            buttonMinWidth +
        (buttonCount - 1) *
            kToolbarButtonGap;

    const int columnsWidth =
        advanced
            ? kAdvancedNameColumnMinWidth +
                kAdvancedBranchColumnMinWidth +
                kAdvancedInstalledColumnMinWidth +
                kAdvancedLatestColumnMinWidth +
                kAdvancedStatusColumnMinWidth
            : kCompactNameColumnMinWidth +
                kCompactStatusColumnMinWidth;

    return
        40 +
        std::max(
            toolbarWidth,
            columnsWidth);
}

int WindowWidthForClient(
    HWND hwnd,
    int clientWidth) {
    RECT rect{
        0,
        0,
        clientWidth,
        400};

    AdjustWindowRectEx(
        &rect,
        static_cast<DWORD>(
            GetWindowLongPtrW(
                hwnd,
                GWL_STYLE)),
        FALSE,
        static_cast<DWORD>(
            GetWindowLongPtrW(
                hwnd,
                GWL_EXSTYLE)));

    return
        static_cast<int>(
            rect.right -
            rect.left);
}

void PositionBranchSelector();

void ResizeListColumns() {
    if (!g_packageList) {
        return;
    }

    RECT rect{};
    GetClientRect(g_packageList, &rect);
    const int width =
        std::max(
            320,
            static_cast<int>(
                rect.right - rect.left - 4));

    if (!g_advancedVisible) {
        const int statusWidth = 150;
        const int nameWidth =
            std::max(
                170,
                width - statusWidth);

        ListView_SetColumnWidth(
            g_packageList,
            0,
            nameWidth);
        ListView_SetColumnWidth(
            g_packageList,
            1,
            0);
        ListView_SetColumnWidth(
            g_packageList,
            2,
            0);
        ListView_SetColumnWidth(
            g_packageList,
            3,
            0);
        ListView_SetColumnWidth(
            g_packageList,
            4,
            statusWidth);
        return;
    }

    const int nameWidth = 240;
    const int branchWidth = 300;
    const int installedWidth = 115;
    const int latestWidth = 115;
    const int statusWidth =
        std::max(
            150,
            width -
                nameWidth -
                branchWidth -
                installedWidth -
                latestWidth);

    ListView_SetColumnWidth(
        g_packageList,
        0,
        nameWidth);
    ListView_SetColumnWidth(
        g_packageList,
        1,
        branchWidth);
    ListView_SetColumnWidth(
        g_packageList,
        2,
        installedWidth);
    ListView_SetColumnWidth(
        g_packageList,
        3,
        latestWidth);
    ListView_SetColumnWidth(
        g_packageList,
        4,
        statusWidth);
}

void LayoutControls(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width =
        client.right - client.left;
    const int height =
        client.bottom - client.top;
    const int contentWidth =
        std::max(
            320,
            width - 40);

    if (g_rootLabel) {
        ShowWindow(g_rootLabel, SW_HIDE);
    }
    if (g_wowStatus) {
        ShowWindow(g_wowStatus, SW_HIDE);
    }
    if (g_githubStatus) {
        ShowWindow(g_githubStatus, SW_HIDE);
    }
    if (g_releaseStatus) {
        ShowWindow(g_releaseStatus, SW_HIDE);
    }
    if (g_stateStatus) {
        ShowWindow(g_stateStatus, SW_HIDE);
    }
    if (g_packageHint) {
        ShowWindow(g_packageHint, SW_HIDE);
    }
    if (g_inspectPackageButton) {
        ShowWindow(g_inspectPackageButton, SW_HIDE);
    }
    if (g_uninstallPackageButton) {
        ShowWindow(g_uninstallPackageButton, SW_HIDE);
    }
    if (g_textScaleLabel) {
        ShowWindow(g_textScaleLabel, SW_HIDE);
    }
    if (g_textScaleCombo) {
        ShowWindow(g_textScaleCombo, SW_HIDE);
    }
    const int buttonY = 16;

    if (!g_advancedVisible) {
        if (g_adoptGitButton) {
            ShowWindow(g_adoptGitButton, SW_HIDE);
        }
        if (g_refreshPackagesButton) {
            ShowWindow(g_refreshPackagesButton, SW_HIDE);
        }
        if (g_installPackageButton) {
            ShowWindow(g_installPackageButton, SW_HIDE);
        }
        if (g_tocPilotButton) {
            ShowWindow(g_tocPilotButton, SW_HIDE);
        }

        const int availableForButtons =
            contentWidth -
            kToolbarButtonGap *
                (kCompactPrimaryButtonCount - 1);
        const int buttonWidth =
            std::max(
                kCompactButtonMinWidth,
                availableForButtons /
                    kCompactPrimaryButtonCount);

        int x = 20;
        const auto placePrimary =
            [&](HWND control) {
                if (!control) {
                    return;
                }
                ShowWindow(control, SW_SHOW);
                MoveWindow(
                    control,
                    x,
                    buttonY,
                    buttonWidth,
                    32,
                    TRUE);
                x +=
                    buttonWidth +
                    kToolbarButtonGap;
            };

        placePrimary(g_updateAllButton);
        placePrimary(g_addPackageButton);
        placePrimary(g_removePackageButton);
        placePrimary(g_advancedButton);
    } else {
        const int availableForButtons =
            contentWidth -
            kToolbarButtonGap *
                (kAdvancedButtonCount - 1);
        const int buttonWidth =
            std::max(
                kAdvancedButtonMinWidth,
                availableForButtons /
                    kAdvancedButtonCount);

        int x = 20;
        const auto placeAdvanced =
            [&](HWND control) {
                if (!control) {
                    return;
                }

                ShowWindow(
                    control,
                    SW_SHOW);
                MoveWindow(
                    control,
                    x,
                    buttonY,
                    buttonWidth,
                    32,
                    TRUE);
                x +=
                    buttonWidth +
                    kToolbarButtonGap;
            };

        placeAdvanced(g_updateAllButton);
        placeAdvanced(g_addPackageButton);
        placeAdvanced(g_refreshPackagesButton);
        placeAdvanced(g_installPackageButton);
        placeAdvanced(g_removePackageButton);
        placeAdvanced(g_adoptGitButton);
        placeAdvanced(g_tocPilotButton);
        placeAdvanced(g_advancedButton);
    }

    const int listTop = 62;
    const int listBottomPadding = 68;
    const int listHeight =
        std::max(
            220,
            height -
                listTop -
                listBottomPadding);

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

    PositionBranchSelector();

    const int iconSize = 40;
    const int iconY =
        std::max(
            listTop + listHeight + 8,
            height - 54);

    if (g_launchWowButton) {
        ShowWindow(g_launchWowButton, SW_SHOW);
        MoveWindow(
            g_launchWowButton,
            width - 20 - iconSize,
            iconY,
            iconSize,
            iconSize,
            TRUE);
    }
}

void AddPackageListColumns() {
    struct ColumnSpec {
        const wchar_t* name;
        int width;
    };

    constexpr std::array<ColumnSpec, 5> columns{{
        {L"Name", 180},
        {L"Branch", 250},
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

bool SupportedBranchProvider(
    const std::wstring& provider) {
    return
        provider == L"github" ||
        provider == L"gitlab";
}

std::wstring BranchProviderHost(
    const std::wstring& provider) {
    if (provider == L"github") {
        return L"github.com";
    }
    if (provider == L"gitlab") {
        return L"gitlab.com";
    }
    return {};
}

bool ResolvePackageBranchHead(
    const tp::PackageRecord& package,
    std::wstring& remoteSha,
    std::wstring& error) {
    const std::wstring host =
        BranchProviderHost(
            package.provider);

    if (host.empty()) {
        error =
            L"Unsupported branch package provider.";
        return false;
    }

    return tp::ResolvePublicGitBranchHead(
        host,
        package.repository,
        package.ref,
        remoteSha,
        error);
}

bool ResetPackageStaging(
    const tp::PackageRecord& package,
    const std::filesystem::path& wowRoot,
    std::filesystem::path& stagingDirectory,
    std::wstring& error) {
    return tp::ResetProviderPackageStaging(
        wowRoot,
        package.provider,
        package.repository,
        stagingDirectory,
        error);
}

bool DownloadPackageBranchArchive(
    const tp::PackageRecord& package,
    std::wstring_view remoteSha,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    std::wstring& error) {
    if (package.provider == L"github") {
        return tp::DownloadGitHubArchive(
            package.repository,
            remoteSha,
            destination,
            downloadedBytes,
            error);
    }

    if (package.provider == L"gitlab") {
        return tp::DownloadGitLabArchive(
            package.repository,
            remoteSha,
            destination,
            downloadedBytes,
            error);
    }

    error =
        L"Unsupported branch package provider.";
    return false;
}

bool DetectPackageAddonCandidates(
    const tp::PackageRecord& package,
    const std::filesystem::path& extractedRoot,
    std::wstring_view existingInstallFolder,
    std::vector<tp::AddonCandidate>& candidates,
    std::wstring& error) {
    if (!tp::DetectRepositoryAddonCandidates(
            extractedRoot,
            ProviderLabel(package.provider),
            package.repository,
            existingInstallFolder,
            candidates,
            error)) {
        return false;
    }

    return tp::SelectRepositoryAddonCandidate(
        package.sourcePath,
        candidates,
        error);
}

bool CleanupPackageStaging(
    const tp::PackageRecord& package,
    const std::filesystem::path& wowRoot,
    std::wstring& error) {
    return tp::CleanupProviderPackageStaging(
        wowRoot,
        package.provider,
        package.repository,
        error);
}

bool PackageBranchMode(
    const tp::PackageRecord& package) {
    return
        SupportedBranchProvider(
            package.provider) &&
        package.mode == L"branch" &&
        !package.ref.empty();
}

std::wstring PackageSourceText(
    const tp::PackageRecord& package) {
    if (PackageBranchMode(package)) {
        return package.ref;
    }

    if (package.mode == L"release") {
        if (package.asset.empty()) {
            return L"Latest stable";
        }
        return
            L"Latest stable / " +
            package.asset;
    }

    if (SupportedBranchProvider(
            package.provider)) {
        return L"Choose branch";
    }

    return L"—";
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

bool PackageNeedsAttention(
    std::wstring_view packageId) {
    return std::find(
               g_packageAttentionIds.begin(),
               g_packageAttentionIds.end(),
               packageId) !=
        g_packageAttentionIds.end();
}

void SetPackageNeedsAttention(
    std::wstring_view packageId,
    bool needsAttention) {
    const auto it =
        std::find(
            g_packageAttentionIds.begin(),
            g_packageAttentionIds.end(),
            packageId);

    if (needsAttention) {
        if (it ==
            g_packageAttentionIds.end()) {
            g_packageAttentionIds.emplace_back(
                packageId);
        }
        return;
    }

    if (it !=
        g_packageAttentionIds.end()) {
        g_packageAttentionIds.erase(it);
    }
}

bool DirectDllResolutionNeedsAttention(
    std::wstring_view error) {
    return
        error.find(
            L"exact configured asset") !=
                std::wstring_view::npos ||
        error.find(
            L"more than one asset named") !=
                std::wstring_view::npos ||
        error.find(
            L"no published stable release") !=
                std::wstring_view::npos ||
        error.find(
            L"no usable SHA-256 digest") !=
                std::wstring_view::npos ||
        error.find(
            L"Checksum asset") !=
                std::wstring_view::npos ||
        error.find(
            L"checksum asset") !=
                std::wstring_view::npos;
}

std::wstring PackageStatusText(
    const tp::PackageRecord& package) {
    if (package.mode == L"release") {
        std::wstring error;
        if (!tp::ValidateDirectDllPackage(
                package,
                error) ||
            PackageNeedsAttention(
                package.id)) {
            return L"Needs attention";
        }

        if (package.installedRevision.empty()) {
            return L"Not installed";
        }

        std::filesystem::path target;
        if (!tp::DirectDllTargetPath(
                g_root,
                package,
                target,
                error)) {
            return L"Needs attention";
        }

        std::error_code ec;
        if (!std::filesystem::is_regular_file(
                target,
                ec) ||
            ec) {
            return L"Needs attention";
        }

        return package.installedRevision ==
                package.latestRevision
            ? L"Current"
            : L"Update available";
    }

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

std::wstring PackageBranchSuffix(
    const tp::PackageRecord& package) {
    if (!PackageBranchMode(package) ||
        package.ref.empty() ||
        CompareInsensitive(package.ref, L"main") == 0 ||
        CompareInsensitive(package.ref, L"master") == 0) {
        return {};
    }

    return
        L" (" +
        package.ref +
        L")";
}

LRESULT HandlePackageListCustomDraw(
    LPARAM lParam) {
    auto* draw =
        reinterpret_cast<NMLVCUSTOMDRAW*>(
            lParam);

    if (!draw) {
        return CDRF_DODEFAULT;
    }

    if (draw->nmcd.dwDrawStage ==
        CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }

    if (draw->nmcd.dwDrawStage ==
        CDDS_ITEMPREPAINT) {
        return CDRF_NOTIFYSUBITEMDRAW;
    }

    if (draw->nmcd.dwDrawStage !=
            (CDDS_ITEMPREPAINT |
             CDDS_SUBITEM) ||
        !g_packageList) {
        return CDRF_DODEFAULT;
    }

    const int displayRow =
        static_cast<int>(
            draw->nmcd.dwItemSpec);

    if (displayRow < 0 ||
        displayRow >=
            static_cast<int>(
                g_packageViewOrder.size())) {
        return CDRF_DODEFAULT;
    }

    const std::size_t packageIndex =
        g_packageViewOrder[
            static_cast<std::size_t>(
                displayRow)];

    if (packageIndex >=
        g_state.packages.size()) {
        return CDRF_DODEFAULT;
    }

    const auto& package =
        g_state.packages[
            packageIndex];

    const bool drawName =
        draw->iSubItem == 0;
    const bool drawBranch =
        g_advancedVisible &&
        draw->iSubItem == 1 &&
        package.provider ==
            L"github";

    if (!drawName &&
        !drawBranch) {
        return CDRF_DODEFAULT;
    }

    RECT rect{};

    if (drawName) {
        if (!ListView_GetItemRect(
                g_packageList,
                displayRow,
                &rect,
                LVIR_BOUNDS)) {
            return CDRF_DODEFAULT;
        }

        rect.right =
            rect.left +
            ListView_GetColumnWidth(
                g_packageList,
                0);
    } else {
        if (!ListView_GetSubItemRect(
                g_packageList,
                displayRow,
                1,
                LVIR_BOUNDS,
                &rect)) {
            return CDRF_DODEFAULT;
        }
    }

    const bool selected =
        (ListView_GetItemState(
             g_packageList,
             displayRow,
             LVIS_SELECTED) &
         LVIS_SELECTED) != 0;
    const bool focused =
        GetFocus() ==
        g_packageList;

    const int backgroundIndex =
        selected
            ? (focused
                ? COLOR_HIGHLIGHT
                : COLOR_BTNFACE)
            : COLOR_WINDOW;
    const int textIndex =
        selected && focused
            ? COLOR_HIGHLIGHTTEXT
            : COLOR_WINDOWTEXT;

    FillRect(
        draw->nmcd.hdc,
        &rect,
        GetSysColorBrush(
            backgroundIndex));

    SetBkMode(
        draw->nmcd.hdc,
        TRANSPARENT);
    SetTextColor(
        draw->nmcd.hdc,
        GetSysColor(textIndex));

    HFONT normal =
        g_uiFont
            ? g_uiFont
            : reinterpret_cast<HFONT>(
                GetStockObject(
                    DEFAULT_GUI_FONT));
    HFONT bold =
        g_boldUiFont
            ? g_boldUiFont
            : normal;

    if (drawBranch) {
        const HGDIOBJ oldFont =
            SelectObject(
                draw->nmcd.hdc,
                normal);

        RECT arrowRect =
            rect;
        const int arrowWidth =
            std::max(
                16,
                std::min(
                    GetSystemMetrics(
                        SM_CXVSCROLL),
                    static_cast<int>(
                        rect.right -
                        rect.left) /
                        3));

        arrowRect.left =
            std::max(
                arrowRect.left,
                arrowRect.right -
                    arrowWidth);

        const bool showArrow =
            !g_branchSelectorLoaded ||
            package.id !=
                g_branchSelectorPackageId ||
            g_branchSelectorBranches.size() > 1;

        if (showArrow) {
            DrawFrameControl(
                draw->nmcd.hdc,
                &arrowRect,
                DFC_SCROLL,
                DFCS_SCROLLCOMBOBOX);
        }

        RECT textRect =
            rect;
        textRect.left += 6;
        textRect.right =
            showArrow
                ? arrowRect.left - 4
                : rect.right - 5;

        const std::wstring branchText =
            PackageSourceText(
                package);

        DrawTextW(
            draw->nmcd.hdc,
            branchText.c_str(),
            -1,
            &textRect,
            DT_SINGLELINE |
                DT_VCENTER |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);

        SelectObject(
            draw->nmcd.hdc,
            oldFont);

        return CDRF_SKIPDEFAULT;
    }

    RECT textRect = rect;
    textRect.left += 8;
    textRect.right -= 5;

    const std::wstring suffix =
        PackageBranchSuffix(
            package);

    const HGDIOBJ oldFont =
        SelectObject(
            draw->nmcd.hdc,
            bold);

    if (suffix.empty()) {
        DrawTextW(
            draw->nmcd.hdc,
            package.name.c_str(),
            -1,
            &textRect,
            DT_SINGLELINE |
                DT_VCENTER |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);
    } else {
        SIZE baseSize{};
        GetTextExtentPoint32W(
            draw->nmcd.hdc,
            package.name.c_str(),
            static_cast<int>(
                package.name.size()),
            &baseSize);

        SelectObject(
            draw->nmcd.hdc,
            normal);

        SIZE suffixSize{};
        GetTextExtentPoint32W(
            draw->nmcd.hdc,
            suffix.c_str(),
            static_cast<int>(
                suffix.size()),
            &suffixSize);

        const int suffixWidth =
            std::min(
                static_cast<int>(
                    suffixSize.cx),
                std::max(
                    0,
                    static_cast<int>(
                        textRect.right -
                        textRect.left) /
                        2));

        RECT baseRect =
            textRect;
        baseRect.right =
            std::max(
                static_cast<LONG>(
                    baseRect.left),
                static_cast<LONG>(
                    textRect.right -
                    suffixWidth));

        SelectObject(
            draw->nmcd.hdc,
            bold);
        DrawTextW(
            draw->nmcd.hdc,
            package.name.c_str(),
            -1,
            &baseRect,
            DT_SINGLELINE |
                DT_VCENTER |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);

        const int suffixX =
            baseSize.cx <=
                    (baseRect.right -
                     baseRect.left)
                ? std::min(
                    textRect.right -
                        suffixWidth,
                    textRect.left +
                        baseSize.cx)
                : baseRect.right;

        RECT suffixRect =
            textRect;
        suffixRect.left =
            suffixX;

        SelectObject(
            draw->nmcd.hdc,
            normal);
        DrawTextW(
            draw->nmcd.hdc,
            suffix.c_str(),
            -1,
            &suffixRect,
            DT_SINGLELINE |
                DT_VCENTER |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);
    }

    SelectObject(
        draw->nmcd.hdc,
        oldFont);

    return CDRF_SKIPDEFAULT;
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
void RefreshPackageStateUi();
void UpdatePackageButtons();

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


bool PackageOperationBusy() {
    return
        g_packageRefreshInProgress ||
        g_packageInspectInProgress ||
        g_packageInstallInProgress ||
        g_updateAllInProgress ||
        g_autoStatusRefreshInProgress ||
        g_appUpdateInProgress;
}

void SetRoutinePackageFeedback(
    std::wstring_view message) {
    if (!g_packageHint) {
        return;
    }

    const std::wstring text(message);
    SetWindowTextW(
        g_packageHint,
        text.c_str());
}

void ScrollPackageListToTop() {
    if (!g_packageList) {
        return;
    }

    const int topRow =
        ListView_GetTopIndex(
            g_packageList);

    if (topRow <= 0) {
        return;
    }

    RECT first{};
    RECT currentTop{};

    if (ListView_GetItemRect(
            g_packageList,
            0,
            &first,
            LVIR_BOUNDS) &&
        ListView_GetItemRect(
            g_packageList,
            topRow,
            &currentTop,
            LVIR_BOUNDS)) {
        ListView_Scroll(
            g_packageList,
            0,
            first.top - currentTop.top);
    }
}

void PopulateBranchSelectorCurrent(
    const tp::PackageRecord& package) {
    if (!g_branchSelector) {
        return;
    }

    g_branchSelectorUpdating = true;

    SendMessageW(
        g_branchSelector,
        CB_RESETCONTENT,
        0,
        0);

    const std::wstring label =
        package.ref.empty()
            ? L"Choose branch"
            : package.ref;

    const LRESULT item =
        SendMessageW(
            g_branchSelector,
            CB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(
                label.c_str()));

    if (item >= 0) {
        SendMessageW(
            g_branchSelector,
            CB_SETITEMDATA,
            static_cast<WPARAM>(item),
            static_cast<LPARAM>(-1));
        SendMessageW(
            g_branchSelector,
            CB_SETCURSEL,
            static_cast<WPARAM>(item),
            0);
    }

    g_branchSelectorUpdating = false;
}

void PopulateBranchSelectorLoaded(
    const tp::PackageRecord& package) {
    if (!g_branchSelector) {
        return;
    }

    g_branchSelectorUpdating = true;

    SendMessageW(
        g_branchSelector,
        CB_RESETCONTENT,
        0,
        0);

    int selected = -1;

    for (std::size_t i = 0;
         i < g_branchSelectorBranches.size();
         ++i) {
        const auto& branch =
            g_branchSelectorBranches[i];

        const std::wstring label =
            branch.name;

        const LRESULT item =
            SendMessageW(
                g_branchSelector,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    label.c_str()));

        if (item < 0) {
            continue;
        }

        SendMessageW(
            g_branchSelector,
            CB_SETITEMDATA,
            static_cast<WPARAM>(item),
            static_cast<LPARAM>(i));

        if (branch.name ==
            package.ref) {
            selected =
                static_cast<int>(item);
        }
    }

    if (selected < 0 &&
        !package.ref.empty()) {
        const std::wstring missing =
            package.ref +
            L" (not found)";

        const LRESULT item =
            SendMessageW(
                g_branchSelector,
                CB_INSERTSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    missing.c_str()));

        if (item >= 0) {
            SendMessageW(
                g_branchSelector,
                CB_SETITEMDATA,
                static_cast<WPARAM>(item),
                static_cast<LPARAM>(-1));
            selected =
                static_cast<int>(item);
        }
    }

    if (selected < 0 &&
        !g_branchSelectorDefaultBranch.empty()) {
        for (int item = 0;
             item <
                static_cast<int>(
                    SendMessageW(
                        g_branchSelector,
                        CB_GETCOUNT,
                        0,
                        0));
             ++item) {
            const LRESULT data =
                SendMessageW(
                    g_branchSelector,
                    CB_GETITEMDATA,
                    static_cast<WPARAM>(item),
                    0);

            if (data == CB_ERR ||
                data < 0 ||
                static_cast<std::size_t>(data) >=
                    g_branchSelectorBranches.size()) {
                continue;
            }

            if (g_branchSelectorBranches[
                    static_cast<std::size_t>(
                        data)].name ==
                g_branchSelectorDefaultBranch) {
                selected = item;
                break;
            }
        }
    }

    if (selected < 0 &&
        SendMessageW(
            g_branchSelector,
            CB_GETCOUNT,
            0,
            0) > 0) {
        selected = 0;
    }

    SendMessageW(
        g_branchSelector,
        CB_SETCURSEL,
        static_cast<WPARAM>(selected),
        0);

    g_branchSelectorUpdating = false;
}

void PositionBranchSelector() {
    if (g_branchSelector) {
        ShowWindow(
            g_branchSelector,
            SW_HIDE);
    }
}

void StartBranchSelectorLoad(
    HWND hwnd,
    std::size_t index) {
    if (index >=
            g_state.packages.size() ||
        g_branchSelectorLoadInProgress) {
        return;
    }

    const auto package =
        g_state.packages[index];

    if (!SupportedBranchProvider(
            package.provider)) {
        return;
    }

    g_branchSelectorLoadInProgress =
        true;
    g_branchSelectorLoaded = false;
    g_branchSelectorLoadFailed = false;

    const std::uint64_t generation =
        ++g_branchSelectorGeneration;

    EnableWindow(
        g_branchSelector,
        FALSE);

    std::thread(
        [hwnd,
         package,
         generation]() {
            auto result =
                std::make_unique<
                    BranchLoadResult>();

            result->generation =
                generation;
            result->packageId =
                package.id;
            const std::wstring host =
                BranchProviderHost(
                    package.provider);

            if (host.empty()) {
                result->error =
                    L"Unsupported branch package provider.";
            } else {
                result->ok =
                    tp::FetchPublicGitRepositoryInfo(
                        host,
                        package.repository,
                        result->info,
                        result->error);
            }

            if (!PostMessageW(
                    hwnd,
                    WM_TP_BRANCHES_READY,
                    0,
                    reinterpret_cast<LPARAM>(
                        result.get()))) {
                return;
            }

            result.release();
        })
        .detach();
}

void UpdateBranchSelector(
    HWND hwnd) {
    if (!g_branchSelector ||
        !g_advancedVisible ||
        !g_stateReady) {
        if (g_branchSelector) {
            ShowWindow(
                g_branchSelector,
                SW_HIDE);
        }
        return;
    }

    const int selected =
        SelectedPackageRow();

    if (selected < 0 ||
        selected >=
            static_cast<int>(
                g_state.packages.size())) {
        ShowWindow(
            g_branchSelector,
            SW_HIDE);
        return;
    }

    const auto& package =
        g_state.packages[
            static_cast<std::size_t>(
                selected)];

    if (!SupportedBranchProvider(
            package.provider) ||
        package.mode == L"release") {
        ShowWindow(
            g_branchSelector,
            SW_HIDE);
        return;
    }

    if (g_branchSelectorPackageId !=
        package.id) {
        if (!g_branchSelectorOpenPackageId.empty() &&
            g_branchSelectorOpenPackageId !=
                package.id) {
            g_branchSelectorOpenPackageId.clear();
        }

        ++g_branchSelectorGeneration;
        g_branchSelectorPackageId =
            package.id;
        g_branchSelectorDefaultBranch.clear();
        g_branchSelectorBranches.clear();
        g_branchSelectorLoaded = false;
        g_branchSelectorLoadFailed = false;
        g_branchSelectorLoadInProgress =
            false;

        PopulateBranchSelectorCurrent(
            package);

        StartBranchSelectorLoad(
            hwnd,
            static_cast<std::size_t>(
                selected));
    }

    PositionBranchSelector();

    const bool canUse =
        !PackageOperationBusy() &&
        !g_branchSelectorLoadInProgress &&
        (g_branchSelectorLoaded ||
         g_branchSelectorLoadFailed);

    EnableWindow(
        g_branchSelector,
        canUse
            ? TRUE
            : FALSE);
}

void ApplyBranchChoice(
    HWND hwnd,
    std::size_t branchIndex);

void OpenBranchSelectorIfReady(
    HWND hwnd) {
    if (g_branchSelectorOpenPackageId.empty() ||
        !g_branchSelectorLoaded ||
        g_branchSelectorLoadInProgress ||
        PackageOperationBusy()) {
        return;
    }

    const int selected =
        SelectedPackageRow();

    if (selected < 0 ||
        selected >=
            static_cast<int>(
                g_state.packages.size())) {
        return;
    }

    const auto& package =
        g_state.packages[
            static_cast<std::size_t>(
                selected)];

    if (package.id !=
            g_branchSelectorOpenPackageId ||
        package.id !=
            g_branchSelectorPackageId) {
        return;
    }

    g_branchSelectorOpenPackageId.clear();

    if (g_branchSelectorBranches.size() <= 1) {
        if (g_packageHint) {
            const std::wstring message =
                package.name +
                L" has no alternative branches.";
            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        if (g_packageList) {
            InvalidateRect(
                g_packageList,
                nullptr,
                FALSE);
        }
        return;
    }

    const int displayRow =
        PackageDisplayRow(
            static_cast<std::size_t>(
                selected));

    if (displayRow < 0 ||
        !g_packageList) {
        return;
    }

    RECT cell{};
    if (!ListView_GetSubItemRect(
            g_packageList,
            displayRow,
            1,
            LVIR_BOUNDS,
            &cell)) {
        return;
    }

    POINT anchor{
        cell.left,
        cell.bottom
    };

    MapWindowPoints(
        g_packageList,
        nullptr,
        &anchor,
        1);

    HMENU menu =
        CreatePopupMenu();

    if (!menu) {
        return;
    }

    for (std::size_t i = 0;
         i < g_branchSelectorBranches.size();
         ++i) {
        const auto& branch =
            g_branchSelectorBranches[i];

        UINT flags =
            MF_STRING;

        if (branch.name == package.ref) {
            flags |= MF_CHECKED;
        }

        AppendMenuW(
            menu,
            flags,
            static_cast<UINT_PTR>(i + 1),
            branch.name.c_str());
    }

    const UINT command =
        TrackPopupMenu(
            menu,
            TPM_RETURNCMD |
                TPM_NONOTIFY |
                TPM_RIGHTBUTTON |
                TPM_LEFTALIGN |
                TPM_TOPALIGN,
            anchor.x,
            anchor.y,
            0,
            hwnd,
            nullptr);

    DestroyMenu(menu);

    if (command > 0 &&
        command <=
            g_branchSelectorBranches.size()) {
        ApplyBranchChoice(
            hwnd,
            static_cast<std::size_t>(
                command - 1));
    }
}

void RequestBranchSelector(
    HWND hwnd,
    int displayRow) {
    if (!g_advancedVisible ||
        !g_packageList ||
        displayRow < 0 ||
        displayRow >=
            static_cast<int>(
                g_packageViewOrder.size())) {
        return;
    }

    const std::size_t packageIndex =
        g_packageViewOrder[
            static_cast<std::size_t>(
                displayRow)];

    if (packageIndex >=
            g_state.packages.size() ||
        g_state.packages[
            packageIndex].provider !=
            L"github" ||
        g_state.packages[
            packageIndex].mode ==
            L"release") {
        return;
    }

    const std::wstring packageId =
        g_state.packages[
            packageIndex].id;

    g_branchSelectorOpenPackageId =
        packageId;

    ListView_SetItemState(
        g_packageList,
        -1,
        0,
        LVIS_SELECTED |
            LVIS_FOCUSED);
    ListView_SetItemState(
        g_packageList,
        displayRow,
        LVIS_SELECTED |
            LVIS_FOCUSED,
        LVIS_SELECTED |
            LVIS_FOCUSED);

    UpdatePackageButtons();

    if (g_branchSelectorPackageId ==
            packageId &&
        g_branchSelectorLoadFailed &&
        !g_branchSelectorLoadInProgress) {
        g_branchSelectorLoadFailed =
            false;
        StartBranchSelectorLoad(
            hwnd,
            packageIndex);
    }

    OpenBranchSelectorIfReady(
        hwnd);
}

void ApplyBranchChoice(
    HWND hwnd,
    std::size_t branchIndex) {
    g_branchSelectorOpenPackageId.clear();

    if (!g_branchSelectorLoaded ||
        branchIndex >=
            g_branchSelectorBranches.size()) {
        return;
    }

    const int selectedPackage =
        SelectedPackageRow();

    if (selectedPackage < 0 ||
        selectedPackage >=
            static_cast<int>(
                g_state.packages.size())) {
        return;
    }

    const std::size_t packageIndex =
        static_cast<std::size_t>(
            selectedPackage);

    if (g_state.packages[
            packageIndex].id !=
        g_branchSelectorPackageId) {
        return;
    }

    const auto& branch =
        g_branchSelectorBranches[
            branchIndex];

    if (branch.name ==
        g_state.packages[
            packageIndex].ref) {
        return;
    }

    tp::AppState updatedState =
        g_state;
    std::wstring error;

    if (!tp::SetPackageBranch(
            updatedState.packages[
                packageIndex],
            branch.name,
            branch.sha,
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
            L"TocPilot - Change Branch",
            MB_OK | MB_ICONERROR);

        PopulateBranchSelectorLoaded(
            g_state.packages[
                packageIndex]);
        return;
    }

    g_state =
        std::move(updatedState);
    g_stateCreated = false;
    g_stateError.clear();

    tp::RecordPackageRefresh(
        g_packageRefreshStamps,
        g_state.packages[
            packageIndex],
        GetTickCount64());

    RefreshPackageStateUi();
    SelectPackageRow(
        packageIndex);
    PopulateBranchSelectorLoaded(
        g_state.packages[
            packageIndex]);
    UpdateBranchSelector(
        hwnd);
}

LRESULT CALLBACK PackageListSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR,
    DWORD_PTR) {
    const LRESULT result =
        DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam);

    if (message == WM_VSCROLL ||
        message == WM_HSCROLL ||
        message == WM_MOUSEWHEEL ||
        message == WM_MOUSEHWHEEL) {
        if (HWND parent =
                GetParent(hwnd)) {
            PostMessageW(
                parent,
                WM_TP_POSITION_BRANCH_SELECTOR,
                0,
                0);
        }
    }

    return result;
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
            SupportedBranchProvider(
                selectedPackage->provider) &&
            selectedPackage->mode != L"release";

        canRefresh =
            canSetBranch &&
            selectedPackage->mode == L"branch" &&
            !selectedPackage->ref.empty();

        canInspect = canRefresh;

        std::wstring directDllError;
        const bool directDll =
            tp::ValidateDirectDllPackage(
                *selectedPackage,
                directDllError);

        canInstall =
            canRefresh ||
            directDll;

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
        PackageOperationBusy();

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
            canUpdateAll && !packageBusy
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
        const wchar_t* label =
            L"Install Addon";

        if (selectedPackage &&
            selectedPackage->mode == L"release") {
            label =
                selectedPackage->installedRevision.empty()
                    ? L"Install DLL"
                    : L"Update DLL";
        } else if (
            selectedPackage &&
            !selectedPackage->installedRevision.empty()) {
            label =
                L"Reinstall Addon";
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

    if (g_packageList) {
        UpdateBranchSelector(
            GetParent(
                g_packageList));
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

    const bool branchPackage =
        SupportedBranchProvider(
            package.provider) &&
        package.mode == L"branch" &&
        !package.ref.empty();

    const bool directDll =
        tp::IsDirectDllPackage(
            package);

    if (!branchPackage &&
        !directDll) {
        return;
    }

    g_packageRefreshInProgress = true;
    UpdatePackageButtons();
    SetPackageRowStatus(
        index,
        L"Checking...");

    if (g_packageHint) {
        std::wstring message =
            package.name +
            L": checking ";

        if (directDll) {
            message +=
                L"the latest stable GitHub release for exact asset '" +
                package.asset +
                L"'. No managed files will be changed.";
        } else {
            message +=
                package.ref +
                L" on " +
                ProviderLabel(
                    package.provider) +
                L". No addon files will be changed.";
        }

        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    const auto wowRoot =
        g_root;

    std::thread(
        [hwnd,
         index,
         package,
         branchPackage,
         directDll,
         wowRoot]() {
            auto result =
                std::make_unique<
                    PackageRefreshResult>();

            result->index =
                index;
            result->packageId =
                package.id;
            result->branch =
                package.ref;
            result->asset =
                package.asset;

            if (branchPackage) {
                result->ok =
                    ResolvePackageBranchHead(
                        package,
                        result->remoteSha,
                        result->error);
            } else if (directDll) {
                std::wstring configError;
                if (!tp::ValidateDirectDllPackage(
                        package,
                        configError)) {
                    result->error =
                        std::move(configError);
                    result->needsAttention =
                        true;
                } else {
                    std::filesystem::path
                        target;

                    if (!tp::DirectDllTargetPath(
                            wowRoot,
                            package,
                            target,
                            result->error)) {
                        result->needsAttention =
                            true;
                    } else if (
                        !package.installedRevision.empty()) {
                        std::error_code ec;
                        const bool present =
                            std::filesystem::is_regular_file(
                                target,
                                ec);

                        if (!present ||
                            ec) {
                            result->error =
                                L"The managed DLL is missing from its exact WoW-root destination. It may have been removed or quarantined.";
                            result->needsAttention =
                                true;
                        }
                    }

                    if (result->error.empty()) {
                        tp::DirectDllRelease
                            release;

                        if (!tp::ResolveLatestDirectDllRelease(
                                package,
                                release,
                                result->error)) {
                            result->needsAttention =
                                DirectDllResolutionNeedsAttention(
                                    result->error);
                        } else {
                            result->remoteSha =
                                std::move(
                                    release.tag);
                            result->ok =
                                true;
                        }
                    }
                }
            }

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

    if (!SupportedBranchProvider(
            package.provider) ||
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

            if (!ResolvePackageBranchHead(
                    package,
                    result->remoteSha,
                    result->error)) {
                result->ok = false;
            } else if (!ResetPackageStaging(
                    package,
                    wowRoot,
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

                if (!DownloadPackageBranchArchive(
                        package,
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
                } else if (!DetectPackageAddonCandidates(
                        package,
                        result->inspection.extractedRoot,
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

void StartDirectDllInstall(
    HWND hwnd,
    std::size_t index,
    std::wstring knownReleaseTag) {
    if (index >=
        g_state.packages.size()) {
        return;
    }

    const auto package =
        g_state.packages[index];

    std::wstring configError;
    if (!tp::ValidateDirectDllPackage(
            package,
            configError)) {
        SetPackageNeedsAttention(
            package.id,
            true);
        SetPackageRowStatus(
            index,
            L"Needs attention");

        if (g_packageHint) {
            const std::wstring message =
                package.name +
                L": " +
                configError;
            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        UpdatePackageButtons();
        return;
    }

    g_packageInstallInProgress =
        true;
    UpdatePackageButtons();

    SetPackageRowStatus(
        index,
        package.installedRevision.empty()
            ? L"Installing DLL..."
            : L"Updating DLL...");

    if (g_packageHint) {
        const std::wstring message =
            package.name +
            L": resolving the exact latest-stable asset, then writing directly to " +
            (g_root /
             package.targetPath).wstring() +
            L". No staged/temp/renamed/backup DLL will be created.";

        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    const auto wowRoot =
        g_root;

    std::thread(
        [hwnd,
         index,
         package,
         knownReleaseTag =
             std::move(
                 knownReleaseTag),
         wowRoot]() mutable {
            auto result =
                std::make_unique<
                    DirectDllInstallResult>();

            result->index =
                index;
            result->packageId =
                package.id;
            result->packageName =
                package.name;
            result->asset =
                package.asset;
            result->targetPath =
                package.targetPath;

            tp::DirectDllRelease release;

            if (!tp::ResolveLatestDirectDllRelease(
                    package,
                    release,
                    result->error)) {
                result->needsAttention =
                    DirectDllResolutionNeedsAttention(
                        result->error);
            } else if (
                !knownReleaseTag.empty() &&
                release.tag !=
                    knownReleaseTag) {
                result->error =
                    L"The latest stable release changed from " +
                    knownReleaseTag +
                    L" to " +
                    release.tag +
                    L" after the saved status check. Run Refresh All and retry so Update New never installs a different release than the one it displayed.";
            } else {
                result->resolvedTag =
                    release.tag;

                result->ok =
                    tp::DownloadAndVerifyDirectDll(
                        package,
                        release,
                        wowRoot,
                        result->downloadedBytes,
                        result->actualSha256,
                        result->error);
            }

            if (!PostMessageW(
                    hwnd,
                    WM_TP_DLL_INSTALL_COMPLETE,
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

    if (tp::IsDirectDllPackage(
            package)) {
        StartDirectDllInstall(
            hwnd,
            index,
            std::move(
                knownRemoteSha));
        return;
    }

    if (!SupportedBranchProvider(
            package.provider) ||
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
                !ResolvePackageBranchHead(
                    package,
                    result->remoteSha,
                    result->error)) {
                result->ok = false;
            } else if (!ResetPackageStaging(
                    package,
                    wowRoot,
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

                if (!DownloadPackageBranchArchive(
                        package,
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
                } else if (!DetectPackageAddonCandidates(
                        package,
                        result->inspection.extractedRoot,
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

bool IsProviderRateLimitError(
    std::wstring_view error) {
    return
        error.find(L"rate-limited") !=
            std::wstring_view::npos ||
        error.find(L"rate limited") !=
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

void SetStartupSplashCompletionPhase() {
    if (!tp::IsStartupSplashActive()) {
        return;
    }

    tp::SetStartupSplashPhase(
        g_startupAppUpdateFailed
            ? tp::StartupSplashPhase::
                  AppUpdateFailed
            : tp::StartupSplashPhase::
                  AwaitingContinue);
}

void FinishAutoStatusRefresh(HWND hwnd) {
    g_autoStatusRefreshInProgress = false;

    if (g_refreshPackagesButton) {
        SetWindowTextW(
            g_refreshPackagesButton,
            L"Refresh All");
    }

    RefreshPackageStateUi();
    ScrollPackageListToTop();

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
            L" A package provider rate-limited the check; remaining packages were left at their saved status.";
    }

    if (g_startupAppUpdateFailed) {
        message +=
            L" TocPilot self-update failed; open TocPilot after continuing to retry.";
    }

    if (g_packageHint) {
        SetWindowTextW(
            g_packageHint,
            message.c_str());
    }

    UpdatePackageButtons();

    SetStartupSplashCompletionPhase();
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
                L"Checking package status " +
                std::to_wstring(
                    g_autoStatusPosition + 1) +
                L" of " +
                std::to_wstring(
                    g_autoStatusPackageIds.size()) +
                L": " +
                package.name +
                L". No managed files will be changed.";

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
    if (!g_stateReady) {
        SetStartupSplashCompletionPhase();
        return;
    }

    if (g_autoStatusRefreshInProgress ||
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
        SetStartupSplashCompletionPhase();
        return;
    }

    g_autoStatusPosition = 0;
    g_autoStatusCurrent = 0;
    g_autoStatusUpdates = 0;
    g_autoStatusFailed = 0;
    g_autoStatusRateLimited = false;
    g_autoStatusRefreshInProgress = true;

    if (g_refreshPackagesButton) {
        SetWindowTextW(
            g_refreshPackagesButton,
            L"Refreshing...");
    }

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

void FinishUpdateAll() {
    const std::size_t queued =
        g_updateAllProgress.packageIds.size();
    const std::size_t processed =
        g_updateAllProgress.current +
        g_updateAllProgress.updated +
        g_updateAllProgress.failed;

    g_updateAllInProgress = false;
    SetWindowTextW(
        g_updateAllButton,
        L"Update New");

    RefreshPackageStateUi();

    std::wstring summary =
        L"Update New finished: " +
        std::to_wstring(g_updateAllProgress.updated) +
        L" updated, " +
        std::to_wstring(g_updateAllProgress.current) +
        L" already current, " +
        std::to_wstring(g_updateAllProgress.failed) +
        L" failed.";

    if (processed < queued) {
        summary +=
            L" " +
            std::to_wstring(queued - processed) +
            L" remaining untouched.";
    }

    if (!g_updateAllProgress.failures.empty()) {
        summary +=
            L" Review failed rows for details.";
    }

    SetRoutinePackageFeedback(summary);
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
                    L": package is no longer eligible for Update New.",
                ignored);
            continue;
        }

        const auto& package =
            g_state.packages[index];

        SetPackageRowStatus(
            index,
            L"Updating...");

        const std::wstring message =
            L"Update New: applying the known update for " +
            package.name +
            L". Refresh All is responsible for discovering newer revisions.";
        SetRoutinePackageFeedback(message);

        StartPackageInstall(
            hwnd,
            index,
            package.latestRevision);
        return;
    }

    FinishUpdateAll();
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
            L"Update New");
        UpdatePackageButtons();

        MessageBoxW(
            hwnd,
            error.c_str(),
            L"TocPilot - Update New",
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
        SetRoutinePackageFeedback(
            L"There are no installed addons currently marked Update available. Run Refresh All to check for new revisions.");
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

void RemovePackage(
    HWND hwnd,
    std::size_t index) {
    if (index >= g_state.packages.size()) {
        return;
    }

    const auto package =
        g_state.packages[index];

    const bool hasInstalledFiles =
        package.target == L"addons" &&
        !package.installedRevision.empty() &&
        !package.installedFiles.empty();

    if (!hasInstalledFiles) {
        const std::wstring prompt =
            L"Remove " +
            package.name +
            L" from TocPilot?";

        if (MessageBoxW(
                hwnd,
                prompt.c_str(),
                L"TocPilot - Remove Addon",
                MB_YESNO |
                    MB_ICONWARNING |
                    MB_DEFBUTTON2) != IDYES) {
            return;
        }

        tp::AppState updatedState =
            g_state;
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
                L"TocPilot - Remove Addon",
                MB_OK | MB_ICONERROR);
            return;
        }

        g_state =
            std::move(updatedState);
        g_stateCreated = false;
        g_stateError.clear();
        g_packageViewOrder.clear();
        RefreshPackageStateUi();
        return;
    }

    std::vector<std::wstring>
        otherInstalledFiles;

    for (std::size_t i = 0;
         i < g_state.packages.size();
         ++i) {
        if (i == index ||
            g_state.packages[i].target !=
                L"addons") {
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
            L"TocPilot - Remove Addon",
            MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring prompt =
        L"Remove " +
        package.name +
        L"?\r\n\r\nTocPilot will remove " +
        std::to_wstring(
            plan.obsoleteInstallFolders.size()) +
        L" owned addon root(s) and delete this addon from TocPilot.";

    if (MessageBoxW(
            hwnd,
            prompt.c_str(),
            L"TocPilot - Remove Addon",
            MB_YESNO |
                MB_ICONWARNING |
                MB_DEFBUTTON2) != IDYES) {
        return;
    }

    g_packageInstallInProgress = true;
    UpdatePackageButtons();
    SetPackageRowStatus(
        index,
        L"Removing...");

    tp::AddonInstallTransaction transaction;
    if (!tp::BeginAddonInstallTransaction(
            plan,
            transaction,
            error)) {
        g_packageInstallInProgress = false;
        SetPackageRowStatus(
            index,
            L"Remove failed");
        UpdatePackageButtons();

        MessageBoxW(
            hwnd,
            error.c_str(),
            L"TocPilot - Remove Failed",
            MB_OK | MB_ICONERROR);
        return;
    }

    tp::AppState updatedState =
        g_state;

    if (!tp::RemovePackageRecord(
            updatedState,
            package.id,
            error) ||
        !tp::SaveState(
            g_root,
            updatedState,
            error)) {
        const std::wstring saveError =
            error;
        std::wstring rollbackError;

        const bool rolledBack =
            tp::RollbackAddonInstallTransaction(
                transaction,
                rollbackError);

        g_packageInstallInProgress = false;

        std::wstring message =
            L"TocPilot could not save the removal: " +
            saveError;

        if (rolledBack) {
            message +=
                L"\r\n\r\nThe addon files were restored.";
        } else {
            message +=
                L"\r\n\r\nFilesystem rollback also failed: " +
                rollbackError;
        }

        MessageBoxW(
            hwnd,
            message.c_str(),
            rolledBack
                ? L"TocPilot - Remove Rolled Back"
                : L"TocPilot - Rollback Failed",
            MB_OK |
                (rolledBack
                    ? MB_ICONWARNING
                    : MB_ICONERROR));

        UpdatePackageButtons();
        return;
    }

    g_state =
        std::move(updatedState);
    g_stateCreated = false;
    g_stateError.clear();
    g_packageViewOrder.clear();

    std::wstring cleanupError;
    const bool cleanupOk =
        tp::FinalizeAddonInstallTransaction(
            transaction,
            cleanupError);

    g_packageInstallInProgress = false;
    RefreshPackageStateUi();

    if (!cleanupOk) {
        const std::wstring message =
            package.name +
            L" was removed, but transaction backup cleanup failed:\r\n\r\n" +
            cleanupError;

        MessageBoxW(
            hwnd,
            message.c_str(),
            L"TocPilot - Remove Cleanup",
            MB_OK | MB_ICONWARNING);
    }
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
            L"TocPilot - Scan Existing Addons",
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
            L"TocPilot - Scan Existing Addons",
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
            L"TocPilot - Scan Existing Addons",
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

void StartUpdateCheck(
    HWND hwnd,
    bool refreshAddonsAfter = false) {
    if (g_appUpdateCheckInProgress ||
        g_appUpdateInProgress) {
        if (g_updateButton) {
            EnableWindow(g_updateButton, FALSE);
            SetWindowTextW(
                g_updateButton,
                g_appUpdateInProgress
                    ? L"Updating..."
                    : L"Checking...");
        }
        if (g_tocPilotUpdateStatus) {
            SetWindowTextW(
                g_tocPilotUpdateStatus,
                g_appUpdateInProgress
                    ? L"Downloading and verifying the TocPilot update..."
                    : L"Checking GitHub for the latest TocPilot release...");
        }
        return;
    }

    g_appUpdateCheckInProgress = true;
    g_refreshAddonsAfterAppCheck =
        refreshAddonsAfter;
    if (refreshAddonsAfter) {
        g_startupAppUpdateFailed = false;
    }

    if (g_updateButton) {
        EnableWindow(g_updateButton, FALSE);
        SetWindowTextW(g_updateButton, L"Checking...");
    }
    if (g_tocPilotUpdateStatus) {
        SetWindowTextW(
            g_tocPilotUpdateStatus,
            L"Checking GitHub for the latest TocPilot release...");
    }
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

void StartUpdate(
    HWND hwnd,
    bool startedFromStartup = false) {
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

    g_appUpdateStartedFromStartup =
        startedFromStartup;
    g_appUpdateInProgress = true;

    if (startedFromStartup &&
        tp::IsStartupSplashActive()) {
        tp::SetStartupSplashPhase(
            tp::StartupSplashPhase::
                ApplyingAppUpdate);
    }

    UpdatePackageButtons();
    if (g_updateButton) {
        EnableWindow(g_updateButton, FALSE);
        SetWindowTextW(g_updateButton, L"Updating...");
    }
    if (g_tocPilotUpdateStatus) {
        SetWindowTextW(
            g_tocPilotUpdateStatus,
            L"Downloading and verifying the TocPilot update...");
    }
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

HICON LoadExecutableIcon(
    const std::filesystem::path& path,
    int size) {
    HICON icon = nullptr;
    UINT resourceId = 0;

    if (PrivateExtractIconsW(
            path.c_str(),
            0,
            size,
            size,
            &icon,
            &resourceId,
            1,
            0) == 0) {
        return nullptr;
    }

    return icon;
}

void LaunchSiblingExecutable(
    HWND hwnd,
    std::wstring_view filename) {
    const std::filesystem::path path =
        g_root /
        std::wstring(filename);

    std::error_code ec;
    if (!std::filesystem::is_regular_file(
            path,
            ec)) {
        const std::wstring message =
            std::wstring(filename) +
            L" was not found beside TocPilot.exe.";

        SetRoutinePackageFeedback(message);
        return;
    }

    const HINSTANCE result =
        ShellExecuteW(
            hwnd,
            L"open",
            path.c_str(),
            nullptr,
            g_root.c_str(),
            SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        const std::wstring message =
            L"Could not launch " +
            std::wstring(filename) +
            L".";

        SetRoutinePackageFeedback(message);
    }
}

void OpenWebLink(
    HWND owner,
    const wchar_t* url) {
    const HINSTANCE result =
        ShellExecuteW(
            owner,
            L"open",
            url,
            nullptr,
            nullptr,
            SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(
            result) <= 32) {
        MessageBoxW(
            owner,
            L"Windows could not open the requested link.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
    }
}

void SetTocPilotUpdateUi(
    const std::wstring& status,
    const wchar_t* buttonText,
    bool buttonEnabled) {
    if (g_tocPilotUpdateStatus) {
        SetWindowTextW(
            g_tocPilotUpdateStatus,
            status.c_str());
    }

    if (g_updateButton) {
        SetWindowTextW(
            g_updateButton,
            buttonText);
        EnableWindow(
            g_updateButton,
            buttonEnabled
                ? TRUE
                : FALSE);
    }
}

LRESULT CALLBACK TocPilotWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        std::wstring versionText =
            L"TocPilot ";
        versionText +=
            TOCPILOT_VERSION_TAG_W;

        HWND version = CreateWindowExW(
            0,
            L"STATIC",
            versionText.c_str(),
            WS_CHILD | WS_VISIBLE,
            18,
            16,
            330,
            24,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        CreateWindowExW(
            0,
            WC_LINK,
            L"<a id=\"github\">GitHub repository</a>",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            18,
            48,
            150,
            24,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_TOCPILOT_GITHUB)),
            GetModuleHandleW(nullptr),
            nullptr);

        CreateWindowExW(
            0,
            WC_LINK,
            L"<a id=\"releases\">Releases</a>",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            182,
            48,
            100,
            24,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_TOCPILOT_RELEASES)),
            GetModuleHandleW(nullptr),
            nullptr);

        CreateWindowExW(
            0,
            L"BUTTON",
            L"TocPilot Update",
            WS_CHILD | WS_VISIBLE |
                BS_GROUPBOX,
            14,
            78,
            344,
            112,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        g_tocPilotUpdateStatus =
            CreateWindowExW(
                0,
                L"STATIC",
                L"Ready to check for updates.",
                WS_CHILD | WS_VISIBLE |
                    SS_LEFT,
                28,
                102,
                316,
                38,
                hwnd,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);

        g_updateButton =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Check for Updates",
                WS_CHILD | WS_VISIBLE |
                    WS_TABSTOP |
                    BS_PUSHBUTTON,
                28,
                148,
                316,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_TOCPILOT_UPDATE)),
                GetModuleHandleW(nullptr),
                nullptr);

        if (g_uiFont) {
            SendMessageW(
                version,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    g_uiFont),
                TRUE);
            EnumChildWindows(
                hwnd,
                ApplyFontToChild,
                reinterpret_cast<LPARAM>(
                    g_uiFont));
        }

        return 0;
    }

    case WM_NOTIFY: {
        const auto* header =
            reinterpret_cast<NMHDR*>(
                lParam);

        if (!header ||
            (header->code != NM_CLICK &&
             header->code != NM_RETURN)) {
            break;
        }

        if (header->idFrom ==
            IDC_TOCPILOT_GITHUB) {
            OpenWebLink(
                hwnd,
                kTocPilotGitHubUrl);
            return 0;
        }

        if (header->idFrom ==
            IDC_TOCPILOT_RELEASES) {
            OpenWebLink(
                hwnd,
                kTocPilotReleasesUrl);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) ==
                IDC_TOCPILOT_UPDATE &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            if (!g_release.assetUrl.empty()) {
                StartUpdate(
                    GetWindow(
                        hwnd,
                        GW_OWNER));
            } else {
                StartUpdateCheck(
                    GetWindow(
                        hwnd,
                        GW_OWNER));
            }
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_tocPilotWindow = nullptr;
        g_tocPilotUpdateStatus = nullptr;
        g_updateButton = nullptr;
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

void ShowTocPilotWindow(
    HWND owner) {
    if (g_tocPilotWindow &&
        IsWindow(g_tocPilotWindow)) {
        ShowWindow(
            g_tocPilotWindow,
            SW_RESTORE);
        SetForegroundWindow(
            g_tocPilotWindow);
        StartUpdateCheck(owner);
        return;
    }

    RECT ownerRect{};
    GetWindowRect(
        owner,
        &ownerRect);

    constexpr int windowWidth = 390;
    constexpr int windowHeight = 245;
    const int x =
        static_cast<int>(
            ownerRect.left) +
        std::max(
            0,
            static_cast<int>(
                ownerRect.right -
                ownerRect.left) -
                windowWidth) /
                2;
    const int y =
        static_cast<int>(
            ownerRect.top) +
        std::max(
            0,
            static_cast<int>(
                ownerRect.bottom -
                ownerRect.top) -
                windowHeight) /
                2;

    g_tocPilotWindow =
        CreateWindowExW(
            WS_EX_TOOLWINDOW,
            kTocPilotWindowClass,
            L"TocPilot",
            WS_OVERLAPPED |
                WS_CAPTION |
                WS_SYSMENU,
            x,
            y,
            windowWidth,
            windowHeight,
            owner,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

    if (!g_tocPilotWindow) {
        MessageBoxW(
            owner,
            L"Could not open the TocPilot window.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return;
    }

    ShowWindow(
        g_tocPilotWindow,
        SW_SHOW);
    UpdateWindow(
        g_tocPilotWindow);
    StartUpdateCheck(owner);
}

void ToggleAdvanced(HWND hwnd) {
    RECT rect{};
    if (!GetWindowRect(
            hwnd,
            &rect)) {
        return;
    }

    g_advancedVisible =
        !g_advancedVisible;

    if (!g_advancedVisible) {
        g_branchSelectorOpenPackageId.clear();
    }

    if (g_advancedButton) {
        SetWindowTextW(
            g_advancedButton,
            g_advancedVisible
                ? L"< Advanced"
                : L"Advanced >");
    }

    const int currentWidth =
        rect.right - rect.left;
    const int currentHeight =
        rect.bottom - rect.top;

    const int compactMinimum =
        std::max(
            kCompactWindowWidth,
            WindowWidthForClient(
                hwnd,
                RequiredClientWidth(
                    false)));
    const int advancedMinimum =
        std::max(
            compactMinimum,
            WindowWidthForClient(
                hwnd,
                RequiredClientWidth(
                    true)));
    const int widthDelta =
        advancedMinimum -
        compactMinimum;

    const int newWidth =
        g_advancedVisible
            ? std::max(
                advancedMinimum,
                currentWidth +
                    widthDelta)
            : std::max(
                compactMinimum,
                currentWidth -
                    widthDelta);

    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        newWidth,
        currentHeight,
        SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);

    LayoutControls(hwnd);
    UpdatePackageButtons();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
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
            L"Update New",
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
            L"Refresh All",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            130,
            145,
            85,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REFRESH_PACKAGES)),
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
            L"Reinstall Addon",
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
            L"Add Git",
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
            L"Remove Addon",
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
            L"Scan Existing Addons",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            750,
            145,
            85,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ADOPT_GIT)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_tocPilotButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"TocPilot",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                BS_PUSHBUTTON,
            0,
            0,
            100,
            32,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_TOCPILOT)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_advancedButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Advanced >",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                BS_PUSHBUTTON,
            0,
            0,
            100,
            32,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_ADVANCED)),
            GetModuleHandleW(nullptr),
            nullptr);

        EnableWindow(g_updateAllButton, FALSE);
        EnableWindow(g_refreshPackagesButton, FALSE);
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

        SetWindowSubclass(
            g_packageList,
            PackageListSubclassProc,
            1,
            0);

        g_branchSelector = CreateWindowExW(
            0,
            L"COMBOBOX",
            nullptr,
            WS_CHILD |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0,
            0,
            240,
            220,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_BRANCH_SELECTOR)),
            GetModuleHandleW(nullptr),
            nullptr);

        ShowWindow(
            g_branchSelector,
            SW_HIDE);

        g_launchWowButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Launch WoW",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                BS_PUSHBUTTON | BS_ICON,
            0,
            0,
            40,
            40,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_LAUNCH_WOW)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_wowIcon =
            LoadExecutableIcon(
                g_root / L"WoW.exe",
                36);

        std::error_code vanillaFixesEc;
        g_hasVanillaFixes =
            std::filesystem::is_regular_file(
                g_root /
                    L"VanillaFixes.exe",
                vanillaFixesEc);

        if (g_hasVanillaFixes) {
            g_vanillaFixesIcon =
                LoadExecutableIcon(
                    g_root /
                        L"VanillaFixes.exe",
                    36);
        }

        const HICON launchIcon =
            g_hasVanillaFixes &&
                    g_vanillaFixesIcon
                ? g_vanillaFixesIcon
                : g_wowIcon;

        SetWindowTextW(
            g_launchWowButton,
            g_hasVanillaFixes
                ? L"Launch VanillaFixes"
                : L"Launch WoW");

        if (launchIcon) {
            SendMessageW(
                g_launchWowButton,
                BM_SETIMAGE,
                IMAGE_ICON,
                reinterpret_cast<LPARAM>(
                    launchIcon));
        }

        AddPackageListColumns();
        PopulatePackageList();
        UpdatePackageButtons();
        ApplyUiFont(hwnd);
        LayoutControls(hwnd);

        return 0;
    }

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_TP_POSITION_BRANCH_SELECTOR:
        PositionBranchSelector();
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info =
            reinterpret_cast<MINMAXINFO*>(
                lParam);

        RECT minimum{
            0,
            0,
            RequiredClientWidth(
                g_advancedVisible),
            400};

        AdjustWindowRectEx(
            &minimum,
            static_cast<DWORD>(
                GetWindowLongPtrW(
                    hwnd,
                    GWL_STYLE)),
            FALSE,
            static_cast<DWORD>(
                GetWindowLongPtrW(
                    hwnd,
                    GWL_EXSTYLE)));

        info->ptMinTrackSize.x =
            minimum.right -
            minimum.left;
        info->ptMinTrackSize.y =
            minimum.bottom -
            minimum.top;
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
            g_packageList &&
            header->hwndFrom ==
                ListView_GetHeader(
                    g_packageList) &&
            (header->code ==
                 HDN_ITEMCHANGEDW ||
             header->code ==
                 HDN_TRACKW ||
             header->code ==
                 HDN_ENDTRACKW)) {
            PostMessageW(
                hwnd,
                WM_TP_POSITION_BRANCH_SELECTOR,
                0,
                0);
        }

        if (header &&
            header->idFrom ==
                IDC_PACKAGE_LIST) {
            if (header->code ==
                NM_CUSTOMDRAW) {
                return
                    HandlePackageListCustomDraw(
                        lParam);
            }

            if (header->code ==
                    NM_CLICK &&
                g_advancedVisible) {
                const auto* activate =
                    reinterpret_cast<
                        NMITEMACTIVATE*>(
                        lParam);

                if (activate &&
                    activate->iItem >= 0 &&
                    activate->iSubItem == 1) {
                    RequestBranchSelector(
                        hwnd,
                        activate->iItem);
                    return 0;
                }
            }

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
        if (LOWORD(wParam) == IDC_ADVANCED &&
            HIWORD(wParam) == BN_CLICKED) {
            ToggleAdvanced(hwnd);
            return 0;
        }

        if (LOWORD(wParam) == IDC_TOCPILOT &&
            HIWORD(wParam) == BN_CLICKED) {
            ShowTocPilotWindow(hwnd);
            return 0;
        }

        if (LOWORD(wParam) == IDC_LAUNCH_WOW &&
            HIWORD(wParam) == BN_CLICKED) {
            LaunchSiblingExecutable(
                hwnd,
                g_hasVanillaFixes
                    ? L"VanillaFixes.exe"
                    : L"WoW.exe");
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
            StartAutoStatusRefresh(hwnd);
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

                if (package.mode ==
                    L"release") {
                    prompt =
                        package.installedRevision.empty()
                            ? L"Install "
                            : L"Update/reinstall ";
                    prompt +=
                        package.name +
                        L" from the latest stable GitHub release?";

                    prompt +=
                        L"\r\n\r\nTocPilot will download only the exact configured asset '" +
                        package.asset +
                        L"' directly to:\r\n" +
                        (g_root /
                         package.targetPath).wstring() +
                        L"\r\n\r\nNo staged, temporary, renamed, or backup DLL will be created. "
                        L"Close WoW before continuing. TocPilot will verify SHA-256 before "
                        L"recording the installed release and will not change antivirus settings.";
                } else {
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
                }

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

        if (LOWORD(wParam) == IDC_REMOVE_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            const int row =
                SelectedPackageRow();

            if (row >= 0 &&
                row < static_cast<int>(
                    g_state.packages.size())) {
                RemovePackage(
                    hwnd,
                    static_cast<std::size_t>(
                        row));
            }
            return 0;
        }

        if (LOWORD(wParam) == IDC_ADD_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            tp::PackageRecord package;
            if (!tp::ShowAddPackageDialog(
                    hwnd,
                    package)) {
                return 0;
            }

            tp::AppState updatedState =
                g_state;
            std::wstring error;

            if (!tp::AppendPackage(
                    updatedState,
                    std::move(package),
                    error)) {
                MessageBoxW(
                    hwnd,
                    error.c_str(),
                    L"TocPilot - Add Git",
                    MB_OK | MB_ICONWARNING);
                return 0;
            }

            const std::size_t index =
                updatedState.packages.size() - 1;

            if (updatedState.packages[index].mode ==
                L"release") {
                if (!tp::SaveState(
                        g_root,
                        updatedState,
                        error)) {
                    MessageBoxW(
                        hwnd,
                        error.c_str(),
                        L"TocPilot - Add DLL",
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

                StartPackageInstall(
                    hwnd,
                    index);
                return 0;
            }

            tp::BranchSelection selection;
            if (!tp::ShowBranchDialog(
                    hwnd,
                    updatedState.packages[index],
                    selection)) {
                return 0;
            }

            if (!tp::SetPackageBranch(
                    updatedState.packages[index],
                    std::move(selection.name),
                    std::move(selection.sha),
                    error) ||
                !tp::SaveState(
                    g_root,
                    updatedState,
                    error)) {
                MessageBoxW(
                    hwnd,
                    error.c_str(),
                    L"TocPilot - Add Git",
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

            StartPackageInstall(
                hwnd,
                index);
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

        g_appUpdateCheckInProgress = false;
        const bool refreshAddonsAfter =
            g_refreshAddonsAfterAppCheck;
        g_refreshAddonsAfterAppCheck =
            false;

        if (!result->ok) {
            g_release = {};
            SetIndicator(
                g_githubStatus,
                L"GitHub Comms: Failed");
            SetIndicator(
                g_releaseStatus,
                L"Release: Unavailable");
            SetTocPilotUpdateUi(
                L"Update check failed. Check your connection and try again.",
                L"Retry Check",
                true);
            if (refreshAddonsAfter) {
                if (tp::IsStartupSplashActive()) {
                    tp::SetStartupSplashPhase(
                        tp::StartupSplashPhase::
                            ScanningAddonUpdates);
                }
                StartAutoStatusRefresh(hwnd);
            }
            return 0;
        }

        SetIndicator(
            g_githubStatus,
            L"GitHub Comms: Good");

        switch (result->state) {
        case tp::ReleaseCheckState::NoRelease:
            g_release = {};
            SetIndicator(
                g_releaseStatus,
                L"Release: Not found");
            SetTocPilotUpdateUi(
                L"No published TocPilot release was found.",
                L"Retry Check",
                true);
            break;

        case tp::ReleaseCheckState::UpdateAvailable:
            g_release =
                result->release;
            SetIndicator(
                g_releaseStatus,
                L"Release: Update available - " +
                    result->release.tag);
            SetTocPilotUpdateUi(
                result->release.tag +
                    L" is available.",
                L"Update Available",
                true);

            if (refreshAddonsAfter) {
                StartUpdate(
                    hwnd,
                    true);
                return 0;
            }
            break;

        case tp::ReleaseCheckState::UpToDate:
            g_release = {};
            SetIndicator(
                g_releaseStatus,
                L"Release: Current - " +
                    result->release.tag);
            SetTocPilotUpdateUi(
                L"Installed " +
                    std::wstring(
                        TOCPILOT_VERSION_TAG_W) +
                    L" is the latest release.",
                L"Up to date",
                false);
            break;
        }

        if (refreshAddonsAfter) {
            if (tp::IsStartupSplashActive()) {
                tp::SetStartupSplashPhase(
                    tp::StartupSplashPhase::
                        ScanningAddonUpdates);
            }
            StartAutoStatusRefresh(hwnd);
        }
        return 0;
    }

    case WM_TP_BRANCHES_READY: {
        std::unique_ptr<BranchLoadResult> result(
            reinterpret_cast<
                BranchLoadResult*>(
                lParam));

        if (result->generation !=
                g_branchSelectorGeneration ||
            result->packageId !=
                g_branchSelectorPackageId) {
            return 0;
        }

        g_branchSelectorLoadInProgress =
            false;

        std::size_t packageIndex =
            g_state.packages.size();

        for (std::size_t i = 0;
             i < g_state.packages.size();
             ++i) {
            if (g_state.packages[i].id ==
                result->packageId) {
                packageIndex = i;
                break;
            }
        }

        if (!result->ok) {
            g_branchSelectorLoaded = false;
            g_branchSelectorLoadFailed =
                true;

            if (g_branchSelectorOpenPackageId ==
                result->packageId) {
                g_branchSelectorOpenPackageId.clear();
            }

            if (packageIndex <
                g_state.packages.size()) {
                PopulateBranchSelectorCurrent(
                    g_state.packages[
                        packageIndex]);
                SetPackageRowStatus(
                    packageIndex,
                    L"Branch lookup failed");
            }

            UpdateBranchSelector(
                hwnd);
            return 0;
        }

        g_branchSelectorDefaultBranch =
            std::move(
                result->info.defaultBranch);
        g_branchSelectorBranches =
            std::move(
                result->info.branches);
        g_branchSelectorLoaded = true;
        g_branchSelectorLoadFailed =
            false;

        if (packageIndex <
            g_state.packages.size()) {
            PopulateBranchSelectorLoaded(
                g_state.packages[
                    packageIndex]);
            SetPackageRowStatus(
                packageIndex,
                PackageStatusText(
                    g_state.packages[
                        packageIndex]));
        }

        UpdateBranchSelector(
            hwnd);
        OpenBranchSelectorIfReady(
            hwnd);
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

        bool trackingMatches = false;

        if (result->index <
                g_state.packages.size() &&
            g_state.packages[
                result->index].id ==
                result->packageId) {
            const auto& tracked =
                g_state.packages[
                    result->index];

            if (!result->asset.empty()) {
                trackingMatches =
                    tracked.mode ==
                        L"release" &&
                    tracked.asset ==
                        result->asset &&
                    tracked.targetPath ==
                        result->asset;
            } else {
                trackingMatches =
                    tracked.mode ==
                        L"branch" &&
                    tracked.ref ==
                        result->branch;
            }
        }

        if (!trackingMatches) {
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
            const bool rateLimited =
                IsProviderRateLimitError(
                    result->error);

            SetPackageNeedsAttention(
                result->packageId,
                result->needsAttention &&
                    !rateLimited);

            SetPackageRowStatus(
                index,
                result->needsAttention &&
                        !rateLimited
                    ? L"Needs attention"
                    : L"Refresh failed");

            const std::wstring message =
                packageName +
                L": status check failed - " +
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
                    rateLimited);
            } else if (autoStatusStep) {
                if (rateLimited) {
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

        SetPackageNeedsAttention(
            result->packageId,
            false);

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
            std::wstring message;

            if (package.mode ==
                L"release") {
                message =
                    package.name +
                    L": latest stable release is " +
                    package.latestRevision +
                    L" and exact asset '" +
                    package.asset +
                    L"' is available. No managed files were changed.";
            } else {
                const std::wstring shortSha =
                    package.latestRevision.substr(
                        0,
                        std::min<std::size_t>(
                            7,
                            package.latestRevision.size()));

                message =
                    package.name +
                    L": " +
                    package.ref +
                    L" remote head is " +
                    shortSha +
                    L". No addon files were changed.";
            }

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

    case WM_TP_DLL_INSTALL_COMPLETE: {
        std::unique_ptr<DirectDllInstallResult>
            result(
                reinterpret_cast<
                    DirectDllInstallResult*>(
                        lParam));

        g_packageInstallInProgress = false;

        const bool updateAllStep =
            IsUpdateAllCurrentPackage(
                result->packageId);

        bool trackingMatches = false;

        if (result->index <
                g_state.packages.size() &&
            g_state.packages[
                result->index].id ==
                result->packageId) {
            const auto& package =
                g_state.packages[
                    result->index];

            trackingMatches =
                package.mode ==
                    L"release" &&
                package.asset ==
                    result->asset &&
                package.targetPath ==
                    result->targetPath;
        }

        if (!trackingMatches) {
            const std::wstring message =
                result->packageId +
                L": DLL install result was discarded because package tracking changed. Installed state was not changed.";

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
                UpdatePackageButtons();
            }
            return 0;
        }

        const auto index =
            result->index;
        const auto& currentPackage =
            g_state.packages[index];

        if (!result->ok) {
            if (result->needsAttention) {
                SetPackageNeedsAttention(
                    result->packageId,
                    true);
            }

            SetPackageRowStatus(
                index,
                result->needsAttention
                    ? L"Needs attention"
                    : L"DLL update failed");

            const std::wstring message =
                currentPackage.name +
                L": DLL install/update failed - " +
                result->error +
                L" Installed state was not advanced.";

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
                    IsProviderRateLimitError(
                        result->error));
            } else {
                MessageBoxW(
                    hwnd,
                    message.c_str(),
                    L"TocPilot - DLL Install Failed",
                    MB_OK |
                        MB_ICONWARNING);
                SelectPackageRow(index);
                UpdatePackageButtons();
            }
            return 0;
        }

        tp::AppState updatedState =
            g_state;
        std::wstring error;

        if (!tp::SetPackageInstalledState(
                updatedState.packages[index],
                result->resolvedTag,
                {result->targetPath},
                error) ||
            !tp::SaveState(
                g_root,
                updatedState,
                error)) {
            SetPackageRowStatus(
                index,
                L"State save failed");

            const std::wstring message =
                currentPackage.name +
                L": the DLL was written and SHA-256 verified at the exact final path, but TocPilot could not save installed state - " +
                error +
                L". Installed state remains unchanged; no backup DLL exists by design.";

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
                MessageBoxW(
                    hwnd,
                    message.c_str(),
                    L"TocPilot - DLL State Save Failed",
                    MB_OK |
                        MB_ICONWARNING);
                SelectPackageRow(index);
                UpdatePackageButtons();
            }
            return 0;
        }

        g_state =
            std::move(updatedState);
        g_stateCreated = false;
        g_stateError.clear();

        SetPackageNeedsAttention(
            result->packageId,
            false);

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

        const auto& installed =
            g_state.packages[index];

        const std::wstring message =
            installed.name +
            L": installed and verified release " +
            installed.installedRevision +
            L" at " +
            (g_root /
             installed.targetPath).wstring() +
            L".";

        if (g_packageHint) {
            SetWindowTextW(
                g_packageHint,
                message.c_str());
        }

        const std::wstring summary =
            installed.name +
            L" installed successfully.\r\n\r\nRepository: " +
            installed.repository +
            L"\r\nRelease: " +
            installed.installedRevision +
            L"\r\nAsset: " +
            installed.asset +
            L"\r\nDestination: " +
            (g_root /
             installed.targetPath).wstring() +
            L"\r\nDownloaded: " +
            std::to_wstring(
                result->downloadedBytes) +
            L" bytes\r\nSHA-256: " +
            result->actualSha256 +
            L"\r\n\r\nNo staged, temporary, renamed, or backup DLL was created. TocPilot did not alter antivirus settings.";

        MessageBoxW(
            hwnd,
            summary.c_str(),
            L"TocPilot - DLL Installed",
            MB_OK |
                MB_ICONINFORMATION);

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
            CleanupPackageStaging(
                installedPackage,
                g_root,
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
        const bool startedFromStartup =
            g_appUpdateStartedFromStartup;
        g_appUpdateStartedFromStartup = false;

        if (!result->ok) {
            g_appUpdateInProgress = false;
            UpdatePackageButtons();
            SetIndicator(
                g_releaseStatus,
                L"Release: Update failed - " + result->error);
            SetTocPilotUpdateUi(
                L"The update could not be applied: " +
                    result->error,
                L"Retry Update",
                true);

            if (startedFromStartup) {
                g_startupAppUpdateFailed = true;
                if (g_packageHint) {
                    const std::wstring hint =
                        L"TocPilot self-update failed: " +
                        result->error +
                        L" Startup package scanning will continue; open TocPilot after continuing to retry.";
                    SetWindowTextW(
                        g_packageHint,
                        hint.c_str());
                }
                if (tp::IsStartupSplashActive()) {
                    tp::SetStartupSplashPhase(
                        tp::StartupSplashPhase::
                            ScanningAddonUpdates);
                }
                StartAutoStatusRefresh(hwnd);
            }
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
        ++g_branchSelectorGeneration;

        if (g_packageList) {
            RemoveWindowSubclass(
                g_packageList,
                PackageListSubclassProc,
                1);
        }

        if (g_wowIcon) {
            DestroyIcon(g_wowIcon);
            g_wowIcon = nullptr;
        }
        if (g_vanillaFixesIcon) {
            DestroyIcon(g_vanillaFixesIcon);
            g_vanillaFixesIcon = nullptr;
        }
        if (g_uiFont) {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
        }
        if (g_boldUiFont) {
            DeleteObject(g_boldUiFont);
            g_boldUiFont = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

std::wstring InstanceKeyForRoot(
    const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::path normalized =
        std::filesystem::absolute(
            root,
            ec);

    if (ec) {
        normalized = root;
    }

    normalized =
        normalized.lexically_normal();

    std::wstring text =
        normalized.wstring();

    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](wchar_t value) {
            return static_cast<wchar_t>(
                std::towupper(value));
        });

    std::uint64_t hash =
        14695981039346656037ULL;

    for (const wchar_t value :
         text) {
        hash ^=
            static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(
                    value));
        hash *=
            1099511628211ULL;
    }

    wchar_t buffer[17]{};
    swprintf_s(
        buffer,
        std::size(buffer),
        L"%016llX",
        static_cast<unsigned long long>(
            hash));
    return buffer;
}

void ActivateExistingInstance(
    const std::wstring& className) {
    HWND existing = nullptr;

    for (int attempt = 0;
         attempt < 40 &&
         !existing;
         ++attempt) {
        existing =
            FindWindowW(
                className.c_str(),
                nullptr);

        if (!existing) {
            Sleep(50);
        }
    }

    if (!existing) {
        return;
    }

    if (IsIconic(existing)) {
        ShowWindow(
            existing,
            SW_RESTORE);
    } else {
        ShowWindow(
            existing,
            SW_SHOW);
    }

    if (!SetForegroundWindow(
            existing)) {
        FLASHWINFO flash{};
        flash.cbSize =
            sizeof(flash);
        flash.hwnd =
            existing;
        flash.dwFlags =
            FLASHW_TRAY |
            FLASHW_TIMERNOFG;
        flash.uCount = 3;
        flash.dwTimeout = 0;
        FlashWindowEx(
            &flash);
    }
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

    const std::wstring instanceKey =
        InstanceKeyForRoot(g_root);
    g_windowClassName =
        std::wstring(
            kWindowClassBase) +
        L"_" +
        instanceKey;

    const std::wstring mutexName =
        L"Local\\TocPilot_" +
        instanceKey;
    HANDLE instanceMutex =
        CreateMutexW(
            nullptr,
            FALSE,
            mutexName.c_str());

    if (!instanceMutex) {
        MessageBoxW(
            nullptr,
            L"Could not create the TocPilot single-instance guard.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 2;
    }

    if (GetLastError() ==
        ERROR_ALREADY_EXISTS) {
        ActivateExistingInstance(
            g_windowClassName);
        CloseHandle(
            instanceMutex);
        return 0;
    }

    std::wstring error;
    if (!ValidateWowRoot(g_root, error)) {
        CloseHandle(
            instanceMutex);
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

    if (!g_stateReady) {
        MessageBoxW(
            nullptr,
            g_stateError.c_str(),
            L"TocPilot - State Error",
            MB_OK | MB_ICONERROR);
    }

    if (g_stateReady) {
        g_packageSortColumn =
            g_state.settings.packageSortColumn;
        g_packageSortAscending =
            g_state.settings.packageSortAscending;
    }

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC =
        ICC_LISTVIEW_CLASSES |
        ICC_STANDARD_CLASSES |
        ICC_LINK_CLASS;
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName =
        g_windowClassName.c_str();
    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);
    wc.hIcon =
        LoadIconW(
            instance,
            MAKEINTRESOURCEW(
                IDI_TOCPILOT));
    wc.hIconSm =
        reinterpret_cast<HICON>(
            LoadImageW(
                instance,
                MAKEINTRESOURCEW(
                    IDI_TOCPILOT),
                IMAGE_ICON,
                16,
                16,
                LR_DEFAULTCOLOR));
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc)) {
        CloseHandle(
            instanceMutex);
        MessageBoxW(
            nullptr,
            L"Could not register the TocPilot window class.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 3;
    }

    WNDCLASSEXW toolClass = wc;
    toolClass.lpfnWndProc =
        TocPilotWindowProc;
    toolClass.lpszClassName =
        kTocPilotWindowClass;

    if (!RegisterClassExW(
            &toolClass) &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        CloseHandle(
            instanceMutex);
        MessageBoxW(
            nullptr,
            L"Could not register the TocPilot tool window.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 3;
    }

    std::wstring title = L"TocPilot ";
    title += TOCPILOT_VERSION_TAG_W;

    HWND hwnd = CreateWindowExW(
        0,
        g_windowClassName.c_str(),
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kCompactWindowWidth,
        kDefaultWindowHeight,
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
        CloseHandle(
            instanceMutex);
        return 4;
    }

    const bool splashShown =
        tp::ShowStartupSplash(
            instance,
            hwnd);

    if (!splashShown) {
        ShowWindow(
            hwnd,
            SW_SHOW);
        UpdateWindow(hwnd);
    }

    StartUpdateCheck(
        hwnd,
        true);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    const int result =
        static_cast<int>(
            msg.wParam);

    CloseHandle(
        instanceMutex);
    return result;
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
