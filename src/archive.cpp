#include "archive.h"

#include <windows.h>

#include "miniz.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tp {
namespace {

constexpr std::uint64_t kMaxArchiveBytes = 256ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxSingleFileBytes = 256ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxTotalUncompressedBytes = 1024ull * 1024ull * 1024ull;
constexpr std::size_t kMaxArchiveEntries = 100000;
constexpr std::size_t kMaxArchiveNameBytes = 32768;
constexpr std::size_t kMaxRelativePathChars = 1024;

std::wstring Utf8ToWide(std::string_view input) {
    if (input.empty()) {
        return {};
    }

    if (input.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    const int sourceLength = static_cast<int>(input.size());
    const int needed = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        sourceLength,
        nullptr,
        0);

    if (needed <= 0) {
        return {};
    }

    std::wstring output(
        static_cast<std::size_t>(needed),
        L'\0');

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            input.data(),
            sourceLength,
            output.data(),
            needed) != needed) {
        return {};
    }

    return output;
}

std::wstring Lower(std::wstring value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
    return value;
}

std::filesystem::path RepositoryRelativePath(
    const std::filesystem::path& sourceRelativePath) {
    auto it = sourceRelativePath.begin();
    const auto end = sourceRelativePath.end();

    if (it == end) {
        return {};
    }

    // Provider-generated source archives always place repository contents
    // under one generated wrapper directory. Persist paths below that wrapper
    // so child-package identity remains stable when the commit changes.
    ++it;

    std::filesystem::path result;
    for (; it != end; ++it) {
        result /= *it;
    }

    return result;
}

bool IsReservedWindowsName(std::wstring_view segment) {
    std::wstring stem(segment);
    const auto dot = stem.find(L'.');
    if (dot != std::wstring::npos) {
        stem.resize(dot);
    }

    stem = Lower(std::move(stem));

    if (stem == L"con" ||
        stem == L"prn" ||
        stem == L"aux" ||
        stem == L"nul") {
        return true;
    }

    if (stem.size() == 4 &&
        (stem.starts_with(L"com") ||
         stem.starts_with(L"lpt")) &&
        stem[3] >= L'1' &&
        stem[3] <= L'9') {
        return true;
    }

    return false;
}

bool IsSafeWindowsSegment(
    std::wstring_view segment,
    std::wstring& error) {
    if (segment.empty() ||
        segment == L"." ||
        segment == L"..") {
        error = L"Archive contains an unsafe path segment.";
        return false;
    }

    if (segment.back() == L'.' ||
        segment.back() == L' ') {
        error =
            L"Archive path ends a segment with a dot or space.";
        return false;
    }

    for (const wchar_t ch : segment) {
        if (ch < 32 ||
            ch == L'<' ||
            ch == L'>' ||
            ch == L':' ||
            ch == L'"' ||
            ch == L'|' ||
            ch == L'?' ||
            ch == L'*') {
            error =
                L"Archive path contains a Windows-unsafe character.";
            return false;
        }
    }

    if (IsReservedWindowsName(segment)) {
        error =
            L"Archive path uses a reserved Windows device name.";
        return false;
    }

    return true;
}

std::wstring SanitizeStagingComponent(
    std::wstring_view value) {
    std::wstring result;
    result.reserve(value.size());

    bool previousDash = false;
    for (const wchar_t ch : value) {
        const bool keep =
            std::iswalnum(ch) ||
            ch == L'.' ||
            ch == L'_' ||
            ch == L'-';

        wchar_t out = keep ? ch : L'-';
        if (out == L'-' && previousDash) {
            continue;
        }

        result.push_back(out);
        previousDash = out == L'-';
    }

    while (!result.empty() &&
           (result.front() == L'.' ||
            result.front() == L'-')) {
        result.erase(result.begin());
    }

    while (!result.empty() &&
           (result.back() == L'.' ||
            result.back() == L'-')) {
        result.pop_back();
    }

    if (result.empty()) {
        result = L"package";
    }

    return result;
}

std::wstring MinizError(mz_zip_archive& archive) {
    const char* text =
        mz_zip_get_error_string(
            mz_zip_get_last_error(&archive));

    if (!text) {
        return L"Unknown ZIP error";
    }

    const std::wstring wide = Utf8ToWide(text);
    return wide.empty()
        ? L"Unknown ZIP error"
        : wide;
}

