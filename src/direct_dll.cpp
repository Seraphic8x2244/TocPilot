#include "direct_dll.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kUserAgent[] =
    L"TocPilot/" TOCPILOT_VERSION_W;
constexpr std::uint64_t kMaxDllBytes =
    256ull * 1024ull * 1024ull;
constexpr std::size_t kMaxChecksumBytes =
    1024 * 1024;

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
    bool secure = false;
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
            reinterpret_cast<LPWSTR>(
                &buffer),
            0,
            nullptr);

    std::wstring message;
    if (length != 0 &&
        buffer) {
        message.assign(
            buffer,
            length);
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

bool EndsWithInsensitive(
    std::wstring_view value,
    std::wstring_view suffix) {
    if (value.size() <
        suffix.size()) {
        return false;
    }

    const std::size_t offset =
        value.size() -
        suffix.size();

    for (std::size_t i = 0;
         i < suffix.size();
         ++i) {
        if (std::towlower(
                value[offset + i]) !=
            std::towlower(
                suffix[i])) {
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
                (ch >= L'0' &&
                 ch <= L'9') ||
                (ch >= L'a' &&
                 ch <= L'f') ||
                (ch >= L'A' &&
                 ch <= L'F');
        });
}

bool NormalizeSha256Digest(
    std::wstring digest,
    std::wstring& normalized) {
    constexpr std::wstring_view prefix =
        L"sha256:";

    if (digest.size() >=
            prefix.size() &&
        Lower(
            digest.substr(
                0,
                prefix.size())) ==
            prefix) {
        digest.erase(
            0,
            prefix.size());
    }

    if (!IsHexDigest(digest)) {
        return false;
    }

    normalized =
        Lower(std::move(digest));
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
            L"Invalid release asset URL: " +
            WindowsError(
                GetLastError());
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
    parts.secure =
        components.nScheme ==
        INTERNET_SCHEME_HTTPS;

    if (!parts.secure) {
        error =
            L"Release asset URL is not HTTPS.";
        return false;
    }

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
            L"Release asset connection failed: " +
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
            L"Release asset request failed: " +
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
            L"Release asset request failed: " +
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
            L"Could not read release asset status: " +
            WindowsError(
                GetLastError());
        return false;
    }

    return true;
}

bool DownloadText(
    const std::wstring& url,
    std::string& body,
    std::wstring& error) {
    body.clear();

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
            L"Checksum asset returned HTTP status " +
            std::to_wstring(status) +
            L".";
        return false;
    }

    std::array<char, 4096> buffer{};

    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(
                handles.request,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read)) {
            error =
                L"Could not read checksum asset: " +
                WindowsError(
                    GetLastError());
            return false;
        }

        if (read == 0) {
            break;
        }

        if (body.size() >
            kMaxChecksumBytes -
                static_cast<std::size_t>(
                    read)) {
            error =
                L"Checksum asset exceeds the 1 MiB safety limit.";
            return false;
        }

        body.append(
            buffer.data(),
            read);
    }

    return true;
}

bool ParseChecksumBody(
    std::string_view body,
    std::wstring& digest) {
    for (std::size_t i = 0;
         i + 64 <= body.size();
         ++i) {
        bool allHex = true;
        for (std::size_t j = 0;
             j < 64;
             ++j) {
            if (!std::isxdigit(
                    static_cast<unsigned char>(
                        body[i + j]))) {
                allHex = false;
                break;
            }
        }

        if (!allHex) {
            continue;
        }

        std::wstring candidate;
        candidate.reserve(64);
        for (std::size_t j = 0;
             j < 64;
             ++j) {
            candidate.push_back(
                static_cast<wchar_t>(
                    body[i + j]));
        }

        digest =
            Lower(
                std::move(candidate));
        return true;
    }

    return false;
}

bool Sha256File(
    const std::filesystem::path& path,
    std::wstring& digest,
    std::wstring& error) {
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
            L"Could not open the completed DLL for verification: " +
            WindowsError(
                GetLastError()) +
            L". The DLL may be in use or security software may have blocked or removed it.";
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
                L"Could not read the completed DLL for verification: " +
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

    digest.clear();
    digest.reserve(
        bytes.size() * 2);

    for (const unsigned char byte :
         bytes) {
        digest.push_back(
            hex[
                (byte >> 4) &
                0x0F]);
        digest.push_back(
            hex[
                byte &
                0x0F]);
    }

    cleanup();
    return true;
}

