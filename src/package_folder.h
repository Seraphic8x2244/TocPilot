#pragma once

#include <cwctype>
#include <filesystem>
#include <optional>
#include <string>

namespace tp::ui {

inline bool PathComponentEquals(const std::filesystem::path& left,
                                const std::filesystem::path& right) {
    const auto a = left.wstring();
    const auto b = right.wstring();
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (std::towlower(a[index]) != std::towlower(b[index])) return false;
    }
    return true;
}

inline bool IsPathWithinRoot(const std::filesystem::path& root,
                             const std::filesystem::path& path) {
    auto rootPart = root.begin();
    auto pathPart = path.begin();
    for (; rootPart != root.end(); ++rootPart, ++pathPart) {
        if (pathPart == path.end() || !PathComponentEquals(*rootPart, *pathPart))
            return false;
    }
    return true;
}

inline std::optional<std::filesystem::path> ResolvePackageFolder(
    const std::filesystem::path& wowRoot,
    const std::filesystem::path& relative) {
    const auto clean = relative.lexically_normal();
    if (clean.empty() || clean.is_absolute() || clean.has_root_name())
        return std::nullopt;
    for (const auto& segment : clean) {
        if (segment == L"..") return std::nullopt;
    }
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(wowRoot, error);
    if (error) return std::nullopt;
    const auto folder = std::filesystem::weakly_canonical(wowRoot / clean, error);
    if (error || !IsPathWithinRoot(root, folder)) return std::nullopt;
    if (!std::filesystem::is_directory(folder, error) || error) return std::nullopt;
    return folder;
}

} // namespace tp::ui
