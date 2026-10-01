#include "mpq_package.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kUserAgent[] =
    L"TocPilot/" TOCPILOT_VERSION_W;

struct Handles {
    HINTERNET session = nullptr;
    HINTERNET connection = nullptr;
    HINTERNET request = nullptr;

    ~Handles() {
        if (request) {
            WinHttpCloseHandle(request);
        }
        if (connection) {
            WinHttpCloseHandle(connection);
        }
        if (session) {
            WinHttpCloseHandle(session);
        }
    }
};

struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
};

std::wstring WindowsError(
    DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length =
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            0,
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

    std::wstring message;
    if (length != 0 && buffer) {
        message.assign(buffer, length);
        while (!message.empty() &&
               (message.back() == L'\r' ||
                message.back() == L'\n')) {
            message.pop_back();
        }
    } else {
        message =
            L"Windows error " +
            std::to_wstring(error);
    }

    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

std::wstring Lower(
    std::wstring value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });
    return value;
}

bool EqualsInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < left.size();
         ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }
    return true;
}

bool IsHexDigest(
    std::wstring_view digest) {
    if (digest.size() != 64) {
        return false;
    }

    return std::all_of(
        digest.begin(),
        digest.end(),
        [](wchar_t ch) {
            return
                (ch >= L'0' && ch <= L'9') ||
                (ch >= L'a' && ch <= L'f') ||
                (ch >= L'A' && ch <= L'F');
        });
}

std::wstring CanonicalSeparators(
    std::wstring value) {
    std::replace(
        value.begin(),
        value.end(),
        L'\\',
        L'/');
    return value;
}

bool PatchLetterFromTarget(
    std::wstring_view targetPath,
    wchar_t& letter) {
    const std::wstring canonical =
        CanonicalSeparators(
            std::wstring(targetPath));

    constexpr std::wstring_view prefix =
        L"Data/patch-";
    constexpr std::wstring_view suffix =
        L".mpq";

    if (canonical.size() !=
            prefix.size() + 1 +
                suffix.size() ||
        !EqualsInsensitive(
            std::wstring_view(canonical)
                .substr(0, prefix.size()),
            prefix) ||
        !EqualsInsensitive(
            std::wstring_view(canonical)
                .substr(
                    canonical.size() -
                        suffix.size()),
            suffix)) {
        return false;
    }

    letter =
        static_cast<wchar_t>(
            std::towupper(
                canonical[prefix.size()]));

    return
        letter >= L'A' &&
        letter <= L'Z';
}

bool PatchLetterFromFileName(
    std::wstring_view fileName,
    wchar_t& letter) {
    constexpr std::wstring_view prefix =
        L"patch-";
    constexpr std::wstring_view suffix =
        L".mpq";

    if (fileName.size() !=
            prefix.size() + 1 +
                suffix.size() ||
        !EqualsInsensitive(
            fileName.substr(
                0,
                prefix.size()),
            prefix) ||
        !EqualsInsensitive(
            fileName.substr(
                fileName.size() -
                    suffix.size()),
            suffix)) {
        return false;
    }

    letter =
        static_cast<wchar_t>(
            std::towupper(
                fileName[prefix.size()]));

    return
        letter >= L'A' &&
        letter <= L'Z';
}

bool ValidateMpqSource(
    const PackageRecord& package,
    std::wstring& error) {
    if (!ValidateLatestStableExactReleaseAssetPackage(
            package,
            ExactReleaseAssetKind::Mpq,
            error)) {
        return false;
    }

    if (package.target != L"data") {
        error =
            L"MPQ packages must target the WoW Data directory.";
        return false;
    }

    if (!package.ref.empty() ||
        !package.sourcePath.empty()) {
        error =
            L"MPQ release packages cannot also track a branch or repository source path.";
        return false;
    }

    if (package.id.empty() ||
        package.name.empty()) {
        error =
            L"MPQ package identity is incomplete.";
        return false;
    }

    const std::wstring expectedId =
        package.provider + L":" +
        package.repository + L":release:" +
        package.asset;

    if (!EqualsInsensitive(
            package.id,
            expectedId)) {
        error =
            L"MPQ package id does not match its exact release asset.";
        return false;
    }

    return true;
}

