#include "github_release.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>

#include <array>
#include <cctype>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace tp {
namespace {

constexpr wchar_t kGitHubApiHost[] = L"api.github.com";
constexpr wchar_t kUserAgent[] = L"TocPilot/" TOCPILOT_VERSION_W;
constexpr std::size_t kMaxReleaseJsonBytes = 8 * 1024 * 1024;
constexpr int kMaxJsonDepth = 64;

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

std::wstring WindowsError(DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
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
               (message.back() == L'\r' || message.back() == L'\n')) {
            message.pop_back();
        }
    } else {
        message = L"Windows error " + std::to_wstring(error);
    }

    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

void SkipWhitespace(
    std::string_view text,
    std::size_t& pos) {
    while (pos < text.size() &&
           std::isspace(
               static_cast<unsigned char>(
                   text[pos]))) {
        ++pos;
    }
}

bool ParseStringRange(
    std::string_view text,
    std::size_t start,
    std::size_t& end) {
    if (start >= text.size() ||
        text[start] != '"') {
        return false;
    }

    bool escaped = false;
    for (std::size_t pos = start + 1;
         pos < text.size();
         ++pos) {
        const unsigned char ch =
            static_cast<unsigned char>(
                text[pos]);

        if (!escaped && ch < 0x20) {
            return false;
        }
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            end = pos + 1;
            return true;
        }
    }

    return false;
}

bool SkipJsonValue(
    std::string_view text,
    std::size_t start,
    std::size_t& end,
    int depth = 0) {
    if (depth > kMaxJsonDepth) {
        return false;
    }

    std::size_t pos = start;
    SkipWhitespace(text, pos);
    if (pos >= text.size()) {
        return false;
    }

    if (text[pos] == '"') {
        return ParseStringRange(
            text,
            pos,
            end);
    }

    if (text[pos] == '{') {
        ++pos;
        SkipWhitespace(text, pos);
        if (pos < text.size() &&
            text[pos] == '}') {
            end = pos + 1;
            return true;
        }

        while (pos < text.size()) {
            std::size_t keyEnd = 0;
            if (!ParseStringRange(
                    text,
                    pos,
                    keyEnd)) {
                return false;
            }

            pos = keyEnd;
            SkipWhitespace(text, pos);
            if (pos >= text.size() ||
                text[pos] != ':') {
                return false;
            }

            ++pos;
            std::size_t valueEnd = 0;
            if (!SkipJsonValue(
                    text,
                    pos,
                    valueEnd,
                    depth + 1)) {
                return false;
            }

            pos = valueEnd;
            SkipWhitespace(text, pos);
            if (pos >= text.size()) {
                return false;
            }
            if (text[pos] == '}') {
                end = pos + 1;
                return true;
            }
            if (text[pos] != ',') {
                return false;
            }
            ++pos;
            SkipWhitespace(text, pos);
        }

        return false;
    }

    if (text[pos] == '[') {
        ++pos;
        SkipWhitespace(text, pos);
        if (pos < text.size() &&
            text[pos] == ']') {
            end = pos + 1;
            return true;
        }

        while (pos < text.size()) {
            std::size_t valueEnd = 0;
            if (!SkipJsonValue(
                    text,
                    pos,
                    valueEnd,
                    depth + 1)) {
                return false;
            }

            pos = valueEnd;
            SkipWhitespace(text, pos);
            if (pos >= text.size()) {
                return false;
            }
            if (text[pos] == ']') {
                end = pos + 1;
                return true;
            }
            if (text[pos] != ',') {
                return false;
            }
            ++pos;
            SkipWhitespace(text, pos);
        }

        return false;
    }

    const std::size_t tokenStart = pos;
    while (pos < text.size()) {
        const char ch = text[pos];
        if (ch == ',' ||
            ch == '}' ||
            ch == ']' ||
            std::isspace(
                static_cast<unsigned char>(
                    ch))) {
            break;
        }
        ++pos;
    }

    if (pos == tokenStart) {
        return false;
    }

    end = pos;
    return true;
}

bool RootObjectRange(
    std::string_view json,
    std::size_t& start,
    std::size_t& end) {
    start = 0;
    SkipWhitespace(json, start);
    if (start >= json.size() ||
        json[start] != '{' ||
        !SkipJsonValue(
            json,
            start,
            end)) {
        return false;
    }

    std::size_t trailing = end;
    SkipWhitespace(json, trailing);
    return trailing == json.size();
}

