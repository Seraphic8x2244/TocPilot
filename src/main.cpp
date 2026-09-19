#include "add_package_dialog.h"
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
    text += std::to_wstring(g_state.packageCount);
    text += g_state.packageCount == 1 ? L" package record" : L" package records";
    return text;
}

std::wstring PackageHintText() {
    if (!g_stateReady) {
        return L"Package state is read-only until TocPilot.json is fixed.";
    }

    if (g_state.packageCount == 0) {
        return L"No managed packages yet. Add Package can validate GitHub/GitLab repository URLs; install flow comes next.";
    }

    return
        std::to_wstring(g_state.packageCount) +
        L" package records are stored; row parsing is not enabled in this first P1 slice.";
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
        MoveWindow(g_updateAllButton, x, buttonY, 105, 32, TRUE);
        x += 115;
    }
    if (g_refreshPackagesButton) {
        MoveWindow(g_refreshPackagesButton, x, buttonY, 120, 32, TRUE);
        x += 130;
    }
    if (g_addPackageButton) {
        MoveWindow(g_addPackageButton, x, buttonY, 115, 32, TRUE);
        x += 125;
    }
    if (g_updateButton) {
        MoveWindow(g_updateButton, x, buttonY, 150, 32, TRUE);
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
            135,
            145,
            120,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REFRESH_PACKAGES)),
            GetModuleHandleW(nullptr),
            nullptr);

        g_addPackageButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Add Package",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            265,
            145,
            115,
            32,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ADD_PACKAGE)),
            GetModuleHandleW(nullptr),
            nullptr);

        EnableWindow(g_updateAllButton, FALSE);
        EnableWindow(g_refreshPackagesButton, FALSE);
        EnableWindow(g_addPackageButton, g_stateReady ? TRUE : FALSE);

        g_updateButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Check app update",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            390,
            145,
            150,
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

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_UPDATE && HIWORD(wParam) == BN_CLICKED) {
            if (!g_release.assetUrl.empty()) {
                StartUpdate(hwnd);
            } else {
                StartUpdateCheck(hwnd);
            }
            return 0;
        }

        if (LOWORD(wParam) == IDC_ADD_PACKAGE &&
            HIWORD(wParam) == BN_CLICKED) {
            tp::ShowAddPackageDialog(hwnd);
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

    case WM_TP_UPDATE_COMPLETE: {
        std::unique_ptr<UpdateResult> result(
            reinterpret_cast<UpdateResult*>(lParam));

        if (!result->ok) {
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
