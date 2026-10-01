#pragma once

#include <windows.h>

namespace tp {

enum class StartupSplashPhase {
    CheckingAppUpdate,
    ApplyingAppUpdate,
    ScanningAddonUpdates,
    AppUpdateFailed
};

bool ShowStartupSplash(
    HINSTANCE instance,
    HWND mainWindow);

void SetStartupSplashPhase(
    StartupSplashPhase phase);

void CompleteStartupSplash();

bool IsStartupSplashActive();

} // namespace tp
