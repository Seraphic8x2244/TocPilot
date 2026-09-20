#include "github_api.h"
#include "git_refs.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace tp {
namespace {

constexpr wchar_t kHost[] = L"api.github.com";
constexpr wchar_t kUserAgent[] = L"TocPilot/" TOCPILOT_VERSION_W;
constexpr std::size_t kMaxResponseBytes = 4 * 1024 * 1024;

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

void SkipWhitespace(std::string_view text, std::size_t& pos) {
    while (pos < text.size() &&
           std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
}

bool ParseJsonString(
    std::string_view text,
    std::size_t& pos,
    std::string& value) {
    value.clear();
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }

    ++pos;
    while (pos < text.size()) {
        const char ch = text[pos++];

        if (ch == '"') {
            return true;
        }

        if (ch != '\\') {
            if (static_cast<unsigned char>(ch) < 0x20) {
                return false;
            }
            value.push_back(ch);
            continue;
        }

        if (pos >= text.size()) {
            return false;
        }

        switch (text[pos++]) {
        case '"':
            value.push_back('"');
            break;
        case '\\':
            value.push_back('\\');
            break;
        case '/':
            value.push_back('/');
            break;
        case 'b':
            value.push_back('\b');
            break;
        case 'f':
            value.push_back('\f');
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        default:
            return false;
        }
    }

    return false;
}

std::optional<std::string> JsonStringField(
    std::string_view object,
    std::string_view key) {
    const std::string quoted =
        "\"" + std::string(key) + "\"";

    std::size_t pos = object.find(quoted);
    while (pos != std::string_view::npos) {
        pos += quoted.size();
        SkipWhitespace(object, pos);

        if (pos >= object.size() || object[pos] != ':') {
            pos = object.find(quoted, pos);
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


std::string WideToUtf8(std::wstring_view input) {
    if (input.empty()) {
        return {};
    }

    if (input.size() >
        static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        return {};
    }

    const int sourceLength =
        static_cast<int>(input.size());

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

    std::string output(
        static_cast<std::size_t>(needed),
        '\0');

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

bool EncodeGitHubPathSegment(
    std::wstring_view input,
    std::wstring& encoded) {
    encoded.clear();

    const std::string utf8 =
        WideToUtf8(input);

    if (utf8.empty() && !input.empty()) {
        return false;
    }

    constexpr char hex[] =
        "0123456789ABCDEF";

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
            encoded.push_back(
                static_cast<wchar_t>(ch));
            continue;
        }

        encoded.push_back(L'%');
        encoded.push_back(
            static_cast<wchar_t>(
                hex[(ch >> 4) & 0x0F]));
        encoded.push_back(
            static_cast<wchar_t>(
                hex[ch & 0x0F]));
    }

    return true;
}

bool ExtractObjectRange(
    std::string_view text,
    std::size_t start,
    std::size_t& end) {
    if (start >= text.size() || text[start] != '{') {
        return false;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (std::size_t pos = start; pos < text.size(); ++pos) {
        const char ch = text[pos];

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
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                end = pos + 1;
                return true;
            }
            if (depth < 0) {
                return false;
            }
        }
    }

    return false;
}

bool HttpGet(
    const std::wstring& path,
    std::string& body,
    DWORD& status,
    std::wstring& error) {
    body.clear();
    error.clear();
    status = 0;

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

    handles.connection = WinHttpConnect(
        handles.session,
        kHost,
        INTERNET_DEFAULT_HTTPS_PORT,
        0);

    if (!handles.connection) {
        error =
            L"WinHTTP connection failed: " +
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
            L"WinHTTP request failed: " +
            WindowsError(GetLastError());
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
            L"Could not add GitHub headers: " +
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
        !WinHttpReceiveResponse(
            handles.request,
            nullptr)) {
        error =
            L"GitHub request failed: " +
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
            L"Could not read GitHub response status: " +
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
                L"Could not read GitHub response: " +
                WindowsError(GetLastError());
            return false;
        }

        if (read == 0) {
            break;
        }

        if (body.size() + read > kMaxResponseBytes) {
            error =
                L"GitHub response is unexpectedly large.";
            return false;
        }

        body.append(buffer, read);
    }

    return true;
}

bool CheckStatus(
    DWORD status,
    std::wstring& error) {
    if (status >= 200 && status < 300) {
        return true;
    }

    if (status == 404) {
        error =
            L"GitHub repository was not found or is not public.";
    } else if (status == 403 || status == 429) {
        error =
            L"GitHub API access was refused or rate-limited. "
            L"Try again later.";
    } else {
        error =
            L"GitHub returned status " +
            std::to_wstring(status) +
            L".";
    }

    return false;
}


bool HttpDownload(
    const std::wstring& path,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    DWORD& status,
    std::wstring& error) {
    constexpr std::uint64_t maxBytes =
        256ull * 1024ull * 1024ull;

    downloadedBytes = 0;
    status = 0;
    error.clear();

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

    handles.connection = WinHttpConnect(
        handles.session,
        kHost,
        INTERNET_DEFAULT_HTTPS_PORT,
        0);

    if (!handles.connection) {
        error =
            L"WinHTTP connection failed: " +
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
            L"WinHTTP archive request failed: " +
            WindowsError(GetLastError());
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
            L"Could not add GitHub archive headers: " +
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
        !WinHttpReceiveResponse(
            handles.request,
            nullptr)) {
        error =
            L"GitHub archive request failed: " +
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
            L"Could not read GitHub archive status: " +
            WindowsError(GetLastError());
        return false;
    }

    if (!CheckStatus(status, error)) {
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
        error =
            L"Could not create staged archive: " +
            WindowsError(GetLastError());
        return false;
    }

    bool ok = true;
    std::array<unsigned char, 4> magic{};
    std::size_t magicBytes = 0;

    for (;;) {
        unsigned char buffer[64 * 1024];
        DWORD read = 0;

        if (!WinHttpReadData(
                handles.request,
                buffer,
                static_cast<DWORD>(
                    sizeof(buffer)),
                &read)) {
            error =
                L"Could not read GitHub archive response: " +
                WindowsError(GetLastError());
            ok = false;
            break;
        }

        if (read == 0) {
            break;
        }

        if (downloadedBytes >
            maxBytes - read) {
            error =
                L"GitHub archive exceeds the 256 MiB inspection limit.";
            ok = false;
            break;
        }

        if (magicBytes < magic.size()) {
            const std::size_t copy =
                std::min<std::size_t>(
                    magic.size() - magicBytes,
                    read);

            std::copy_n(
                buffer,
                copy,
                magic.begin() +
                    static_cast<std::ptrdiff_t>(
                        magicBytes));

            magicBytes += copy;
        }

        DWORD written = 0;
        if (!WriteFile(
                file,
                buffer,
                read,
                &written,
                nullptr) ||
            written != read) {
            error =
                L"Could not write staged GitHub archive: " +
                WindowsError(GetLastError());
            ok = false;
            break;
        }

        downloadedBytes += read;
    }

    if (ok &&
        !FlushFileBuffers(file)) {
        error =
            L"Could not flush staged GitHub archive: " +
            WindowsError(GetLastError());
        ok = false;
    }

    CloseHandle(file);

    const bool zipSignature =
        magicBytes == magic.size() &&
        magic[0] == 'P' &&
        magic[1] == 'K' &&
        ((magic[2] == 3 && magic[3] == 4) ||
         (magic[2] == 5 && magic[3] == 6) ||
         (magic[2] == 7 && magic[3] == 8));

    if (ok && !zipSignature) {
        error =
            L"GitHub archive response is not a ZIP file.";
        ok = false;
    }

    if (!ok) {
        std::error_code removeError;
        std::filesystem::remove(
            destination,
            removeError);
        downloadedBytes = 0;
    }

    return ok;
}

} // namespace

