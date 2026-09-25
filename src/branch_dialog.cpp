#include "branch_dialog.h"

#include "git_refs.h"

#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace tp {
namespace {

constexpr wchar_t kClassName[] =
    L"TocPilotBranchDialog";
constexpr UINT WM_TP_BRANCHES_READY =
    WM_APP + 30;

constexpr int IDC_BRANCH_COMBO = 3001;
constexpr int IDC_SAVE_BRANCH = 3002;
constexpr int IDC_BRANCH_STATUS = 3003;
constexpr int IDC_CANCEL_BRANCH = 3004;

struct LoadResult {
    bool ok = false;
    GitRemoteRepositoryInfo info;
    std::wstring error;
};

struct DialogContext {
    std::wstring host;
    std::wstring providerName;
    std::wstring repository;
    std::wstring currentRef;
    BranchSelection* selection = nullptr;
    GitRemoteRepositoryInfo info;
    bool accepted = false;
};

DialogContext* Context(HWND hwnd) {
    return reinterpret_cast<DialogContext*>(
        GetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA));
}

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

void StartLoad(
    HWND hwnd,
    std::wstring host,
    std::wstring repository) {
    std::thread(
        [hwnd,
         host = std::move(host),
         repository = std::move(repository)]() {
            auto result =
                std::make_unique<LoadResult>();

            result->ok =
                FetchPublicGitRepositoryInfo(
                    host,
                    repository,
                    result->info,
                    result->error);

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

void PopulateBranches(
    HWND hwnd,
    DialogContext& context) {
    const HWND combo =
        GetDlgItem(hwnd, IDC_BRANCH_COMBO);

    SendMessageW(
        combo,
        CB_RESETCONTENT,
        0,
        0);

    int selected = -1;

    for (std::size_t i = 0;
         i < context.info.branches.size();
         ++i) {
        const auto& branch =
            context.info.branches[i];

        const LRESULT index =
            SendMessageW(
                combo,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    branch.name.c_str()));

        if (index >= 0 &&
            ((!context.currentRef.empty() &&
              branch.name ==
                  context.currentRef) ||
             (context.currentRef.empty() &&
              branch.name ==
                  context.info.defaultBranch))) {
            selected =
                static_cast<int>(index);
        }
    }

    if (selected < 0 &&
        !context.info.branches.empty()) {
        selected = 0;
    }

    SendMessageW(
        combo,
        CB_SETCURSEL,
        static_cast<WPARAM>(selected),
        0);

    std::wstring status =
        L"Loaded " +
        std::to_wstring(
            context.info.branches.size()) +
        L" branch";

    if (context.info.branches.size() != 1) {
        status += L"es";
    }

    status +=
        L". Choose the branch TocPilot should track.";

    SetWindowTextW(
        GetDlgItem(
            hwnd,
            IDC_BRANCH_STATUS),
        status.c_str());

    EnableWindow(
        GetDlgItem(
            hwnd,
            IDC_SAVE_BRANCH),
        selected >= 0 ? TRUE : FALSE);
}

LRESULT CALLBACK DialogProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_NCCREATE: {
        const auto* create =
            reinterpret_cast<CREATESTRUCTW*>(
                lParam);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                create->lpCreateParams));

        return TRUE;
    }

    case WM_CREATE: {
        DialogContext* context =
            Context(hwnd);

        const auto instance =
            GetModuleHandleW(nullptr);

        std::wstring source =
            context
                ? context->providerName +
                    L" repository: " +
                    context->repository
                : L"Repository";

        HWND label = CreateWindowExW(
            0,
            L"STATIC",
            source.c_str(),
            WS_CHILD | WS_VISIBLE |
                SS_PATHELLIPSIS,
            20,
            18,
            540,
            22,
            hwnd,
            nullptr,
            instance,
            nullptr);

        HWND combo = CreateWindowExW(
            0,
            L"COMBOBOX",
            nullptr,
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            20,
            50,
            540,
            220,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_BRANCH_COMBO)),
            instance,
            nullptr);

        HWND status = CreateWindowExW(
            0,
            L"STATIC",
            L"Loading branches via Git smart HTTP...",
            WS_CHILD | WS_VISIBLE |
                SS_LEFT,
            20,
            88,
            540,
            52,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_BRANCH_STATUS)),
            instance,
            nullptr);

        HWND save = CreateWindowExW(
            0,
            L"BUTTON",
            L"Use branch",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                BS_DEFPUSHBUTTON,
            20,
            152,
            115,
            30,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_SAVE_BRANCH)),
            instance,
            nullptr);

        HWND cancel = CreateWindowExW(
            0,
            L"BUTTON",
            L"Cancel",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP,
            450,
            152,
            110,
            30,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IDC_CANCEL_BRANCH)),
            instance,
            nullptr);

        SetControlFont(label);
        SetControlFont(combo);
        SetControlFont(status);
        SetControlFont(save);
        SetControlFont(cancel);

        EnableWindow(save, FALSE);

        if (context) {
            StartLoad(
                hwnd,
                context->host,
                context->repository);
        }

        return 0;
    }

    case WM_TP_BRANCHES_READY: {
        std::unique_ptr<LoadResult> result(
            reinterpret_cast<LoadResult*>(
                lParam));

        DialogContext* context =
            Context(hwnd);

        if (!context) {
            return 0;
        }

        if (!result->ok) {
            const std::wstring messageText =
                context->providerName +
                L" branch lookup failed: " +
                result->error;

            SetWindowTextW(
                GetDlgItem(
                    hwnd,
                    IDC_BRANCH_STATUS),
                messageText.c_str());

            return 0;
        }

        context->info =
            std::move(result->info);

        PopulateBranches(
            hwnd,
            *context);

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) ==
                IDC_SAVE_BRANCH &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            DialogContext* context =
                Context(hwnd);

            const LRESULT selected =
                SendMessageW(
                    GetDlgItem(
                        hwnd,
                        IDC_BRANCH_COMBO),
                    CB_GETCURSEL,
                    0,
                    0);

            if (!context ||
                !context->selection ||
                selected == CB_ERR ||
                selected < 0 ||
                static_cast<std::size_t>(
                    selected) >=
                    context->info.branches.size()) {
                return 0;
            }

            const auto& branch =
                context->info.branches[
                    static_cast<std::size_t>(
                        selected)];

            context->selection->name =
                branch.name;
            context->selection->sha =
                branch.sha;
            context->selection->repositoryInfo =
                context->info;
            context->accepted = true;

            DestroyWindow(hwnd);
            return 0;
        }

        if (LOWORD(wParam) ==
                IDC_CANCEL_BRANCH &&
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
    static bool registered = false;

    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DialogProc;
    wc.hInstance =
        GetModuleHandleW(nullptr);
    wc.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc) &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    registered = true;
    return true;
}

} // namespace

