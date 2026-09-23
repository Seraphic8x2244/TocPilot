#include "gitlab_api.h"
#include "version.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>

namespace tp {
namespace {

constexpr wchar_t kGitLabHost[] = L"gitlab.com";
constexpr wchar_t kUserAgent[] = L"TocPilot/" TOCPILOT_VERSION_W;
constexpr std::uint64_t kMaxArchiveBytes =
    256ull * 1024ull * 1024ull;

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

bool EncodeUrlComponent(
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

    encoded.reserve(
        utf8.size() * 3);

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

bool ValidRepository(
    std::wstring_view repository) {
    if (repository.empty() ||
        repository.front() == L'/' ||
        repository.back() == L'/' ||
        repository.find(L'/') ==
            std::wstring_view::npos) {
        return false;
    }

    std::size_t start = 0;

    while (start <
           repository.size()) {
        const std::size_t slash =
            repository.find(
                L'/',
                start);

        const std::size_t end =
            slash ==
                    std::wstring_view::npos
                ? repository.size()
                : slash;

        const auto segment =
            repository.substr(
                start,
                end - start);

        if (segment.empty() ||
            segment == L"." ||
            segment == L"..") {
            return false;
        }

        for (const wchar_t ch :
             segment) {
            if (ch == L'\\' ||
                ch < 0x20 ||
                ch == 0x7f) {
                return false;
            }
        }

        if (slash ==
            std::wstring_view::npos) {
            break;
        }

        start = slash + 1;
    }

    return true;
}

bool HttpDownload(
    const std::wstring& path,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    std::wstring& error) {
    downloadedBytes = 0;
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
            kGitLabHost,
            INTERNET_DEFAULT_HTTPS_PORT,
            0);

    if (!handles.connection) {
        error =
            L"GitLab HTTPS connection failed: " +
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
            L"GitLab archive request failed: " +
            WindowsError(
                GetLastError());
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
            L"Could not add GitLab archive headers: " +
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
            L"GitLab archive request failed: " +
            WindowsError(
                GetLastError());
        return false;
    }

    DWORD status = 0;
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
            L"Could not read GitLab archive status: " +
            WindowsError(
                GetLastError());
        return false;
    }

    if (status < 200 ||
        status >= 300) {
        if (status == 404) {
            error =
                L"GitLab repository archive was not found or is not public.";
        } else if (status == 400) {
            error =
                L"GitLab rejected the requested archive revision.";
        } else if (
            status == 401 ||
            status == 403) {
            error =
                L"GitLab archive download was refused.";
        } else if (
            status == 429) {
            error =
                L"GitLab temporarily rate-limited the archive download.";
        } else {
            error =
                L"GitLab archive endpoint returned status " +
                std::to_wstring(
                    status) +
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

    if (file ==
        INVALID_HANDLE_VALUE) {
        error =
            L"Could not create staged archive: " +
            WindowsError(
                GetLastError());
        return false;
    }

    bool ok = true;
    std::array<unsigned char, 4>
        magic{};
    std::size_t magicBytes = 0;

    for (;;) {
        unsigned char
            buffer[64 * 1024];
        DWORD read = 0;

        if (!WinHttpReadData(
                handles.request,
                buffer,
                static_cast<DWORD>(
                    sizeof(buffer)),
                &read)) {
            error =
                L"Could not read GitLab archive response: " +
                WindowsError(
                    GetLastError());
            ok = false;
            break;
        }

        if (read == 0) {
            break;
        }

        if (downloadedBytes >
            kMaxArchiveBytes -
                static_cast<std::uint64_t>(
                    read)) {
            error =
                L"GitLab archive exceeds the 256 MiB inspection limit.";
            ok = false;
            break;
        }

        if (magicBytes <
            magic.size()) {
            const std::size_t copy =
                std::min<
                    std::size_t>(
                    magic.size() -
                        magicBytes,
                    read);

            std::copy_n(
                buffer,
                copy,
                magic.begin() +
                    static_cast<
                        std::ptrdiff_t>(
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
                L"Could not write staged GitLab archive: " +
                WindowsError(
                    GetLastError());
            ok = false;
            break;
        }

        downloadedBytes += read;
    }

    if (ok &&
        !FlushFileBuffers(file)) {
        error =
            L"Could not flush staged GitLab archive: " +
            WindowsError(
                GetLastError());
        ok = false;
    }

    CloseHandle(file);

    const bool zipSignature =
        magicBytes ==
            magic.size() &&
        magic[0] == 'P' &&
        magic[1] == 'K' &&
        ((magic[2] == 3 &&
          magic[3] == 4) ||
         (magic[2] == 5 &&
          magic[3] == 6) ||
         (magic[2] == 7 &&
          magic[3] == 8));

    if (ok &&
        !zipSignature) {
        error =
            L"GitLab archive response is not a ZIP file.";
        ok = false;
    }

    if (!ok) {
        std::error_code
            removeError;
        std::filesystem::remove(
            destination,
            removeError);
        downloadedBytes = 0;
    }

    return ok;
}

} // namespace

bool BuildGitLabArchivePath(
    std::wstring_view repository,
    std::wstring_view ref,
    std::wstring& path,
    std::wstring& error) {
    path.clear();
    error.clear();

    if (!ValidRepository(
            repository)) {
        error =
            L"GitLab repository identity is invalid.";
        return false;
    }

    if (ref.empty()) {
        error =
            L"GitLab archive ref is empty.";
        return false;
    }

    std::wstring encodedRepository;
    std::wstring encodedRef;

    if (!EncodeUrlComponent(
            repository,
            encodedRepository) ||
        !EncodeUrlComponent(
            ref,
            encodedRef)) {
        error =
            L"GitLab repository or ref contains invalid Unicode.";
        return false;
    }

    path =
        L"/api/v4/projects/" +
        encodedRepository +
        L"/repository/archive.zip?sha=" +
        encodedRef +
        L"&include_lfs_blobs=false";

    return true;
}

bool DownloadGitLabArchive(
    std::wstring_view repository,
    std::wstring_view ref,
    const std::filesystem::path& destination,
    std::uint64_t& downloadedBytes,
    std::wstring& error) {
    downloadedBytes = 0;
    error.clear();

    std::wstring path;

    if (!BuildGitLabArchivePath(
            repository,
            ref,
            path,
            error)) {
        return false;
    }

    return HttpDownload(
        path,
        destination,
        downloadedBytes,
        error);
}

} // namespace tp
