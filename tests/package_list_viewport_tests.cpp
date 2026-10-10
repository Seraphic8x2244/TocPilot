#include "package_list_viewport.h"

int main() {
    using tp::PackageListViewportPhase;
    using tp::PackageListWorkScope;
    using tp::ShouldResetPackageListViewport;

    static_assert(
        !ShouldResetPackageListViewport(
            PackageListWorkScope::Presentation,
            PackageListViewportPhase::Start));
    static_assert(
        !ShouldResetPackageListViewport(
            PackageListWorkScope::Presentation,
            PackageListViewportPhase::Finish));

    static_assert(
        !ShouldResetPackageListViewport(
            PackageListWorkScope::SinglePackage,
            PackageListViewportPhase::Start));
    static_assert(
        !ShouldResetPackageListViewport(
            PackageListWorkScope::SinglePackage,
            PackageListViewportPhase::Finish));

    static_assert(
        ShouldResetPackageListViewport(
            PackageListWorkScope::MultiPackage,
            PackageListViewportPhase::Start));
    static_assert(
        ShouldResetPackageListViewport(
            PackageListWorkScope::MultiPackage,
            PackageListViewportPhase::Finish));

    static_assert(
        ShouldResetPackageListViewport(
            PackageListWorkScope::WholeList,
            PackageListViewportPhase::Start));
    static_assert(
        ShouldResetPackageListViewport(
            PackageListWorkScope::WholeList,
            PackageListViewportPhase::Finish));

    return 0;
}
