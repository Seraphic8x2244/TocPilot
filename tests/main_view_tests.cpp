#include <initializer_list>
#include "main_view.h"

int main() {
    using tp::ui::MainView;
    for (const MainView view : {MainView::Packages, MainView::AccountSync, MainView::TocPilot}) {
        const int visible = static_cast<int>(tp::ui::ShowsPackages(view))
            + static_cast<int>(tp::ui::ShowsSync(view))
            + static_cast<int>(tp::ui::ShowsTocPilot(view));
        if (visible != 1) return 1;
    }
    if (!tp::ui::ShowsPackages(MainView::Packages)
        || !tp::ui::ShowsSync(MainView::AccountSync)
        || !tp::ui::ShowsTocPilot(MainView::TocPilot)) return 2;
    if (tp::ui::ShowsSync(MainView::Packages)
        || tp::ui::ShowsPackages(MainView::AccountSync)
        || tp::ui::ShowsTocPilot(MainView::AccountSync)) return 3;
    return 0;
}
