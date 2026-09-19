#include "update.h"
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
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kLatestReleaseUrl[] =
    L"https://api.github.com/repos/Seraphic8x2244/TocPilot/releases/latest";
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
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";

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

bool HttpGetString(
    const std::wstring& url,
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

        body.append(buffer.data(), bytesRead);
    }

    return true;
}

bool DownloadFile(
    const std::wstring& url,
    const std::filesystem::path& destination,
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

void SkipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() &&
           std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
}

bool ParseJsonString(
    const std::string& json,
    size_t& pos,
    std::string& value) {
    SkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '"') {
        return false;
    }

    ++pos;
    value.clear();

    while (pos < json.size()) {
        const char ch = json[pos++];
        if (ch == '"') {
            return true;
        }

        if (ch != '\\') {
            value.push_back(ch);
            continue;
        }

        if (pos >= json.size()) {
            return false;
        }

        const char escaped = json[pos++];
        switch (escaped) {
        case '"': value.push_back('"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        default:
            return false;
        }
    }

    return false;
}

std::optional<std::string> JsonStringField(
    const std::string& object,
    const std::string& key) {
    const std::string quotedKey = "\"" + key + "\"";
    size_t pos = object.find(quotedKey);

    while (pos != std::string::npos) {
        pos += quotedKey.size();
        SkipWhitespace(object, pos);
        if (pos >= object.size() || object[pos] != ':') {
            pos = object.find(quotedKey, pos);
            continue;
        }

        ++pos;
        SkipWhitespace(object, pos);

        std::string value;
        if (ParseJsonString(object, pos, value)) {
            return value;
        }
        return std::nullopt;
    }

    return std::nullopt;
}

bool ExtractAssets(
    const std::string& json,
    ReleaseInfo& release,
    std::wstring& error) {
    const size_t assetsKey = json.find("\"assets\"");
    if (assetsKey == std::string::npos) {
        error = L"GitHub release response has no assets array.";
        return false;
    }

    const size_t arrayStart = json.find('[', assetsKey);
    if (arrayStart == std::string::npos) {
        error = L"GitHub release assets array is malformed.";
        return false;
    }

    bool inString = false;
    bool escaped = false;
    int objectDepth = 0;
    size_t objectStart = std::string::npos;

    for (size_t i = arrayStart + 1; i < json.size(); ++i) {
        const char ch = json[i];

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }

        if (ch == '{') {
            if (objectDepth == 0) {
                objectStart = i;
            }
            ++objectDepth;
            continue;
        }

        if (ch == '}') {
            if (objectDepth == 0) {
                continue;
            }

            --objectDepth;
            if (objectDepth != 0 || objectStart == std::string::npos) {
                continue;
            }

            const std::string object =
                json.substr(objectStart, i - objectStart + 1);
            const auto name = JsonStringField(object, "name");
            const auto url = JsonStringField(object, "browser_download_url");

            if (name && url) {
                if (*name == "TocPilot.exe") {
                    release.assetUrl = Utf8ToWide(*url);
                    if (const auto digest = JsonStringField(object, "digest")) {
                        release.assetDigest = Utf8ToWide(*digest);
                    }
                } else if (*name == "TocPilot.exe.sha256") {
                    release.checksumUrl = Utf8ToWide(*url);
                }
            }

            objectStart = std::string::npos;
            continue;
        }

        if (ch == ']' && objectDepth == 0) {
            break;
        }
    }

    if (release.assetUrl.empty()) {
        error = L"Latest release has no TocPilot.exe asset.";
        return false;
    }

    return true;
}

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

bool ParseSemVer(std::wstring_view text, SemVer& version) {
    if (!text.empty() && (text.front() == L'v' || text.front() == L'V')) {
        text.remove_prefix(1);
    }

    std::array<int, 3> values{};
    size_t component = 0;
    int current = 0;
    bool haveDigit = false;

    for (wchar_t ch : text) {
        if (std::iswdigit(ch)) {
            haveDigit = true;
            current = current * 10 + (ch - L'0');
            continue;
        }

        if (ch == L'.' && haveDigit && component < 2) {
            values[component++] = current;
            current = 0;
            haveDigit = false;
            continue;
        }

        if ((ch == L'-' || ch == L'+') && haveDigit) {
            break;
        }

        return false;
    }

    if (!haveDigit || component != 2) {
        return false;
    }

    values[2] = current;
    version.major = values[0];
    version.minor = values[1];
    version.patch = values[2];
    return true;
}

bool IsNewer(std::wstring_view candidate, std::wstring_view current) {
    SemVer a;
    SemVer b;
    if (!ParseSemVer(candidate, a) || !ParseSemVer(current, b)) {
        return false;
    }

    if (a.major != b.major) {
        return a.major > b.major;
    }
    if (a.minor != b.minor) {
        return a.minor > b.minor;
    }
    return a.patch > b.patch;
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
    release = {};
    state = ReleaseCheckState::NoRelease;

    std::string body;
    DWORD status = 0;
    if (!HttpGetString(kLatestReleaseUrl, body, status, error)) {
        return false;
    }

    if (status == 404) {
        return true;
    }

    if (status < 200 || status >= 300) {
        error = L"GitHub returned status " + std::to_wstring(status);
        return false;
    }

    const auto tag = JsonStringField(body, "tag_name");
    if (!tag) {
        error = L"GitHub release response has no tag_name.";
        return false;
    }

    release.tag = Utf8ToWide(*tag);
    if (release.tag.empty()) {
        error = L"GitHub release tag is not valid UTF-8.";
        return false;
    }

    if (!ExtractAssets(body, release, error)) {
        return false;
    }

    state = IsNewer(release.tag, TOCPILOT_VERSION_TAG_W)
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
    if (release.assetUrl.empty()) {
        error = L"No update asset URL is available.";
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

    if (!DownloadFile(release.assetUrl, stagedExe, error)) {
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
