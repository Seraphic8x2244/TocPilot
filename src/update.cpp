#include "update.h"
#include "github_release.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kLatestReleaseUrl[] =
    L"https://github.com/Seraphic8x2244/TocPilot/releases/latest";
constexpr wchar_t kReleaseTagPathPrefix[] =
    L"/Seraphic8x2244/TocPilot/releases/tag/";
constexpr wchar_t kUserAgent[] = L"TocPilot/" TOCPILOT_VERSION_W;

struct InternetHandles {
    HINTERNET session = nullptr;
    HINTERNET connection = nullptr;
    HINTERNET request = nullptr;

    ~InternetHandles() {
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

std::wstring Utf8ToWide(std::string_view input) {
    if (input.empty()) {
        return {};
    }

    const int count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        nullptr,
        0);

    if (count <= 0) {
        return {};
    }

    std::wstring output(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        output.data(),
        count);
    return output;
}

std::wstring WindowsError(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    if (length == 0 || !buffer) {
        return L"Windows error " + std::to_wstring(code);
    }

    std::wstring message(buffer, length);
    LocalFree(buffer);

    while (!message.empty() &&
           (message.back() == L'\r' ||
            message.back() == L'\n' ||
            std::iswspace(message.back()))) {
        message.pop_back();
    }
    return message;
}

bool CrackUrl(const std::wstring& url, UrlParts& parts, std::wstring& error) {
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = static_cast<DWORD>(-1);
    uc.dwUrlPathLength = static_cast<DWORD>(-1);
    uc.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        error = L"Invalid URL: " + WindowsError(GetLastError());
        return false;
    }

    parts.host.assign(uc.lpszHostName, uc.dwHostNameLength);
    parts.path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0 && uc.lpszExtraInfo) {
        parts.path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    }
    if (parts.path.empty()) {
        parts.path = L"/";
    }
    parts.port = uc.nPort;
    parts.secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

bool OpenRequest(
    const std::wstring& url,
    InternetHandles& handles,
    DWORD& status,
    std::wstring& error) {
    UrlParts parts;
    if (!CrackUrl(url, parts, error)) {
        return false;
    }

    if (!parts.secure) {
        error = L"Self-update transport requires an initial HTTPS URL.";
        return false;
    }

    handles.session = WinHttpOpen(
        kUserAgent,
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!handles.session) {
        error = L"WinHTTP session failed: " + WindowsError(GetLastError());
        return false;
    }

    WinHttpSetTimeouts(handles.session, 10000, 10000, 30000, 30000);

    handles.connection = WinHttpConnect(
        handles.session,
        parts.host.c_str(),
        parts.port,
        0);

    if (!handles.connection) {
        error = L"WinHTTP connection failed: " + WindowsError(GetLastError());
        return false;
    }

    handles.request = WinHttpOpenRequest(
        handles.connection,
        L"GET",
        parts.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        parts.secure ? WINHTTP_FLAG_SECURE : 0);

    if (!handles.request) {
        error = L"WinHTTP request failed: " + WindowsError(GetLastError());
        return false;
    }

    const wchar_t headers[] =
        L"Accept: */*\r\n";

    if (!WinHttpAddRequestHeaders(
            handles.request,
            headers,
            static_cast<DWORD>(-1),
            WINHTTP_ADDREQ_FLAG_ADD)) {
        error = L"Could not add HTTP headers: " + WindowsError(GetLastError());
        return false;
    }

    if (!WinHttpSendRequest(
            handles.request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0)) {
        error = L"HTTP send failed: " + WindowsError(GetLastError());
        return false;
    }

    if (!WinHttpReceiveResponse(handles.request, nullptr)) {
        error = L"HTTP response failed: " + WindowsError(GetLastError());
        return false;
    }

    status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(
            handles.request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        error = L"Could not read HTTP status: " + WindowsError(GetLastError());
        return false;
    }

    return true;
}

