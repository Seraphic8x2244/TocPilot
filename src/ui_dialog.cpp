#include "ui_dialog.h"

#include <commctrl.h>

#include <string>

#ifdef _MSC_VER
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

namespace tp {
namespace {

PCWSTR TaskDialogIcon(DialogIcon icon) {
    switch (icon) {
    case DialogIcon::Information:
        return TD_INFORMATION_ICON;
    case DialogIcon::Warning:
        return TD_WARNING_ICON;
    case DialogIcon::Error:
        return TD_ERROR_ICON;
    }

    return TD_INFORMATION_ICON;
}

UINT MessageBoxIcon(DialogIcon icon) {
    switch (icon) {
    case DialogIcon::Information:
        return MB_ICONINFORMATION;
    case DialogIcon::Warning:
        return MB_ICONWARNING;
    case DialogIcon::Error:
        return MB_ICONERROR;
    }

    return MB_ICONINFORMATION;
}

TASKDIALOG_COMMON_BUTTON_FLAGS CommonButtons(
    DialogButtons buttons) {
    switch (buttons) {
    case DialogButtons::Ok:
        return TDCBF_OK_BUTTON;
    case DialogButtons::YesNo:
        return static_cast<TASKDIALOG_COMMON_BUTTON_FLAGS>(
            TDCBF_YES_BUTTON |
            TDCBF_NO_BUTTON);
    }

    return TDCBF_OK_BUTTON;
}

UINT MessageBoxButtons(DialogButtons buttons) {
    switch (buttons) {
    case DialogButtons::Ok:
        return MB_OK;
    case DialogButtons::YesNo:
        return MB_YESNO;
    }

    return MB_OK;
}

} // namespace

int ShowExpandableDialog(
    HWND owner,
    std::wstring_view title,
    std::wstring_view instruction,
    std::wstring_view content,
    std::wstring_view details,
    DialogIcon icon,
    DialogButtons buttons,
    int defaultButton) {
    const std::wstring titleText(title);
    const std::wstring instructionText(instruction);
    const std::wstring contentText(content);
    const std::wstring detailsText(details);

    TASKDIALOGCONFIG config{};
    config.cbSize =
        sizeof(config);
    config.hwndParent =
        owner;
    config.hInstance =
        GetModuleHandleW(nullptr);
    config.dwFlags =
        TDF_ALLOW_DIALOG_CANCELLATION |
        TDF_SIZE_TO_CONTENT;
    config.dwCommonButtons =
        CommonButtons(buttons);
    config.pszWindowTitle =
        titleText.c_str();
    config.pszMainInstruction =
        instructionText.empty()
            ? nullptr
            : instructionText.c_str();
    config.pszContent =
        contentText.empty()
            ? nullptr
            : contentText.c_str();
    config.pszMainIcon =
        TaskDialogIcon(icon);

    if (!detailsText.empty()) {
        config.pszExpandedInformation =
            detailsText.c_str();
        config.pszExpandedControlText =
            L"Hide details";
        config.pszCollapsedControlText =
            L"Show details";
    }

    if (defaultButton != 0) {
        config.nDefaultButton =
            defaultButton;
    } else if (buttons ==
               DialogButtons::YesNo) {
        config.nDefaultButton =
            IDNO;
    }

    int pressed = 0;
    const HRESULT result =
        TaskDialogIndirect(
            &config,
            &pressed,
            nullptr,
            nullptr);

    if (SUCCEEDED(result)) {
        return pressed;
    }

    std::wstring fallback =
        instructionText;

    if (!fallback.empty() &&
        !contentText.empty()) {
        fallback +=
            L"\r\n\r\n";
    }

    fallback +=
        contentText;

    if (!detailsText.empty()) {
        fallback +=
            L"\r\n\r\nDetails:\r\n" +
            detailsText;
    }

    UINT flags =
        MessageBoxButtons(buttons) |
        MessageBoxIcon(icon);

    if (buttons ==
            DialogButtons::YesNo &&
        (defaultButton == IDNO ||
         defaultButton == 0)) {
        flags |=
            MB_DEFBUTTON2;
    }

    return MessageBoxW(
        owner,
        fallback.c_str(),
        titleText.c_str(),
        flags);
}

} // namespace tp
