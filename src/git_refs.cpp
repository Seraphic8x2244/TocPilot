#include "git_refs.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <string_view>

namespace tp {
namespace {

constexpr wchar_t kUserAgent[] = L"TocPilot/" TOCPILOT_VERSION_W;
constexpr std::size_t kMaxAdvertisementBytes = 8 * 1024 * 1024;

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

    std::wstring message;
    if (length != 0 && buffer) {
        message.assign(buffer, length);
        while (!message.empty() &&
               (message.back() == L'\r' || message.back() == L'\n')) {
            message.pop_back();
        }
    } else {
        message = L"Windows error " + std::to_wstring(code);
    }

    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

std::string WideToUtf8(std::wstring_view input) {
    if (input.empty()) {
        return {};
    }

    if (input.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    const int sourceLength = static_cast<int>(input.size());
    const int needed = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        input.data(),
        sourceLength,
        nullptr,
        0,
        nullptr,
        nullptr);

    if (needed <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(needed), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            input.data(),
            sourceLength,
            output.data(),
            needed,
            nullptr,
            nullptr) != needed) {
        return {};
    }
    return output;
}

std::wstring AsciiToWide(std::string_view input) {
    std::wstring output;
    output.reserve(input.size());
    for (const unsigned char ch : input) {
        if (ch > 0x7f) {
            return {};
        }
        output.push_back(static_cast<wchar_t>(ch));
    }
    return output;
}

bool IsHexObjectId(std::string_view value) {
    if (value.size() != 40 && value.size() != 64) {
        return false;
    }

    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char ch) {
            return std::isxdigit(ch) != 0;
        });
}

bool ParseHexLength(std::string_view text, std::size_t& length) {
    if (text.size() != 4) {
        return false;
    }

    length = 0;
    for (const unsigned char ch : text) {
        unsigned value = 0;
        if (ch >= '0' && ch <= '9') {
            value = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            value = 10 + ch - 'a';
        } else if (ch >= 'A' && ch <= 'F') {
            value = 10 + ch - 'A';
        } else {
            return false;
        }
        length = (length << 4) | value;
    }
    return true;
}

bool ValidHost(std::wstring_view host) {
    if (host.empty()) {
        return false;
    }

    for (const wchar_t ch : host) {
        const bool valid =
            (ch >= L'a' && ch <= L'z') ||
            (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'0' && ch <= L'9') ||
            ch == L'.' ||
            ch == L'-';

        if (!valid) {
            return false;
        }
    }
    return true;
}

bool EncodePathSegment(
    std::wstring_view input,
    std::wstring& encoded) {
    encoded.clear();

    const std::string utf8 = WideToUtf8(input);
    if (utf8.empty() && !input.empty()) {
        return false;
    }

    constexpr char hex[] = "0123456789ABCDEF";
    encoded.reserve(utf8.size() * 3);

    for (const unsigned char ch : utf8) {
        const bool unreserved =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' ||
            ch == '.' ||
            ch == '_' ||
            ch == '~';

        if (unreserved) {
            encoded.push_back(static_cast<wchar_t>(ch));
        } else {
            encoded.push_back(L'%');
            encoded.push_back(
                static_cast<wchar_t>(hex[(ch >> 4) & 0x0f]));
            encoded.push_back(
                static_cast<wchar_t>(hex[ch & 0x0f]));
        }
    }
    return true;
}