bool TargetReservedByOtherPackage(
    const std::vector<PackageRecord>& packages,
    const PackageRecord& package,
    std::wstring& error) {
    for (const auto& other : packages) {
        if (EqualsInsensitive(
                other.id,
                package.id)) {
            continue;
        }

        if (other.mode == L"release" &&
            other.target == L"data" &&
            !other.targetPath.empty() &&
            EqualsInsensitive(
                other.targetPath,
                package.targetPath)) {
            error =
                L"MPQ target '" +
                package.targetPath +
                L"' is already reserved by package '" +
                other.id +
                L"'.";
            return true;
        }
    }

    return false;
}

bool PackageOwnsTarget(
    const PackageRecord& package) {
    return
        !package.installedRevision.empty() &&
        package.installedFiles.size() == 1 &&
        EqualsInsensitive(
            package.installedFiles.front(),
            package.targetPath);
}

bool InspectPath(
    const std::filesystem::path& path,
    bool& exists,
    bool& regular,
    std::wstring& error) {
    exists = false;
    regular = false;

    const DWORD attributes =
        GetFileAttributesW(
            path.c_str());

    if (attributes ==
        INVALID_FILE_ATTRIBUTES) {
        const DWORD code =
            GetLastError();

        if (code == ERROR_FILE_NOT_FOUND ||
            code == ERROR_PATH_NOT_FOUND) {
            return true;
        }

        error =
            L"Could not inspect '" +
            path.wstring() +
            L"': " +
            WindowsError(code);
        return false;
    }

    exists = true;
    regular =
        (attributes &
         FILE_ATTRIBUTE_DIRECTORY) == 0;
    return true;
}

bool EnsureTransactionPathsFree(
    const std::filesystem::path& staging,
    const std::filesystem::path& rollback,
    std::wstring& error) {
    bool exists = false;
    bool regular = false;

    if (!InspectPath(
            staging,
            exists,
            regular,
            error)) {
        return false;
    }

    if (exists) {
        error =
            L"Refusing MPQ operation because stale TocPilot staging path exists: " +
            staging.wstring();
        return false;
    }

    if (!InspectPath(
            rollback,
            exists,
            regular,
            error)) {
        return false;
    }

    if (exists) {
        error =
            L"Refusing MPQ operation because stale TocPilot rollback path exists: " +
            rollback.wstring();
        return false;
    }

    return true;
}

bool TransactionPaths(
    const std::filesystem::path& wowRoot,
    const PackageRecord& package,
    std::filesystem::path& staging,
    std::filesystem::path& rollback,
    std::wstring& error) {
    wchar_t letter = 0;
    if (!PatchLetterFromTarget(
            package.targetPath,
            letter)) {
        error =
            L"Configured MPQ target is not a patch-letter Data path.";
        return false;
    }

    const std::wstring stem =
        L".tocpilot-mpq-" +
        std::wstring(1, letter);

    staging =
        wowRoot / L"Data" /
        (stem + L".new");

    rollback =
        wowRoot / L"Data" /
        (stem + L".rollback");

    return true;
}

bool CrackUrl(
    const std::wstring& url,
    UrlParts& parts,
    std::wstring& error) {
    URL_COMPONENTSW components{};
    components.dwStructSize =
        sizeof(components);
    components.dwHostNameLength =
        static_cast<DWORD>(-1);
    components.dwUrlPathLength =
        static_cast<DWORD>(-1);
    components.dwExtraInfoLength =
        static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(
            url.c_str(),
            0,
            0,
            &components)) {
        error =
            L"Invalid MPQ release asset URL: " +
            WindowsError(
                GetLastError());
        return false;
    }

    if (components.nScheme !=
        INTERNET_SCHEME_HTTPS) {
        error =
            L"MPQ release asset URL is not HTTPS.";
        return false;
    }

    parts.host.assign(
        components.lpszHostName,
        components.dwHostNameLength);
    parts.path.assign(
        components.lpszUrlPath,
        components.dwUrlPathLength);

    if (components.dwExtraInfoLength >
            0 &&
        components.lpszExtraInfo) {
        parts.path.append(
            components.lpszExtraInfo,
            components.dwExtraInfoLength);
    }

    if (parts.path.empty()) {
        parts.path = L"/";
    }

    parts.port =
        components.nPort;
    return true;
}

