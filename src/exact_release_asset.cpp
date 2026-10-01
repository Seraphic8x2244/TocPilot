#include "exact_release_asset.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cwctype>
#include <string>
#include <string_view>

namespace tp {
namespace {

constexpr wchar_t kUserAgent[] =
    L"TocPilot/" TOCPILOT_VERSION_W;
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
        normalized.clear();
        return false;
    }

    normalized =
        Lower(std::move(digest));
    return true;
}

std::wstring_view KindLabel(
    ExactReleaseAssetKind kind) {
    switch (kind) {
    case ExactReleaseAssetKind::Dll:
        return L"DLL";
    case ExactReleaseAssetKind::Mpq:
        return L"MPQ";
    }

    return L"release asset";
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

} // namespace

std::wstring_view ExactReleaseAssetExtension(
    ExactReleaseAssetKind kind) {
    switch (kind) {
    case ExactReleaseAssetKind::Dll:
        return L".dll";
    case ExactReleaseAssetKind::Mpq:
        return L".mpq";
    }

    return {};
}

bool IsExactReleaseAssetName(
    std::wstring_view name,
    ExactReleaseAssetKind kind) {
    const auto extension =
        ExactReleaseAssetExtension(kind);

    return
        !name.empty() &&
        !extension.empty() &&
        name.find_first_of(
            L"/\\") ==
            std::wstring_view::npos &&
        EndsWithInsensitive(
            name,
            extension);
}

bool ValidateLatestStableExactReleaseAssetPackage(
    const PackageRecord& package,
    ExactReleaseAssetKind kind,
    std::wstring& error) {
    error.clear();

    if (package.provider != L"github" ||
        package.mode != L"release") {
        error =
            L"Package is not a GitHub exact-release-asset package.";
        return false;
    }

    if (package.releasePolicy !=
        L"latest_stable") {
        error =
            L"Exact release-asset packages require latest-stable release tracking.";
        return false;
    }

    if (!IsExactReleaseAssetName(
            package.asset,
            kind)) {
        error =
            L"Exact " +
            std::wstring(KindLabel(kind)) +
            L" release assets require one exact " +
            std::wstring(
                ExactReleaseAssetExtension(kind)) +
            L" filename.";
        return false;
    }

    return true;
}

bool ResolveExactReleaseAssetMetadata(
    const PackageRecord& package,
    ExactReleaseAssetKind kind,
    const GitHubReleaseInfo& metadata,
    ExactReleaseAssetRelease& release,
    std::wstring& error) {
    release = {};
    error.clear();

    if (!ValidateLatestStableExactReleaseAssetPackage(
            package,
            kind,
            error)) {
        return false;
    }

    if (metadata.tag.empty() ||
        metadata.draft ||
        metadata.prerelease) {
        error =
            L"Release metadata is not a published stable release.";
        return false;
    }

    if (!FindExactGitHubReleaseAsset(
            metadata,
            package.asset,
            release.asset,
            error)) {
        return false;
    }

    release.kind = kind;
    release.tag =
        metadata.tag;
    NormalizeSha256Digest(
        release.asset.digest,
        release.expectedSha256);

    return true;
}

bool ResolveLatestStableExactReleaseAsset(
    const PackageRecord& package,
    ExactReleaseAssetKind kind,
    ExactReleaseAssetRelease& release,
    std::wstring& error) {
    release = {};
    error.clear();

    if (!ValidateLatestStableExactReleaseAssetPackage(
            package,
            kind,
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

    if (!ResolveExactReleaseAssetMetadata(
            package,
            kind,
            metadata,
            release,
            error)) {
        return false;
    }

    if (!release.expectedSha256.empty()) {
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
            L"' checksum asset. TocPilot will not install an unverifiable " +
            std::wstring(KindLabel(kind)) +
            L".";
        release = {};
        return false;
    }

    std::string body;
    if (!DownloadText(
            checksum.downloadUrl,
            body,
            error)) {
        error =
            L"Could not obtain the " +
            std::wstring(KindLabel(kind)) +
            L" checksum: " +
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

} // namespace tp