bool BuildInfoRefsPath(
    std::wstring_view repository,
    std::wstring& path,
    std::wstring& error) {
    path.clear();

    if (repository.empty() ||
        repository.front() == L'/' ||
        repository.back() == L'/') {
        error = L"Git repository identity is invalid.";
        return false;
    }

    std::size_t start = 0;
    while (start < repository.size()) {
        const std::size_t slash = repository.find(L'/', start);
        const std::size_t end =
            slash == std::wstring_view::npos
                ? repository.size()
                : slash;

        const std::wstring_view segment =
            repository.substr(start, end - start);

        if (segment.empty() ||
            segment == L"." ||
            segment == L"..") {
            error = L"Git repository identity is invalid.";
            return false;
        }

        for (const wchar_t ch : segment) {
            if (ch == L'\\' || ch < 0x20 || ch == 0x7f) {
                error = L"Git repository identity is invalid.";
                return false;
            }
        }

        std::wstring encoded;
        if (!EncodePathSegment(segment, encoded)) {
            error = L"Git repository path is not valid Unicode.";
            return false;
        }

        path += L"/";
        path += encoded;

        if (slash == std::wstring_view::npos) {
            break;
        }
        start = slash + 1;
    }

    path += L".git/info/refs?service=git-upload-pack";
    return true;
}

bool HttpGetAdvertisement(
    std::wstring_view host,
    const std::wstring& path,
    std::string& body,
    DWORD& status,
    std::wstring& error) {
    body.clear();
    status = 0;
    error.clear();

    if (!ValidHost(host)) {
        error = L"Git host name is invalid.";
        return false;
    }

    Handles handles;
    handles.session = WinHttpOpen(
        kUserAgent,
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!handles.session) {
        error =
            L"WinHTTP session failed: " +
            WindowsError(GetLastError());
        return false;
    }

    WinHttpSetTimeouts(
        handles.session,
        10000,
        10000,
        30000,
        30000);

    const std::wstring hostString(host);
    handles.connection = WinHttpConnect(
        handles.session,
        hostString.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT,
        0);

    if (!handles.connection) {
        error =
            L"Git HTTPS connection failed: " +
            WindowsError(GetLastError());
        return false;
    }

    handles.request = WinHttpOpenRequest(
        handles.connection,
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!handles.request) {
        error =
            L"Git ref-discovery request failed: " +
            WindowsError(GetLastError());
        return false;
    }

    const wchar_t headers[] =
        L"Accept: application/x-git-upload-pack-advertisement\r\n"
        L"Pragma: no-cache\r\n"
        L"Cache-Control: no-cache\r\n";

    if (!WinHttpAddRequestHeaders(
            handles.request,
            headers,
            static_cast<DWORD>(-1),
            WINHTTP_ADDREQ_FLAG_ADD)) {
        error =
            L"Could not add Git ref-discovery headers: " +
            WindowsError(GetLastError());
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
        !WinHttpReceiveResponse(handles.request, nullptr)) {
        error =
            L"Git ref-discovery request failed: " +
            WindowsError(GetLastError());
        return false;
    }

    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(
            handles.request,
            WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        error =
            L"Could not read Git ref-discovery status: " +
            WindowsError(GetLastError());
        return false;
    }

    for (;;) {
        char buffer[16 * 1024];
        DWORD read = 0;

        if (!WinHttpReadData(
                handles.request,
                buffer,
                static_cast<DWORD>(sizeof(buffer)),
                &read)) {
            error =
                L"Could not read Git ref advertisement: " +
                WindowsError(GetLastError());
            return false;
        }

        if (read == 0) {
            break;
        }

        if (body.size() + read > kMaxAdvertisementBytes) {
            error =
                L"Git ref advertisement is unexpectedly large.";
            return false;
        }

        body.append(buffer, read);
    }

    return true;
}

bool CheckHttpStatus(
    DWORD status,
    std::wstring& error) {
    if (status >= 200 && status < 300) {
        return true;
    }

    if (status == 404) {
        error =
            L"Git repository was not found or is not public.";
    } else if (status == 401 || status == 403) {
        error =
            L"Public Git ref discovery was refused by the host.";
    } else if (status == 429) {
        error =
            L"Git host temporarily rate-limited ref discovery.";
    } else {
        error =
            L"Git host returned HTTP status " +
            std::to_wstring(status) +
            L" during ref discovery.";
    }
    return false;
}

} // namespace