bool HexDigit(
    char ch,
    unsigned int& value) {
    if (ch >= '0' && ch <= '9') {
        value =
            static_cast<unsigned int>(
                ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        value =
            static_cast<unsigned int>(
                ch - 'a' + 10);
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        value =
            static_cast<unsigned int>(
                ch - 'A' + 10);
        return true;
    }
    return false;
}

bool ParseHex4(
    std::string_view text,
    std::size_t pos,
    unsigned int& value) {
    if (pos + 4 > text.size()) {
        return false;
    }

    value = 0;
    for (std::size_t i = 0;
         i < 4;
         ++i) {
        unsigned int digit = 0;
        if (!HexDigit(
                text[pos + i],
                digit)) {
            return false;
        }
        value =
            (value << 4) |
            digit;
    }
    return true;
}

void AppendUtf8CodePoint(
    std::string& output,
    unsigned int codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(
            static_cast<char>(
                codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(
            static_cast<char>(
                0xC0 |
                (codePoint >> 6)));
        output.push_back(
            static_cast<char>(
                0x80 |
                (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(
            static_cast<char>(
                0xE0 |
                (codePoint >> 12)));
        output.push_back(
            static_cast<char>(
                0x80 |
                ((codePoint >> 6) & 0x3F)));
        output.push_back(
            static_cast<char>(
                0x80 |
                (codePoint & 0x3F)));
    } else {
        output.push_back(
            static_cast<char>(
                0xF0 |
                (codePoint >> 18)));
        output.push_back(
            static_cast<char>(
                0x80 |
                ((codePoint >> 12) & 0x3F)));
        output.push_back(
            static_cast<char>(
                0x80 |
                ((codePoint >> 6) & 0x3F)));
        output.push_back(
            static_cast<char>(
                0x80 |
                (codePoint & 0x3F)));
    }
}

bool DecodeJsonStringUtf8(
    std::string_view token,
    std::string& output) {
    output.clear();

    if (token.size() < 2 ||
        token.front() != '"' ||
        token.back() != '"') {
        return false;
    }

    for (std::size_t pos = 1;
         pos + 1 < token.size();
         ++pos) {
        const char ch = token[pos];
        if (ch != '\\') {
            output.push_back(ch);
            continue;
        }

        ++pos;
        if (pos + 1 > token.size()) {
            return false;
        }

        switch (token[pos]) {
        case '"':
        case '\\':
        case '/':
            output.push_back(token[pos]);
            break;
        case 'b':
            output.push_back('\b');
            break;
        case 'f':
            output.push_back('\f');
            break;
        case 'n':
            output.push_back('\n');
            break;
        case 'r':
            output.push_back('\r');
            break;
        case 't':
            output.push_back('\t');
            break;
        case 'u': {
            unsigned int high = 0;
            if (!ParseHex4(
                    token,
                    pos + 1,
                    high)) {
                return false;
            }
            pos += 4;

            unsigned int codePoint = high;
            if (high >= 0xD800 &&
                high <= 0xDBFF) {
                if (pos + 6 >= token.size() ||
                    token[pos + 1] != '\\' ||
                    token[pos + 2] != 'u') {
                    return false;
                }

                unsigned int low = 0;
                if (!ParseHex4(
                        token,
                        pos + 3,
                        low) ||
                    low < 0xDC00 ||
                    low > 0xDFFF) {
                    return false;
                }

                codePoint =
                    0x10000 +
                    ((high - 0xD800) << 10) +
                    (low - 0xDC00);
                pos += 6;
            } else if (
                high >= 0xDC00 &&
                high <= 0xDFFF) {
                return false;
            }

            AppendUtf8CodePoint(
                output,
                codePoint);
            break;
        }
        default:
            return false;
        }
    }

    return true;
}

bool Utf8ToWide(
    std::string_view input,
    std::wstring& output) {
    output.clear();
    if (input.empty()) {
        return true;
    }

    if (input.size() >
        static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return false;
    }

    const int length =
        static_cast<int>(
            input.size());
    const int needed =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            input.data(),
            length,
            nullptr,
            0);

    if (needed <= 0) {
        return false;
    }

    output.resize(
        static_cast<std::size_t>(
            needed));

    return MultiByteToWideChar(
               CP_UTF8,
               MB_ERR_INVALID_CHARS,
               input.data(),
               length,
               output.data(),
               needed) == needed;
}

bool DecodeJsonString(
    std::string_view token,
    std::wstring& output) {
    std::string utf8;
    return DecodeJsonStringUtf8(
               token,
               utf8) &&
        Utf8ToWide(
            utf8,
            output);
}

bool FindObjectMember(
    std::string_view json,
    std::size_t objectStart,
    std::size_t objectEnd,
    std::string_view key,
    std::size_t& valueStart,
    std::size_t& valueEnd) {
    if (objectStart >= objectEnd ||
        objectEnd > json.size() ||
        json[objectStart] != '{') {
        return false;
    }

    std::size_t pos = objectStart + 1;
    SkipWhitespace(json, pos);

    while (pos < objectEnd &&
           json[pos] != '}') {
        std::size_t keyEnd = 0;
        if (!ParseStringRange(
                json,
                pos,
                keyEnd)) {
            return false;
        }

        std::string decodedKey;
        if (!DecodeJsonStringUtf8(
                json.substr(
                    pos,
                    keyEnd - pos),
                decodedKey)) {
            return false;
        }

        pos = keyEnd;
        SkipWhitespace(json, pos);
        if (pos >= objectEnd ||
            json[pos] != ':') {
            return false;
        }

        ++pos;
        SkipWhitespace(json, pos);

        const std::size_t currentStart =
            pos;
        std::size_t currentEnd = 0;
        if (!SkipJsonValue(
                json,
                currentStart,
                currentEnd)) {
            return false;
        }

        if (decodedKey == key) {
            valueStart =
                currentStart;
            valueEnd =
                currentEnd;
            return true;
        }

        pos = currentEnd;
        SkipWhitespace(json, pos);
        if (pos < objectEnd &&
            json[pos] == ',') {
            ++pos;
            SkipWhitespace(json, pos);
        } else if (
            pos < objectEnd &&
            json[pos] != '}') {
            return false;
        }
    }

    return false;
}

bool GetStringMember(
    std::string_view json,
    std::size_t objectStart,
    std::size_t objectEnd,
    std::string_view key,
    std::wstring& value) {
    std::size_t start = 0;
    std::size_t end = 0;

    return FindObjectMember(
               json,
               objectStart,
               objectEnd,
               key,
               start,
               end) &&
        DecodeJsonString(
            json.substr(
                start,
                end - start),
            value);
}

bool GetOptionalStringMember(
    std::string_view json,
    std::size_t objectStart,
    std::size_t objectEnd,
    std::string_view key,
    std::wstring& value) {
    std::size_t start = 0;
    std::size_t end = 0;

    if (!FindObjectMember(
            json,
            objectStart,
            objectEnd,
            key,
            start,
            end)) {
        value.clear();
        return true;
    }

    const std::string_view token =
        json.substr(
            start,
            end - start);

    if (token == "null") {
        value.clear();
        return true;
    }

    return DecodeJsonString(
        token,
        value);
}

bool GetBoolMember(
    std::string_view json,
    std::size_t objectStart,
    std::size_t objectEnd,
    std::string_view key,
    bool& value) {
    std::size_t start = 0;
    std::size_t end = 0;

    if (!FindObjectMember(
            json,
            objectStart,
            objectEnd,
            key,
            start,
            end)) {
        return false;
    }

    const std::string_view token =
        json.substr(
            start,
            end - start);

    if (token == "true") {
        value = true;
        return true;
    }
    if (token == "false") {
        value = false;
        return true;
    }

    return false;
}

bool GetUInt64Member(
    std::string_view json,
    std::size_t objectStart,
    std::size_t objectEnd,
    std::string_view key,
    std::uint64_t& value) {
    std::size_t start = 0;
    std::size_t end = 0;

    if (!FindObjectMember(
            json,
            objectStart,
            objectEnd,
            key,
            start,
            end)) {
        return false;
    }

    const std::string token(
        json.substr(
            start,
            end - start));

    if (token.empty() ||
        token.front() == '-') {
        return false;
    }

    try {
        std::size_t used = 0;
        const unsigned long long parsed =
            std::stoull(
                token,
                &used,
                10);

        if (used != token.size()) {
            return false;
        }

        value =
            static_cast<std::uint64_t>(
                parsed);
        return true;
    } catch (...) {
        return false;
    }
}

std::string WideToUtf8(
    std::wstring_view input) {
    if (input.empty()) {
        return {};
    }

    if (input.size() >
        static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return {};
    }

    const int length =
        static_cast<int>(
            input.size());
    const int needed =
        WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            input.data(),
            length,
            nullptr,
            0,
            nullptr,
            nullptr);

    if (needed <= 0) {
        return {};
    }

    std::string output(
        static_cast<std::size_t>(
            needed),
        '\0');

    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            input.data(),
            length,
            output.data(),
            needed,
            nullptr,
            nullptr) != needed) {
        return {};
    }

    return output;
}

bool EncodePathSegment(
    std::wstring_view input,
    std::wstring& encoded) {
    encoded.clear();

    const std::string utf8 =
        WideToUtf8(input);

    if (utf8.empty() &&
        !input.empty()) {
        return false;
    }

    constexpr char hex[] =
        "0123456789ABCDEF";

    for (const unsigned char ch :
         utf8) {
        const bool unreserved =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' ||
            ch == '.' ||
            ch == '_' ||
            ch == '~';

        if (unreserved) {
            encoded.push_back(
                static_cast<wchar_t>(
                    ch));
        } else {
            encoded.push_back(L'%');
            encoded.push_back(
                static_cast<wchar_t>(
                    hex[(ch >> 4) & 0x0F]));
            encoded.push_back(
                static_cast<wchar_t>(
                    hex[ch & 0x0F]));
        }
    }

    return true;
}

bool BuildLatestReleasePath(
    std::wstring_view repository,
    std::wstring& path,
    std::wstring& error) {
    const std::size_t slash =
        repository.find(L'/');

    if (slash == std::wstring_view::npos ||
        slash == 0 ||
        slash + 1 >= repository.size() ||
        repository.find(
            L'/',
            slash + 1) !=
            std::wstring_view::npos) {
        error =
            L"GitHub repository identity is invalid.";
        return false;
    }

    std::wstring owner;
    std::wstring name;

    if (!EncodePathSegment(
            repository.substr(
                0,
                slash),
            owner) ||
        !EncodePathSegment(
            repository.substr(
                slash + 1),
            name)) {
        error =
            L"GitHub repository identity contains invalid Unicode.";
        return false;
    }

    path =
        L"/repos/" +
        owner +
        L"/" +
        name +
        L"/releases/latest";

    return true;
}

bool HttpGetRelease(
    const std::wstring& path,
    std::string& body,
    DWORD& status,
    std::wstring& error) {
    body.clear();
    status = 0;

    Handles handles;
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
            kGitHubApiHost,
            INTERNET_DEFAULT_HTTPS_PORT,
            0);

    if (!handles.connection) {
        error =
            L"GitHub API connection failed: " +
            WindowsError(
                GetLastError());
        return false;
    }

    handles.request =
        WinHttpOpenRequest(
            handles.connection,
            L"GET",
            path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

    if (!handles.request) {
        error =
            L"GitHub release request failed: " +
            WindowsError(
                GetLastError());
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
        error =
            L"Could not add GitHub release headers: " +
            WindowsError(
                GetLastError());
        return false;
    }

    if (!WinHttpSendRequest(
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
            L"GitHub release request failed: " +
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
            L"Could not read GitHub release status: " +
            WindowsError(
                GetLastError());
        return false;
    }

    std::array<char, 16384> buffer{};

    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(
                handles.request,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read)) {
            error =
                L"Could not read GitHub release response: " +
                WindowsError(
                    GetLastError());
            return false;
        }

        if (read == 0) {
            break;
        }

        if (body.size() >
            kMaxReleaseJsonBytes -
                static_cast<std::size_t>(
                    read)) {
            error =
                L"GitHub release response exceeds the 8 MiB metadata limit.";
            return false;
        }

        body.append(
            buffer.data(),
            read);
    }

    return true;
}

} // namespace