bool QueryEffectiveUrl(
    HINTERNET request,
    std::wstring& url,
    std::wstring& error) {
    url.clear();

    DWORD bytes = 0;
    WinHttpQueryOption(
        request,
        WINHTTP_OPTION_URL,
        nullptr,
        &bytes);

    if (bytes == 0) {
        error =
            L"Could not determine final release URL: " +
            WindowsError(GetLastError());
        return false;
    }

    std::vector<wchar_t> buffer(
        bytes / sizeof(wchar_t) + 1,
        L'\0');

    if (!WinHttpQueryOption(
            request,
            WINHTTP_OPTION_URL,
            buffer.data(),
            &bytes)) {
        error =
            L"Could not determine final release URL: " +
            WindowsError(GetLastError());
        return false;
    }

    url.assign(buffer.data());
    if (url.empty()) {
        error = L"GitHub returned an empty final release URL.";
        return false;
    }

    return true;
}

bool HttpGetString(
    const std::wstring& url,
    std::size_t maxBytes,
    std::string& body,
    DWORD& status,
    std::wstring& error) {
    InternetHandles handles;
    if (!OpenRequest(url, handles, status, error)) {
        return false;
    }

    body.clear();
    std::array<char, 16384> buffer{};

    for (;;) {
        DWORD bytesRead = 0;
        if (!WinHttpReadData(
                handles.request,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead)) {
            error = L"HTTP read failed: " + WindowsError(GetLastError());
            return false;
        }

        if (bytesRead == 0) {
            break;
        }

        if (body.size() > maxBytes ||
            static_cast<std::size_t>(bytesRead) >
                maxBytes - body.size()) {
            error = L"Checksum response exceeds the 64 KiB limit.";
            return false;
        }

        body.append(buffer.data(), bytesRead);
    }

    return true;
}

bool DownloadFile(
    const std::wstring& url,
    const std::filesystem::path& destination,
    std::uint64_t expectedSize,
    std::wstring& error) {
    InternetHandles handles;
    DWORD status = 0;
    if (!OpenRequest(url, handles, status, error)) {
        return false;
    }

    if (status < 200 || status >= 300) {
        error = L"HTTP request returned status " + std::to_wstring(status);
        return false;
    }

    HANDLE file = CreateFileW(
        destination.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        error = L"Could not create update file: " + WindowsError(GetLastError());
        return false;
    }

    bool ok = true;
    std::uint64_t totalBytes = 0;
    std::array<unsigned char, 65536> buffer{};

    for (;;) {
        DWORD bytesRead = 0;
        if (!WinHttpReadData(
                handles.request,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead)) {
            error = L"Download read failed: " + WindowsError(GetLastError());
            ok = false;
            break;
        }

        if (bytesRead == 0) {
            break;
        }

        if (totalBytes > expectedSize ||
            static_cast<std::uint64_t>(bytesRead) >
                expectedSize - totalBytes) {
            error =
                L"Downloaded update is larger than the release asset size.";
            ok = false;
            break;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(
                file,
                buffer.data(),
                bytesRead,
                &bytesWritten,
                nullptr) ||
            bytesWritten != bytesRead) {
            error = L"Download write failed: " + WindowsError(GetLastError());
            ok = false;
            break;
        }

        totalBytes +=
            static_cast<std::uint64_t>(bytesWritten);
    }

    if (ok &&
        !ValidateSelfUpdateDownloadSize(
            expectedSize,
            totalBytes,
            error)) {
        ok = false;
    }

    if (ok && !FlushFileBuffers(file)) {
        error = L"Could not flush downloaded update: " +
            WindowsError(GetLastError());
        ok = false;
    }

    CloseHandle(file);

    if (!ok) {
        DeleteFileW(destination.c_str());
    }

    return ok;
}

struct SemVerIdentifier {
    std::wstring text;
    bool numeric = false;
    std::uint64_t number = 0;
};

struct SemVer {
    std::uint64_t major = 0;
    std::uint64_t minor = 0;
    std::uint64_t patch = 0;
    std::vector<SemVerIdentifier> prerelease;
};

bool ParseUnsigned(
    std::wstring_view text,
    bool allowLeadingZero,
    std::uint64_t& value) {
    if (text.empty() ||
        (!allowLeadingZero &&
         text.size() > 1 &&
         text.front() == L'0')) {
        return false;
    }

    std::uint64_t parsed = 0;
    for (const wchar_t ch : text) {
        if (ch < L'0' || ch > L'9') {
            return false;
        }

        const std::uint64_t digit =
            static_cast<std::uint64_t>(
                ch - L'0');

        if (parsed >
            (std::numeric_limits<std::uint64_t>::max() -
             digit) /
                10) {
            return false;
        }

        parsed =
            parsed * 10 +
            digit;
    }

    value = parsed;
    return true;
}

