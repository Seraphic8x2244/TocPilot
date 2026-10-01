#include "repository_selection_dialog.h"

#include "mpq_package.h"
#include "repository_selection.h"

#include <commctrl.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kClassName[] =
    L"TocPilotRepositorySelectionDialog";

constexpr int IDC_COMPONENT_LIST = 6101;
constexpr int IDC_SELECTION_STATUS = 6102;
constexpr int IDC_ADD_SELECTED = 6103;
constexpr int IDC_CANCEL_SELECTION = 6104;

struct DialogContext {
    const RepositoryDiscoveryResult* discovery = nullptr;
    std::filesystem::path wowRoot;
    std::vector<RepositorySelectionRow> rows;
    std::vector<std::size_t>* selectedIndices = nullptr;
    bool accepted = false;
};

DialogContext* Context(
    HWND hwnd) {
    return reinterpret_cast<
        DialogContext*>(
        GetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA));
}

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

std::wstring DiscoveryStatus(
    const RepositoryDiscoveryResult& discovery,
    std::size_t count) {
    std::wstring status =
        L"Found " +
        std::to_wstring(count) +
        (count == 1
            ? L" supported component."
            : L" supported components.");

    if (discovery.addonLayout.kind ==
        RepositoryAddonLayoutKind::MixedAmbiguous) {
        status +=
            L" Branch addons are not selectable because the repository has both root and immediate-child addon roots.";
    } else if (
        discovery.branchAttempted &&
        !discovery.branchSucceeded &&
        !discovery.branchError.empty()) {
        status +=
            L" Branch scan unavailable: " +
            discovery.branchError;
    }

    if (discovery.releaseAttempted &&
        !discovery.releaseSucceeded &&
        !discovery.releaseError.empty()) {
        status +=
            L" Latest-stable release scan unavailable: " +
            discovery.releaseError;
    }

    status +=
        L" Check one or more components, then choose Add Selected.";
    return status;
}

bool MakeMpqPreviewPackage(
    const RepositoryDiscoveryResult& discovery,
    const RepositoryCandidate& candidate,
    PackageRecord& package,
    std::wstring& error) {
    error.clear();

    if (discovery.provider != L"github" ||
        candidate.kind !=
            RepositoryCandidateKind::Mpq ||
        candidate.releaseAsset.name.empty() ||
        candidate.releaseTag.empty()) {
        error =
            L"MPQ discovery metadata is incomplete.";
        return false;
    }

    package =
        MakeRepositoryPackage(
            L"github",
            discovery.repository);

    package.id =
        L"github:" +
        discovery.repository +
        L":release:" +
        candidate.releaseAsset.name;
    package.name =
        candidate.releaseAsset.name;
    package.mode =
        L"release";
    package.ref.clear();
    package.releasePolicy =
        L"latest_stable";
    package.asset =
        candidate.releaseAsset.name;
    package.sourcePath.clear();
    package.target =
        L"data";
    package.targetPath.clear();
    package.installedRevision.clear();
    package.latestRevision =
        candidate.releaseTag;
    package.installedFiles.clear();
    return true;
}

bool BuildPreviewRows(
    const RepositoryDiscoveryResult& discovery,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& existingPackages,
    std::vector<RepositorySelectionRow>& rows,
    std::wstring& error) {
    std::vector<std::wstring>
        mpqDestinations(
            discovery.candidates.size());
    std::vector<std::wstring>
        mpqErrors(
            discovery.candidates.size());

    std::vector<PackageRecord> reserved =
        existingPackages;

    for (std::size_t i = 0;
         i < discovery.candidates.size();
         ++i) {
        const auto& candidate =
            discovery.candidates[i];

        if (candidate.kind !=
            RepositoryCandidateKind::Mpq) {
            continue;
        }

        PackageRecord preview;
        std::wstring previewError;

        if (!MakeMpqPreviewPackage(
                discovery,
                candidate,
                preview,
                previewError) ||
            !AssignMpqTargetPath(
                wowRoot,
                reserved,
                preview,
                previewError)) {
            mpqErrors[i] =
                std::move(previewError);
            continue;
        }

        mpqDestinations[i] =
            preview.targetPath;
        reserved.push_back(
            std::move(preview));
    }

    return BuildRepositorySelectionRows(
        discovery,
        mpqDestinations,
        mpqErrors,
        rows,
        error);
}

void AddColumn(
    HWND list,
    int index,
    int width,
    const wchar_t* text) {
    LVCOLUMNW column{};
    column.mask =
        LVCF_TEXT |
        LVCF_WIDTH |
        LVCF_SUBITEM;
    column.pszText =
        const_cast<wchar_t*>(text);
    column.cx =
        width;
    column.iSubItem =
        index;

    ListView_InsertColumn(
        list,
        index,
        &column);
}