struct ZipReaderGuard {
    mz_zip_archive* archive = nullptr;

    ~ZipReaderGuard() {
        if (archive) {
            mz_zip_reader_end(archive);
        }
    }
};

struct EntryPlan {
    mz_uint index = 0;
    std::filesystem::path relativePath;
    bool directory = false;
    std::uint64_t uncompressedBytes = 0;
};

bool IsTocExtension(const std::filesystem::path& path) {
    return Lower(path.extension().wstring()) == L".toc";
}

std::wstring CandidateKey(
    const std::filesystem::path& path) {
    return Lower(path.generic_wstring());
}

} // namespace

std::filesystem::path ProviderPackageStagingDirectory(
    const std::filesystem::path& wowRoot,
    std::wstring_view provider,
    std::wstring_view repository) {
    const std::wstring leaf =
        SanitizeStagingComponent(provider) +
        L"-" +
        SanitizeStagingComponent(repository);

    return wowRoot /
        L"Interface" /
        L"TocPilot" /
        L"staging" /
        leaf;
}

bool ResetProviderPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view provider,
    std::wstring_view repository,
    std::filesystem::path& stagingDirectory,
    std::wstring& error) {
    error.clear();

    if (provider.empty()) {
        error = L"Package provider is empty.";
        return false;
    }

    stagingDirectory =
        ProviderPackageStagingDirectory(
            wowRoot,
            provider,
            repository);

    const auto stagingRoot =
        wowRoot /
        L"Interface" /
        L"TocPilot" /
        L"staging";

    std::error_code ec;
    std::filesystem::create_directories(
        stagingRoot,
        ec);

    if (ec) {
        error =
            L"Could not create TocPilot staging root: " +
            std::to_wstring(ec.value());
        return false;
    }

    ec.clear();
    std::filesystem::remove_all(
        stagingDirectory,
        ec);

    if (ec) {
        error =
            L"Could not reset package staging directory: " +
            std::to_wstring(ec.value());
        return false;
    }

    ec.clear();
    if (!std::filesystem::create_directories(
            stagingDirectory,
            ec) &&
        ec) {
        error =
            L"Could not create package staging directory: " +
            std::to_wstring(ec.value());
        return false;
    }

    return true;
}

bool CleanupProviderPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view provider,
    std::wstring_view repository,
    std::wstring& error) {
    error.clear();

    if (provider.empty()) {
        error = L"Package provider is empty.";
        return false;
    }

    const auto stagingDirectory =
        ProviderPackageStagingDirectory(
            wowRoot,
            provider,
            repository);

    std::error_code ec;
    std::filesystem::remove_all(
        stagingDirectory,
        ec);

    if (ec) {
        error =
            L"Could not remove package staging directory: " +
            std::to_wstring(ec.value());
        return false;
    }

    return true;
}

std::filesystem::path GitHubPackageStagingDirectory(
    const std::filesystem::path& wowRoot,
    std::wstring_view repository) {
    return ProviderPackageStagingDirectory(
        wowRoot,
        L"github",
        repository);
}

bool ResetGitHubPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view repository,
    std::filesystem::path& stagingDirectory,
    std::wstring& error) {
    return ResetProviderPackageStaging(
        wowRoot,
        L"github",
        repository,
        stagingDirectory,
        error);
}

bool CleanupGitHubPackageStaging(
    const std::filesystem::path& wowRoot,
    std::wstring_view repository,
    std::wstring& error) {
    return CleanupProviderPackageStaging(
        wowRoot,
        L"github",
        repository,
        error);
}

