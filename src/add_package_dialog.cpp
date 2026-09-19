#include "add_package_dialog.h"

#include "provider.h"

#include <algorithm>
#include <string>

namespace tp {
namespace {

constexpr wchar_t kClassName[] = L"TocPilotAddPackageDialog";
constexpr int IDC_REPOSITORY_URL = 2001;
constexpr int IDC_CHECK_URL = 2002;
constexpr int IDC_RESULT = 2003;
constexpr int IDC_CLOSE_DIALOG = 2004;

void SetControlFont(HWND control) {
    if (control) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                GetStockObject(DEFAULT_GUI_FONT)),
            TRUE);
    }
}

std::wstring ControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(
        static_cast<std::size_t>(std::max(0, length)) + 1,
        L'\0');

    if (length > 0) {
        GetWindowTextW(control, text.data(), length + 1);
    }
    text.resize(static_cast<std::size_t>(
        std::max(0, length)));
    return text;
}

void CheckRepositoryUrl(HWND hwnd) {
    const HWND edit = GetDlgItem(hwnd, IDC_REPOSITORY_URL);
    const HWND result = GetDlgItem(hwnd, IDC_RESULT);

    RepositoryIdentity identity;
    std::wstring error;
    if (!NormalizeRepositoryUrl(
            ControlText(edit),
            identity,
            error)) {
        SetWindowTextW(
            result,
            (L"Not recognized: " + error).c_str());
        return;
    }

    std::wstring message =
        L"Recognized " +
        std::wstring(ProviderName(identity.provider)) +
        L" repository\r\nRepository: " +
        identity.repository +
        L"\r\nNormalized: " +
        identity.canonicalUrl +
        L"\r\n\r\nInstallation options are not enabled yet.";
    SetWindowTextW(result, message.c_str());
}

LRESULT CALLBACK DialogProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        const auto instance = GetModuleHandleW(nullptr);

        HWND intro = CreateWindowExW(
            0,
            L"STATIC",
            L"Paste a public GitHub or GitLab repository URL.",
            WS_CHILD | WS_VISIBLE,
            20,
            18,
            540,
            22,
            hwnd,
            nullptr,
            instance,
            nullptr);

        HWND edit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                ES_AUTOHSCROLL,
            20,
            48,
            540,
            26,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(IDC_REPOSITORY_URL)),
            instance,
            nullptr);

        HWND check = CreateWindowExW(
            0,
            L"BUTTON",
            L"Check URL",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                BS_DEFPUSHBUTTON,
            20,
            86,
            100,
            30,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(IDC_CHECK_URL)),
            instance,
            nullptr);

        HWND close = CreateWindowExW(
            0,
            L"BUTTON",
            L"Close",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                BS_PUSHBUTTON,
            460,
            86,
            100,
            30,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(IDC_CLOSE_DIALOG)),
            instance,
            nullptr);

        HWND result = CreateWindowExW(
            0,
            L"STATIC",
            L"Nothing is installed or saved by this dialog.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20,
            128,
            540,
            88,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(IDC_RESULT)),
            instance,
            nullptr);

        SetControlFont(intro);
        SetControlFont(edit);
        SetControlFont(check);
        SetControlFont(close);
        SetControlFont(result);
        SetFocus(edit);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CHECK_URL &&
            HIWORD(wParam) == BN_CLICKED) {
            CheckRepositoryUrl(hwnd);
            return 0;
        }

        if (LOWORD(wParam) == IDC_CLOSE_DIALOG &&
            HIWORD(wParam) == BN_CLICKED) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool EnsureDialogClass() {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    registered = true;
    return true;
}

} // namespace

void ShowAddPackageDialog(HWND owner) {
    if (!EnsureDialogClass()) {
        MessageBoxW(
            owner,
            L"Could not create the Add Package window.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return;
    }

    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);

    constexpr int width = 600;
    constexpr int height = 270;
    const int ownerWidth =
        static_cast<int>(ownerRect.right - ownerRect.left);
    const int ownerHeight =
        static_cast<int>(ownerRect.bottom - ownerRect.top);
    const int x = static_cast<int>(ownerRect.left) +
        std::max(0, (ownerWidth - width) / 2);
    const int y = static_cast<int>(ownerRect.top) +
        std::max(0, (ownerHeight - height) / 2);

    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        kClassName,
        L"Add Package - Check Repository",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);

    if (!dialog) {
        MessageBoxW(
            owner,
            L"Could not create the Add Package window.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);
        return;
    }

    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    MSG msg{};
    while (IsWindow(dialog)) {
        const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) {
                PostQuitMessage(
                    static_cast<int>(msg.wParam));
            }
            break;
        }

        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
}

} // namespace tp