bool OpenUrl(
    const std::wstring& url,
    Handles& handles,
    DWORD& status,
    std::wstring& error) {
    UrlParts parts;
    if (!CrackUrl(
            url,
            parts,
            error)) {
        return false;
    }

    handles.session =
        WinHttpOpen(
            kUserAgent,
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

    if (!handles.session) {
        error =
            L"WinHTTP session failed: " +
            WindowsError(
                GetLastError());
        return false;
    }

    WinHttpSetTimeouts(
        handles.session,
        10000,
        10000,
        30000,
        30000);

    handles.connection =
        WinHttpConnect(
            handles.session,
            parts.host.c_str(),
            parts.port,
            0);

    if (!handles.connection) {
        error =
            L"MPQ release asset connection failed: " +
            WindowsError(
                GetLastError());
        return false;
    }

    handles.request =
        WinHttpOpenRequest(
            handles.connection,
            L"GET",
            parts.path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

    if (!handles.request) {
        error =
            L"MPQ release asset request failed: " +
            WindowsError(
                GetLastError());
        return false;
    }

    const wchar_t headers[] =
        L"Accept: application/octet-stream\r\n";

    if (!WinHttpAddRequestHeaders(
            handles.request,
            headers,
            static_cast<DWORD>(-1),
            WINHTTP_ADDREQ_FLAG_ADD) ||
        !WinHttpSendRequest(
            handles.request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) ||
        !WinHttpReceiveResponse(
            handles.request,
            nullptr)) {
        error =
            L"MPQ release asset request failed: " +
            WindowsError(
                GetLastError());
        return false;
    }

    DWORD statusSize =
        sizeof(status);

    if (!WinHttpQueryHeaders(
            handles.request,
            WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        error =
            L"Could not read MPQ release asset status: " +
            WindowsError(
                GetLastError());
        return false;
    }

    return true;
}

bool Sha256File(
    const std::filesystem::path& path,
    std::wstring& digest,
    std::wstring& error) {
    digest.clear();

    BCRYPT_ALG_HANDLE algorithm =
        nullptr;
    BCRYPT_HASH_HANDLE hash =
        nullptr;
    HANDLE file =
        INVALID_HANDLE_VALUE;

    auto cleanup = [&]() {
        if (file !=
            INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
        if (hash) {
            BCryptDestroyHash(hash);
        }
        if (algorithm) {
            BCryptCloseAlgorithmProvider(
                algorithm,
                0);
        }
    };

    NTSTATUS status =
        BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0);

    if (status < 0) {
        error =
            L"Could not initialize SHA-256.";
        cleanup();
        return false;
    }

    DWORD objectLength = 0;
    DWORD resultLength = 0;

    status =
        BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(
                &objectLength),
            sizeof(objectLength),
            &resultLength,
            0);

    if (status < 0) {
        error =
            L"Could not query SHA-256 object size.";
        cleanup();
        return false;
    }

    DWORD hashLength = 0;
    status =
        BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(
                &hashLength),
            sizeof(hashLength),
            &resultLength,
            0);

    if (status < 0) {
        error =
            L"Could not query SHA-256 digest size.";
        cleanup();
        return false;
    }

    std::vector<unsigned char> object(
        objectLength);
    std::vector<unsigned char> bytes(
        hashLength);

    status =
        BCryptCreateHash(
            algorithm,
            &hash,
            object.data(),
            static_cast<ULONG>(
                object.size()),
            nullptr,
            0,
            0);

    if (status < 0) {
        error =
            L"Could not create SHA-256 hash.";
        cleanup();
        return false;
    }

    file =
        CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL |
                FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);

    if (file ==
        INVALID_HANDLE_VALUE) {
        error =
            L"Could not open MPQ for verification: " +
            WindowsError(
                GetLastError());
        cleanup();
        return false;
    }

    std::array<unsigned char, 65536>
        buffer{};

    for (;;) {
        DWORD read = 0;
        if (!ReadFile(
                file,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read,
                nullptr)) {
            error =
                L"Could not read MPQ for verification: " +
                WindowsError(
                    GetLastError());
            cleanup();
            return false;
        }

        if (read == 0) {
            break;
        }

        status =
            BCryptHashData(
                hash,
                buffer.data(),
                read,
                0);

        if (status < 0) {
            error =
                L"SHA-256 hashing failed.";
            cleanup();
            return false;
        }
    }

    status =
        BCryptFinishHash(
            hash,
            bytes.data(),
            static_cast<ULONG>(
                bytes.size()),
            0);

    if (status < 0) {
        error =
            L"Could not finalize SHA-256.";
        cleanup();
        return false;
    }

    constexpr wchar_t hex[] =
        L"0123456789abcdef";

    digest.reserve(
        bytes.size() * 2);

    for (const unsigned char byte :
         bytes) {
        digest.push_back(
            hex[(byte >> 4) & 0x0F]);
        digest.push_back(
            hex[byte & 0x0F]);
    }

    cleanup();
    return true;
}

