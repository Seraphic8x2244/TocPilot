#pragma once
// Compact package list model. The legacy Status subitem remains authoritative
// for in-flight operations despite having zero visual width.
#include "ui_theme.h"
#include <string>
#include <string_view>

namespace tp::ui {
enum class PackageStatusIcon {
    Busy, UpdateAvailable, Current, NotConfigured, NotInstalled,
    NeedsAttention, Error
};
constexpr PackageStatusIcon ResolvePackageStatusIcon(std::wstring_view status) {
    if (status == L"Update Available") return PackageStatusIcon::UpdateAvailable;
    if (status == L"Up To Date") return PackageStatusIcon::Current;
    if (status == L"Not Configured") return PackageStatusIcon::NotConfigured;
    if (status == L"Not Installed") return PackageStatusIcon::NotInstalled;
    if (status == L"Needs Attention") return PackageStatusIcon::NeedsAttention;
    if (status.ends_with(L"...")) return PackageStatusIcon::Busy;
    if (status.find(L"failed") != std::wstring_view::npos ||
        status.find(L"rolled back") != std::wstring_view::npos)
        return PackageStatusIcon::Error;
    // Do not invent success for a status the renderer has not recognised.
    return PackageStatusIcon::NeedsAttention;
}
constexpr std::wstring_view PackageKindLabel(PackageKind kind) {
    switch (kind) {
    case PackageKind::Addon: return L"Addon";
    case PackageKind::Dll: return L"DLL";
    case PackageKind::Mpq: return L"MPQ";
    }
    return L"Addon";
}
inline std::wstring AccessiblePackageCell(
    std::wstring_view name, std::wstring_view status, PackageKind kind) {
    std::wstring result(name);
    result += L", ";
    result += PackageKindLabel(kind);
    result += L", ";
    result += status;
    return result;
}
inline std::wstring PackageCellTooltip(
    std::wstring_view name, std::wstring_view status, PackageKind kind) {
    std::wstring result(name);
    result += L"\r\nType: ";
    result += PackageKindLabel(kind);
    result += L"\r\nStatus: ";
    result += status;
    return result;
}
struct CompactPackageCellOffsets { int statusLeft, typeLeft, nameLeft; };
constexpr CompactPackageCellOffsets PackageCellOffsets() {
    return {0, 22, 60};
}
} // namespace tp::ui