bool SafeArchiveRelativePath(
    std::string_view archiveName,
    std::filesystem::path& relativePath,
    std::wstring& error) {
    relativePath.clear();
    error.clear();

    if (archiveName.empty()) {
        error = L"Archive contains an empty path.";
        return false;
    }

    if (archiveName.size() > kMaxArchiveNameBytes) {
        error = L"Archive path is unreasonably long.";
        return false;
    }

    if (archiveName.front() == '/' ||
        archiveName.front() == '\\') {
        error = L"Archive contains an absolute path.";
        return false;
    }

    if (archiveName.find('\\') !=
        std::string_view::npos) {
        error =
            L"Archive path uses backslash separators.";
        return false;
    }

    std::size_t start = 0;
    while (start < archiveName.size()) {
        const std::size_t slash =
            archiveName.find('/', start);

        const std::size_t end =
            slash == std::string_view::npos
                ? archiveName.size()
                : slash;

        const std::string_view raw =
            archiveName.substr(
                start,
                end - start);

        if (raw.empty()) {
            if (end == archiveName.size()) {
                break;
            }

            error =
                L"Archive path contains an empty segment.";
            return false;
        }

        const std::wstring segment =
            Utf8ToWide(raw);

        if (segment.empty() && !raw.empty()) {
            error =
                L"Archive path is not valid UTF-8.";
            return false;
        }

        if (!IsSafeWindowsSegment(
                segment,
                error)) {
            return false;
        }

        relativePath /= segment;

        if (relativePath.native().size() >
            kMaxRelativePathChars) {
            error =
                L"Archive path is too long for safe extraction.";
            return false;
        }

        if (slash == std::string_view::npos) {
            break;
        }

        start = slash + 1;
    }

    if (relativePath.empty()) {
        error = L"Archive path resolves to no file or directory.";
        return false;
    }

    return true;
}