bool DownloadToPath(
    const std::wstring& url,
    const std::filesystem::path& path,
    std::uint64_t expectedSize,
    std::uint64_t& downloadedBytes,
    std::wstring& error) {
    downloadedBytes = 0;

    if (expectedSize == 0) {
        error =
            L"GitHub reports an empty MPQ asset.";
        return false;
    }

    Handles handles;
    DWORD status = 0;

    if (!OpenUrl(
            url,
            handles,
            status,
            error)) {
        return false;
    }

    if (status < 200 ||
        status >= 300) {
        error =
            L"MPQ asset returned HTTP status " +
            std::to_wstring(status) +
            L".";
        return false;
    }

    HANDLE file =
        CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (file ==
        INVALID_HANDLE_VALUE) {
        error =
            L"Could not create MPQ staging file: " +
            WindowsError(
                GetLastError());
        return false;
    }

    bool ok = true;
    std::array<unsigned char, 65536>
        buffer{};

    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(
                handles.request,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read)) {
            error =
                L"Could not read MPQ download: " +
                WindowsError(
                    GetLastError());
            ok = false;
            break;
        }

        if (read == 0) {
            break;
        }

        if (downloadedBytes >
            expectedSize -
                std::min<std::uint64_t>(
                    expectedSize,
                    static_cast<std::uint64_t>(
                        read))) {
            error =
                L"MPQ download exceeded GitHub release metadata size.";
            ok = false;
            break;
        }

        if (downloadedBytes +
                static_cast<std::uint64_t>(
                    read) >
            expectedSize) {
            error =
                L"MPQ download exceeded GitHub release metadata size.";
            ok = false;
            break;
        }

        DWORD written = 0;
        if (!WriteFile(
                file,
                buffer.data(),
                read,
                &written,
                nullptr) ||
            written != read) {
            error =
                L"Could not write MPQ staging file: " +
                WindowsError(
                    GetLastError());
            ok = false;
            break;
        }

        downloadedBytes +=
            static_cast<std::uint64_t>(
                read);
    }

    if (ok &&
        !FlushFileBuffers(file)) {
        error =
            L"Could not flush MPQ staging file: " +
            WindowsError(
                GetLastError());
        ok = false;
    }

    CloseHandle(file);

    if (ok &&
        downloadedBytes !=
            expectedSize) {
        error =
            L"Completed MPQ size does not match GitHub release metadata.";
        ok = false;
    }

    if (!ok) {
        DeleteFileW(
            path.c_str());
        downloadedBytes = 0;
    }

    return ok;
}

bool ValidateResolvedRelease(
    const PackageRecord& package,
    const MpqRelease& release,
    std::wstring& error) {
    if (release.kind !=
            ExactReleaseAssetKind::Mpq ||
        release.tag.empty() ||
        release.asset.name !=
            package.asset ||
        release.asset.downloadUrl.empty() ||
        release.asset.size == 0 ||
        !IsHexDigest(
            release.expectedSha256)) {
        error =
            L"Resolved MPQ release metadata is incomplete or does not match the configured exact asset.";
        return false;
    }

    return true;
}

bool ValidateInstallTarget(
    const PackageRecord& package,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    std::filesystem::path& target,
    bool& targetExists,
    std::wstring& error) {
    if (TargetReservedByOtherPackage(
            packages,
            package,
            error)) {
        return false;
    }

    if (!MpqTargetPath(
            wowRoot,
            package,
            target,
            error)) {
        return false;
    }

    bool regular = false;
    if (!InspectPath(
            target,
            targetExists,
            regular,
            error)) {
        return false;
    }

    if (targetExists &&
        !regular) {
        error =
            L"Configured MPQ target exists but is not a regular file: " +
            target.wstring();
        return false;
    }

    if (targetExists &&
        !PackageOwnsTarget(
            package)) {
        error =
            L"Refusing to overwrite existing unmanaged MPQ slot '" +
            package.targetPath +
            L"'.";
        return false;
    }

    return true;
}