bool ParseGitHubReleaseJson(
    std::string_view json,
    GitHubReleaseInfo& release,
    std::wstring& error) {
    release = {};
    error.clear();

    std::size_t rootStart = 0;
    std::size_t rootEnd = 0;

    if (!RootObjectRange(
            json,
            rootStart,
            rootEnd)) {
        error =
            L"GitHub release response is not a valid JSON object.";
        return false;
    }

    if (!GetStringMember(
            json,
            rootStart,
            rootEnd,
            "tag_name",
            release.tag) ||
        release.tag.empty()) {
        error =
            L"GitHub release response has no valid tag_name.";
        return false;
    }

    if (!GetOptionalStringMember(
            json,
            rootStart,
            rootEnd,
            "name",
            release.name) ||
        !GetBoolMember(
            json,
            rootStart,
            rootEnd,
            "prerelease",
            release.prerelease) ||
        !GetBoolMember(
            json,
            rootStart,
            rootEnd,
            "draft",
            release.draft)) {
        error =
            L"GitHub release response has invalid release metadata.";
        return false;
    }

    std::size_t assetsStart = 0;
    std::size_t assetsEnd = 0;

    if (!FindObjectMember(
            json,
            rootStart,
            rootEnd,
            "assets",
            assetsStart,
            assetsEnd) ||
        assetsStart >= assetsEnd ||
        json[assetsStart] != '[') {
        error =
            L"GitHub release response has no valid assets array.";
        return false;
    }

    std::size_t pos =
        assetsStart + 1;
    SkipWhitespace(json, pos);

    while (pos < assetsEnd &&
           json[pos] != ']') {
        const std::size_t objectStart =
            pos;
        std::size_t objectEnd = 0;

        if (!SkipJsonValue(
                json,
                objectStart,
                objectEnd) ||
            objectStart >= json.size() ||
            json[objectStart] != '{' ||
            objectEnd > assetsEnd) {
            error =
                L"GitHub release response contains an invalid asset record.";
            return false;
        }

        GitHubReleaseAsset asset;

        if (!GetStringMember(
                json,
                objectStart,
                objectEnd,
                "name",
                asset.name) ||
            asset.name.empty() ||
            !GetStringMember(
                json,
                objectStart,
                objectEnd,
                "browser_download_url",
                asset.downloadUrl) ||
            asset.downloadUrl.empty() ||
            !GetOptionalStringMember(
                json,
                objectStart,
                objectEnd,
                "digest",
                asset.digest) ||
            !GetUInt64Member(
                json,
                objectStart,
                objectEnd,
                "size",
                asset.size)) {
            error =
                L"GitHub release response contains incomplete asset metadata.";
            return false;
        }

        release.assets.push_back(
            std::move(asset));

        pos = objectEnd;
        SkipWhitespace(json, pos);

        if (pos >= assetsEnd) {
            error =
                L"GitHub release assets array is malformed.";
            return false;
        }

        if (json[pos] == ']') {
            break;
        }

        if (json[pos] != ',') {
            error =
                L"GitHub release assets array is malformed.";
            return false;
        }

        ++pos;
        SkipWhitespace(json, pos);
    }

    return true;
}

