#include "state.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace tp {
namespace {

constexpr std::size_t kMaxStateBytes = 8 * 1024 * 1024;

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

bool FindValueStart(
    std::string_view json,
    std::string_view key,
    std::size_t& valueStart) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t keyPos = json.find(needle);
    if (keyPos == std::string_view::npos) {
        return false;
    }

    std::size_t pos = keyPos + needle.size();
    SkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return false;
    }

    ++pos;
    SkipWhitespace(json, pos);
    if (pos >= json.size()) {
        return false;
    }

    valueStart = pos;
    return true;
}

bool ParseIntegerField(
    std::string_view json,
    std::string_view key,
    int& value) {
    std::size_t pos = 0;
    if (!FindValueStart(json, key, pos)) {
        return false;
    }

    bool negative = false;
    if (json[pos] == '-') {
        negative = true;
        ++pos;
    }

    if (pos >= json.size() ||
        !std::isdigit(static_cast<unsigned char>(json[pos]))) {
        return false;
    }

    long long parsed = 0;
    while (pos < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[pos]))) {
        parsed = parsed * 10 + (json[pos] - '0');
        if (parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        ++pos;
    }

    value = static_cast<int>(negative ? -parsed : parsed);
    return true;
}

bool ParseDoubleField(
    std::string_view json,
    std::string_view key,
    double& value) {
    std::size_t pos = 0;
    if (!FindValueStart(json, key, pos)) {
        return false;
    }

    const std::size_t start = pos;
    if (json[pos] == '-' || json[pos] == '+') {
        ++pos;
    }

    bool haveDigit = false;
    bool haveDot = false;
    while (pos < json.size()) {
        const char ch = json[pos];
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            haveDigit = true;
            ++pos;
            continue;
        }
        if (ch == '.' && !haveDot) {
            haveDot = true;
            ++pos;
            continue;
        }
        break;
    }

    if (!haveDigit) {
        return false;
    }

    try {
        value = std::stod(std::string(json.substr(start, pos - start)));
    } catch (...) {
        return false;
    }

    return std::isfinite(value);
}

bool ParseBoolField(
    std::string_view json,
    std::string_view key,
    bool& value) {
    std::size_t pos = 0;
    if (!FindValueStart(json, key, pos)) {
        return false;
    }

    if (json.substr(pos, 4) == "true") {
        value = true;
        return true;
    }
    if (json.substr(pos, 5) == "false") {
        value = false;
        return true;
    }
    return false;
}

bool FindArrayRange(
    std::string_view json,
    std::string_view key,
    std::size_t& start,
    std::size_t& end) {
    if (!FindValueStart(json, key, start) ||
        start >= json.size() ||
        json[start] != '[') {
        return false;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (std::size_t pos = start; pos < json.size(); ++pos) {
        const char ch = json[pos];

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
        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
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

std::size_t CountArrayItems(std::string_view arrayJson) {
    if (arrayJson.size() < 2 || arrayJson.front() != '[') {
        return 0;
    }

    std::size_t first = 1;
    SkipWhitespace(arrayJson, first);
    if (first >= arrayJson.size() || arrayJson[first] == ']') {
        return 0;
    }

    std::size_t count = 1;
    int arrayDepth = 1;
    int objectDepth = 0;
    bool inString = false;
    bool escaped = false;

    for (std::size_t pos = 1; pos + 1 < arrayJson.size(); ++pos) {
        const char ch = arrayJson[pos];

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
        } else if (ch == '[') {
            ++arrayDepth;
        } else if (ch == ']') {
            --arrayDepth;
        } else if (ch == '{') {
            ++objectDepth;
        } else if (ch == '}') {
            --objectDepth;
        } else if (ch == ',' && arrayDepth == 1 && objectDepth == 0) {
            ++count;
        }
    }

    return count;
}

bool ReadUtf8File(
    const std::filesystem::path& path,
    std::string& content,
    std::wstring& error) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        error = L"Could not open TocPilot.json: " +
            WindowsError(GetLastError());
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) ||
        size.QuadPart < 0 ||
        static_cast<unsigned long long>(size.QuadPart) > kMaxStateBytes) {
        const DWORD lastError = GetLastError();
        CloseHandle(file);
        error = lastError == ERROR_SUCCESS
            ? L"TocPilot.json is unexpectedly large."
            : L"Could not read TocPilot.json size: " + WindowsError(lastError);
        return false;
    }

    content.assign(static_cast<std::size_t>(size.QuadPart), '\0');
    std::size_t offset = 0;

    while (offset < content.size()) {
        DWORD bytesRead = 0;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
            content.size() - offset,
            64 * 1024));

        if (!ReadFile(
                file,
                content.data() + offset,
                request,
                &bytesRead,
                nullptr)) {
            const DWORD lastError = GetLastError();
            CloseHandle(file);
            error = L"Could not read TocPilot.json: " +
                WindowsError(lastError);
            return false;
        }

        if (bytesRead == 0) {
            break;
        }
        offset += bytesRead;
    }

    CloseHandle(file);
    content.resize(offset);

    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }

    return true;
}

std::string FormatScale(double scale) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << scale;
    std::string value = stream.str();

    while (!value.empty() && value.back() == '0') {
        value.pop_back();
    }
    if (!value.empty() && value.back() == '.') {
        value.pop_back();
    }
    return value.empty() ? "1" : value;
}

bool ReplaceScalar(
    std::string& json,
    std::string_view key,
    std::string_view replacement) {
    std::size_t start = 0;
    if (!FindValueStart(json, key, start)) {
        return false;
    }

    std::size_t end = start;
    while (end < json.size()) {
        const char ch = json[end];
        if (ch == ',' || ch == '}' ||
            std::isspace(static_cast<unsigned char>(ch))) {
            break;
        }
        ++end;
    }

    if (end == start) {
        return false;
    }

    json.replace(start, end - start, replacement);
    return true;
}

