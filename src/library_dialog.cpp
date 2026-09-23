#include "library_dialog.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kClassName[] =
    L"TocPilotRepositoryLibraryDialog";

constexpr int IDC_LIBRARY_LIST = 4101;
constexpr int IDC_SELECT_ALL = 4102;
constexpr int IDC_INSTALL_SELECTED = 4103;
constexpr int IDC_CANCEL_LIBRARY = 4104;
constexpr int IDC_LIBRARY_STATUS = 4105;

struct DialogContext {
    std::wstring repository;
    std::wstring branch;
    const std::vector<AddonCandidate>* candidates = nullptr;
    std::vector<std::size_t>* selectedIndices = nullptr;
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
                GetStockObject(
                    DEFAULT_GUI_FONT)),
            TRUE);
    }
}

void UpdateSelectionState(HWND hwnd) {
    const LRESULT count =
        SendMessageW(
            GetDlgItem(
                hwnd,
                IDC_LIBRARY_LIST),
            LB_GETSELCOUNT,
            0,
            0);

    EnableWindow(
        GetDlgItem(
            hwnd,
            IDC_INSTALL_SELECTED),
        count > 0 ? TRUE : FALSE);

    std::wstring status;
    if (count <= 0) {
        status =
            L"Select one or more addons to manage independently.";
    } else {
        status =
            std::to_wstring(
                static_cast<std::size_t>(
                    count)) +
            (count == 1
                ? L" addon selected."
                : L" addons selected.");
    }

    SetWindowTextW(
        GetDlgItem(
            hwnd,
            IDC_LIBRARY_STATUS),
        status.c_str());
}

void SelectAll(HWND hwnd) {
    const HWND list =
        GetDlgItem(
            hwnd,
            IDC_LIBRARY_LIST);

    const LRESULT count =
        SendMessageW(
            list,
            LB_GETCOUNT,
            0,
            0);

    for (LRESULT i = 0; i < count; ++i) {
        SendMessageW(
            list,
            LB_SETSEL,
            TRUE,
            static_cast<LPARAM>(i));
    }

    UpdateSelectionState(hwnd);
}

void AcceptSelection(HWND hwnd) {
    DialogContext* context =
        Context(hwnd);

    if (!context ||
        !context->candidates ||
        !context->selectedIndices) {
        return;
    }

    const HWND list =
        GetDlgItem(
            hwnd,
            IDC_LIBRARY_LIST);

    const LRESULT selectedCount =
        SendMessageW(
            list,
            LB_GETSELCOUNT,
            0,
            0);

    if (selectedCount <= 0) {
        return;
    }

    std::vector<int> rows(
        static_cast<std::size_t>(
            selectedCount));

    const LRESULT copied =
        SendMessageW(
            list,
            LB_GETSELITEMS,
            static_cast<WPARAM>(
                rows.size()),
            reinterpret_cast<LPARAM>(
                rows.data()));

    if (copied <= 0) {
        return;
    }

    context->selectedIndices->clear();
    context->selectedIndices->reserve(
        static_cast<std::size_t>(
            copied));

    for (LRESULT i = 0; i < copied; ++i) {
        const int row =
            rows[static_cast<std::size_t>(i)];

        if (row < 0 ||
            static_cast<std::size_t>(
                row) >=
                context->candidates->size()) {
            continue;
        }

        context->selectedIndices->push_back(
            static_cast<std::size_t>(
                row));
    }

    if (context->selectedIndices->empty()) {
        return;
    }

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
        DialogContext* context =
            Context(hwnd);

        const HINSTANCE instance =
            GetModuleHandleW(
                nullptr);

        std::wstring intro =
            L"TocPilot detected a repository library.";

        if (context) {
            intro +=
                L"\r\n" +
                context->repository +
                L" / " +
                context->branch;
        }

        HWND introControl =
            CreateWindowExW(
                0,
                L"STATIC",
                intro.c_str(),
                WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                20,
                16,
                560,
                42,
                hwnd,
                nullptr,
                instance,
                nullptr);

        HWND list =
            CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"LISTBOX",
                nullptr,
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    WS_VSCROLL |
                    LBS_EXTENDEDSEL |
                    LBS_NOINTEGRALHEIGHT,
                20,
                66,
                560,
                245,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_LIBRARY_LIST)),
                instance,
                nullptr);

        if (context &&
            context->candidates) {
            for (const auto& candidate :
                 *context->candidates) {
                std::wstring label =
                    candidate.installFolder;

                if (!candidate.repositoryRelativePath.empty()) {
                    label +=
                        L"    [" +
                        candidate.repositoryRelativePath
                            .generic_wstring() +
                        L"]";
                }

                SendMessageW(
                    list,
                    LB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(
                        label.c_str()));
            }
        }

        HWND status =
            CreateWindowExW(
                0,
                L"STATIC",
                L"Select one or more addons to manage independently.",
                WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                20,
                320,
                560,
                22,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_LIBRARY_STATUS)),
                instance,
                nullptr);

        HWND selectAll =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Select All",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_PUSHBUTTON,
                20,
                352,
                105,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_SELECT_ALL)),
                instance,
                nullptr);

        HWND install =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Install Selected",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_DEFPUSHBUTTON,
                305,
                352,
                145,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_INSTALL_SELECTED)),
                instance,
                nullptr);

        HWND cancel =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Cancel",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_PUSHBUTTON,
                470,
                352,
                110,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_CANCEL_LIBRARY)),
                instance,
                nullptr);

        SetControlFont(introControl);
        SetControlFont(list);
        SetControlFont(status);
        SetControlFont(selectAll);
        SetControlFont(install);
        SetControlFont(cancel);

        EnableWindow(
            install,
            FALSE);

        SetFocus(list);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) ==
                IDC_LIBRARY_LIST &&
            HIWORD(wParam) ==
                LBN_SELCHANGE) {
            UpdateSelectionState(
                hwnd);
            return 0;
        }

        if (LOWORD(wParam) ==
                IDC_SELECT_ALL &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            SelectAll(hwnd);
            return 0;
        }

        if (LOWORD(wParam) ==
                IDC_INSTALL_SELECTED &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            AcceptSelection(hwnd);
            return 0;
        }

        if (LOWORD(wParam) ==
                IDC_CANCEL_LIBRARY &&
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

bool ShowRepositoryLibraryDialog(
    HWND owner,
    std::wstring_view repository,
    std::wstring_view branch,
    const std::vector<AddonCandidate>& candidates,
    std::vector<std::size_t>& selectedIndices) {
    selectedIndices.clear();

    if (candidates.empty() ||
        !EnsureDialogClass()) {
        return false;
    }

    RECT ownerRect{};
    GetWindowRect(
        owner,
        &ownerRect);

    constexpr int width = 620;
    constexpr int height = 430;

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
            (ownerWidth - width) / 2);

    const int y =
        static_cast<int>(
            ownerRect.top) +
        std::max(
            0,
            (ownerHeight - height) / 2);

    DialogContext context;
    context.repository =
        std::wstring(repository);
    context.branch =
        std::wstring(branch);
    context.candidates =
        &candidates;
    context.selectedIndices =
        &selectedIndices;

    HWND dialog =
        CreateWindowExW(
            WS_EX_DLGMODALFRAME |
                WS_EX_CONTROLPARENT,
            kClassName,
            L"Repository Library",
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
        return false;
    }

    EnableWindow(
        owner,
        FALSE);
    ShowWindow(
        dialog,
        SW_SHOW);
    UpdateWindow(
        dialog);

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
    SetActiveWindow(
        owner);

    return context.accepted;
}

} // namespace tp