bool ParseIdentifiers(
    std::wstring_view text,
    bool prerelease,
    std::vector<SemVerIdentifier>* identifiers) {
    if (text.empty()) {
        return false;
    }

    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t dot =
            text.find(L'.', start);
        const std::size_t end =
            dot == std::wstring_view::npos
                ? text.size()
                : dot;
        const std::wstring_view part =
            text.substr(
                start,
                end - start);

        if (part.empty()) {
            return false;
        }

        bool numeric = true;
        for (const wchar_t ch : part) {
            const bool valid =
                (ch >= L'0' && ch <= L'9') ||
                (ch >= L'a' && ch <= L'z') ||
                (ch >= L'A' && ch <= L'Z') ||
                ch == L'-';

            if (!valid) {
                return false;
            }

            if (ch < L'0' || ch > L'9') {
                numeric = false;
            }
        }

        SemVerIdentifier identifier;
        identifier.text.assign(part);
        identifier.numeric = numeric;

        if (numeric) {
            if (!ParseUnsigned(
                    part,
                    !prerelease,
                    identifier.number)) {
                return false;
            }
        }

        if (identifiers) {
            identifiers->push_back(
                std::move(identifier));
        }

        if (dot == std::wstring_view::npos) {
            break;
        }
        start = dot + 1;
    }

    return true;
}

bool ParseSemVer(
    std::wstring_view text,
    SemVer& version) {
    version = {};

    if (!text.empty() &&
        (text.front() == L'v' ||
         text.front() == L'V')) {
        text.remove_prefix(1);
    }

    const std::size_t plus =
        text.find(L'+');

    std::wstring_view build;
    if (plus != std::wstring_view::npos) {
        build =
            text.substr(plus + 1);
        text =
            text.substr(0, plus);

        if (!ParseIdentifiers(
                build,
                false,
                nullptr)) {
            return false;
        }
    }

    const std::size_t dash =
        text.find(L'-');

    std::wstring_view prerelease;
    if (dash != std::wstring_view::npos) {
        prerelease =
            text.substr(dash + 1);
        text =
            text.substr(0, dash);
    }

    const std::size_t firstDot =
        text.find(L'.');
    if (firstDot == std::wstring_view::npos) {
        return false;
    }

    const std::size_t secondDot =
        text.find(
            L'.',
            firstDot + 1);
    if (secondDot == std::wstring_view::npos ||
        text.find(
            L'.',
            secondDot + 1) !=
            std::wstring_view::npos) {
        return false;
    }

    if (!ParseUnsigned(
            text.substr(0, firstDot),
            false,
            version.major) ||
        !ParseUnsigned(
            text.substr(
                firstDot + 1,
                secondDot - firstDot - 1),
            false,
            version.minor) ||
        !ParseUnsigned(
            text.substr(secondDot + 1),
            false,
            version.patch)) {
        return false;
    }

    if (!prerelease.empty() &&
        !ParseIdentifiers(
            prerelease,
            true,
            &version.prerelease)) {
        return false;
    }

    if (dash != std::wstring_view::npos &&
        version.prerelease.empty()) {
        return false;
    }

    return true;
}

int CompareSemVer(
    const SemVer& left,
    const SemVer& right) {
    if (left.major != right.major) {
        return left.major < right.major ? -1 : 1;
    }
    if (left.minor != right.minor) {
        return left.minor < right.minor ? -1 : 1;
    }
    if (left.patch != right.patch) {
        return left.patch < right.patch ? -1 : 1;
    }

    if (left.prerelease.empty() &&
        right.prerelease.empty()) {
        return 0;
    }
    if (left.prerelease.empty()) {
        return 1;
    }
    if (right.prerelease.empty()) {
        return -1;
    }

    const std::size_t common =
        std::min(
            left.prerelease.size(),
            right.prerelease.size());

    for (std::size_t i = 0;
         i < common;
         ++i) {
        const auto& a =
            left.prerelease[i];
        const auto& b =
            right.prerelease[i];

        if (a.numeric &&
            b.numeric) {
            if (a.number != b.number) {
                return a.number < b.number
                    ? -1
                    : 1;
            }
            continue;
        }

        if (a.numeric != b.numeric) {
            return a.numeric
                ? -1
                : 1;
        }

        if (a.text != b.text) {
            return a.text < b.text
                ? -1
                : 1;
        }
    }

    if (left.prerelease.size() ==
        right.prerelease.size()) {
        return 0;
    }

    return left.prerelease.size() <
            right.prerelease.size()
        ? -1
        : 1;
}

