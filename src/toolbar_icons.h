#pragma once

#include <windows.h>

namespace tp {

struct ToolbarIcons {
    HICON update = nullptr;
    HICON refresh = nullptr;
    HICON addRepository = nullptr;
    HICON reinstallRepository = nullptr;
    HICON removeRepository = nullptr;
    HICON inspect = nullptr;
    HICON scan = nullptr;
    HICON tocPilot = nullptr;
    HICON advanced = nullptr;
    HANDLE fontResource = nullptr;
};

bool InitializeToolbarIcons(HINSTANCE instance, ToolbarIcons& icons);
void DestroyToolbarIcons(ToolbarIcons& icons);
void ApplyToolbarIcon(HWND button, HICON icon, const wchar_t* accessibleName);
HWND CreateToolbarTooltip(HWND owner);
void AddToolbarTooltip(HWND tooltip, HWND owner, HWND control, const wchar_t* text);

}  // namespace tp