bool ParseGitHubRepositoryJson(
    std::string_view json,
    std::wstring& defaultBranch,
    std::wstring& error) {
    error.clear();
    defaultBranch.clear();

    const auto value =
        JsonStringField(json, "default_branch");

    if (!value) {
        error =
            L"GitHub repository response has no valid default_branch.";
        return false;
    }

    defaultBranch = Utf8ToWide(*value);
    if (defaultBranch.empty() && !value->empty()) {
        error =
            L"GitHub default branch is not valid UTF-8.";
        return false;
    }

    return true;
}

bool ParseGitHubBranchesJson(
    std::string_view json,
    std::vector<GitHubBranch>& branches,
    std::wstring& error) {
    branches.clear();
    error.clear();

    std::size_t pos = 0;
    SkipWhitespace(json, pos);

    if (pos >= json.size() || json[pos] != '[') {
        error =
            L"GitHub branches response is not an array.";
        return false;
    }

    ++pos;
    for (;;) {
        SkipWhitespace(json, pos);

        if (pos >= json.size()) {
            error =
                L"GitHub branches response is incomplete.";
            return false;
        }

        if (json[pos] == ']') {
            return true;
        }

        if (json[pos] != '{') {
            error =
                L"GitHub branch entry is not an object.";
            return false;
        }

        std::size_t end = 0;
        if (!ExtractObjectRange(json, pos, end)) {
            error =
                L"GitHub branch entry is malformed.";
            return false;
        }

        const std::string_view object =
            json.substr(pos, end - pos);

        const auto name =
            JsonStringField(object, "name");
        const auto sha =
            JsonStringField(object, "sha");

        if (!name || !sha) {
            error =
                L"GitHub branch entry has no valid name/commit SHA.";
            return false;
        }

        GitHubBranch branch;
        branch.name = Utf8ToWide(*name);
        branch.sha = Utf8ToWide(*sha);

        if ((branch.name.empty() && !name->empty()) ||
            branch.sha.empty()) {
            error =
                L"GitHub branch entry contains invalid UTF-8 or SHA data.";
            return false;
        }

        branches.push_back(std::move(branch));

        pos = end;
        SkipWhitespace(json, pos);

        if (pos < json.size() && json[pos] == ',') {
            ++pos;
            continue;
        }

        if (pos < json.size() && json[pos] == ']') {
            return true;
        }

        error =
            L"GitHub branches response has invalid separators.";
        return false;
    }
}