bool ShowBranchDialog(
    HWND owner,
    const PackageRecord& package,
    BranchSelection& selection) {
    selection = {};

    std::wstring host;
    std::wstring providerName;

    if (package.provider == L"github") {
        host = L"github.com";
        providerName = L"GitHub";
    } else if (package.provider == L"gitlab") {
        host = L"gitlab.com";
        providerName = L"GitLab";
    } else {
        MessageBoxW(
            owner,
            L"Branch browsing is not available for this package provider.",
            L"TocPilot - Set Branch",
            MB_OK | MB_ICONINFORMATION);

        return false;
    }

    if (!EnsureDialogClass()) {
        MessageBoxW(
            owner,
            L"Could not create the branch selection window.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);

        return false;
    }

    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);

    constexpr int width = 600;
    constexpr int height = 235;

    const int ownerWidth =
        static_cast<int>(
            ownerRect.right -
            ownerRect.left);

    const int ownerHeight =
        static_cast<int>(
            ownerRect.bottom -
            ownerRect.top);

    const int x =
        static_cast<int>(ownerRect.left) +
        std::max(
            0,
            (ownerWidth - width) / 2);

    const int y =
        static_cast<int>(ownerRect.top) +
        std::max(
            0,
            (ownerHeight - height) / 2);

    DialogContext context;
    context.host =
        std::move(host);
    context.providerName =
        std::move(providerName);
    context.repository =
        package.repository;
    context.currentRef =
        package.ref;
    context.selection =
        &selection;

    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME |
            WS_EX_CONTROLPARENT,
        kClassName,
        (L"Set " +
         context.providerName +
         L" Branch").c_str(),
        WS_CAPTION |
            WS_SYSMENU |
            WS_POPUP,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        GetModuleHandleW(nullptr),
        &context);

    if (!dialog) {
        MessageBoxW(
            owner,
            L"Could not create the branch selection window.",
            L"TocPilot",
            MB_OK | MB_ICONERROR);

        return false;
    }

    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    MSG msg{};

    while (IsWindow(dialog)) {
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
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);

    return context.accepted;
}

} // namespace tp
