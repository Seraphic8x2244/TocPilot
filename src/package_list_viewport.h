#pragma once

namespace tp {

enum class PackageListWorkScope {
    Presentation,
    SinglePackage,
    MultiPackage,
    WholeList,
};

enum class PackageListViewportPhase {
    Start,
    Finish,
};

constexpr bool ShouldResetPackageListViewport(
    PackageListWorkScope scope,
    PackageListViewportPhase) noexcept {
    return
        scope == PackageListWorkScope::MultiPackage ||
        scope == PackageListWorkScope::WholeList;
}

}  // namespace tp