bool FindGitHubBranchHead(
    const std::vector<GitHubBranch>& branches,
    std::wstring_view branch,
    std::wstring& remoteSha,
    std::wstring& error) {
    remoteSha.clear();
    error.clear();

    if (branch.empty()) {
        error = L"Tracked GitHub branch name is empty.";
        return false;
    }

    for (const auto& item : branches) {
        if (item.name == branch) {
            if (item.sha.empty()) {
                error =
                    L"GitHub returned an empty commit SHA for the tracked branch.";
                return false;
            }

            remoteSha = item.sha;
            return true;
        }
    }

    error =
        L"Tracked GitHub branch was not found: " +
        std::wstring(branch);
    return false;
}

bool FetchGitHubRepositoryInfo(
    std::wstring_view repository,
    GitHubRepositoryInfo& info,
    std::wstring& error) {
    info = {};
    error.clear();

    const std::size_t slash =
        repository.find(L'/');

    if (slash == std::wstring_view::npos ||
        slash == 0 ||
        slash + 1 >= repository.size() ||
        repository.find(
            L'/',
            slash + 1) != std::wstring_view::npos) {
        error =
            L"GitHub repository identity is invalid.";
        return false;
    }

    const std::wstring base =
        L"/repos/" +
        std::wstring(repository);

    std::string metadata;
    DWORD status = 0;

    if (!HttpGet(
            base,
            metadata,
            status,
            error) ||
        !CheckStatus(status, error) ||
        !ParseGitHubRepositoryJson(
            metadata,
            info.defaultBranch,
            error)) {
        return false;
    }

    constexpr int pageSize = 100;

    for (int page = 1; page <= 10; ++page) {
        const std::wstring path =
            base +
            L"/branches?per_page=" +
            std::to_wstring(pageSize) +
            L"&page=" +
            std::to_wstring(page);

        std::string body;
        status = 0;

        if (!HttpGet(
                path,
                body,
                status,
                error) ||
            !CheckStatus(status, error)) {
            return false;
        }

        std::vector<GitHubBranch> pageBranches;
        if (!ParseGitHubBranchesJson(
                body,
                pageBranches,
                error)) {
            return false;
        }

        const std::size_t count =
            pageBranches.size();

        info.branches.insert(
            info.branches.end(),
            std::make_move_iterator(
                pageBranches.begin()),
            std::make_move_iterator(
                pageBranches.end()));

        if (count <
            static_cast<std::size_t>(pageSize)) {
            break;
        }
    }

    if (info.branches.empty()) {
        error =
            L"GitHub repository has no visible branches.";
        return false;
    }

    return true;
}

bool ResolveGitHubBranchHead(
    std::wstring_view repository,
    std::wstring_view branch,
    std::wstring& remoteSha,
    std::wstring& error) {
    return ResolvePublicGitBranchHead(
        L"github.com",
        repository,
        branch,
        remoteSha,
        error);
}

bool BuildGitHubArchiveApiPath(
    std::wstring_view repository,
    std::wstring_view ref,
    std::wstring& apiPath,
    std::wstring& error) {
    apiPath.clear();
    error.clear();

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

    if (ref.empty()) {
        error =
            L"GitHub archive ref is empty.";
        return false;
    }

    std::wstring owner;
    std::wstring name;
    std::wstring encodedRef;

    if (!EncodeGitHubPathSegment(
            repository.substr(0, slash),
            owner) ||
        !EncodeGitHubPathSegment(
            repository.substr(slash + 1),
            name) ||
        !EncodeGitHubPathSegment(
            ref,
            encodedRef)) {
        error =
            L"GitHub repository or ref contains invalid Unicode.";
        return false;
    }

    apiPath =
        L"/repos/" +
        owner +
        L"/" +
        name +
        L"/zipball/" +
        encodedRef;

    return true;
}

bool DownloadGitHubArchive(
    std::wstring_view repository,
    std::wstring_view ref,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    std::wstring& error) {
    downloadedBytes = 0;
    error.clear();

    std::wstring path;
    if (!BuildGitHubArchiveApiPath(
            repository,
            ref,
            path,
            error)) {
        return false;
    }

    DWORD status = 0;
    return HttpDownload(
        path,
        destination,
        downloadedBytes,
        status,
        error);
}


} // namespace tp