bool ExtractZipSecure(
    const std::filesystem::path& archivePath,
    const std::filesystem::path& extractedRoot,
    std::size_t& entryCount,
    std::uint64_t& totalUncompressedBytes,
    std::wstring& error) {
    entryCount = 0;
    totalUncompressedBytes = 0;
    error.clear();

    std::ifstream input(
        archivePath,
        std::ios::binary |
            std::ios::ate);

    if (!input) {
        error = L"Could not open downloaded ZIP archive.";
        return false;
    }

    const std::streampos end = input.tellg();
    if (end <= 0) {
        error = L"Downloaded ZIP archive is empty.";
        return false;
    }

    const auto archiveBytes =
        static_cast<std::uint64_t>(end);

    if (archiveBytes > kMaxArchiveBytes) {
        error =
            L"Downloaded ZIP archive exceeds the 256 MiB inspection limit.";
        return false;
    }

    if (archiveBytes >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        error = L"Downloaded ZIP archive is too large to inspect.";
        return false;
    }

    std::vector<unsigned char> bytes(
        static_cast<std::size_t>(
            archiveBytes));

    input.seekg(0, std::ios::beg);
    if (!input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(
                bytes.size()))) {
        error = L"Could not read downloaded ZIP archive.";
        return false;
    }

    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(
            &archive,
            bytes.data(),
            bytes.size(),
            0)) {
        error =
            L"Downloaded file is not a readable ZIP archive: " +
            MinizError(archive);
        return false;
    }

    ZipReaderGuard guard{&archive};

    const mz_uint fileCount =
        mz_zip_reader_get_num_files(
            &archive);

    if (fileCount >
        kMaxArchiveEntries) {
        error =
            L"ZIP archive contains too many entries.";
        return false;
    }

    std::vector<EntryPlan> plans;
    plans.reserve(fileCount);

    std::unordered_set<std::wstring> seenPaths;

    for (mz_uint index = 0;
         index < fileCount;
         ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(
                &archive,
                index,
                &stat)) {
            error =
                L"Could not inspect ZIP entry: " +
                MinizError(archive);
            return false;
        }

        const mz_uint required =
            mz_zip_reader_get_filename(
                &archive,
                index,
                nullptr,
                0);

        if (required == 0 ||
            required > kMaxArchiveNameBytes) {
            error =
                L"ZIP archive contains an invalid or excessively long filename.";
            return false;
        }

        std::vector<char> filename(
            static_cast<std::size_t>(required) + 1,
            '\0');

        if (mz_zip_reader_get_filename(
                &archive,
                index,
                filename.data(),
                static_cast<mz_uint>(
                    filename.size())) == 0) {
            error =
                L"Could not read ZIP entry filename.";
            return false;
        }

        const std::size_t actual =
            std::char_traits<char>::length(
                filename.data());

        if (actual + 1 < required) {
            error =
                L"ZIP archive filename contains an embedded null byte.";
            return false;
        }

        std::filesystem::path relative;
        if (!SafeArchiveRelativePath(
                std::string_view(
                    filename.data(),
                    actual),
                relative,
                error)) {
            return false;
        }

        const std::wstring canonical =
            Lower(
                relative.generic_wstring());

        if (!seenPaths.insert(
                canonical)
                 .second) {
            error =
                L"ZIP archive contains duplicate paths on Windows: " +
                relative.generic_wstring();
            return false;
        }

        const bool directory =
            stat.m_is_directory != 0;

        if (!directory) {
            if (stat.m_is_encrypted) {
                error =
                    L"ZIP archive contains an encrypted entry.";
                return false;
            }

            if (!stat.m_is_supported) {
                error =
                    L"ZIP archive uses an unsupported compression method.";
                return false;
            }

            if (stat.m_uncomp_size >
                kMaxSingleFileBytes) {
                error =
                    L"ZIP entry exceeds the 256 MiB per-file inspection limit.";
                return false;
            }

            if (totalUncompressedBytes >
                kMaxTotalUncompressedBytes -
                    stat.m_uncomp_size) {
                error =
                    L"ZIP archive exceeds the 1 GiB extracted-size inspection limit.";
                return false;
            }

            totalUncompressedBytes +=
                stat.m_uncomp_size;
        }

        plans.push_back(
            EntryPlan{
                index,
                std::move(relative),
                directory,
                stat.m_uncomp_size});
    }

    std::error_code ec;
    std::filesystem::remove_all(
        extractedRoot,
        ec);

    if (ec) {
        error =
            L"Could not reset extracted staging directory: " +
            std::to_wstring(ec.value());
        return false;
    }

    ec.clear();
    if (!std::filesystem::create_directories(
            extractedRoot,
            ec) &&
        ec) {
        error =
            L"Could not create extracted staging directory: " +
            std::to_wstring(ec.value());
        return false;
    }

    const auto cleanupPartial = [&]() {
        std::error_code cleanupError;
        std::filesystem::remove_all(
            extractedRoot,
            cleanupError);
    };

    for (const auto& plan : plans) {
        const auto output =
            extractedRoot /
            plan.relativePath;

        if (plan.directory) {
            ec.clear();
            std::filesystem::create_directories(
                output,
                ec);

            if (ec) {
                error =
                    L"Could not create extracted directory: " +
                    std::to_wstring(ec.value());
                cleanupPartial();
                return false;
            }
            continue;
        }

        ec.clear();
        std::filesystem::create_directories(
            output.parent_path(),
            ec);

        if (ec) {
            error =
                L"Could not create extracted file directory: " +
                std::to_wstring(ec.value());
            cleanupPartial();
            return false;
        }

        if (plan.uncompressedBytes >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max())) {
            error =
                L"ZIP entry is too large for this process.";
            cleanupPartial();
            return false;
        }

        std::vector<unsigned char> fileBytes(
            static_cast<std::size_t>(
                plan.uncompressedBytes));

        if (!fileBytes.empty() &&
            !mz_zip_reader_extract_to_mem(
                &archive,
                plan.index,
                fileBytes.data(),
                fileBytes.size(),
                0)) {
            error =
                L"Could not extract ZIP entry: " +
                MinizError(archive);
            cleanupPartial();
            return false;
        }

        std::ofstream outputFile(
            output,
            std::ios::binary |
                std::ios::trunc);

        if (!outputFile) {
            error =
                L"Could not create extracted file.";
            cleanupPartial();
            return false;
        }

        if (!fileBytes.empty()) {
            outputFile.write(
                reinterpret_cast<const char*>(
                    fileBytes.data()),
                static_cast<std::streamsize>(
                    fileBytes.size()));
        }

        outputFile.flush();
        if (!outputFile) {
            error =
                L"Could not finish writing extracted file.";
            cleanupPartial();
            return false;
        }
    }

    entryCount = plans.size();
    return true;
}