void AddRow(
    HWND list,
    int rowIndex,
    const RepositorySelectionRow& row) {
    LVITEMW item{};
    item.mask =
        LVIF_TEXT;
    item.iItem =
        rowIndex;
    item.iSubItem =
        0;
    item.pszText =
        const_cast<wchar_t*>(
            row.type.c_str());

    const int inserted =
        ListView_InsertItem(
            list,
            &item);

    if (inserted < 0) {
        return;
    }

    ListView_SetItemText(
        list,
        inserted,
        1,
        const_cast<wchar_t*>(
            row.name.c_str()));
    ListView_SetItemText(
        list,
        inserted,
        2,
        const_cast<wchar_t*>(
            row.source.c_str()));
    ListView_SetItemText(
        list,
        inserted,
        3,
        const_cast<wchar_t*>(
            row.destination.c_str()));
}

void UpdateSelectionState(
    HWND hwnd) {
    DialogContext* context =
        Context(hwnd);

    if (!context) {
        return;
    }

    const HWND list =
        GetDlgItem(
            hwnd,
            IDC_COMPONENT_LIST);

    std::size_t selected = 0;
    const RepositorySelectionRow*
        unavailable = nullptr;

    for (std::size_t i = 0;
         i < context->rows.size();
         ++i) {
        if (!ListView_GetCheckState(
                list,
                static_cast<int>(i))) {
            continue;
        }

        ++selected;

        if (!context->rows[i].selectable &&
            !unavailable) {
            unavailable =
                &context->rows[i];
        }
    }

    HWND add =
        GetDlgItem(
            hwnd,
            IDC_ADD_SELECTED);

    if (unavailable) {
        EnableWindow(
            add,
            FALSE);

        const std::wstring message =
            unavailable->name +
            L" cannot be selected: " +
            unavailable->unavailableReason;

        SetWindowTextW(
            GetDlgItem(
                hwnd,
                IDC_SELECTION_STATUS),
            message.c_str());
        return;
    }

    EnableWindow(
        add,
        selected > 0
            ? TRUE
            : FALSE);

    if (selected == 0) {
        SetWindowTextW(
            GetDlgItem(
                hwnd,
                IDC_SELECTION_STATUS),
            DiscoveryStatus(
                *context->discovery,
                context->rows.size())
                .c_str());
        return;
    }

    const std::wstring message =
        std::to_wstring(selected) +
        (selected == 1
            ? L" component selected."
            : L" components selected.");

    SetWindowTextW(
        GetDlgItem(
            hwnd,
            IDC_SELECTION_STATUS),
        message.c_str());
}

bool ConfirmDllTrust(
    HWND hwnd,
    const DialogContext& context,
    const RepositorySelectionRow& row) {
    if (!row.requiresDllTrust ||
        !context.discovery ||
        row.candidateIndex >=
            context.discovery->candidates.size()) {
        return true;
    }

    const auto& candidate =
        context.discovery->candidates[
            row.candidateIndex];

    const std::filesystem::path
        destination =
            context.wowRoot /
            candidate.releaseAsset.name;

    const std::wstring warning =
        L"DLLs contain executable code. Continue only if you trust this publisher.\r\n\r\n"
        L"Repository: " +
        context.discovery->repository +
        L"\r\nSource: latest stable release " +
        candidate.releaseTag +
        L"\r\nSelected asset: " +
        candidate.releaseAsset.name +
        L"\r\nDestination: " +
        destination.wstring() +
        L"\r\n\r\nSecurity software may block or quarantine DLLs. TocPilot will not add exclusions, disable security software, or change antivirus settings. "
        L"When installing or updating, TocPilot writes only to the exact destination above, creates no staged/temp/renamed/backup DLL, and records the release only after SHA-256 verification.\r\n\r\nAdd this DLL to the selected components?";

    return MessageBoxW(
               hwnd,
               warning.c_str(),
               L"TocPilot - Trust DLL Publisher",
               MB_YESNO |
                   MB_ICONWARNING |
                   MB_DEFBUTTON2) ==
        IDYES;
}

