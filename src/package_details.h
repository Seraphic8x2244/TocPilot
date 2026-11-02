#pragma once

// Selected-package presentation and clipboard transaction, independent of
// package installation state or asynchronous provider operations.
#include "package_list_status.h"
#include "state.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace tp::ui {

enum class PackageDetailAction : int {
    Install = 1013,
    Remove = 1014,
    OpenRepo = 1050,
    CopyLink = 1053,
    OpenFolder = 1049,
    ChangeBranch = 1051
};
inline constexpr std::array<PackageDetailAction, 6> kPackageDetailActions{
    PackageDetailAction::Install, PackageDetailAction::Remove,
    PackageDetailAction::OpenRepo, PackageDetailAction::CopyLink,
    PackageDetailAction::OpenFolder, PackageDetailAction::ChangeBranch
};
constexpr PackageDetailAction PackageDetailActionAt(int row, int column) {
    return kPackageDetailActions[static_cast<std::size_t>(row * 2 + column)];
}

inline std::wstring RepositoryOwner(std::wstring_view repository) {
    const auto slash = repository.rfind(L'/');
    if (slash == std::wstring_view::npos || slash == 0 ||
        slash + 1 >= repository.size()) return L"\u2014";
    // GitLab nested groups are a namespace, not the Git commit author.
    return std::wstring(repository.substr(0, slash));
}
inline std::wstring PresentValue(std::wstring_view value) {
    return value.empty() ? L"\u2014" : std::wstring(value);
}
// Prefer the actual in-flight ListView status over the saved package model.
// Empty/no-row states fall back to the authoritative package status.
constexpr std::wstring_view EffectiveDetailStatus(
    std::wstring_view live, std::wstring_view stored) noexcept {
    return live.empty() ? stored : live;
}
inline std::wstring PackageDetailsText(
    const tp::PackageRecord& package,
    std::wstring_view providerLabel,
    std::wstring_view branchText,
    std::wstring_view status) {
    const std::wstring source = PresentValue(providerLabel) + L" / " +
        PresentValue(package.repository);
    const std::wstring type(PackageKindLabel(KindFromTarget(package.target)));
    return L"Source: " + source +
        L"\r\nAuthor: " + RepositoryOwner(package.repository) +
        L"\r\nType: " + type +
        L"\r\nBranch: " + PresentValue(branchText) +
        L"\r\nInstalled: " + PresentValue(package.installedRevision) +
        L"\r\nLatest: " + PresentValue(package.latestRevision) +
        L"\r\nStatus: " + PresentValue(status);
}

// The adapter is deliberately injectable: tests can exercise every clipboard
// failure/ownership branch without changing the Windows user's clipboard.
struct Win32ClipboardApi {
    bool Open(HWND owner) { return OpenClipboard(owner) != FALSE; }
    bool Empty() { return EmptyClipboard() != FALSE; }
    HGLOBAL Allocate(std::size_t bytes) {
        return GlobalAlloc(GMEM_MOVEABLE, bytes);
    }
    void* Lock(HGLOBAL value) { return GlobalLock(value); }
    void Unlock(HGLOBAL value) { GlobalUnlock(value); }
    HANDLE Set(HGLOBAL value) { return SetClipboardData(CF_UNICODETEXT, value); }
    void Free(HGLOBAL value) { GlobalFree(value); }
    void Close() { CloseClipboard(); }
};

template <typename Api>
bool CopyUnicodeClipboard(Api& api, HWND owner, std::wstring_view text) {
    if (text.empty() || text.size() >= SIZE_MAX / sizeof(wchar_t)) return false;
    if (!api.Open(owner)) return false;
    if (!api.Empty()) {
        api.Close();
        return false;
    }
    HGLOBAL bytes = api.Allocate((text.size() + 1) * sizeof(wchar_t));
    if (!bytes) {
        api.Close();
        return false;
    }
    auto* characters = static_cast<wchar_t*>(api.Lock(bytes));
    if (!characters) {
        api.Free(bytes);
        api.Close();
        return false;
    }
    std::copy(text.begin(), text.end(), characters);
    characters[text.size()] = L'\0';
    api.Unlock(bytes);
    // SetClipboardData takes ownership only after a successful call.
    if (!api.Set(bytes)) {
        api.Free(bytes);
        api.Close();
        return false;
    }
    api.Close();
    return true;
}
} // namespace tp::ui
