#pragma once

#include <windows.h>

namespace tp {

struct ToolbarIcon {
    HICON normal = nullptr;
    HICON disabled = nullptr;
};

struct ToolbarIcons {
    ToolbarIcon update;
    ToolbarIcon refresh;
    ToolbarIcon addRepository;
    ToolbarIcon reinstallRepository;
    ToolbarIcon removeRepository;
    ToolbarIcon inspect;
    ToolbarIcon scan;
    ToolbarIcon accountSync;
    ToolbarIcon info;
    ToolbarIcon advanced;
    HANDLE fontResource = nullptr;
    int pixelSize = 0;
};

bool InitializeToolbarIcons(
    HINSTANCE instance,
    int pixelSize,
    ToolbarIcons& icons);

void DestroyToolbarIcons(
    ToolbarIcons& icons);

HWND CreateToolbarTooltip(
    HWND owner);

void AddToolbarTooltip(
    HWND tooltip,
    HWND owner,
    HWND control,
    const wchar_t* text);

}  // namespace tp
