#pragma once

#include <windows.h>

#include <string_view>

namespace tp {

enum class DialogIcon {
    Information,
    Warning,
    Error
};

enum class DialogButtons {
    Ok,
    YesNo
};

int ShowExpandableDialog(
    HWND owner,
    std::wstring_view title,
    std::wstring_view instruction,
    std::wstring_view content,
    std::wstring_view details,
    DialogIcon icon,
    DialogButtons buttons,
    int defaultButton = 0);

} // namespace tp