void AcceptSelection(
    HWND hwnd) {
    DialogContext* context =
        Context(hwnd);

    if (!context ||
        !context->selectedIndices) {
        return;
    }

    const HWND list =
        GetDlgItem(
            hwnd,
            IDC_COMPONENT_LIST);

    std::vector<std::size_t>
        selected;

    for (std::size_t i = 0;
         i < context->rows.size();
         ++i) {
        if (!ListView_GetCheckState(
                list,
                static_cast<int>(i))) {
            continue;
        }

        if (!context->rows[i].selectable) {
            return;
        }

        selected.push_back(
            context->rows[i]
                .candidateIndex);
    }

    if (selected.empty()) {
        return;
    }

    for (std::size_t i = 0;
         i < context->rows.size();
         ++i) {
        if (!ListView_GetCheckState(
                list,
                static_cast<int>(i))) {
            continue;
        }

        if (!ConfirmDllTrust(
                hwnd,
                *context,
                context->rows[i])) {
            SetWindowTextW(
                GetDlgItem(
                    hwnd,
                    IDC_SELECTION_STATUS),
                L"DLL trust confirmation was declined. No components were added.");
            return;
        }
    }

    *context->selectedIndices =
        std::move(selected);
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

        const auto instance =
            GetModuleHandleW(
                nullptr);

        std::wstring intro =
            L"Available components";

        if (context &&
            context->discovery) {
            intro +=
                L" in " +
                context->discovery
                    ->repository;
        }

        HWND introControl =
            CreateWindowExW(
                0,
                L"STATIC",
                intro.c_str(),
                WS_CHILD |
                    WS_VISIBLE |
                    SS_PATHELLIPSIS,
                20,
                16,
                740,
                22,
                hwnd,
                nullptr,
                instance,
                nullptr);

        HWND list =
            CreateWindowExW(
                WS_EX_CLIENTEDGE,
                WC_LISTVIEWW,
                L"",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    LVS_REPORT |
                    LVS_SHOWSELALWAYS,
                20,
                46,
                740,
                270,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_COMPONENT_LIST)),
                instance,
                nullptr);

        ListView_SetExtendedListViewStyleEx(
            list,
            LVS_EX_CHECKBOXES |
                LVS_EX_FULLROWSELECT |
                LVS_EX_DOUBLEBUFFER,
            LVS_EX_CHECKBOXES |
                LVS_EX_FULLROWSELECT |
                LVS_EX_DOUBLEBUFFER);

        AddColumn(
            list,
            0,
            105,
            L"Type");
        AddColumn(
            list,
            1,
            175,
            L"Component");
        AddColumn(
            list,
            2,
            220,
            L"Source");
        AddColumn(
            list,
            3,
            220,
            L"Destination");

        if (context) {
            for (std::size_t i = 0;
                 i < context->rows.size();
                 ++i) {
                AddRow(
                    list,
                    static_cast<int>(i),
                    context->rows[i]);
            }
        }

        HWND status =
            CreateWindowExW(
                0,
                L"STATIC",
                context &&
                        context->discovery
                    ? DiscoveryStatus(
                          *context->discovery,
                          context->rows.size())
                          .c_str()
                    : L"",
                WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                20,
                326,
                740,
                50,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_SELECTION_STATUS)),
                instance,
                nullptr);

        HWND add =
            CreateWindowExW(
                0,
                L"BUTTON",
                L"Add Selected",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_DEFPUSHBUTTON,
                20,
                384,
                125,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_ADD_SELECTED)),
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
                650,
                384,
                110,
                30,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        IDC_CANCEL_SELECTION)),
                instance,
                nullptr);

        SetControlFont(introControl);
        SetControlFont(list);
        SetControlFont(status);
        SetControlFont(add);
        SetControlFont(cancel);

        EnableWindow(
            add,
            FALSE);

        SetFocus(list);
        return 0;
    }

    case WM_NOTIFY: {
        const auto* header =
            reinterpret_cast<NMHDR*>(
                lParam);

        if (header &&
            header->idFrom ==
                IDC_COMPONENT_LIST &&
            header->code ==
                LVN_ITEMCHANGED) {
            UpdateSelectionState(
                hwnd);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) ==
                IDC_ADD_SELECTED &&
            HIWORD(wParam) ==
                BN_CLICKED) {
            AcceptSelection(hwnd);
            return 0;
        }

        if (LOWORD(wParam) ==
                IDC_CANCEL_SELECTION &&
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

bool ShowRepositorySelectionDialog(
    HWND owner,
    const RepositoryDiscoveryResult& discovery,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& existingPackages,
    std::vector<std::size_t>& selectedIndices,
    std::wstring& error) {
    selectedIndices.clear();
    error.clear();

    if (discovery.candidates.empty()) {
        error =
            L"No supported addon, DLL, or MPQ components were discovered.";

        if (discovery.addonLayout.kind ==
            RepositoryAddonLayoutKind::MixedAmbiguous) {
            error +=
                L" The branch addon layout is ambiguous because both root and immediate-child addon roots were found.";
        } else if (
            discovery.branchAttempted &&
            !discovery.branchSucceeded &&
            !discovery.branchError.empty()) {
            error +=
                L" Branch scan: " +
                discovery.branchError;
        }

        if (discovery.releaseAttempted &&
            !discovery.releaseSucceeded &&
            !discovery.releaseError.empty()) {
            error +=
                L" Latest-stable release scan: " +
                discovery.releaseError;
        }

        return false;
    }

    if (!EnsureDialogClass()) {
        error =
            L"Could not create the repository component selection window.";
        return false;
    }

    DialogContext context;
    context.discovery =
        &discovery;
    context.wowRoot =
        wowRoot;
    context.selectedIndices =
        &selectedIndices;

    if (!BuildPreviewRows(
            discovery,
            wowRoot,
            existingPackages,
            context.rows,
            error)) {
        return false;
    }

    RECT ownerRect{};
    GetWindowRect(
        owner,
        &ownerRect);

    constexpr int width = 800;
    constexpr int height = 470;

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

    HWND dialog =
        CreateWindowExW(
            WS_EX_DLGMODALFRAME |
                WS_EX_CONTROLPARENT,
            kClassName,
            L"Add Git - Choose Components",
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
        error =
            L"Could not create the repository component selection window.";
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
    SetActiveWindow(
        owner);

    return context.accepted;
}

} // namespace tp