bool DetectAddonCandidates(
    const std::filesystem::path& extractedRoot,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error) {
    candidates.clear();
    error.clear();

    std::error_code ec;
    if (!std::filesystem::is_directory(
            extractedRoot,
            ec) ||
        ec) {
        error =
            L"Extracted staging directory is not readable.";
        return false;
    }

    struct CandidateBuild {
        std::filesystem::path relativePath;
        std::vector<std::filesystem::path> tocFiles;
    };

    std::map<std::wstring, CandidateBuild> found;

    std::filesystem::recursive_directory_iterator it(
        extractedRoot,
        std::filesystem::directory_options::skip_permission_denied,
        ec);
    const std::filesystem::recursive_directory_iterator end;

    if (ec) {
        error =
            L"Could not enumerate extracted staging directory: " +
            std::to_wstring(ec.value());
        return false;
    }

    while (it != end) {
        const auto entry = *it;

        ec.clear();
        const auto status =
            entry.symlink_status(ec);

        if (ec) {
            error =
                L"Could not inspect extracted entry: " +
                std::to_wstring(ec.value());
            return false;
        }

        if (std::filesystem::is_symlink(status)) {
            error =
                L"Extracted staging unexpectedly contains a symbolic link.";
            return false;
        }

        if (std::filesystem::is_regular_file(status) &&
            IsTocExtension(entry.path())) {
            const auto parent =
                entry.path().parent_path();

            const auto relative =
                parent.lexically_relative(
                    extractedRoot);

            if (relative.empty() ||
                relative.native().starts_with(
                    L"..")) {
                error =
                    L"Detected .toc file is outside the extraction root.";
                return false;
            }

            const std::wstring key =
                CandidateKey(relative);

            auto& candidate = found[key];
            if (candidate.relativePath.empty()) {
                candidate.relativePath =
                    relative;
            }

            candidate.tocFiles.push_back(
                entry.path().filename());
        }

        it.increment(ec);
        if (ec) {
            error =
                L"Could not continue enumerating extracted staging directory: " +
                std::to_wstring(ec.value());
            return false;
        }
    }

    candidates.reserve(found.size());

    for (auto& [key, build] : found) {
        (void)key;

        std::sort(
            build.tocFiles.begin(),
            build.tocFiles.end(),
            [](const auto& left, const auto& right) {
                return Lower(left.wstring()) <
                    Lower(right.wstring());
            });

        AddonCandidate candidate;
        candidate.sourceRelativePath =
            std::move(build.relativePath);
        candidate.installFolder =
            candidate.sourceRelativePath
                .filename()
                .wstring();
        candidate.tocFiles =
            std::move(build.tocFiles);

        candidates.push_back(
            std::move(candidate));
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const AddonCandidate& left,
           const AddonCandidate& right) {
            return Lower(
                       left.sourceRelativePath
                           .generic_wstring()) <
                Lower(
                       right.sourceRelativePath
                           .generic_wstring());
        });

    return true;
}