std::string DefaultJson(const AppState& state) {
    std::ostringstream stream;
    stream
        << "{\n"
        << "  \"schema\": 1,\n"
        << "  \"settings\": {\n"
        << "    \"text_scale\": " << FormatScale(state.settings.textScale) << ",\n"
        << "    \"check_app_updates\": "
        << (state.settings.checkAppUpdates ? "true" : "false") << "\n"
        << "  },\n"
        << "  \"packages\": []\n"
        << "}\n";
    return stream.str();
}

bool AtomicWriteUtf8(
    const std::filesystem::path& target,
    std::string_view content,
    std::wstring& error) {
    const std::filesystem::path temporary =
        target.wstring() + L".tmp";

    HANDLE file = CreateFileW(
        temporary.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        error = L"Could not create TocPilot.json.tmp: " +
            WindowsError(GetLastError());
        return false;
    }

    std::size_t offset = 0;
    while (offset < content.size()) {
        DWORD written = 0;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
            content.size() - offset,
            64 * 1024));

        if (!WriteFile(
                file,
                content.data() + offset,
                request,
                &written,
                nullptr)) {
            const DWORD lastError = GetLastError();
            CloseHandle(file);
            DeleteFileW(temporary.c_str());
            error = L"Could not write TocPilot.json.tmp: " +
                WindowsError(lastError);
            return false;
        }

        offset += written;
    }

    if (!FlushFileBuffers(file)) {
        const DWORD lastError = GetLastError();
        CloseHandle(file);
        DeleteFileW(temporary.c_str());
        error = L"Could not flush TocPilot.json.tmp: " +
            WindowsError(lastError);
        return false;
    }

    CloseHandle(file);

    const DWORD attributes = GetFileAttributesW(target.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if (!ReplaceFileW(
                target.c_str(),
                temporary.c_str(),
                nullptr,
                REPLACEFILE_WRITE_THROUGH,
                nullptr,
                nullptr)) {
            const DWORD lastError = GetLastError();
            DeleteFileW(temporary.c_str());
            error = L"Could not replace TocPilot.json: " +
                WindowsError(lastError);
            return false;
        }
    } else {
        if (!MoveFileExW(
                temporary.c_str(),
                target.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const DWORD lastError = GetLastError();
            DeleteFileW(temporary.c_str());
            error = L"Could not create TocPilot.json: " +
                WindowsError(lastError);
            return false;
        }
    }

    return true;
}

} // namespace

std::filesystem::path StatePath(const std::filesystem::path& wowRoot) {
    return wowRoot / L"TocPilot.json";
}

bool LoadOrCreateState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    bool& created,
    std::wstring& error) {
    state = {};
    created = false;
    error.clear();

    const auto path = StatePath(wowRoot);
    const DWORD attributes = GetFileAttributesW(path.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND &&
            GetLastError() != ERROR_PATH_NOT_FOUND) {
            error = L"Could not inspect TocPilot.json: " +
                WindowsError(GetLastError());
            return false;
        }

        state.sourceJson = DefaultJson(state);
        if (!AtomicWriteUtf8(path, state.sourceJson, error)) {
            state.sourceJson.clear();
            return false;
        }

        created = true;
        return true;
    }

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        error = L"TocPilot.json is a directory, not a state file.";
        return false;
    }

    std::string json;
    if (!ReadUtf8File(path, json, error)) {
        return false;
    }

    int schema = 0;
    if (!ParseIntegerField(json, "schema", schema)) {
        error = L"TocPilot.json has no valid schema number.";
        return false;
    }
    if (schema != 1) {
        error =
            L"TocPilot.json uses unsupported schema " +
            std::to_wstring(schema) + L".";
        return false;
    }

    double textScale = 1.0;
    if (!ParseDoubleField(json, "text_scale", textScale)) {
        error = L"TocPilot.json has no valid settings.text_scale value.";
        return false;
    }

    bool checkUpdates = true;
    if (!ParseBoolField(json, "check_app_updates", checkUpdates)) {
        error =
            L"TocPilot.json has no valid settings.check_app_updates value.";
        return false;
    }

    std::size_t packagesStart = 0;
    std::size_t packagesEnd = 0;
    if (!FindArrayRange(json, "packages", packagesStart, packagesEnd)) {
        error = L"TocPilot.json has no valid packages array.";
        return false;
    }

    state.schema = schema;
    state.settings.textScale =
        std::clamp(textScale, 0.75, 2.0);
    state.settings.checkAppUpdates = checkUpdates;
    state.packageCount = CountArrayItems(
        std::string_view(json).substr(
            packagesStart,
            packagesEnd - packagesStart));
    state.sourceJson = std::move(json);
    return true;
}

bool SaveState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    std::wstring& error) {
    error.clear();

    std::string json = state.sourceJson.empty()
        ? DefaultJson(state)
        : state.sourceJson;

    if (!ReplaceScalar(
            json,
            "text_scale",
            FormatScale(state.settings.textScale)) ||
        !ReplaceScalar(
            json,
            "check_app_updates",
            state.settings.checkAppUpdates ? "true" : "false")) {
        error =
            L"TocPilot.json settings could not be updated safely. "
            L"The existing file was left unchanged.";
        return false;
    }

    if (!AtomicWriteUtf8(StatePath(wowRoot), json, error)) {
        return false;
    }

    state.sourceJson = std::move(json);
    return true;
}

} // namespace tp