bool ExistingTargetWritable(
    const std::filesystem::path& target,
    std::wstring& error) {
    const DWORD attributes =
        GetFileAttributesW(
            target.c_str());

    if (attributes ==
        INVALID_FILE_ATTRIBUTES) {
        const DWORD code =
            GetLastError();

        if (code ==
                ERROR_FILE_NOT_FOUND ||
            code ==
                ERROR_PATH_NOT_FOUND) {
            return true;
        }

        error =
            L"Could not inspect the target DLL: " +
            WindowsError(code);
        return false;
    }

    if ((attributes &
         FILE_ATTRIBUTE_DIRECTORY) != 0) {
        error =
            L"The configured DLL destination is a directory.";
        return false;
    }

    HANDLE probe =
        CreateFileW(
            target.c_str(),
            GENERIC_READ |
                GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (probe ==
        INVALID_HANDLE_VALUE) {
        error =
            L"The existing DLL cannot be opened for update: " +
            WindowsError(
                GetLastError()) +
            L". Close WoW and retry; security software may also be blocking the file.";
        return false;
    }

    CloseHandle(probe);
    return true;
}

bool DownloadToFinalPath(
    const std::wstring& url,
    const std::filesystem::path& target,
    std::uint64_t expectedSize,
    std::uint64_t& downloadedBytes,
    std::wstring& error) {
    downloadedBytes = 0;

    if (expectedSize == 0) {
        error =
            L"GitHub reports an empty DLL asset.";
        return false;
    }

    if (expectedSize >
        kMaxDllBytes) {
        error =
            L"DLL asset exceeds the 256 MiB safety limit.";
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
            L"DLL asset returned HTTP status " +
            std::to_wstring(status) +
            L".";
        return false;
    }

    HANDLE file =
        CreateFileW(
            target.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (file ==
        INVALID_HANDLE_VALUE) {
        error =
            L"Could not open the exact DLL destination for writing: " +
            WindowsError(
                GetLastError()) +
            L". Close WoW and retry; security software may also be blocking the file.";
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
                L"Could not read the DLL download: " +
                WindowsError(
                    GetLastError());
            ok = false;
            break;
        }

        if (read == 0) {
            break;
        }

        if (downloadedBytes >
            kMaxDllBytes -
                static_cast<std::uint64_t>(
                    read)) {
            error =
                L"DLL download exceeds the 256 MiB safety limit.";
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
                L"Could not write the DLL to its exact destination: " +
                WindowsError(
                    GetLastError()) +
                L". Close WoW and retry; security software may also be blocking the file.";
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
            L"Could not flush the completed DLL to disk: " +
            WindowsError(
                GetLastError());
        ok = false;
    }

    CloseHandle(file);

    if (ok &&
        downloadedBytes !=
            expectedSize) {
        error =
            L"Completed DLL size does not match GitHub release metadata.";
        ok = false;
    }

    if (!ok) {
        DeleteFileW(
            target.c_str());
        downloadedBytes = 0;
    }

    return ok;
}

} // namespace

bool IsDirectDllPackage(
    const PackageRecord& package) {
    return
        package.provider == L"github" &&
        package.mode == L"release" &&
        package.target == L"wow_root";
}

bool ValidateDirectDllPackage(
    const PackageRecord& package,
    std::wstring& error) {
    error.clear();

    if (!IsDirectDllPackage(
            package)) {
        error =
            L"Package is not a GitHub direct-DLL release package.";
        return false;
    }

    if (package.releasePolicy !=
        L"latest_stable") {
        error =
            L"Direct DLL packages currently require latest-stable release tracking.";
        return false;
    }

    if (package.asset.empty() ||
        !EndsWithInsensitive(
            package.asset,
            L".dll")) {
        error =
            L"Direct DLL packages require one exact .dll release asset name.";
        return false;
    }

    if (package.asset.find_first_of(
            L"/\\") !=
        std::wstring::npos) {
        error =
            L"Direct DLL asset name must be a filename, not a path.";
        return false;
    }

    if (package.targetPath.empty() ||
        package.targetPath !=
            package.asset ||
        package.targetPath.find_first_of(
            L"/\\") !=
            std::wstring::npos) {
        error =
            L"Direct DLL destination must exactly match the selected DLL asset filename in the WoW root.";
        return false;
    }

    const std::filesystem::path relative(
        package.targetPath);

    if (relative.is_absolute() ||
        relative.has_parent_path() ||
        relative.filename().wstring() !=
            package.targetPath) {
        error =
            L"Direct DLL destination is not a safe WoW-root filename.";
        return false;
    }

    return true;
}

bool DirectDllTargetPath(
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

    if (!ValidateDirectDllPackage(
            package,
            error)) {
        return false;
    }

    target =
        wowRoot /
        package.targetPath;
    return true;
}

bool ResolveLatestDirectDllRelease(
    const PackageRecord& package,
    DirectDllRelease& release,
    std::wstring& error) {
    release = {};
    error.clear();

    if (!ValidateDirectDllPackage(
            package,
            error)) {
        return false;
    }

    GitHubReleaseInfo metadata;
    if (!FetchLatestStableGitHubRelease(
            package.repository,
            metadata,
            error)) {
        return false;
    }

    if (!FindExactGitHubReleaseAsset(
            metadata,
            package.asset,
            release.asset,
            error)) {
        return false;
    }

    release.tag =
        metadata.tag;

    if (NormalizeSha256Digest(
            release.asset.digest,
            release.expectedSha256)) {
        return true;
    }

    GitHubReleaseAsset checksum;
    std::wstring lookupError;
    const std::wstring checksumName =
        package.asset +
        L".sha256";

    if (!FindExactGitHubReleaseAsset(
            metadata,
            checksumName,
            checksum,
            lookupError)) {
        error =
            L"Release asset has no usable SHA-256 digest and no exact '" +
            checksumName +
            L"' checksum asset. TocPilot will not install an unverifiable DLL.";
        release = {};
        return false;
    }

    std::string body;
    if (!DownloadText(
            checksum.downloadUrl,
            body,
            error)) {
        error =
            L"Could not obtain the DLL checksum: " +
            error;
        release = {};
        return false;
    }

    if (!ParseChecksumBody(
            body,
            release.expectedSha256)) {
        error =
            L"Checksum asset does not contain a SHA-256 digest.";
        release = {};
        return false;
    }

    return true;
}

bool DownloadAndVerifyDirectDll(
    const PackageRecord& package,
    const DirectDllRelease& release,
    const std::filesystem::path& wowRoot,
    std::uint64_t& downloadedBytes,
    std::wstring& actualSha256,
    std::wstring& error) {
    downloadedBytes = 0;
    actualSha256.clear();
    error.clear();

    std::filesystem::path target;
    if (!DirectDllTargetPath(
            wowRoot,
            package,
            target,
            error)) {
        return false;
    }

    if (release.tag.empty() ||
        release.asset.name !=
            package.asset ||
        release.asset.downloadUrl.empty() ||
        !IsHexDigest(
            release.expectedSha256)) {
        error =
            L"Resolved DLL release metadata is incomplete or does not match the configured exact asset.";
        return false;
    }

    if (!ExistingTargetWritable(
            target,
            error)) {
        return false;
    }

    if (!DownloadToFinalPath(
            release.asset.downloadUrl,
            target,
            release.asset.size,
            downloadedBytes,
            error)) {
        return false;
    }

    if (!Sha256File(
            target,
            actualSha256,
            error)) {
        DeleteFileW(
            target.c_str());
        downloadedBytes = 0;
        return false;
    }

    if (Lower(actualSha256) !=
        Lower(release.expectedSha256)) {
        error =
            L"SHA-256 verification failed. The downloaded DLL was removed and installed state was not changed.";
        DeleteFileW(
            target.c_str());
        downloadedBytes = 0;
        actualSha256.clear();
        return false;
    }

    const DWORD attributes =
        GetFileAttributesW(
            target.c_str());

    if (attributes ==
            INVALID_FILE_ATTRIBUTES ||
        (attributes &
         FILE_ATTRIBUTE_DIRECTORY) != 0) {
        error =
            L"The verified DLL disappeared before TocPilot could record installed state. Security software may have quarantined it.";
        downloadedBytes = 0;
        actualSha256.clear();
        return false;
    }

    return true;
}

} // namespace tp