bool DetectShallowRepositoryAddonLayout(
    const std::filesystem::path& extractedRoot,
    std::wstring_view providerName,
    std::wstring_view repository,
    RepositoryAddonLayout& layout,
    std::wstring& error) {
    layout = {};
    error.clear();

    std::error_code ec;
    if (!std::filesystem::is_directory(
            extractedRoot,
            ec) ||
        ec) {
        error =
            L"Extracted staging directory is not readable.";
        return false;
    }

    const std::size_t slash =
        repository.find_last_of(L'/');
    const std::wstring repositoryName =
        slash == std::wstring_view::npos
            ? std::wstring(repository)
            : std::wstring(repository.substr(slash + 1));

    if (repositoryName.empty()) {
        error =
            std::wstring(providerName) +
            L" repository name is empty.";
        return false;
    }

    std::filesystem::path repositoryRoot;
    std::size_t wrapperDirectoryCount = 0;

    std::filesystem::directory_iterator rootIt(
        extractedRoot,
        std::filesystem::directory_options::skip_permission_denied,
        ec);
    const std::filesystem::directory_iterator end;

    if (ec) {
        error =
            L"Could not enumerate extracted staging directory: " +
            std::to_wstring(ec.value());
        return false;
    }

    while (rootIt != end) {
        const auto entry = *rootIt;
        ec.clear();
        const auto status =
            entry.symlink_status(ec);

        if (ec) {
            error =
                L"Could not inspect extracted archive wrapper: " +
                std::to_wstring(ec.value());
            return false;
        }

        if (std::filesystem::is_symlink(status)) {
            error =
                L"Extracted staging unexpectedly contains a symbolic link.";
            return false;
        }

        if (std::filesystem::is_directory(status)) {
            ++wrapperDirectoryCount;
            repositoryRoot = entry.path();
        }

        rootIt.increment(ec);
        if (ec) {
            error =
                L"Could not continue enumerating extracted staging directory: " +
                std::to_wstring(ec.value());
            return false;
        }
    }

    if (wrapperDirectoryCount != 1) {
        error =
            std::wstring(providerName) +
            L" source archive did not contain exactly one repository wrapper directory.";
        return false;
    }

    const auto wrapperRelative =
        repositoryRoot.lexically_relative(
            extractedRoot);

    if (wrapperRelative.empty() ||
        wrapperRelative.native().starts_with(
            L"..")) {
        error =
            L"Repository archive wrapper is outside the extraction root.";
        return false;
    }

    auto collectDirectTocs =
        [&](const std::filesystem::path& directory,
            std::vector<std::filesystem::path>& tocFiles) {
            tocFiles.clear();

            std::filesystem::directory_iterator it(
                directory,
                std::filesystem::directory_options::skip_permission_denied,
                ec);

            if (ec) {
                error =
                    L"Could not enumerate repository directory: " +
                    std::to_wstring(ec.value());
                return false;
            }

            while (it != end) {
                const auto entry = *it;
                ec.clear();
                const auto status =
                    entry.symlink_status(ec);

                if (ec) {
                    error =
                        L"Could not inspect repository entry: " +
                        std::to_wstring(ec.value());
                    return false;
                }

                if (std::filesystem::is_symlink(status)) {
                    error =
                        L"Extracted staging unexpectedly contains a symbolic link.";
                    return false;
                }

                if (std::filesystem::is_regular_file(status) &&
                    IsTocExtension(entry.path())) {
                    tocFiles.push_back(
                        entry.path().filename());
                }

                it.increment(ec);
                if (ec) {
                    error =
                        L"Could not continue enumerating repository directory: " +
                        std::to_wstring(ec.value());
                    return false;
                }
            }

            std::sort(
                tocFiles.begin(),
                tocFiles.end(),
                [](const auto& left,
                   const auto& right) {
                    return Lower(left.wstring()) <
                        Lower(right.wstring());
                });
            return true;
        };

    std::vector<std::filesystem::path> rootTocs;
    if (!collectDirectTocs(
            repositoryRoot,
            rootTocs)) {
        return false;
    }

    std::vector<AddonCandidate> childCandidates;

    std::filesystem::directory_iterator childIt(
        repositoryRoot,
        std::filesystem::directory_options::skip_permission_denied,
        ec);

    if (ec) {
        error =
            L"Could not enumerate repository root: " +
            std::to_wstring(ec.value());
        return false;
    }

    while (childIt != end) {
        const auto entry = *childIt;
        ec.clear();
        const auto status =
            entry.symlink_status(ec);

        if (ec) {
            error =
                L"Could not inspect repository root entry: " +
                std::to_wstring(ec.value());
            return false;
        }

        if (std::filesystem::is_symlink(status)) {
            error =
                L"Extracted staging unexpectedly contains a symbolic link.";
            return false;
        }

        if (std::filesystem::is_directory(status)) {
            std::vector<std::filesystem::path> tocFiles;
            if (!collectDirectTocs(
                    entry.path(),
                    tocFiles)) {
                return false;
            }

            if (!tocFiles.empty()) {
                AddonCandidate candidate;
                candidate.sourceRelativePath =
                    wrapperRelative /
                    entry.path().filename();
                candidate.repositoryRelativePath =
                    entry.path().filename();
                candidate.installFolder =
                    entry.path().filename().wstring();
                candidate.tocFiles =
                    std::move(tocFiles);
                childCandidates.push_back(
                    std::move(candidate));
            }
        }

        childIt.increment(ec);
        if (ec) {
            error =
                L"Could not continue enumerating repository root: " +
                std::to_wstring(ec.value());
            return false;
        }
    }

    std::sort(
        childCandidates.begin(),
        childCandidates.end(),
        [](const AddonCandidate& left,
           const AddonCandidate& right) {
            return Lower(
                       left.repositoryRelativePath
                           .generic_wstring()) <
                Lower(
                       right.repositoryRelativePath
                           .generic_wstring());
        });

    const bool hasRootAddon =
        !rootTocs.empty();
    const bool hasChildAddons =
        !childCandidates.empty();

    if (hasRootAddon) {
        AddonCandidate rootCandidate;
        rootCandidate.sourceRelativePath =
            wrapperRelative;
        rootCandidate.installFolder =
            repositoryName;
        rootCandidate.tocFiles =
            std::move(rootTocs);
        layout.candidates.push_back(
            std::move(rootCandidate));
    }

    layout.candidates.insert(
        layout.candidates.end(),
        std::make_move_iterator(
            childCandidates.begin()),
        std::make_move_iterator(
            childCandidates.end()));

    if (hasRootAddon && hasChildAddons) {
        layout.kind =
            RepositoryAddonLayoutKind::MixedAmbiguous;
    } else if (hasRootAddon) {
        layout.kind =
            RepositoryAddonLayoutKind::RootAddon;
    } else if (layout.candidates.size() == 1) {
        layout.kind =
            RepositoryAddonLayoutKind::SingleNestedAddon;
    } else if (layout.candidates.size() > 1) {
        layout.kind =
            RepositoryAddonLayoutKind::RepositoryLibrary;
    } else {
        layout.kind =
            RepositoryAddonLayoutKind::None;
    }

    return true;
}

