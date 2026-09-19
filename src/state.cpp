#include "state.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace tp {
namespace {

constexpr std::size_t kMaxStateBytes = 8 * 1024 * 1024;
constexpr int kMaxJsonDepth = 64;

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

bool ParseStringRange(
    std::string_view text,
    std::size_t start,
    std::size_t& end) {
    if (start >= text.size() || text[start] != '"') {
        return false;
    }

    bool escaped = false;
    for (std::size_t pos = start + 1; pos < text.size(); ++pos) {
        const unsigned char ch =
            static_cast<unsigned char>(text[pos]);

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
        return ParseStringRange(text, pos, end);
    }

    if (text[pos] == '{') {
        ++pos;
        SkipWhitespace(text, pos);
        if (pos < text.size() && text[pos] == '}') {
            end = pos + 1;
            return true;
        }

        while (pos < text.size()) {
            std::size_t keyEnd = 0;
            if (!ParseStringRange(text, pos, keyEnd)) {
                return false;
            }

            pos = keyEnd;
            SkipWhitespace(text, pos);
            if (pos >= text.size() || text[pos] != ':') {
                return false;
            }

            ++pos;
            std::size_t valueEnd = 0;
            if (!SkipJsonValue(text, pos, valueEnd, depth + 1)) {
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
        if (pos < text.size() && text[pos] == ']') {
            end = pos + 1;
            return true;
        }

        while (pos < text.size()) {
            std::size_t valueEnd = 0;
            if (!SkipJsonValue(text, pos, valueEnd, depth + 1)) {
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
        if (ch == ',' || ch == '}' || ch == ']' ||
            std::isspace(static_cast<unsigned char>(ch))) {
            break;
        }
        ++pos;
    }

    if (pos == tokenStart) {
        return false;
    }

    const std::string_view token =
        text.substr(tokenStart, pos - tokenStart);
    if (token == "true" || token == "false" || token == "null") {
        end = pos;
        return true;
    }

    bool haveDigit = false;
    for (const char ch : token) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            haveDigit = true;
            continue;
        }
        if (ch != '-' && ch != '+' && ch != '.' &&
            ch != 'e' && ch != 'E') {
            return false;
        }
    }

    if (!haveDigit) {
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
    if (start >= json.size() || json[start] != '{') {
        return false;
    }

    if (!SkipJsonValue(json, start, end)) {
        return false;
    }

    std::size_t trailing = end;
    SkipWhitespace(json, trailing);
    return trailing == json.size();
}

bool HexDigit(char ch, unsigned int& value) {
    if (ch >= '0' && ch <= '9') {
        value = static_cast<unsigned int>(ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        value = static_cast<unsigned int>(ch - 'a' + 10);
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        value = static_cast<unsigned int>(ch - 'A' + 10);
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
    for (std::size_t i = 0; i < 4; ++i) {
        unsigned int digit = 0;
        if (!HexDigit(text[pos + i], digit)) {
            return false;
        }
        value = (value << 4) | digit;
    }
    return true;
}

void AppendUtf8CodePoint(std::string& output, unsigned int codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(
            0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(
            0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(
            0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
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

    for (std::size_t pos = 1; pos + 1 < token.size(); ++pos) {
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
            if (!ParseHex4(token, pos + 1, high)) {
                return false;
            }
            pos += 4;

            unsigned int codePoint = high;
            if (high >= 0xD800 && high <= 0xDBFF) {
                if (pos + 6 >= token.size() ||
                    token[pos + 1] != '\\' ||
                    token[pos + 2] != 'u') {
                    return false;
                }

                unsigned int low = 0;
                if (!ParseHex4(token, pos + 3, low) ||
                    low < 0xDC00 ||
                    low > 0xDFFF) {
                    return false;
                }

                codePoint =
                    0x10000 +
                    ((high - 0xD800) << 10) +
                    (low - 0xDC00);
                pos += 6;
            } else if (high >= 0xDC00 && high <= 0xDFFF) {
                return false;
            }

            AppendUtf8CodePoint(output, codePoint);
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
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
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
        return false;
    }

    output.resize(static_cast<std::size_t>(needed));
    return MultiByteToWideChar(
               CP_UTF8,
               MB_ERR_INVALID_CHARS,
               input.data(),
               sourceLength,
               output.data(),
               needed) == needed;
}

bool WideToUtf8(
    std::wstring_view input,
    std::string& output) {
    output.clear();
    if (input.empty()) {
        return true;
    }

    if (input.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
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
        return false;
    }

    output.resize(static_cast<std::size_t>(needed));
    return WideCharToMultiByte(
               CP_UTF8,
               WC_ERR_INVALID_CHARS,
               input.data(),
               sourceLength,
               output.data(),
               needed,
               nullptr,
               nullptr) == needed;
}

bool DecodeJsonString(
    std::string_view token,
    std::wstring& output) {
    std::string utf8;
    return DecodeJsonStringUtf8(token, utf8) &&
        Utf8ToWide(utf8, output);
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

    while (pos < objectEnd && json[pos] != '}') {
        std::size_t keyEnd = 0;
        if (!ParseStringRange(json, pos, keyEnd)) {
            return false;
        }

        std::string decodedKey;
        if (!DecodeJsonStringUtf8(
                json.substr(pos, keyEnd - pos),
                decodedKey)) {
            return false;
        }

        pos = keyEnd;
        SkipWhitespace(json, pos);
        if (pos >= objectEnd || json[pos] != ':') {
            return false;
        }

        ++pos;
        SkipWhitespace(json, pos);
        const std::size_t currentValueStart = pos;
        std::size_t currentValueEnd = 0;
        if (!SkipJsonValue(
                json,
                currentValueStart,
                currentValueEnd)) {
            return false;
        }

        if (decodedKey == key) {
            valueStart = currentValueStart;
            valueEnd = currentValueEnd;
            return true;
        }

        pos = currentValueEnd;
        SkipWhitespace(json, pos);
        if (pos < objectEnd && json[pos] == ',') {
            ++pos;
            SkipWhitespace(json, pos);
        } else if (pos < objectEnd && json[pos] != '}') {
            return false;
        }
    }

    return false;
}

bool GetRootMember(
    std::string_view json,
    std::string_view key,
    std::size_t& valueStart,
    std::size_t& valueEnd) {
    std::size_t rootStart = 0;
    std::size_t rootEnd = 0;
    return RootObjectRange(json, rootStart, rootEnd) &&
        FindObjectMember(
            json,
            rootStart,
            rootEnd,
            key,
            valueStart,
            valueEnd);
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
        DecodeJsonString(json.substr(start, end - start), value);
}

bool ParseIntegerToken(
    std::string_view token,
    int& value) {
    try {
        std::size_t used = 0;
        const long long parsed =
            std::stoll(std::string(token), &used, 10);
        if (used != token.size() ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseDoubleToken(
    std::string_view token,
    double& value) {
    try {
        std::size_t used = 0;
        value = std::stod(std::string(token), &used);
        return used == token.size() && std::isfinite(value);
    } catch (...) {
        return false;
    }
}

bool ParseBoolToken(
    std::string_view token,
    bool& value) {
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

std::string EscapeJson(std::wstring_view value) {
    std::string utf8;
    if (!WideToUtf8(value, utf8)) {
        return {};
    }

    std::string output;
    output.reserve(utf8.size() + 8);
    output.push_back('"');

    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char ch : utf8) {
        switch (ch) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (ch < 0x20) {
                output += "\\u00";
                output.push_back(hex[(ch >> 4) & 0x0F]);
                output.push_back(hex[ch & 0x0F]);
            } else {
                output.push_back(static_cast<char>(ch));
            }
            break;
        }
    }

    output.push_back('"');
    return output;
}

std::string SerializePackage(const PackageRecord& package) {
    std::ostringstream stream;
    stream
        << "{"
        << "\"id\":" << EscapeJson(package.id) << ","
        << "\"name\":" << EscapeJson(package.name) << ","
        << "\"provider\":" << EscapeJson(package.provider) << ","
        << "\"repository\":" << EscapeJson(package.repository) << ","
        << "\"mode\":" << EscapeJson(package.mode) << ","
        << "\"ref\":null,"
        << "\"asset\":null,"
        << "\"target\":" << EscapeJson(package.target) << ","
        << "\"installed_revision\":null,"
        << "\"installed_files\":[]"
        << "}";
    return stream.str();
}

std::string IndentJsonObject(std::string_view json) {
    std::string result;
    result.reserve(json.size() + 16);

    bool atLineStart = true;
    for (const char ch : json) {
        if (atLineStart) {
            result += "    ";
            atLineStart = false;
        }
        result.push_back(ch);
        if (ch == '\n') {
            atLineStart = true;
        }
    }
    return result;
}

std::string SerializePackages(const AppState& state) {
    if (state.packages.empty()) {
        return "[]";
    }

    std::ostringstream stream;
    stream << "[\n";
    for (std::size_t i = 0; i < state.packages.size(); ++i) {
        const auto& package = state.packages[i];
        const std::string object = package.sourceJson.empty()
            ? SerializePackage(package)
            : package.sourceJson;

        stream << IndentJsonObject(object);
        if (i + 1 != state.packages.size()) {
            stream << ",";
        }
        stream << "\n";
    }
    stream << "  ]";
    return stream.str();
}

std::string DefaultJson(const AppState& state) {
    std::ostringstream stream;
    stream
        << "{\n"
        << "  \"schema\": 1,\n"
        << "  \"settings\": {\n"
        << "    \"text_scale\": "
        << FormatScale(state.settings.textScale)
        << ",\n"
        << "    \"check_app_updates\": "
        << (state.settings.checkAppUpdates ? "true" : "false")
        << "\n"
        << "  },\n"
        << "  \"packages\": "
        << SerializePackages(state)
        << "\n"
        << "}\n";
    return stream.str();
}

bool ReplaceRange(
    std::string& json,
    std::size_t start,
    std::size_t end,
    std::string_view replacement) {
    if (start > end || end > json.size()) {
        return false;
    }
    json.replace(start, end - start, replacement);
    return true;
}

bool ReplaceSettingsMember(
    std::string& json,
    std::string_view key,
    std::string_view replacement) {
    std::size_t settingsStart = 0;
    std::size_t settingsEnd = 0;
    if (!GetRootMember(
            json,
            "settings",
            settingsStart,
            settingsEnd) ||
        settingsStart >= json.size() ||
        json[settingsStart] != '{') {
        return false;
    }

    std::size_t valueStart = 0;
    std::size_t valueEnd = 0;
    if (!FindObjectMember(
            json,
            settingsStart,
            settingsEnd,
            key,
            valueStart,
            valueEnd)) {
        return false;
    }

    return ReplaceRange(
        json,
        valueStart,
        valueEnd,
        replacement);
}

bool ReplacePackages(
    std::string& json,
    const AppState& state) {
    std::size_t start = 0;
    std::size_t end = 0;
    if (!GetRootMember(json, "packages", start, end) ||
        start >= json.size() ||
        json[start] != '[') {
        return false;
    }

    return ReplaceRange(
        json,
        start,
        end,
        SerializePackages(state));
}

bool ParsePackages(
    std::string_view json,
    std::size_t arrayStart,
    std::size_t arrayEnd,
    std::vector<PackageRecord>& packages,
    std::wstring& error) {
    packages.clear();

    if (arrayStart >= arrayEnd ||
        arrayEnd > json.size() ||
        json[arrayStart] != '[') {
        error = L"TocPilot.json has no valid packages array.";
        return false;
    }

    std::size_t pos = arrayStart + 1;
    SkipWhitespace(json, pos);
    if (pos < arrayEnd && json[pos] == ']') {
        return true;
    }

    while (pos < arrayEnd) {
        const std::size_t objectStart = pos;
        std::size_t objectEnd = 0;
        if (!SkipJsonValue(json, objectStart, objectEnd) ||
            objectStart >= json.size() ||
            json[objectStart] != '{' ||
            objectEnd > arrayEnd) {
            error =
                L"TocPilot.json contains an invalid package record.";
            return false;
        }

        PackageRecord package;
        if (!GetStringMember(
                json,
                objectStart,
                objectEnd,
                "id",
                package.id) ||
            !GetStringMember(
                json,
                objectStart,
                objectEnd,
                "name",
                package.name) ||
            !GetStringMember(
                json,
                objectStart,
                objectEnd,
                "provider",
                package.provider) ||
            !GetStringMember(
                json,
                objectStart,
                objectEnd,
                "repository",
                package.repository) ||
            !GetStringMember(
                json,
                objectStart,
                objectEnd,
                "mode",
                package.mode)) {
            error =
                L"TocPilot.json contains a package record "
                L"without the required id/name/provider/repository/mode fields.";
            return false;
        }

        std::wstring target;
        if (GetStringMember(
                json,
                objectStart,
                objectEnd,
                "target",
                target)) {
            package.target = std::move(target);
        }

        if (package.id.empty() ||
            package.name.empty() ||
            package.provider.empty() ||
            package.repository.empty() ||
            package.mode.empty()) {
            error =
                L"TocPilot.json contains an incomplete package record.";
            return false;
        }

        package.sourceJson =
            std::string(json.substr(
                objectStart,
                objectEnd - objectStart));
        packages.push_back(std::move(package));

        pos = objectEnd;
        SkipWhitespace(json, pos);
        if (pos >= arrayEnd) {
            error =
                L"TocPilot.json contains an unterminated packages array.";
            return false;
        }

        if (json[pos] == ']') {
            return true;
        }
        if (json[pos] != ',') {
            error =
                L"TocPilot.json contains an invalid packages array.";
            return false;
        }

        ++pos;
        SkipWhitespace(json, pos);
    }

    error = L"TocPilot.json contains an unterminated packages array.";
    return false;
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
        static_cast<unsigned long long>(size.QuadPart) >
            kMaxStateBytes) {
        const DWORD lastError = GetLastError();
        CloseHandle(file);
        error = lastError == ERROR_SUCCESS
            ? L"TocPilot.json is unexpectedly large."
            : L"Could not read TocPilot.json size: " +
                WindowsError(lastError);
        return false;
    }

    content.assign(
        static_cast<std::size_t>(size.QuadPart),
        '\0');
    std::size_t offset = 0;

    while (offset < content.size()) {
        DWORD bytesRead = 0;
        const DWORD request =
            static_cast<DWORD>(
                std::min<std::size_t>(
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
            error =
                L"Could not read TocPilot.json: " +
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
        error =
            L"Could not create TocPilot.json.tmp: " +
            WindowsError(GetLastError());
        return false;
    }

    std::size_t offset = 0;
    while (offset < content.size()) {
        DWORD written = 0;
        const DWORD request =
            static_cast<DWORD>(
                std::min<std::size_t>(
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
            error =
                L"Could not write TocPilot.json.tmp: " +
                WindowsError(lastError);
            return false;
        }

        if (written == 0) {
            CloseHandle(file);
            DeleteFileW(temporary.c_str());
            error =
                L"Windows wrote zero bytes to TocPilot.json.tmp.";
            return false;
        }
        offset += written;
    }

    if (!FlushFileBuffers(file)) {
        const DWORD lastError = GetLastError();
        CloseHandle(file);
        DeleteFileW(temporary.c_str());
        error =
            L"Could not flush TocPilot.json.tmp: " +
            WindowsError(lastError);
        return false;
    }

    CloseHandle(file);

    const DWORD attributes =
        GetFileAttributesW(target.c_str());
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
            error =
                L"Could not replace TocPilot.json: " +
                WindowsError(lastError);
            return false;
        }
    } else {
        if (!MoveFileExW(
                temporary.c_str(),
                target.c_str(),
                MOVEFILE_REPLACE_EXISTING |
                    MOVEFILE_WRITE_THROUGH)) {
            const DWORD lastError = GetLastError();
            DeleteFileW(temporary.c_str());
            error =
                L"Could not create TocPilot.json: " +
                WindowsError(lastError);
            return false;
        }
    }

    return true;
}

bool EqualsInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

std::filesystem::path StatePath(
    const std::filesystem::path& wowRoot) {
    return wowRoot / L"TocPilot.json";
}

PackageRecord MakeRepositoryPackage(
    std::wstring provider,
    std::wstring repository) {
    PackageRecord package;
    package.provider = std::move(provider);
    package.repository = std::move(repository);

    const std::size_t slash =
        package.repository.find_last_of(L'/');
    package.name = slash == std::wstring::npos
        ? package.repository
        : package.repository.substr(slash + 1);

    package.id =
        package.provider + L":" + package.repository;
    package.mode = L"unconfigured";
    package.target = L"addons";
    package.sourceJson = SerializePackage(package);
    return package;
}

bool AppendPackage(
    AppState& state,
    PackageRecord package,
    std::wstring& error) {
    error.clear();

    if (package.id.empty() ||
        package.name.empty() ||
        package.provider.empty() ||
        package.repository.empty()) {
        error =
            L"The package source is incomplete and was not saved.";
        return false;
    }

    for (const auto& existing : state.packages) {
        if (EqualsInsensitive(existing.id, package.id)) {
            error =
                L"This repository is already managed by TocPilot.";
            return false;
        }
    }

    if (package.sourceJson.empty()) {
        package.sourceJson = SerializePackage(package);
    }

    state.packages.push_back(std::move(package));
    return true;
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
    const DWORD attributes =
        GetFileAttributesW(path.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND &&
            GetLastError() != ERROR_PATH_NOT_FOUND) {
            error =
                L"Could not inspect TocPilot.json: " +
                WindowsError(GetLastError());
            return false;
        }

        state.sourceJson = DefaultJson(state);
        if (!AtomicWriteUtf8(
                path,
                state.sourceJson,
                error)) {
            state.sourceJson.clear();
            return false;
        }

        created = true;
        return true;
    }

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        error =
            L"TocPilot.json is a directory, not a state file.";
        return false;
    }

    std::string json;
    if (!ReadUtf8File(path, json, error)) {
        return false;
    }

    std::size_t schemaStart = 0;
    std::size_t schemaEnd = 0;
    int schema = 0;
    if (!GetRootMember(
            json,
            "schema",
            schemaStart,
            schemaEnd) ||
        !ParseIntegerToken(
            std::string_view(json).substr(
                schemaStart,
                schemaEnd - schemaStart),
            schema)) {
        error =
            L"TocPilot.json has no valid schema number.";
        return false;
    }
    if (schema != 1) {
        error =
            L"TocPilot.json uses unsupported schema " +
            std::to_wstring(schema) + L".";
        return false;
    }

    std::size_t settingsStart = 0;
    std::size_t settingsEnd = 0;
    if (!GetRootMember(
            json,
            "settings",
            settingsStart,
            settingsEnd) ||
        settingsStart >= json.size() ||
        json[settingsStart] != '{') {
        error =
            L"TocPilot.json has no valid settings object.";
        return false;
    }

    std::size_t scaleStart = 0;
    std::size_t scaleEnd = 0;
    double textScale = 1.0;
    if (!FindObjectMember(
            json,
            settingsStart,
            settingsEnd,
            "text_scale",
            scaleStart,
            scaleEnd) ||
        !ParseDoubleToken(
            std::string_view(json).substr(
                scaleStart,
                scaleEnd - scaleStart),
            textScale)) {
        error =
            L"TocPilot.json has no valid settings.text_scale value.";
        return false;
    }

    std::size_t checkStart = 0;
    std::size_t checkEnd = 0;
    bool checkUpdates = true;
    if (!FindObjectMember(
            json,
            settingsStart,
            settingsEnd,
            "check_app_updates",
            checkStart,
            checkEnd) ||
        !ParseBoolToken(
            std::string_view(json).substr(
                checkStart,
                checkEnd - checkStart),
            checkUpdates)) {
        error =
            L"TocPilot.json has no valid "
            L"settings.check_app_updates value.";
        return false;
    }

    std::size_t packagesStart = 0;
    std::size_t packagesEnd = 0;
    if (!GetRootMember(
            json,
            "packages",
            packagesStart,
            packagesEnd) ||
        packagesStart >= json.size() ||
        json[packagesStart] != '[') {
        error =
            L"TocPilot.json has no valid packages array.";
        return false;
    }

    std::vector<PackageRecord> packages;
    if (!ParsePackages(
            json,
            packagesStart,
            packagesEnd,
            packages,
            error)) {
        return false;
    }

    state.schema = schema;
    state.settings.textScale =
        std::clamp(textScale, 0.75, 2.0);
    state.settings.checkAppUpdates = checkUpdates;
    state.packages = std::move(packages);
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

    if (!ReplaceSettingsMember(
            json,
            "text_scale",
            FormatScale(state.settings.textScale)) ||
        !ReplaceSettingsMember(
            json,
            "check_app_updates",
            state.settings.checkAppUpdates
                ? "true"
                : "false") ||
        !ReplacePackages(json, state)) {
        error =
            L"TocPilot.json could not be updated safely. "
            L"The existing file was left unchanged.";
        return false;
    }

    if (!AtomicWriteUtf8(
            StatePath(wowRoot),
            json,
            error)) {
        return false;
    }

    state.sourceJson = std::move(json);
    return true;
}

} // namespace tp