bool IsDevelopmentBuildVersion(
    const SemVer& version) {
    return
        version.prerelease.size() == 2 &&
        !version.prerelease[0].numeric &&
        version.prerelease[0].text == L"dev" &&
        version.prerelease[1].numeric;
}

bool Sha256File(
    const std::filesystem::path& path,
    std::wstring& hexDigest,
    std::wstring& error) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hashHandle = nullptr;
    HANDLE file = INVALID_HANDLE_VALUE;

    auto cleanup = [&]() {
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
        if (hashHandle) {
            BCryptDestroyHash(hashHandle);
        }
        if (algorithm) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    };

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0);
    if (status < 0) {
        error = L"Could not initialize SHA-256.";
        cleanup();
        return false;
    }

    DWORD objectLength = 0;
    DWORD resultLength = 0;
    status = BCryptGetProperty(
        algorithm,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectLength),
        sizeof(objectLength),
        &resultLength,
        0);
    if (status < 0) {
        error = L"Could not query SHA-256 object size.";
        cleanup();
        return false;
    }

    DWORD hashLength = 0;
    status = BCryptGetProperty(
        algorithm,
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&hashLength),
        sizeof(hashLength),
        &resultLength,
        0);
    if (status < 0) {
        error = L"Could not query SHA-256 digest size.";
        cleanup();
        return false;
    }

    std::vector<unsigned char> object(objectLength);
    std::vector<unsigned char> digest(hashLength);

    status = BCryptCreateHash(
        algorithm,
        &hashHandle,
        object.data(),
        static_cast<ULONG>(object.size()),
        nullptr,
        0,
        0);
    if (status < 0) {
        error = L"Could not create SHA-256 hash.";
        cleanup();
        return false;
    }

    file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        error = L"Could not open downloaded update for hashing: " +
            WindowsError(GetLastError());
        cleanup();
        return false;
    }

    std::array<unsigned char, 65536> buffer{};
    for (;;) {
        DWORD bytesRead = 0;
        if (!ReadFile(
                file,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr)) {
            error = L"Could not read downloaded update for hashing: " +
                WindowsError(GetLastError());
            cleanup();
            return false;
        }

        if (bytesRead == 0) {
            break;
        }

        status = BCryptHashData(hashHandle, buffer.data(), bytesRead, 0);
        if (status < 0) {
            error = L"SHA-256 hashing failed.";
            cleanup();
            return false;
        }
    }

    status = BCryptFinishHash(
        hashHandle,
        digest.data(),
        static_cast<ULONG>(digest.size()),
        0);
    if (status < 0) {
        error = L"Could not finalize SHA-256.";
        cleanup();
        return false;
    }

    static constexpr wchar_t kHex[] = L"0123456789abcdef";
    hexDigest.clear();
    hexDigest.reserve(digest.size() * 2);

    for (unsigned char byte : digest) {
        hexDigest.push_back(kHex[(byte >> 4) & 0x0F]);
        hexDigest.push_back(kHex[byte & 0x0F]);
    }

    cleanup();
    return true;
}

bool IsHexDigest(std::wstring_view digest) {
    if (digest.size() != 64) {
        return false;
    }

    return std::all_of(
        digest.begin(),
        digest.end(),
        [](wchar_t ch) {
            return (ch >= L'0' && ch <= L'9') ||
                (ch >= L'a' && ch <= L'f') ||
                (ch >= L'A' && ch <= L'F');
        });
}