bool CommitPreparedStaging(
    const PackageRecord& package,
    const MpqRelease& release,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    const std::filesystem::path& staging,
    const std::filesystem::path& rollback,
    MpqInstallTransaction& transaction,
    std::wstring& actualSha256,
    std::wstring& error) {
    std::filesystem::path target;
    bool targetExists = false;

    if (!ValidateInstallTarget(
            package,
            wowRoot,
            packages,
            target,
            targetExists,
            error)) {
        DeleteFileW(
            staging.c_str());
        return false;
    }

    if (targetExists) {
        if (!ReplaceFileW(
                target.c_str(),
                staging.c_str(),
                rollback.c_str(),
                0,
                nullptr,
                nullptr)) {
            error =
                L"Could not replace managed MPQ in place: " +
                WindowsError(
                    GetLastError());
            DeleteFileW(
                staging.c_str());
            return false;
        }
    } else {
        if (!MoveFileExW(
                staging.c_str(),
                target.c_str(),
                MOVEFILE_WRITE_THROUGH)) {
            error =
                L"Could not move verified MPQ into its assigned Data slot: " +
                WindowsError(
                    GetLastError());
            DeleteFileW(
                staging.c_str());
            return false;
        }
    }

    transaction.targetPath =
        target;
    transaction.rollbackPath =
        rollback;
    transaction.hadPreviousTarget =
        targetExists;
    transaction.active =
        true;

    if (!VerifyMpqFile(
            target,
            release.asset.size,
            release.expectedSha256,
            actualSha256,
            error)) {
        std::wstring rollbackError;
        if (!RollbackMpqInstall(
                transaction,
                rollbackError)) {
            error +=
                L" Rollback also failed: " +
                rollbackError;
        }
        return false;
    }

    return true;
}

bool PathExists(
    const std::filesystem::path& path,
    bool& exists,
    std::wstring& error) {
    bool regular = false;
    return InspectPath(
        path,
        exists,
        regular,
        error);
}

} // namespace

bool IsMpqPackage(
    const PackageRecord& package) {
    return
        package.provider == L"github" &&
        package.mode == L"release" &&
        package.target == L"data";
}

bool ValidateMpqPackage(
    const PackageRecord& package,
    std::wstring& error) {
    if (!ValidateMpqPackageRecord(
            package,
            error)) {
        return false;
    }

    return ValidateLatestStableExactReleaseAssetPackage(
        package,
        ExactReleaseAssetKind::Mpq,
        error);
}

std::wstring MpqPatchTargetForLetter(
    wchar_t letter) {
    letter =
        static_cast<wchar_t>(
            std::towupper(letter));

    if (letter < L'A' ||
        letter > L'Z') {
        return {};
    }

    return
        L"Data/patch-" +
        std::wstring(1, letter) +
        L".mpq";
}

