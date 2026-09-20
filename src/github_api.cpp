#include "github_api.h"
#include "git_refs.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>

namespace tp {
namespace {

constexpr wchar_t kCodeloadHost[] = L"codeload.github.com";
constexpr wchar_t kUserAgent[] = L"TocPilot/" TOCPILOT_VERSION_W;

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

bool HttpDownload(
    std::wstring_view host,
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

    const std::wstring hostString(host);
    handles.connection = WinHttpConnect(
        handles.session,
        hostString.c_str(),
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
        L"Accept: application/zip\r\n";

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

    if (status < 200 || status >= 300) {
        if (status == 404) {
            error =
                L"GitHub archive was not found for the requested revision.";
        } else if (status == 400) {
            error =
                L"GitHub codeload rejected the requested archive revision.";
        } else if (status == 403 || status == 429) {
            error =
                L"GitHub archive download was refused or temporarily limited.";
        } else {
            error =
                L"GitHub archive host returned status " +
                std::to_wstring(status) +
                L".";
        }
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

    GitRemoteRepositoryInfo remote;
    if (!FetchPublicGitRepositoryInfo(
            L"github.com",
            repository,
            remote,
            error)) {
        return false;
    }

    info.defaultBranch =
        std::move(remote.defaultBranch);
    info.branches.reserve(
        remote.branches.size());

    for (auto& branch : remote.branches) {
        info.branches.push_back(
            GitHubBranch{
                std::move(branch.name),
                std::move(branch.sha)});
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

bool BuildGitHubCodeloadPath(
    std::wstring_view repository,
    std::wstring_view ref,
    std::wstring& path,
    std::wstring& error) {
    path.clear();
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

    path =
        L"/" +
        owner +
        L"/" +
        name +
        L"/zip/" +
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
    if (!BuildGitHubCodeloadPath(
            repository,
            ref,
            path,
            error)) {
        return false;
    }

    DWORD status = 0;
    return HttpDownload(
        kCodeloadHost,
        path,
        destination,
        downloadedBytes,
        status,
        error);
}

} // namespace tp
