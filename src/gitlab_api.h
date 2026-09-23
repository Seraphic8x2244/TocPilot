#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace tp {

bool BuildGitLabArchivePath(
    std::wstring_view repository,
    std::wstring_view ref,
    std::wstring& path,
    std::wstring& error);

bool DownloadGitLabArchive(
    std::wstring_view repository,
    std::wstring_view ref,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    std::wstring& error);

} // namespace tp