bool AssignMpqTargetPath(
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    PackageRecord& package,
    std::wstring& error) {
    error.clear();

    if (wowRoot.empty()) {
        error =
            L"World of Warcraft root path is empty.";
        return false;
    }

    if (!ValidateMpqSource(
            package,
            error)) {
        return false;
    }

    const PackageRecord* persisted =
        nullptr;

    for (const auto& existing :
         packages) {
        if (!EqualsInsensitive(
                existing.id,
                package.id)) {
            continue;
        }

        if (persisted) {
            error =
                L"Duplicate persisted package identity prevents deterministic MPQ target reuse.";
            return false;
        }

        persisted =
            &existing;
    }

    if (persisted &&
        !persisted->targetPath.empty()) {
        if (!package.targetPath.empty() &&
            !EqualsInsensitive(
                package.targetPath,
                persisted->targetPath)) {
            error =
                L"Refusing to change the persisted MPQ patch-letter destination from '" +
                persisted->targetPath +
                L"' to '" +
                package.targetPath +
                L"'.";
            return false;
        }

        package.targetPath =
            persisted->targetPath;
    }

    if (!package.targetPath.empty()) {
        if (!ValidateMpqPackage(
                package,
                error)) {
            return false;
        }

        if (TargetReservedByOtherPackage(
                packages,
                package,
                error)) {
            return false;
        }

        return true;
    }

    const std::filesystem::path dataRoot =
        wowRoot / L"Data";

    std::error_code ec;
    if (!std::filesystem::is_directory(
            dataRoot,
            ec) ||
        ec) {
        error =
            L"WoW Data directory is missing or inaccessible.";
        return false;
    }

    std::array<bool, 26> occupied{};

    for (const auto& entry :
         std::filesystem::directory_iterator(
             dataRoot,
             ec)) {
        if (ec) {
            break;
        }

        wchar_t letter = 0;
        if (PatchLetterFromFileName(
                entry.path()
                    .filename()
                    .wstring(),
                letter)) {
            occupied[
                static_cast<std::size_t>(
                    letter - L'A')] =
                true;
        }
    }

    if (ec) {
        error =
            L"Could not scan WoW Data directory for existing MPQ patch slots.";
        return false;
    }

    for (const auto& existing :
         packages) {
        if (EqualsInsensitive(
                existing.id,
                package.id)) {
            continue;
        }

        if (existing.mode != L"release" ||
            existing.target != L"data") {
            continue;
        }

        wchar_t letter = 0;
        if (!PatchLetterFromTarget(
                existing.targetPath,
                letter)) {
            error =
                L"Existing TocPilot MPQ package '" +
                existing.id +
                L"' has an invalid persisted patch-letter target.";
            return false;
        }

        occupied[
            static_cast<std::size_t>(
                letter - L'A')] =
            true;
    }

    for (wchar_t letter = L'A';
         letter <= L'Z';
         ++letter) {
        if (occupied[
                static_cast<std::size_t>(
                    letter - L'A')]) {
            continue;
        }

        package.targetPath =
            MpqPatchTargetForLetter(
                letter);

        if (!ValidateMpqPackage(
                package,
                error)) {
            package.targetPath.clear();
            return false;
        }

        return true;
    }

    error =
        L"No free Data patch-letter MPQ slot remains from A through Z.";
    return false;
}

bool MpqTargetPath(
    const std::filesystem::path& wowRoot,
    const PackageRecord& package,
    std::filesystem::path& target,
    std::wstring& error) {
    target.clear();

    if (wowRoot.empty()) {
        error =
            L"World of Warcraft root path is empty.";
        return false;
    }

    if (!ValidateMpqPackage(
            package,
            error)) {
        return false;
    }

    const std::wstring canonical =
        CanonicalSeparators(
            package.targetPath);

    target =
        wowRoot /
        std::filesystem::path(
            canonical);
    return true;
}

bool ResolveLatestMpqRelease(
    const PackageRecord& package,
    MpqRelease& release,
    std::wstring& error) {
    release = {};
    error.clear();

    if (!ValidateMpqPackage(
            package,
            error)) {
        return false;
    }

    return ResolveLatestStableExactReleaseAsset(
        package,
        ExactReleaseAssetKind::Mpq,
        release,
        error);
}

bool VerifyMpqFile(
    const std::filesystem::path& path,
    std::uint64_t expectedSize,
    std::wstring_view expectedSha256,
    std::wstring& actualSha256,
    std::wstring& error) {
    actualSha256.clear();
    error.clear();

    if (expectedSize == 0 ||
        !IsHexDigest(
            expectedSha256)) {
        error =
            L"MPQ integrity metadata is incomplete.";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(
            path,
            ec) ||
        ec) {
        error =
            L"MPQ file is missing or not a regular file: " +
            path.wstring();
        return false;
    }

    const auto size =
        std::filesystem::file_size(
            path,
            ec);

    if (ec) {
        error =
            L"Could not read MPQ file size.";
        return false;
    }

    if (size !=
        expectedSize) {
        error =
            L"MPQ file size does not match GitHub release metadata.";
        return false;
    }

    if (!Sha256File(
            path,
            actualSha256,
            error)) {
        return false;
    }

    if (Lower(actualSha256) !=
        Lower(
            std::wstring(
                expectedSha256))) {
        error =
            L"MPQ SHA-256 verification failed.";
        actualSha256.clear();
        return false;
    }

    return true;
}