std::wstring Lower(std::wstring value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

bool ResolveExpectedDigest(
    const ReleaseInfo& release,
    std::wstring& expected,
    std::wstring& error) {
    if (!release.assetDigest.empty()) {
        std::wstring digest = release.assetDigest;
        constexpr std::wstring_view prefix = L"sha256:";
        if (digest.size() >= prefix.size() &&
            Lower(digest.substr(0, prefix.size())) == prefix) {
            digest.erase(0, prefix.size());
        }

        if (IsHexDigest(digest)) {
            expected = Lower(std::move(digest));
            return true;
        }
    }

    if (release.checksumUrl.empty()) {
        error =
            L"Release asset has no usable SHA-256 digest and no "
            L"TocPilot.exe.sha256 fallback asset.";
        return false;
    }

    std::string checksumBody;
    DWORD checksumStatus = 0;
    if (!HttpGetString(
            release.checksumUrl,
            static_cast<std::size_t>(
                kMaxSelfUpdateChecksumBytes),
            checksumBody,
            checksumStatus,
            error)) {
        error = L"Could not download checksum: " + error;
        return false;
    }

    if (checksumStatus < 200 || checksumStatus >= 300) {
        error =
            L"Checksum request returned status " +
            std::to_wstring(checksumStatus);
        return false;
    }

    for (size_t i = 0; i + 64 <= checksumBody.size(); ++i) {
        bool allHex = true;
        for (size_t j = 0; j < 64; ++j) {
            if (!std::isxdigit(
                    static_cast<unsigned char>(checksumBody[i + j]))) {
                allHex = false;
                break;
            }
        }

        if (allHex) {
            expected = Lower(Utf8ToWide(
                std::string_view(checksumBody).substr(i, 64)));
            return true;
        }
    }

    error = L"Checksum asset does not contain a SHA-256 digest.";
    return false;
}

std::wstring QuoteArgument(const std::wstring& argument) {
    if (argument.empty()) {
        return L"\"\"";
    }

    if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return argument;
    }

    std::wstring result = L"\"";
    size_t backslashes = 0;

    for (wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }

        if (ch == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
            continue;
        }

        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(ch);
    }

    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

bool LaunchProcess(
    const std::filesystem::path& executable,
    const std::wstring& arguments,
    const std::filesystem::path& workingDirectory,
    std::wstring& error) {
    std::wstring commandLine =
        QuoteArgument(executable.wstring()) + L" " + arguments;

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
            executable.c_str(),
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
            &startup,
            &process)) {
        error = L"Could not start process: " + WindowsError(GetLastError());
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

template <typename Operation>
bool RetryFileOperation(Operation&& operation, DWORD& lastError) {
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (operation()) {
            lastError = ERROR_SUCCESS;
            return true;
        }

        lastError = GetLastError();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    return false;
}

void TryDeleteFile(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }

    DWORD lastError = ERROR_SUCCESS;
    if (RetryFileOperation(
            [&]() {
                if (DeleteFileW(path.c_str())) {
                    return true;
                }
                return GetLastError() == ERROR_FILE_NOT_FOUND;
            },
            lastError)) {
        return;
    }

    MoveFileExW(path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
}

} // namespace

bool CompareSelfUpdateVersionTags(
    std::wstring_view left,
    std::wstring_view right,
    int& comparison) {
    comparison = 0;

    SemVer leftVersion;
    SemVer rightVersion;

    if (!ParseSemVer(
            left,
            leftVersion) ||
        !ParseSemVer(
            right,
            rightVersion)) {
        return false;
    }

    comparison =
        CompareSemVer(
            leftVersion,
            rightVersion);
    return true;
}

bool SelectSelfUpdateReleaseForChannel(
    const std::vector<GitHubReleaseInfo>& releases,
    bool receiveDevelopmentBuilds,
    GitHubReleaseInfo& release,
    std::wstring& error) {
    release = {};
    error.clear();

    bool found = false;
    SemVer selectedVersion;

    for (const auto& candidate :
         releases) {
        if (candidate.draft) {
            continue;
        }

        SemVer version;
        if (!ParseSemVer(
                candidate.tag,
                version)) {
            continue;
        }

        if (candidate.prerelease) {
            if (!receiveDevelopmentBuilds ||
                !IsDevelopmentBuildVersion(
                    version)) {
                continue;
            }
        } else if (
            !version.prerelease.empty()) {
            continue;
        }

        if (!found ||
            CompareSemVer(
                version,
                selectedVersion) > 0) {
            release = candidate;
            selectedVersion =
                std::move(version);
            found = true;
        }
    }

    if (!found) {
        error =
            receiveDevelopmentBuilds
                ? L"No valid stable or development TocPilot release was found."
                : L"No valid stable TocPilot release was found.";
        return false;
    }

    return true;
}

