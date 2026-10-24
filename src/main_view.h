#pragma once

namespace tp::ui {

enum class MainView { Packages, AccountSync, TocPilot };

constexpr bool ShowsPackages(MainView view) {
    return view == MainView::Packages;
}

constexpr bool ShowsSync(MainView view) {
    return view == MainView::AccountSync;
}

constexpr bool ShowsTocPilot(MainView view) {
    return view == MainView::TocPilot;
}

} // namespace tp::ui