bool DetectRepositoryAddonCandidates(
    const std::filesystem::path& extractedRoot,
    std::wstring_view providerName,
    std::wstring_view repository,
    std::wstring_view existingInstallFolder,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error) {
    if (!DetectAddonCandidates(
            extractedRoot,
            candidates,
            error)) {
        return false;
    }

    const std::size_t slash =
        repository.find_last_of(L'/');
    const std::wstring repositoryName =
        slash == std::wstring_view::npos
            ? std::wstring(repository)
            : std::wstring(repository.substr(slash + 1));

    if (repositoryName.empty()) {
        error =
            std::wstring(providerName) +
            L" repository name is empty.";
        return false;
    }

    for (auto& candidate : candidates) {
        candidate.repositoryRelativePath =
            RepositoryRelativePath(
                candidate.sourceRelativePath);

        const auto parent =
            candidate.sourceRelativePath.parent_path();

        // Provider-generated source archives add one generated top-level
        // wrapper. Recursive install validation sees a repository-root addon as
        // that wrapper directory. Discovery has already classified this shape,
        // so map the root deterministically to the repository name instead of
        // ever installing the generated wrapper name.
        if (!parent.empty()) {
            continue;
        }

        if (!existingInstallFolder.empty() &&
            candidates.size() == 1) {
            // An already-installed/adopted single-root package has authoritative
            // ownership for its live addon folder.
            candidate.installFolder =
                std::wstring(existingInstallFolder);
        } else {
            candidate.installFolder =
                repositoryName;
        }
    }

    return true;
}

bool DetectGitHubAddonCandidates(
    const std::filesystem::path& extractedRoot,
    std::wstring_view repository,
    std::wstring_view existingInstallFolder,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error) {
    return DetectRepositoryAddonCandidates(
        extractedRoot,
        L"GitHub",
        repository,
        existingInstallFolder,
        candidates,
        error);
}

bool IsRepositoryLibrary(
    const std::vector<AddonCandidate>& candidates) {
    if (candidates.size() < 2) {
        return false;
    }

    for (const auto& candidate : candidates) {
        if (candidate.repositoryRelativePath.empty() ||
            !candidate.repositoryRelativePath.parent_path().empty()) {
            return false;
        }
    }

    return true;
}

bool SelectRepositoryAddonCandidate(
    std::wstring_view sourcePath,
    std::vector<AddonCandidate>& candidates,
    std::wstring& error) {
    error.clear();

    if (sourcePath.empty()) {
        return true;
    }

    std::wstring normalized(sourcePath);
    std::replace(
        normalized.begin(),
        normalized.end(),
        L'\\',
        L'/');
    normalized = Lower(std::move(normalized));

    const auto it = std::find_if(
        candidates.begin(),
        candidates.end(),
        [&](const AddonCandidate& candidate) {
            return Lower(
                       candidate.repositoryRelativePath
                           .generic_wstring()) ==
                normalized;
        });

    if (it == candidates.end()) {
        error =
            L"Configured addon path was not found in this repository revision: " +
            std::wstring(sourcePath);
        candidates.clear();
        return false;
    }

    AddonCandidate selected = *it;
    candidates.clear();
    candidates.push_back(
        std::move(selected));
    return true;
}

} // namespace tp