bool ShouldInstallSelfUpdateVersion(
    std::wstring_view candidateTag,
    std::wstring_view currentTag,
    bool receiveDevelopmentBuilds,
    bool& shouldInstall,
    std::wstring& error) {
    shouldInstall = false;
    error.clear();

    SemVer candidate;
    SemVer current;

    if (!ParseSemVer(
            candidateTag,
            candidate) ||
        !ParseSemVer(
            currentTag,
            current)) {
        error =
            L"Self-update version tag is not valid semantic version metadata.";
        return false;
    }

    if (!receiveDevelopmentBuilds &&
        !current.prerelease.empty()) {
        shouldInstall =
            candidate.prerelease.empty() &&
            CompareSemVer(
                candidate,
                current) != 0;
        return true;
    }

    shouldInstall =
        CompareSemVer(
            candidate,
            current) > 0;
    return true;
}

bool ParseLatestReleaseTagFromUrl(
    std::wstring_view url,
    std::wstring& tag,
    std::wstring& error) {
    tag.clear();
    error.clear();

    UrlParts parts;
    if (!CrackUrl(std::wstring(url), parts, error)) {
        return false;
    }

    if (!parts.secure ||
        Lower(parts.host) != L"github.com") {
        error = L"Latest release redirect did not resolve to github.com.";
        return false;
    }

    std::wstring path = parts.path;
    const std::size_t suffix = path.find_first_of(L"?#");
    if (suffix != std::wstring::npos) {
        path.resize(suffix);
    }

    while (path.size() > 1 && path.back() == L'/') {
        path.pop_back();
    }

    constexpr std::wstring_view prefix =
        kReleaseTagPathPrefix;

    if (path.size() <= prefix.size() ||
        path.compare(0, prefix.size(), prefix) != 0) {
        error =
            L"Latest release redirect has an unexpected GitHub path.";
        return false;
    }

    tag = path.substr(prefix.size());
    if (tag.find(L'/') != std::wstring::npos) {
        tag.clear();
        error =
            L"Latest release redirect contains an invalid tag.";
        return false;
    }

    SemVer version;
    if (!ParseSemVer(tag, version)) {
        tag.clear();
        error =
            L"Latest release redirect does not contain a valid semantic version tag.";
        return false;
    }

    return true;
}

bool ResolveLatestReleaseTag(
    std::wstring& tag,
    std::wstring& error) {
    tag.clear();
    error.clear();

    InternetHandles handles;
    DWORD status = 0;
    if (!OpenRequest(
            kLatestReleaseUrl,
            handles,
            status,
            error)) {
        return false;
    }

    if (status < 200 || status >= 300) {
        error =
            L"GitHub latest-release page returned status " +
            std::to_wstring(status);
        return false;
    }

    std::wstring finalUrl;
    if (!QueryEffectiveUrl(
            handles.request,
            finalUrl,
            error)) {
        return false;
    }

    return ParseLatestReleaseTagFromUrl(
        finalUrl,
        tag,
        error);
}

bool ValidateSelfUpdateRelease(
    const ReleaseInfo& release,
    std::wstring& error) {
    error.clear();

    if (release.assetUrl.empty()) {
        error = L"No update asset URL is available.";
        return false;
    }

    UrlParts assetParts;
    if (!CrackUrl(
            release.assetUrl,
            assetParts,
            error)) {
        error =
            L"Invalid update asset URL: " +
            error;
        return false;
    }

    if (!assetParts.secure) {
        error =
            L"Update executable URL must use HTTPS.";
        return false;
    }

    if (release.assetSize == 0) {
        error =
            L"Update executable has an invalid zero-byte release asset size.";
        return false;
    }

    if (!release.checksumUrl.empty()) {
        UrlParts checksumParts;
        if (!CrackUrl(
                release.checksumUrl,
                checksumParts,
                error)) {
            error =
                L"Invalid update checksum URL: " +
                error;
            return false;
        }

        if (!checksumParts.secure) {
            error =
                L"Update checksum URL must use HTTPS.";
            return false;
        }

        if (release.checksumSize >
            kMaxSelfUpdateChecksumBytes) {
            error =
                L"Update checksum asset exceeds the 64 KiB limit.";
            return false;
        }
    }

    return true;
}

