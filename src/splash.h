#pragma once

#include <windows.h>

namespace tp {

enum class StartupSplashPhase {
    CheckingAppUpdate,
    ScanningAddonUpdates,
    AwaitingContinue
};

bool ShowStartupSplash(
    HINSTANCE instance,
    HWND mainWindow);

void SetStartupSplashPhase(
    StartupSplashPhase phase);

bool IsStartupSplashActive();

} // namespace tp
