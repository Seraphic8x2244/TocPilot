#include "add_package_dialog.h"

#include "provider.h"

#include <algorithm>
#include <string>
#include <utility>

namespace tp {
namespace {

constexpr wchar_t kClassName[] =
    L"TocPilotAddPackageDialog";

constexpr int IDC_REPOSITORY_URL = 2001;
constexpr int IDC_SCAN_SOURCE = 2002;
constexpr int IDC_RESULT = 2003;
constexpr int IDC_CLOSE_DIALOG = 2004;

struct DialogContext {
    PackageRecord* package = nullptr;
    bool accepted = false;
};

void SetControlFont(
    HWND control) {
    if (control) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                GetStockObject(
                    DEFAULT_GUI_FONT)),
            TRUE);
    }
}

std::wstring ControlText(
    HWND control) {
    const int length =
        GetWindowTextLengthW(
            control);

    std::wstring text(
        static_cast<std::size_t>(
            std::max(
                0,
                length)) +
            1,
        L'\0');

    if (length > 0) {
        GetWindowTextW(
            control,
            text.data(),
            length + 1);
    }

    text.resize(
        static_cast<std::size_t>(
            std::max(
                0,
                length)));
    return text;
}

DialogContext* Context(
    HWND hwnd) {
    return reinterpret_cast<
        DialogContext*>(
        GetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA));
}

void ValidateAndAccept(
    HWND hwnd) {
    const HWND edit =
        GetDlgItem(
            hwnd,
            IDC_REPOSITORY_URL);

    const HWND result =
        GetDlgItem(
            hwnd,
            IDC_RESULT);

    RepositoryIdentity identity;
    std::wstring error;

    if (!NormalizeRepositoryUrl(
            ControlText(edit),
            identity,
            error)) {
        SetWindowTextW(
            result,
            (L"Not recognized: " +
             error).c_str());
        return;
    }

    DialogContext* context =
        Context(hwnd);

    if (!context ||
        !context->package) {
        SetWindowTextW(
            result,
            L"Could not prepare the repository scan.");
        return;
    }

    std::wstring provider =
        identity.provider ==
                ProviderKind::GitHub
            ? L"github"
            : L"gitlab";

    *context->package =
        MakeRepositoryPackage(
            std::move(provider),
            identity.repository);

    context->accepted = true;
    DestroyWindow(hwnd);
}

LRESULT CALLBACK DialogProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_NCCREATE: {
        const auto* create =
            reinterpret_cast<
                CREATESTRUCTW*>(
                    lParam);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                create->lpCreateParams));
        return TRUE;
    }

    case WM_CREATE: {
        const auto instance =
            GetModuleHandleW(
                nullptr);

        HWND intro =
            CreateWindowExW(
                0,
                L"STATIC",
                L"Paste a public GitHub or GitLab repository URL. TocPilot will scan the selected branch and, for GitHub, the latest stable release for supported components.",
                WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                20,
                16,
                540,
                42,
                hwnd,
                nullptr,
                instance,
                nullptr);

        HWND edit =
            CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    ES_AUTOHSCROLL,
                20,
                64,
                540,
                26,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_REPOSITORY_URL)),
                instance,
                nullptr);

        HWND scan =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Scan",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_DEFPUSHBUTTON,
                20,
                108,
                110,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_SCAN_SOURCE)),
                instance,
                nullptr);

        HWND close =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Cancel",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_PUSHBUTTON,
                450,
                108,
                110,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_CLOSE_DIALOG)),
                instance,
                nullptr);

        HWND result =
            CreateWindowExW(
                0,
                L"STATIC",
                L"Scan will ask which branch to inspect before showing every supported component TocPilot finds.",
                WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                20,
                150,
                540,
                48,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_RESULT)),
                instance,
                nullptr);

        SetControlFont(intro);
        SetControlFont(edit);
        SetControlFont(scan);
        SetControlFont(close);
        SetControlFont(result);

        SetFocus(edit);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) ==
                IDC_SCAN_SOURCE &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            ValidateAndAccept(hwnd);
            return 0;
        }

        if (LOWORD(wParam) ==
                IDC_CLOSE_DIALOG &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

bool EnsureDialogClass() {
    static bool registered =
        false;

    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize =
        sizeof(wc);
    wc.lpfnWndProc =
        DialogProc;
    wc.hInstance =
        GetModuleHandleW(
            nullptr);
    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);
    wc.lpszClassName =
        kClassName;

    if (!RegisterClassExW(
            &wc) &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    registered = true;
    return true;
}

} // namespace

bool ShowAddPackageDialog(
    HWND owner,
    PackageRecord& package) {
    package = {};

    if (!EnsureDialogClass()) {
        MessageBoxW(
            owner,
            L"Could not create the Add Git window.",
            L"TocPilot",
            MB_OK |
                MB_ICONERROR);
        return false;
    }

    RECT ownerRect{};
    GetWindowRect(
        owner,
        &ownerRect);

    constexpr int width = 600;
    constexpr int height = 250;

    const int ownerWidth =
        static_cast<int>(
            ownerRect.right -
            ownerRect.left);
    const int ownerHeight =
        static_cast<int>(
            ownerRect.bottom -
            ownerRect.top);

    const int x =
        static_cast<int>(
            ownerRect.left) +
        std::max(
            0,
            (ownerWidth -
             width) /
                2);

    const int y =
        static_cast<int>(
            ownerRect.top) +
        std::max(
            0,
            (ownerHeight -
             height) /
                2);

    DialogContext context;
    context.package =
        &package;

    HWND dialog =
        CreateWindowExW(
            WS_EX_DLGMODALFRAME |
                WS_EX_CONTROLPARENT,
            kClassName,
            L"Add Git Repository",
            WS_CAPTION |
                WS_SYSMENU |
                WS_POPUP,
            x,
            y,
            width,
            height,
            owner,
            nullptr,
            GetModuleHandleW(
                nullptr),
            &context);

    if (!dialog) {
        MessageBoxW(
            owner,
            L"Could not create the Add Git window.",
            L"TocPilot",
            MB_OK |
                MB_ICONERROR);
        return false;
    }

    EnableWindow(
        owner,
        FALSE);
    ShowWindow(
        dialog,
        SW_SHOW);
    UpdateWindow(dialog);

    MSG msg{};
    while (IsWindow(
        dialog)) {
        const BOOL result =
            GetMessageW(
                &msg,
                nullptr,
                0,
                0);

        if (result <= 0) {
            if (result == 0) {
                PostQuitMessage(
                    static_cast<int>(
                        msg.wParam));
            }
            break;
        }

        if (!IsDialogMessageW(
                dialog,
                &msg)) {
            TranslateMessage(
                &msg);
            DispatchMessageW(
                &msg);
        }
    }

    EnableWindow(
        owner,
        TRUE);
    SetActiveWindow(owner);
    return context.accepted;
}

} // namespace tp