bool ValidateSelfUpdateDownloadSize(
    std::uint64_t expectedSize,
    std::uint64_t actualSize,
    std::wstring& error) {
    if (actualSize == expectedSize) {
        return true;
    }

    error =
        actualSize < expectedSize
            ? L"Downloaded update is smaller than the release asset size."
            : L"Downloaded update is larger than the release asset size.";
    return false;
}

std::filesystem::path ExecutablePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));

    if (length == 0 || length >= buffer.size()) {
        return {};
    }

    buffer.resize(length);
    return std::filesystem::path(buffer);
}

bool CheckLatestRelease(
    ReleaseInfo& release,
    ReleaseCheckState& state,
    std::wstring& error) {
    return CheckLatestRelease(
        false,
        release,
        state,
        error);
}

bool CheckLatestRelease(
    bool receiveDevelopmentBuilds,
    ReleaseInfo& release,
    ReleaseCheckState& state,
    std::wstring& error) {
    release = {};
    state = ReleaseCheckState::NoRelease;

    GitHubReleaseInfo metadata;

    if (receiveDevelopmentBuilds) {
        std::vector<GitHubReleaseInfo>
            releases;

        if (!FetchGitHubReleases(
                L"Seraphic8x2244/TocPilot",
                releases,
                error) ||
            !SelectSelfUpdateReleaseForChannel(
                releases,
                true,
                metadata,
                error)) {
            return false;
        }
    } else if (
        !FetchLatestStableGitHubRelease(
            L"Seraphic8x2244/TocPilot",
            metadata,
            error)) {
        return false;
    }

    SemVer candidate;
    if (!ParseSemVer(
            metadata.tag,
            candidate)) {
        error =
            L"Selected GitHub release does not have a valid semantic version tag.";
        return false;
    }

    GitHubReleaseAsset executable;
    if (!FindExactGitHubReleaseAsset(
            metadata,
            L"TocPilot.exe",
            executable,
            error)) {
        return false;
    }

    release.tag = metadata.tag;
    release.assetUrl =
        executable.downloadUrl;
    release.assetDigest =
        executable.digest;
    release.assetSize =
        executable.size;

    GitHubReleaseAsset checksum;
    std::wstring checksumError;
    if (FindExactGitHubReleaseAsset(
            metadata,
            L"TocPilot.exe.sha256",
            checksum,
            checksumError)) {
        release.checksumUrl =
            checksum.downloadUrl;
        release.checksumSize =
            checksum.size;
    }

    if (!ValidateSelfUpdateRelease(
            release,
            error)) {
        return false;
    }

    bool shouldInstall = false;
    if (!ShouldInstallSelfUpdateVersion(
            release.tag,
            TOCPILOT_VERSION_TAG_W,
            receiveDevelopmentBuilds,
            shouldInstall,
            error)) {
        return false;
    }

    state =
        shouldInstall
            ? ReleaseCheckState::UpdateAvailable
            : ReleaseCheckState::UpToDate;
    return true;
}

bool DownloadVerifyAndLaunchUpdater(
    const ReleaseInfo& release,
    DWORD parentPid,
    const std::filesystem::path& targetExe,
    const std::filesystem::path& workingDirectory,
    std::wstring& error) {
    if (!ValidateSelfUpdateRelease(
            release,
            error)) {
        return false;
    }

    wchar_t tempBuffer[MAX_PATH + 1]{};
    const DWORD tempLength = GetTempPathW(
        static_cast<DWORD>(std::size(tempBuffer)),
        tempBuffer);

    if (tempLength == 0 || tempLength >= std::size(tempBuffer)) {
        error = L"Could not locate the Windows temporary directory.";
        return false;
    }

    const std::filesystem::path tempRoot(tempBuffer);
    const std::filesystem::path stageDirectory =
        tempRoot /
        (L"TocPilot-update-" +
         std::to_wstring(parentPid) +
         L"-" +
         std::to_wstring(GetTickCount64()));

    std::error_code ec;
    std::filesystem::create_directories(stageDirectory, ec);
    if (ec) {
        error = L"Could not create update staging directory.";
        return false;
    }

    const auto stagedExe = stageDirectory / L"TocPilot.exe";

    if (!DownloadFile(
            release.assetUrl,
            stagedExe,
            release.assetSize,
            error)) {
        std::filesystem::remove_all(stageDirectory, ec);
        return false;
    }

    std::wstring expected;
    if (!ResolveExpectedDigest(release, expected, error)) {
        std::filesystem::remove_all(stageDirectory, ec);
        return false;
    }

    std::wstring actual;
    if (!Sha256File(stagedExe, actual, error)) {
        std::filesystem::remove_all(stageDirectory, ec);
        return false;
    }

    if (Lower(actual) != Lower(expected)) {
        error =
            L"SHA-256 mismatch. The downloaded executable will not be installed.";
        std::filesystem::remove_all(stageDirectory, ec);
        return false;
    }

    std::wstring arguments =
        L"--apply-update " +
        std::to_wstring(parentPid) +
        L" " +
        QuoteArgument(targetExe.wstring()) +
        L" " +
        QuoteArgument(workingDirectory.wstring());

    if (!LaunchProcess(stagedExe, arguments, stageDirectory, error)) {
        std::filesystem::remove_all(stageDirectory, ec);
        return false;
    }

    return true;
}