bool VerifyInstalledMpq(
    const PackageRecord& package,
    const MpqRelease& release,
    const std::filesystem::path& wowRoot,
    std::wstring& actualSha256,
    std::wstring& error) {
    actualSha256.clear();
    error.clear();

    if (!ValidateMpqPackage(
            package,
            error) ||
        !ValidateResolvedRelease(
            package,
            release,
            error)) {
        return false;
    }

    if (package.installedRevision.empty() ||
        !EqualsInsensitive(
            package.installedRevision,
            release.tag) ||
        !PackageOwnsTarget(
            package)) {
        error =
            L"Resolved MPQ release does not describe the package's installed revision.";
        return false;
    }

    std::filesystem::path target;
    if (!MpqTargetPath(
            wowRoot,
            package,
            target,
            error)) {
        return false;
    }

    return VerifyMpqFile(
        target,
        release.asset.size,
        release.expectedSha256,
        actualSha256,
        error);
}

bool BeginMpqInstallFromFile(
    const PackageRecord& package,
    const MpqRelease& release,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    const std::filesystem::path& payloadPath,
    MpqInstallTransaction& transaction,
    std::wstring& actualSha256,
    std::wstring& error) {
    transaction = {};
    actualSha256.clear();
    error.clear();

    if (!ValidateMpqPackage(
            package,
            error) ||
        !ValidateResolvedRelease(
            package,
            release,
            error)) {
        return false;
    }

    std::filesystem::path staging;
    std::filesystem::path rollback;

    if (!TransactionPaths(
            wowRoot,
            package,
            staging,
            rollback,
            error) ||
        !EnsureTransactionPathsFree(
            staging,
            rollback,
            error)) {
        return false;
    }

    if (!CopyFileW(
            payloadPath.c_str(),
            staging.c_str(),
            TRUE)) {
        error =
            L"Could not stage MPQ payload: " +
            WindowsError(
                GetLastError());
        return false;
    }

    std::wstring stagedDigest;
    if (!VerifyMpqFile(
            staging,
            release.asset.size,
            release.expectedSha256,
            stagedDigest,
            error)) {
        DeleteFileW(
            staging.c_str());
        return false;
    }

    return CommitPreparedStaging(
        package,
        release,
        wowRoot,
        packages,
        staging,
        rollback,
        transaction,
        actualSha256,
        error);
}

bool DownloadAndBeginMpqInstall(
    const PackageRecord& package,
    const MpqRelease& release,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    MpqInstallTransaction& transaction,
    std::uint64_t& downloadedBytes,
    std::wstring& actualSha256,
    std::wstring& error) {
    transaction = {};
    downloadedBytes = 0;
    actualSha256.clear();
    error.clear();

    if (!ValidateMpqPackage(
            package,
            error) ||
        !ValidateResolvedRelease(
            package,
            release,
            error)) {
        return false;
    }

    std::filesystem::path staging;
    std::filesystem::path rollback;

    if (!TransactionPaths(
            wowRoot,
            package,
            staging,
            rollback,
            error) ||
        !EnsureTransactionPathsFree(
            staging,
            rollback,
            error)) {
        return false;
    }

    if (!DownloadToPath(
            release.asset.downloadUrl,
            staging,
            release.asset.size,
            downloadedBytes,
            error)) {
        return false;
    }

    std::wstring stagedDigest;
    if (!VerifyMpqFile(
            staging,
            release.asset.size,
            release.expectedSha256,
            stagedDigest,
            error)) {
        DeleteFileW(
            staging.c_str());
        downloadedBytes = 0;
        return false;
    }

    if (!CommitPreparedStaging(
            package,
            release,
            wowRoot,
            packages,
            staging,
            rollback,
            transaction,
            actualSha256,
            error)) {
        downloadedBytes = 0;
        return false;
    }

    return true;
}

bool RollbackMpqInstall(
    MpqInstallTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.active) {
        error =
            L"MPQ install transaction is not active.";
        return false;
    }

    if (transaction.hadPreviousTarget) {
        bool targetExists = false;
        bool regular = false;

        if (!InspectPath(
                transaction.targetPath,
                targetExists,
                regular,
                error)) {
            return false;
        }

        bool rollbackExists = false;
        if (!PathExists(
                transaction.rollbackPath,
                rollbackExists,
                error)) {
            return false;
        }

        if (!rollbackExists) {
            error =
                L"MPQ rollback payload is missing.";
            return false;
        }

        BOOL restored = FALSE;

        if (targetExists) {
            restored =
                ReplaceFileW(
                    transaction.targetPath.c_str(),
                    transaction.rollbackPath.c_str(),
                    nullptr,
                    0,
                    nullptr,
                    nullptr);
        } else {
            restored =
                MoveFileExW(
                    transaction.rollbackPath.c_str(),
                    transaction.targetPath.c_str(),
                    MOVEFILE_WRITE_THROUGH);
        }

        if (!restored) {
            error =
                L"Could not restore previous MPQ during rollback: " +
                WindowsError(
                    GetLastError());
            return false;
        }
    } else {
        if (!DeleteFileW(
                transaction.targetPath.c_str())) {
            const DWORD code =
                GetLastError();

            if (code != ERROR_FILE_NOT_FOUND &&
                code != ERROR_PATH_NOT_FOUND) {
                error =
                    L"Could not remove newly installed MPQ during rollback: " +
                    WindowsError(code);
                return false;
            }
        }
    }

    transaction.active = false;
    return true;
}

