#include "update.h"
#include "version.h"

#include <windows.h>
#include <shellapi.h>

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

HWND g_wowStatus = nullptr;
HWND g_githubStatus = nullptr;
HWND g_releaseStatus = nullptr;
HWND g_updateButton = nullptr;
tp::ReleaseInfo g_release;

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

void StartUpdateCheck(HWND hwnd) {
    EnableWindow(g_updateButton, FALSE);
    SetWindowTextW(g_updateButton, L"Checking...");
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
    SetWindowTextW(g_updateButton, L"Updating...");
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
    } else {
        return 0;
    }

    HDC dc = reinterpret_cast<HDC>(wParam);
    SetTextColor(dc, colour);
    SetBkColor(dc, GetSysColor(COLOR_WINDOW));
    return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        const auto root = tp::ExecutablePath().parent_path();

        CreateWindowExW(
            0,
            L"STATIC",
            L"TocPilot",
            WS_CHILD | WS_VISIBLE,
            20,
            18,
            180,
            28,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        std::wstring versionText = L"Version ";
        versionText += TOCPILOT_VERSION_TAG_W;
        CreateWindowExW(
            0,
            L"STATIC",
            versionText.c_str(),
            WS_CHILD | WS_VISIBLE,
            20,
            50,
            300,
            22,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        std::wstring rootText = L"WoW Path: ";
        rootText += root.wstring();
        CreateWindowExW(
            0,
            L"STATIC",
            rootText.c_str(),
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            20,
            78,
            620,
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
            116,
            360,
            24,
            hwnd,
            reinterpret_cast<HMENU>(IDC_WOW_STATUS),
            GetModuleHandleW(nullptr),
            nullptr);

        g_githubStatus = CreateWindowExW(
            0,
            L"STATIC",
            L"GitHub Comms: Checking...",
            WS_CHILD | WS_VISIBLE,
            20,
            146,
            360,
            24,
            hwnd,
            reinterpret_cast<HMENU>(IDC_GITHUB_STATUS),
            GetModuleHandleW(nullptr),
            nullptr);

        g_releaseStatus = CreateWindowExW(
            0,
            L"STATIC",
            L"Release: Checking...",
            WS_CHILD | WS_VISIBLE,
            20,
            176,
            500,
            24,
            hwnd,
            reinterpret_cast<HMENU>(IDC_RELEASE_STATUS),
            GetModuleHandleW(nullptr),
            nullptr);

        g_updateButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Check for updates",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            20,
            218,
            170,
            32,
            hwnd,
            reinterpret_cast<HMENU>(IDC_UPDATE),
            GetModuleHandleW(nullptr),
            nullptr);

        StartUpdateCheck(hwnd);
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
        break;

    case WM_TP_CHECK_COMPLETE: {
        std::unique_ptr<CheckResult> result(
            reinterpret_cast<CheckResult*>(lParam));

        if (!result->ok) {
            g_release = {};
            SetIndicator(g_githubStatus, L"GitHub Comms: Failed");
            SetIndicator(g_releaseStatus, L"Release: Unavailable");
            SetWindowTextW(g_updateButton, L"Retry");
            EnableWindow(g_updateButton, TRUE);

            return 0;
        }

        SetIndicator(g_githubStatus, L"GitHub Comms: Good");

        switch (result->state) {
        case tp::ReleaseCheckState::NoRelease:
            g_release = {};
            SetIndicator(g_releaseStatus, L"Release: Not found");
            SetWindowTextW(g_updateButton, L"Check again");
            EnableWindow(g_updateButton, TRUE);
            break;

        case tp::ReleaseCheckState::UpdateAvailable:
            g_release = result->release;
            SetIndicator(
                g_releaseStatus,
                L"Release: Update available - " + result->release.tag);
            SetWindowTextW(g_updateButton, L"Update now");
            EnableWindow(g_updateButton, TRUE);
            break;

        case tp::ReleaseCheckState::UpToDate:
            g_release = {};
            SetIndicator(
                g_releaseStatus,
                L"Release: Current - " + result->release.tag);
            SetWindowTextW(g_updateButton, L"Check again");
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
            SetWindowTextW(g_updateButton, L"Retry update");
            EnableWindow(g_updateButton, TRUE);
            return 0;
        }

        SetIndicator(g_releaseStatus, L"Release: Verified - restarting...");
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
    }

    case WM_DESTROY:
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

    const auto root = exe.parent_path();
    std::wstring error;
    if (!ValidateWowRoot(root, error)) {
        MessageBoxW(
            nullptr,
            error.c_str(),
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return 2;
    }

    SetCurrentDirectoryW(root.c_str());

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
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        680,
        310,
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