int RunUpdaterMode(
    DWORD parentPid,
    const std::filesystem::path& targetExe,
    const std::filesystem::path& workingDirectory) {
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (parent) {
        WaitForSingleObject(parent, INFINITE);
        CloseHandle(parent);
    }

    const auto stagedExe = ExecutablePath();
    const std::filesystem::path replacement =
        targetExe.wstring() + L".new";
    const std::filesystem::path backup =
        targetExe.wstring() + L".update-backup";

    TryDeleteFile(replacement);
    TryDeleteFile(backup);

    DWORD lastError = ERROR_SUCCESS;
    if (!RetryFileOperation(
            [&]() {
                return CopyFileW(
                    stagedExe.c_str(),
                    replacement.c_str(),
                    FALSE) != FALSE;
            },
            lastError)) {
        MessageBoxW(
            nullptr,
            (L"Could not stage the replacement executable.\n\n" +
             WindowsError(lastError))
                .c_str(),
            L"TocPilot update",
            MB_OK | MB_ICONERROR);
        return 10;
    }

    if (!RetryFileOperation(
            [&]() {
                return ReplaceFileW(
                    targetExe.c_str(),
                    replacement.c_str(),
                    backup.c_str(),
                    REPLACEFILE_WRITE_THROUGH,
                    nullptr,
                    nullptr) != FALSE;
            },
            lastError)) {
        TryDeleteFile(replacement);
        MessageBoxW(
            nullptr,
            (L"Could not replace TocPilot.exe. The existing executable "
             L"was left in place.\n\n" +
             WindowsError(lastError))
                .c_str(),
            L"TocPilot update",
            MB_OK | MB_ICONERROR);
        return 11;
    }

    std::wstring launchError;
    const std::wstring arguments =
        L"--post-update-cleanup " +
        QuoteArgument(backup.wstring()) +
        L" " +
        QuoteArgument(stagedExe.wstring());

    if (!LaunchProcess(
            targetExe,
            arguments,
            workingDirectory,
            launchError)) {
        DWORD rollbackError = ERROR_SUCCESS;
        const bool rolledBack = RetryFileOperation(
            [&]() {
                return ReplaceFileW(
                    targetExe.c_str(),
                    backup.c_str(),
                    nullptr,
                    REPLACEFILE_WRITE_THROUGH,
                    nullptr,
                    nullptr) != FALSE;
            },
            rollbackError);

        std::wstring message =
            L"The new TocPilot executable was installed but could not be "
            L"started.\n\n" +
            launchError;

        if (rolledBack) {
            message += L"\n\nThe previous executable was restored.";
        } else {
            message +=
                L"\n\nAutomatic rollback also failed: " +
                WindowsError(rollbackError) +
                L"\nBackup: " +
                backup.wstring();
        }

        MessageBoxW(
            nullptr,
            message.c_str(),
            L"TocPilot update",
            MB_OK | MB_ICONERROR);
        return 12;
    }

    return 0;
}

void CleanupAfterUpdate(
    const std::filesystem::path& backupExe,
    const std::filesystem::path& stagedExe) {
    TryDeleteFile(backupExe);
    TryDeleteFile(stagedExe);

    std::error_code ec;
    if (!stagedExe.empty()) {
        std::filesystem::remove(stagedExe.parent_path(), ec);
    }
}

} // namespace tp
