#include "add_package_dialog.h"

#include "direct_dll.h"
#include "github_release.h"
#include "provider.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>
#include <utility>

namespace tp {
namespace {

constexpr wchar_t kClassName[] =
    L"TocPilotAddPackageDialog";
constexpr int IDC_REPOSITORY_URL = 2001;
constexpr int IDC_ADD_SOURCE = 2002;
constexpr int IDC_RESULT = 2003;
constexpr int IDC_CLOSE_DIALOG = 2004;
constexpr int IDC_RELEASE_DLL = 2005;
constexpr int IDC_DLL_ASSET_LABEL = 2006;
constexpr int IDC_DLL_ASSET = 2007;

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

std::wstring Trim(
    std::wstring value) {
    while (!value.empty() &&
           std::iswspace(
               value.front())) {
        value.erase(
            value.begin());
    }

    while (!value.empty() &&
           std::iswspace(
               value.back())) {
        value.pop_back();
    }

    return value;
}

DialogContext* Context(
    HWND hwnd) {
    return reinterpret_cast<
        DialogContext*>(
        GetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA));
}

bool ReleaseDllChecked(
    HWND hwnd) {
    return SendMessageW(
               GetDlgItem(
                   hwnd,
                   IDC_RELEASE_DLL),
               BM_GETCHECK,
               0,
               0) ==
        BST_CHECKED;
}

void UpdateDllControls(
    HWND hwnd) {
    const BOOL enabled =
        ReleaseDllChecked(hwnd)
            ? TRUE
            : FALSE;

    EnableWindow(
        GetDlgItem(
            hwnd,
            IDC_DLL_ASSET_LABEL),
        enabled);
    EnableWindow(
        GetDlgItem(
            hwnd,
            IDC_DLL_ASSET),
        enabled);
}

std::filesystem::path ExecutableDirectory() {
    std::wstring buffer(
        32768,
        L'\0');

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));

    if (length == 0 ||
        length >=
            buffer.size()) {
        return {};
    }

    buffer.resize(length);
    return std::filesystem::path(
        buffer).parent_path();
}

bool PrepareReleaseDll(
    HWND hwnd,
    const RepositoryIdentity& identity,
    PackageRecord& package,
    std::wstring& error) {
    error.clear();

    if (identity.provider !=
        ProviderKind::GitHub) {
        error =
            L"Direct DLL management currently supports GitHub repositories only.";
        return false;
    }

    const std::wstring assetName =
        Trim(
            ControlText(
                GetDlgItem(
                    hwnd,
                    IDC_DLL_ASSET)));

    if (assetName.empty()) {
        error =
            L"Enter the exact .dll release asset name.";
        return false;
    }

    package =
        MakeRepositoryPackage(
            L"github",
            identity.repository);

    package.id =
        L"github:" +
        identity.repository +
        L":release:" +
        assetName;
    package.name =
        assetName;
    package.mode =
        L"release";
    package.ref.clear();
    package.releasePolicy =
        L"latest_stable";
    package.asset =
        assetName;
    package.target =
        L"wow_root";
    package.targetPath =
        assetName;
    package.installedRevision.clear();
    package.latestRevision.clear();
    package.installedFiles.clear();

    if (!ValidateDirectDllPackage(
            package,
            error)) {
        return false;
    }

    SetWindowTextW(
        GetDlgItem(
            hwnd,
            IDC_RESULT),
        L"Checking the latest stable GitHub release for that exact asset...");
    UpdateWindow(hwnd);

    GitHubReleaseInfo release;
    if (!FetchLatestStableGitHubRelease(
            identity.repository,
            release,
            error)) {
        return false;
    }

    GitHubReleaseAsset asset;
    if (!FindExactGitHubReleaseAsset(
            release,
            assetName,
            asset,
            error)) {
        return false;
    }

    const auto root =
        ExecutableDirectory();

    if (root.empty()) {
        error =
            L"Could not resolve the TocPilot/WoW directory.";
        return false;
    }

    package.latestRevision =
        release.tag;

    const std::filesystem::path
        destination =
            root /
            package.targetPath;

    const std::wstring warning =
        L"DLLs contain executable code. Continue only if you trust this publisher.\r\n\r\n"
        L"Repository: " +
        identity.repository +
        L"\r\nRelease policy: latest stable\r\nAsset: " +
        package.asset +
        L"\r\nDestination: " +
        destination.wstring() +
        L"\r\n\r\nSecurity software may block or quarantine DLLs. TocPilot will not add exclusions, disable security software, or change antivirus settings. "
        L"When installing or updating, TocPilot writes only to the exact destination above, creates no staged/temp/renamed/backup DLL, and records the release only after SHA-256 verification.\r\n\r\nManage this DLL?";

    if (MessageBoxW(
            hwnd,
            warning.c_str(),
            L"TocPilot - Trust DLL Publisher",
            MB_YESNO |
                MB_ICONWARNING |
                MB_DEFBUTTON2) !=
        IDYES) {
        error.clear();
        return false;
    }

    return true;
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
            L"Could not prepare the package record.");
        return;
    }

    if (ReleaseDllChecked(
            hwnd)) {
        PackageRecord package;

        if (!PrepareReleaseDll(
                hwnd,
                identity,
                package,
                error)) {
            if (!error.empty()) {
                SetWindowTextW(
                    result,
                    (L"DLL package not added: " +
                     error).c_str());
            } else {
                SetWindowTextW(
                    result,
                    L"DLL package was not added.");
            }
            return;
        }

        *context->package =
            std::move(package);
        context->accepted =
            true;
        DestroyWindow(hwnd);
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
                L"Paste a public repository URL. Normal addon packages track a branch; the optional DLL mode tracks one exact asset from the latest stable GitHub release.",
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

        HWND dll =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Manage an exact latest-stable release DLL in the WoW root",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_AUTOCHECKBOX,
                20,
                100,
                540,
                24,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_RELEASE_DLL)),
                instance,
                nullptr);

        HWND assetLabel =
            CreateWindowExW(
                0,
                L"STATIC",
                L"Exact DLL asset filename:",
                WS_CHILD |
                    WS_VISIBLE,
                20,
                132,
                170,
                22,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_DLL_ASSET_LABEL)),
                instance,
                nullptr);

        HWND asset =
            CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    ES_AUTOHSCROLL,
                195,
                128,
                365,
                26,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_DLL_ASSET)),
                instance,
                nullptr);

        HWND add =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Continue",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_DEFPUSHBUTTON,
                20,
                168,
                110,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_ADD_SOURCE)),
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
                168,
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
                L"Normal mode will ask for a branch next. DLL mode verifies that the exact asset exists before showing the one-time trust warning.",
                WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                20,
                210,
                540,
                70,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_RESULT)),
                instance,
                nullptr);

        SetControlFont(intro);
        SetControlFont(edit);
        SetControlFont(dll);
        SetControlFont(assetLabel);
        SetControlFont(asset);
        SetControlFont(add);
        SetControlFont(close);
        SetControlFont(result);

        UpdateDllControls(hwnd);
        SetFocus(edit);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) ==
                IDC_RELEASE_DLL &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            UpdateDllControls(hwnd);
            return 0;
        }

        if (LOWORD(wParam) ==
                IDC_ADD_SOURCE &&
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
            &wc)) {
        if (GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
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
            L"Could not create the Add Package window.",
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
    constexpr int height = 330;

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
            L"Add Package",
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
            L"Could not create the Add Package window.",
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