bool FindExactGitHubReleaseAsset(
    const GitHubReleaseInfo& release,
    std::wstring_view assetName,
    GitHubReleaseAsset& asset,
    std::wstring& error) {
    asset = {};
    error.clear();

    if (assetName.empty()) {
        error =
            L"Release asset name is empty.";
        return false;
    }

    std::size_t matches = 0;

    for (const auto& candidate :
         release.assets) {
        if (candidate.name ==
            assetName) {
            asset = candidate;
            ++matches;
        }
    }

    if (matches == 1) {
        return true;
    }

    asset = {};

    if (matches == 0) {
        error =
            L"Latest release does not contain the exact configured asset '" +
            std::wstring(assetName) +
            L"'.";
    } else {
        error =
            L"Latest release contains more than one asset named '" +
            std::wstring(assetName) +
            L"'; TocPilot will not guess.";
    }

    return false;
}

bool FetchLatestStableGitHubRelease(
    std::wstring_view repository,
    GitHubReleaseInfo& release,
    std::wstring& error) {
    release = {};
    error.clear();

    std::wstring path;
    if (!BuildLatestReleasePath(
            repository,
            path,
            error)) {
        return false;
    }

    std::string body;
    DWORD status = 0;

    if (!HttpGetRelease(
            path,
            body,
            status,
            error)) {
        return false;
    }

    if (status < 200 ||
        status >= 300) {
        if (status == 404) {
            error =
                L"GitHub repository has no published stable release.";
        } else if (
            status == 403 ||
            status == 429) {
            error =
                L"GitHub release metadata request was refused or rate limited.";
        } else {
            error =
                L"GitHub release API returned status " +
                std::to_wstring(status) +
                L".";
        }
        return false;
    }

    if (!ParseGitHubReleaseJson(
            body,
            release,
            error)) {
        return false;
    }

    if (release.draft ||
        release.prerelease) {
        error =
            L"GitHub latest stable release response unexpectedly identified a draft or prerelease.";
        release = {};
        return false;
    }

    return true;
}

} // namespace tp