bool FinalizeMpqInstall(
    MpqInstallTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.active) {
        error =
            L"MPQ install transaction is not active.";
        return false;
    }

    if (transaction.hadPreviousTarget) {
        if (!DeleteFileW(
                transaction.rollbackPath.c_str())) {
            const DWORD code =
                GetLastError();

            if (code != ERROR_FILE_NOT_FOUND &&
                code != ERROR_PATH_NOT_FOUND) {
                error =
                    L"Could not remove MPQ rollback file after commit: " +
                    WindowsError(code);
                return false;
            }
        }
    }

    transaction.active = false;
    return true;
}

bool BeginMpqRemoval(
    const PackageRecord& package,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& packages,
    MpqRemovalTransaction& transaction,
    std::wstring& error) {
    transaction = {};
    error.clear();

    if (!ValidateMpqPackage(
            package,
            error)) {
        return false;
    }

    if (!PackageOwnsTarget(
            package)) {
        error =
            L"MPQ package does not have coherent installed ownership to remove.";
        return false;
    }

    if (TargetReservedByOtherPackage(
            packages,
            package,
            error)) {
        return false;
    }

    std::filesystem::path target;
    if (!MpqTargetPath(
            wowRoot,
            package,
            target,
            error)) {
        return false;
    }

    bool exists = false;
    bool regular = false;
    if (!InspectPath(
            target,
            exists,
            regular,
            error)) {
        return false;
    }

    if (!exists ||
        !regular) {
        error =
            L"Managed MPQ target is missing; removal was not recorded.";
        return false;
    }

    std::filesystem::path staging;
    std::filesystem::path rollback;
    if (!TransactionPaths(
            wowRoot,
            package,
            staging,
            rollback,
            error) ||
        !EnsureTransactionPathsFree(
            staging,
            rollback,
            error)) {
        return false;
    }

    if (!MoveFileExW(
            target.c_str(),
            rollback.c_str(),
            MOVEFILE_WRITE_THROUGH)) {
        error =
            L"Could not stage managed MPQ for removal: " +
            WindowsError(
                GetLastError());
        return false;
    }

    transaction.targetPath =
        target;
    transaction.rollbackPath =
        rollback;
    transaction.active =
        true;
    return true;
}

bool RollbackMpqRemoval(
    MpqRemovalTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.active) {
        error =
            L"MPQ removal transaction is not active.";
        return false;
    }

    bool targetExists = false;
    bool regular = false;
    if (!InspectPath(
            transaction.targetPath,
            targetExists,
            regular,
            error)) {
        return false;
    }

    if (targetExists) {
        error =
            L"Cannot rollback MPQ removal because the target slot is no longer empty.";
        return false;
    }

    if (!MoveFileExW(
            transaction.rollbackPath.c_str(),
            transaction.targetPath.c_str(),
            MOVEFILE_WRITE_THROUGH)) {
        error =
            L"Could not restore MPQ after removal rollback: " +
            WindowsError(
                GetLastError());
        return false;
    }

    transaction.active = false;
    return true;
}

bool FinalizeMpqRemoval(
    MpqRemovalTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.active) {
        error =
            L"MPQ removal transaction is not active.";
        return false;
    }

    if (!DeleteFileW(
            transaction.rollbackPath.c_str())) {
        const DWORD code =
            GetLastError();

        if (code != ERROR_FILE_NOT_FOUND &&
            code != ERROR_PATH_NOT_FOUND) {
            error =
                L"Could not delete removed MPQ rollback file: " +
                WindowsError(code);
            return false;
        }
    }

    transaction.active = false;
    return true;
}

} // namespace tp
