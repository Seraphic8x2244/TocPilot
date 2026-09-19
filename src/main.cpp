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
constexpr int IDC_STATUS = 1002;

HWND g_status = nullptr;
HWND g_updateButton = nullptr;
tp::ReleaseInfo g_release;

struct CheckResult {
    bool ok = false;
    bool updateAvailable = false;
    tp::ReleaseInfo release;
    std::wstring error;
};

struct UpdateResult {
    bool ok = false;
    std::wstring error;
};

void SetStatus(const std::wstring& text) {
    if (g_status) {
        SetWindowTextW(g_status, text.c_str());
    }
}

void StartUpdateCheck(HWND hwnd) {
    EnableWindow(g_updateButton, FALSE);
    SetWindowTextW(g_updateButton, L"Checking...");
    SetStatus(L"Checking GitHub for updates...");

    std::thread([hwnd]() {
        auto result = std::make_unique<CheckResult>();
        result->ok = tp::CheckLatestRelease(
            result->release,
            result->updateAvailable,
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
    SetStatus(L"Downloading and verifying update...");

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
            52,
            300,
            22,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        std::wstring rootText = L"World of Warcraft: ";
        rootText += root.wstring();
        CreateWindowExW(
            0,
            L"STATIC",
            rootText.c_str(),
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            20,
            80,
            620,
            22,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        g_status = CreateWindowExW(
            0,
            L"STATIC",
            L"Starting...",
            WS_CHILD | WS_VISIBLE,
            20,
            118,
            620,
            42,
            hwnd,
            reinterpret_cast<HMENU>(IDC_STATUS),
            GetModuleHandleW(nullptr),
            nullptr);

        g_updateButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Check for updates",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            20,
            170,
            170,
            32,
            hwnd,
            reinterpret_cast<HMENU>(IDC_UPDATE),
            GetModuleHandleW(nullptr),
            nullptr);

        StartUpdateCheck(hwnd);
        return 0;
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
            SetStatus(L"Update check failed: " + result->error);
            SetWindowTextW(g_updateButton, L"Retry");
            EnableWindow(g_updateButton, TRUE);
            return 0;
        }

        if (result->updateAvailable) {
            g_release = result->release;
            SetStatus(L"Update available: " + result->release.tag);
            SetWindowTextW(g_updateButton, L"Update now");
            EnableWindow(g_updateButton, TRUE);
        } else {
            g_release = {};
            SetStatus(L"Up to date. Latest release: " + result->release.tag);
            SetWindowTextW(g_updateButton, L"Check again");
            EnableWindow(g_updateButton, TRUE);
        }
        return 0;
    }

    case WM_TP_UPDATE_COMPLETE: {
        std::unique_ptr<UpdateResult> result(
            reinterpret_cast<UpdateResult*>(lParam));

        if (!result->ok) {
            SetStatus(L"Update failed: " + result->error);
            SetWindowTextW(g_updateButton, L"Retry update");
            EnableWindow(g_updateButton, TRUE);
            return 0;
        }

        SetStatus(L"Update verified. Restarting...");
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
        260,
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