bool ParseGitSmartHttpBranchAdvertisement(
    std::string_view advertisement,
    std::string_view branch,
    std::string& remoteSha,
    std::wstring& error) {
    remoteSha.clear();
    error.clear();

    if (branch.empty()) {
        error = L"Tracked Git branch name is empty.";
        return false;
    }

    const std::string target =
        "refs/heads/" + std::string(branch);

    bool sawService = false;
    bool sawRef = false;
    std::size_t pos = 0;

    while (pos < advertisement.size()) {
        if (advertisement.size() - pos < 4) {
            error = L"Git ref advertisement has a truncated packet length.";
            return false;
        }

        std::size_t packetLength = 0;
        if (!ParseHexLength(
                advertisement.substr(pos, 4),
                packetLength)) {
            error = L"Git ref advertisement has an invalid packet length.";
            return false;
        }
        pos += 4;

        if (packetLength == 0 ||
            packetLength == 1 ||
            packetLength == 2) {
            continue;
        }

        if (packetLength < 4) {
            error = L"Git ref advertisement has an invalid packet length.";
            return false;
        }

        const std::size_t payloadLength = packetLength - 4;
        if (payloadLength > advertisement.size() - pos) {
            error = L"Git ref advertisement has a truncated packet.";
            return false;
        }

        std::string_view payload =
            advertisement.substr(pos, payloadLength);
        pos += payloadLength;

        if (payload == "# service=git-upload-pack\n") {
            sawService = true;
            continue;
        }

        if (payload == "version 2\n") {
            error =
                L"Git host returned protocol v2 without a ref advertisement.";
            return false;
        }

        if (payload == "version 1\n") {
            continue;
        }

        if (!payload.empty() && payload.back() == '\n') {
            payload.remove_suffix(1);
        }

        const std::size_t nul = payload.find('\0');
        if (nul != std::string_view::npos) {
            payload = payload.substr(0, nul);
        }

        const std::size_t space = payload.find(' ');
        if (space == std::string_view::npos) {
            continue;
        }

        const std::string_view objectId =
            payload.substr(0, space);
        const std::string_view refName =
            payload.substr(space + 1);

        if (!IsHexObjectId(objectId)) {
            error =
                L"Git ref advertisement contains an invalid object ID.";
            return false;
        }

        sawRef = true;
        if (refName == target) {
            if (!sawService) {
                error =
                    L"Server response is not a Git smart-HTTP upload-pack advertisement.";
                return false;
            }

            remoteSha.assign(objectId);
            return true;
        }
    }

    if (!sawService) {
        error =
            L"Server response is not a Git smart-HTTP upload-pack advertisement.";
        return false;
    }

    if (!sawRef) {
        error = L"Git repository advertised no visible refs.";
        return false;
    }

    const std::wstring branchWide = AsciiToWide(branch);
    error =
        L"Tracked Git branch was not found" +
        (branchWide.empty()
            ? std::wstring(L".")
            : std::wstring(L": ") + branchWide);
    return false;
}

bool ResolvePublicGitBranchHead(
    std::wstring_view host,
    std::wstring_view repository,
    std::wstring_view branch,
    std::wstring& remoteSha,
    std::wstring& error) {
    remoteSha.clear();
    error.clear();

    if (branch.empty()) {
        error = L"Tracked Git branch name is empty.";
        return false;
    }

    std::wstring path;
    if (!BuildInfoRefsPath(repository, path, error)) {
        return false;
    }

    const std::string branchUtf8 = WideToUtf8(branch);
    if (branchUtf8.empty() && !branch.empty()) {
        error = L"Tracked Git branch name is not valid Unicode.";
        return false;
    }

    std::string body;
    DWORD status = 0;
    if (!HttpGetAdvertisement(
            host,
            path,
            body,
            status,
            error) ||
        !CheckHttpStatus(status, error)) {
        return false;
    }

    std::string sha;
    if (!ParseGitSmartHttpBranchAdvertisement(
            body,
            branchUtf8,
            sha,
            error)) {
        return false;
    }

    remoteSha = AsciiToWide(sha);
    if (remoteSha.empty()) {
        error = L"Git host returned an invalid branch object ID.";
        return false;
    }
    return true;
}

} // namespace tp
