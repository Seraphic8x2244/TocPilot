#include "install.h"

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <map>
#include <chrono>
#include <cwctype>
#include <iomanip>
#include <set>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace tp {
namespace {

constexpr int kFilesystemRetries = 5;
constexpr auto kFilesystemRetryDelay = std::chrono::milliseconds(50);

struct InsensitiveLess {
    bool operator()(
        const std::wstring& left,
        const std::wstring& right) const {
        return std::lexicographical_compare(
            left.begin(),
            left.end(),
            right.begin(),
            right.end(),
            [](wchar_t a, wchar_t b) {
                return std::towlower(a) < std::towlower(b);
            });
    }
};

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

bool IsReservedDeviceName(std::wstring_view segment) {
    const std::size_t dot = segment.find(L'.');
    std::wstring stem(
        segment.substr(
            0,
            dot == std::wstring_view::npos
                ? segment.size()
                : dot));

    std::transform(
        stem.begin(),
        stem.end(),
        stem.begin(),
        [](wchar_t ch) {
            return std::towupper(ch);
        });

    if (stem == L"CON" ||
        stem == L"PRN" ||
        stem == L"AUX" ||
        stem == L"NUL") {
        return true;
    }

    if (stem.size() == 4 &&
        (stem.rfind(L"COM", 0) == 0 ||
         stem.rfind(L"LPT", 0) == 0) &&
        stem[3] >= L'1' &&
        stem[3] <= L'9') {
        return true;
    }

    return false;
}

bool SafeInstallFolderName(
    std::wstring_view folder,
    std::wstring& error) {
    if (folder.empty() ||
        folder == L"." ||
        folder == L"..") {
        error = L"Addon install folder is empty or unsafe.";
        return false;
    }

    if (folder.back() == L'.' ||
        folder.back() == L' ') {
        error =
            L"Addon install folder ends with a dot or space.";
        return false;
    }

    for (const wchar_t ch : folder) {
        if (ch < 0x20 ||
            ch == L'<' ||
            ch == L'>' ||
            ch == L':' ||
            ch == L'"' ||
            ch == L'/' ||
            ch == L'\\' ||
            ch == L'|' ||
            ch == L'?' ||
            ch == L'*') {
            error =
                L"Addon install folder contains an unsafe Windows character.";
            return false;
        }
    }

    if (IsReservedDeviceName(folder)) {
        error =
            L"Addon install folder uses a reserved Windows device name.";
        return false;
    }

    return true;
}


struct AddonTransactionJournal {
    std::string phase;
    std::wstring transactionId;
    std::wstring packageId;
    AddonTransactionStateMarker preState;
    AddonTransactionStateMarker postState;
    std::vector<std::wstring> preExistingRoots;
    std::vector<std::wstring> postRoots;
};

constexpr std::size_t kMaxJournalBytes = 1024 * 1024;
constexpr char kJournalMagic[] = "TOCPILOT_ADDON_TRANSACTION_V1";

std::wstring GenerateTransactionId() {
    GUID guid{};
    if (SUCCEEDED(CoCreateGuid(&guid))) {
        wchar_t buffer[40]{};
        if (StringFromGUID2(
                guid,
                buffer,
                static_cast<int>(sizeof(buffer) / sizeof(buffer[0]))) > 0) {
            return buffer;
        }
    }

    const auto ticks =
        std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count();
    return
        L"fallback-" +
        std::to_wstring(
            static_cast<unsigned long long>(ticks)) +
        L"-" +
        std::to_wstring(GetCurrentProcessId());
}

bool WideToUtf8(
    std::wstring_view value,
    std::string& utf8) {
    utf8.clear();
    if (value.empty()) {
        return true;
    }

    const int bytes = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (bytes <= 0) {
        return false;
    }

    utf8.resize(static_cast<std::size_t>(bytes));
    return WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        utf8.data(),
        bytes,
        nullptr,
        nullptr) == bytes;
}

bool Utf8ToWide(
    std::string_view value,
    std::wstring& wide) {
    wide.clear();
    if (value.empty()) {
        return true;
    }

    const int chars = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (chars <= 0) {
        return false;
    }

    wide.resize(static_cast<std::size_t>(chars));
    return MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        wide.data(),
        chars) == chars;
}

char HexDigit(unsigned value) {
    return static_cast<char>(
        value < 10 ? '0' + value : 'a' + (value - 10));
}

int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + ch - 'a';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + ch - 'A';
    }
    return -1;
}

bool EncodeJournalText(
    std::wstring_view value,
    std::string& encoded) {
    std::string utf8;
    if (!WideToUtf8(value, utf8)) {
        return false;
    }

    encoded.clear();
    encoded.reserve(utf8.size() * 2);
    for (const unsigned char ch : utf8) {
        encoded.push_back(HexDigit(ch >> 4));
        encoded.push_back(HexDigit(ch & 0x0f));
    }
    return true;
}

bool DecodeJournalText(
    std::string_view encoded,
    std::wstring& value) {
    if ((encoded.size() % 2) != 0) {
        return false;
    }

    std::string utf8;
    utf8.reserve(encoded.size() / 2);
    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        const int high = HexValue(encoded[i]);
        const int low = HexValue(encoded[i + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        utf8.push_back(
            static_cast<char>((high << 4) | low));
    }

    return Utf8ToWide(utf8, value);
}

std::filesystem::path JournalPath(
    const std::filesystem::path& transactionRoot) {
    return transactionRoot / L"journal.v1";
}

bool WriteAllJournalBytes(
    HANDLE file,
    std::string_view content,
    std::wstring& error) {
    std::size_t offset = 0;
    while (offset < content.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(
                content.size() - offset,
                1024 * 1024));
        DWORD written = 0;
        if (!WriteFile(
                file,
                content.data() + offset,
                chunk,
                &written,
                nullptr) ||
            written != chunk) {
            error =
                L"Could not write addon transaction journal.";
            return false;
        }
        offset += written;
    }

    if (!FlushFileBuffers(file)) {
        error =
            L"Could not flush addon transaction journal.";
        return false;
    }
    return true;
}

bool AtomicWriteJournal(
    const std::filesystem::path& path,
    std::string_view content,
    std::wstring& error) {
    if (content.size() > kMaxJournalBytes) {
        error =
            L"Addon transaction journal is unexpectedly large.";
        return false;
    }

    auto temp = path;
    temp += L".tmp";

    HANDLE file = CreateFileW(
        temp.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error =
            L"Could not create addon transaction journal.";
        return false;
    }

    const bool wrote =
        WriteAllJournalBytes(
            file,
            content,
            error);
    CloseHandle(file);

    if (!wrote) {
        DeleteFileW(temp.c_str());
        return false;
    }

    std::error_code ec;
    const bool destinationExists =
        std::filesystem::exists(path, ec);
    if (ec) {
        DeleteFileW(temp.c_str());
        error =
            L"Could not inspect addon transaction journal.";
        return false;
    }

    BOOL replaced = FALSE;
    if (destinationExists) {
        replaced = ReplaceFileW(
            path.c_str(),
            temp.c_str(),
            nullptr,
            REPLACEFILE_WRITE_THROUGH,
            nullptr,
            nullptr);
    } else {
        replaced = MoveFileExW(
            temp.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH);
    }

    if (!replaced) {
        DeleteFileW(temp.c_str());
        error =
            L"Could not atomically publish addon transaction journal.";
        return false;
    }

    return true;
}

bool ReadJournalBytes(
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
        error =
            L"Unfinished addon transaction has no readable journal: " +
            path.parent_path().wstring();
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) ||
        size.QuadPart < 0 ||
        static_cast<unsigned long long>(size.QuadPart) >
            kMaxJournalBytes) {
        CloseHandle(file);
        error =
            L"Addon transaction journal has an invalid size: " +
            path.wstring();
        return false;
    }

    content.resize(
        static_cast<std::size_t>(size.QuadPart));
    std::size_t offset = 0;
    while (offset < content.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(
                content.size() - offset,
                1024 * 1024));
        DWORD read = 0;
        if (!ReadFile(
                file,
                content.data() + offset,
                chunk,
                &read,
                nullptr) ||
            read == 0) {
            CloseHandle(file);
            error =
                L"Could not read addon transaction journal: " +
                path.wstring();
            return false;
        }
        offset += read;
    }

    CloseHandle(file);
    return true;
}

void AppendJournalText(
    std::string& output,
    std::string_view key,
    std::wstring_view value,
    bool& ok) {
    if (!ok) {
        return;
    }

    std::string encoded;
    if (!EncodeJournalText(value, encoded)) {
        ok = false;
        return;
    }

    output.append(key);
    output.push_back('=');
    output.append(encoded);
    output.push_back('\n');
}

std::string SerializeJournal(
    const AddonInstallTransaction& transaction,
    std::string_view phase,
    bool& ok) {
    ok = true;
    std::string output;
    output.reserve(1024);
    output += kJournalMagic;
    output.push_back('\n');
    output += "phase=";
    output.append(phase);
    output.push_back('\n');

    AppendJournalText(
        output,
        "transaction_id",
        transaction.transactionId,
        ok);
    AppendJournalText(
        output,
        "package_id",
        transaction.plan.packageId,
        ok);

    if (phase == "armed") {
        output += transaction.preState.present
            ? "pre_present=1\n"
            : "pre_present=0\n";
        AppendJournalText(
            output,
            "pre_package_id",
            transaction.preState.packageId,
            ok);
        AppendJournalText(
            output,
            "pre_transaction_id",
            transaction.preState.transactionId,
            ok);

        output += transaction.postState.present
            ? "post_present=1\n"
            : "post_present=0\n";
        AppendJournalText(
            output,
            "post_package_id",
            transaction.postState.packageId,
            ok);
        AppendJournalText(
            output,
            "post_transaction_id",
            transaction.postState.transactionId,
            ok);

        for (const auto& root :
             transaction.preExistingRoots) {
            AppendJournalText(
                output,
                "pre_root",
                root,
                ok);
        }
    }

    for (const auto& root : transaction.postRoots) {
        AppendJournalText(
            output,
            "post_root",
            root,
            ok);
    }

    return output;
}

bool WriteTransactionJournal(
    const AddonInstallTransaction& transaction,
    std::string_view phase,
    std::wstring& error) {
    bool encoded = false;
    const std::string content =
        SerializeJournal(
            transaction,
            phase,
            encoded);
    if (!encoded) {
        error =
            L"Addon transaction journal contains text that cannot be encoded.";
        return false;
    }

    return AtomicWriteJournal(
        JournalPath(
            transaction.plan.transactionRoot),
        content,
        error);
}

bool GetSingleJournalField(
    const std::map<std::string, std::vector<std::string>>& fields,
    const char* key,
    std::string& value) {
    const auto it = fields.find(key);
    if (it == fields.end() ||
        it->second.size() != 1) {
        return false;
    }

    value = it->second.front();
    return true;
}

bool ParsePresentField(
    const std::map<std::string, std::vector<std::string>>& fields,
    const char* key,
    bool& present) {
    std::string value;
    if (!GetSingleJournalField(
            fields,
            key,
            value)) {
        return false;
    }
    if (value == "1") {
        present = true;
        return true;
    }
    if (value == "0") {
        present = false;
        return true;
    }
    return false;
}

bool ParseEncodedField(
    const std::map<std::string, std::vector<std::string>>& fields,
    const char* key,
    std::wstring& value) {
    std::string encoded;
    return
        GetSingleJournalField(fields, key, encoded) &&
        DecodeJournalText(encoded, value);
}

bool ParseJournal(
    std::string_view content,
    AddonTransactionJournal& journal,
    std::wstring& error) {
    journal = {};

    std::map<std::string, std::vector<std::string>> fields;
    std::size_t pos = 0;
    const std::size_t firstEnd =
        content.find('\n', pos);
    if (firstEnd == std::string_view::npos ||
        content.substr(0, firstEnd) !=
            kJournalMagic) {
        error =
            L"Addon transaction journal has an unsupported format.";
        return false;
    }
    pos = firstEnd + 1;

    while (pos < content.size()) {
        const std::size_t end =
            content.find('\n', pos);
        const std::size_t lineEnd =
            end == std::string_view::npos
                ? content.size()
                : end;
        const std::string_view line =
            content.substr(pos, lineEnd - pos);

        if (!line.empty()) {
            const std::size_t equals =
                line.find('=');
            if (equals == std::string_view::npos ||
                equals == 0) {
                error =
                    L"Addon transaction journal contains a malformed field.";
                return false;
            }
            fields[
                std::string(line.substr(0, equals))]
                .push_back(
                    std::string(line.substr(equals + 1)));
        }

        if (end == std::string_view::npos) {
            break;
        }
        pos = end + 1;
    }

    std::string phase;
    if (!GetSingleJournalField(
            fields,
            "phase",
            phase) ||
        (phase != "prepared" &&
         phase != "armed")) {
        error =
            L"Addon transaction journal has an invalid phase.";
        return false;
    }
    journal.phase = std::move(phase);

    if (!ParseEncodedField(
            fields,
            "transaction_id",
            journal.transactionId) ||
        !ParseEncodedField(
            fields,
            "package_id",
            journal.packageId) ||
        journal.transactionId.empty() ||
        journal.packageId.empty()) {
        error =
            L"Addon transaction journal is missing transaction identity.";
        return false;
    }

    const auto postRoots =
        fields.find("post_root");
    if (postRoots != fields.end()) {
        for (const auto& encoded :
             postRoots->second) {
            std::wstring root;
            std::wstring rootError;
            if (!DecodeJournalText(
                    encoded,
                    root) ||
                !SafeInstallFolderName(
                    root,
                    rootError)) {
                error =
                    L"Addon transaction journal contains an unsafe post-state root.";
                return false;
            }
            journal.postRoots.push_back(
                std::move(root));
        }
    }

    if (journal.phase == "prepared") {
        return true;
    }

    if (!ParsePresentField(
            fields,
            "pre_present",
            journal.preState.present) ||
        !ParseEncodedField(
            fields,
            "pre_package_id",
            journal.preState.packageId) ||
        !ParseEncodedField(
            fields,
            "pre_transaction_id",
            journal.preState.transactionId) ||
        !ParsePresentField(
            fields,
            "post_present",
            journal.postState.present) ||
        !ParseEncodedField(
            fields,
            "post_package_id",
            journal.postState.packageId) ||
        !ParseEncodedField(
            fields,
            "post_transaction_id",
            journal.postState.transactionId) ||
        journal.preState.packageId.empty() ||
        journal.postState.packageId.empty()) {
        error =
            L"Addon transaction journal is missing durable-state discriminators.";
        return false;
    }

    if (journal.postState.present &&
        journal.postState.transactionId !=
            journal.transactionId) {
        error =
            L"Addon transaction journal post-state marker does not match its transaction identity.";
        return false;
    }

    const auto preRoots =
        fields.find("pre_root");
    if (preRoots != fields.end()) {
        for (const auto& encoded :
             preRoots->second) {
            std::wstring root;
            std::wstring rootError;
            if (!DecodeJournalText(
                    encoded,
                    root) ||
                !SafeInstallFolderName(
                    root,
                    rootError)) {
                error =
                    L"Addon transaction journal contains an unsafe pre-state root.";
                return false;
            }
            journal.preExistingRoots.push_back(
                std::move(root));
        }
    }

    return true;
}

bool ReadTransactionJournal(
    const std::filesystem::path& transactionRoot,
    AddonTransactionJournal& journal,
    std::wstring& error) {
    std::string content;
    return
        ReadJournalBytes(
            JournalPath(transactionRoot),
            content,
            error) &&
        ParseJournal(
            content,
            journal,
            error);
}

bool ContainsInsensitive(
    const std::vector<std::wstring>& values,
    std::wstring_view value) {
    return std::any_of(
        values.begin(),
        values.end(),
        [&](const std::wstring& candidate) {
            return EqualsInsensitive(
                candidate,
                value);
        });
}

std::vector<std::wstring> RootsToBackup(
    const AddonInstallPlan& plan) {
    std::vector<std::wstring> roots;
    roots.reserve(
        plan.roots.size() +
        plan.obsoleteInstallFolders.size());

    for (const auto& root : plan.roots) {
        roots.push_back(root.installFolder);
    }
    roots.insert(
        roots.end(),
        plan.obsoleteInstallFolders.begin(),
        plan.obsoleteInstallFolders.end());

    std::sort(
        roots.begin(),
        roots.end(),
        InsensitiveLess{});
    roots.erase(
        std::unique(
            roots.begin(),
            roots.end(),
            [](const std::wstring& left,
               const std::wstring& right) {
                return EqualsInsensitive(
                    left,
                    right);
            }),
        roots.end());
    return roots;
}

bool SafeSourceRelativePath(
    const std::filesystem::path& path) {
    if (path.empty() ||
        path.is_absolute() ||
        path.has_root_name() ||
        path.has_root_directory()) {
        return false;
    }

    for (const auto& part : path) {
        if (part.empty() ||
            part == L"." ||
            part == L"..") {
            return false;
        }
    }

    return true;
}

std::wstring TransactionKey(std::wstring_view packageId) {
    std::wstring key;
    key.reserve(std::min<std::size_t>(packageId.size(), 48) + 17);

    for (const wchar_t ch : packageId) {
        if (key.size() >= 48) {
            break;
        }

        if ((ch >= L'a' && ch <= L'z') ||
            (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'0' && ch <= L'9') ||
            ch == L'-' ||
            ch == L'_' ||
            ch == L'.') {
            key.push_back(ch);
        } else {
            key.push_back(L'-');
        }
    }

    if (key.empty()) {
        key = L"package";
    }

    std::uint64_t hash = 1469598103934665603ULL;
    for (const wchar_t ch : packageId) {
        hash ^= static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(ch));
        hash *= 1099511628211ULL;
    }

    std::wostringstream stream;
    stream
        << key
        << L"-"
        << std::hex
        << std::setw(16)
        << std::setfill(L'0')
        << hash;
    return stream.str();
}

bool ParseOwnedAddonRoot(
    std::wstring_view ownedFile,
    std::wstring& root,
    std::wstring& error) {
    std::wstring normalized(ownedFile);
    std::replace(
        normalized.begin(),
        normalized.end(),
        L'\\',
        L'/');

    if (normalized.empty() ||
        normalized.front() == L'/' ||
        normalized.find(L':') != std::wstring::npos) {
        error =
            L"Installed-file ownership contains an unsafe path.";
        return false;
    }

    std::vector<std::wstring> parts;
    std::size_t start = 0;
    while (start <= normalized.size()) {
        const std::size_t slash =
            normalized.find(L'/', start);
        const std::size_t end =
            slash == std::wstring::npos
                ? normalized.size()
                : slash;

        if (end == start) {
            error =
                L"Installed-file ownership contains an empty path segment.";
            return false;
        }

        std::wstring part =
            normalized.substr(start, end - start);
        if (part == L"." || part == L"..") {
            error =
                L"Installed-file ownership contains traversal.";
            return false;
        }

        parts.push_back(std::move(part));
        if (slash == std::wstring::npos) {
            break;
        }
        start = slash + 1;
    }

    if (parts.size() < 4 ||
        !EqualsInsensitive(parts[0], L"Interface") ||
        !EqualsInsensitive(parts[1], L"AddOns")) {
        error =
            L"Installed-file ownership is outside Interface\\AddOns.";
        return false;
    }

    if (!SafeInstallFolderName(parts[2], error)) {
        return false;
    }

    root = std::move(parts[2]);
    return true;
}

bool CollectOwnedRoots(
    const std::vector<std::wstring>& files,
    std::set<std::wstring, InsensitiveLess>& roots,
    std::wstring& error) {
    roots.clear();

    for (const auto& file : files) {
        std::wstring root;
        if (!ParseOwnedAddonRoot(file, root, error)) {
            return false;
        }
        roots.insert(std::move(root));
    }

    return true;
}

std::wstring OwnedFilePath(
    std::wstring_view installFolder,
    const std::filesystem::path& relative) {
    return (
        std::filesystem::path(L"Interface") /
        L"AddOns" /
        installFolder /
        relative)
        .generic_wstring();
}

std::wstring ErrorMessage(
    const std::error_code& ec) {
    const std::string message = ec.message();
    return std::wstring(
        message.begin(),
        message.end());
}

bool RemoveAllWithRetries(
    const std::filesystem::path& path,
    std::wstring& error) {
    std::error_code last;

    for (int attempt = 0;
         attempt < kFilesystemRetries;
         ++attempt) {
        last.clear();
        std::filesystem::remove_all(path, last);

        std::error_code existsError;
        const bool exists =
            std::filesystem::exists(path, existsError);

        if (!last && !existsError && !exists) {
            return true;
        }

        if (attempt + 1 < kFilesystemRetries) {
            std::this_thread::sleep_for(
                kFilesystemRetryDelay);
        }
    }

    error =
        L"Could not remove " +
        path.wstring() +
        L": " +
        ErrorMessage(last);
    return false;
}

bool RenameWithRetries(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    std::wstring& error) {
    std::error_code last;

    for (int attempt = 0;
         attempt < kFilesystemRetries;
         ++attempt) {
        last.clear();
        std::filesystem::rename(from, to, last);
        if (!last) {
            return true;
        }

        if (attempt + 1 < kFilesystemRetries) {
            std::this_thread::sleep_for(
                kFilesystemRetryDelay);
        }
    }

    error =
        L"Could not move " +
        from.wstring() +
        L" to " +
        to.wstring() +
        L": " +
        ErrorMessage(last);
    return false;
}

bool CopyTree(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::wstring& error) {
    std::error_code ec;
    const auto sourceStatus =
        std::filesystem::symlink_status(source, ec);

    if (ec ||
        std::filesystem::is_symlink(sourceStatus) ||
        !std::filesystem::is_directory(sourceStatus)) {
        error =
            L"Staged addon root is not a normal directory: " +
            source.wstring();
        return false;
    }

    std::filesystem::create_directories(
        destination,
        ec);
    if (ec) {
        error =
            L"Could not prepare transaction directory: " +
            destination.wstring();
        return false;
    }

    std::filesystem::recursive_directory_iterator it(
        source,
        std::filesystem::directory_options::none,
        ec);
    const std::filesystem::recursive_directory_iterator end;

    if (ec) {
        error =
            L"Could not enumerate staged addon root: " +
            source.wstring();
        return false;
    }

    for (; it != end; it.increment(ec)) {
        if (ec) {
            error =
                L"Could not enumerate staged addon files: " +
                source.wstring();
            return false;
        }

        const auto status =
            std::filesystem::symlink_status(
                it->path(),
                ec);
        if (ec) {
            error =
                L"Could not inspect staged addon path: " +
                it->path().wstring();
            return false;
        }

        if (std::filesystem::is_symlink(status)) {
            error =
                L"Staged addon contains a symbolic link: " +
                it->path().wstring();
            return false;
        }

        const auto relative =
            it->path().lexically_relative(source);
        if (relative.empty() ||
            !SafeSourceRelativePath(relative)) {
            error =
                L"Staged addon contains an unsafe relative path.";
            return false;
        }

        const auto target =
            destination / relative;

        if (std::filesystem::is_directory(status)) {
            std::filesystem::create_directories(
                target,
                ec);
            if (ec) {
                error =
                    L"Could not create prepared addon directory: " +
                    target.wstring();
                return false;
            }
            continue;
        }

        if (!std::filesystem::is_regular_file(status)) {
            error =
                L"Staged addon contains an unsupported filesystem object: " +
                it->path().wstring();
            return false;
        }

        std::filesystem::create_directories(
            target.parent_path(),
            ec);
        if (ec) {
            error =
                L"Could not create prepared addon parent directory.";
            return false;
        }

        std::filesystem::copy_file(
            it->path(),
            target,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        if (ec) {
            error =
                L"Could not copy staged addon file into the transaction: " +
                it->path().wstring();
            return false;
        }
    }

    return true;
}

bool RootExists(
    const std::filesystem::path& path,
    bool& exists,
    std::wstring& error) {
    std::error_code ec;
    const auto status =
        std::filesystem::symlink_status(path, ec);

    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            exists = false;
            return true;
        }

        error =
            L"Could not inspect live addon path: " +
            path.wstring();
        return false;
    }

    if (!std::filesystem::exists(status)) {
        exists = false;
        return true;
    }

    if (std::filesystem::is_symlink(status) ||
        !std::filesystem::is_directory(status)) {
        error =
            L"Live addon destination is not a normal directory: " +
            path.wstring();
        return false;
    }

    exists = true;
    return true;
}

std::filesystem::path BackupPath(
    const AddonInstallPlan& plan,
    std::wstring_view folder) {
    return
        plan.transactionRoot /
        L"backup" /
        folder;
}

bool NormalDirectoryExists(
    const std::filesystem::path& path,
    bool& exists,
    std::wstring& error) {
    std::error_code ec;
    const auto status =
        std::filesystem::symlink_status(path, ec);

    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            exists = false;
            return true;
        }

        error =
            L"Could not inspect addon transaction path: " +
            path.wstring();
        return false;
    }

    if (!std::filesystem::exists(status)) {
        exists = false;
        return true;
    }

    if (std::filesystem::is_symlink(status) ||
        !std::filesystem::is_directory(status)) {
        error =
            L"Addon transaction path is not a normal directory: " +
            path.wstring();
        return false;
    }

    exists = true;
    return true;
}

bool RestoreFromIntent(
    const std::filesystem::path& addOnsRoot,
    const std::filesystem::path& transactionRoot,
    const std::vector<std::wstring>& preExistingRoots,
    const std::vector<std::wstring>& postRoots,
    std::wstring& error) {
    error.clear();

    std::vector<std::wstring> affected =
        preExistingRoots;
    affected.insert(
        affected.end(),
        postRoots.begin(),
        postRoots.end());
    std::sort(
        affected.begin(),
        affected.end(),
        InsensitiveLess{});
    affected.erase(
        std::unique(
            affected.begin(),
            affected.end(),
            [](const std::wstring& left,
               const std::wstring& right) {
                return EqualsInsensitive(
                    left,
                    right);
            }),
        affected.end());

    for (const auto& root : affected) {
        const bool existedBefore =
            ContainsInsensitive(
                preExistingRoots,
                root);
        const bool existsAfter =
            ContainsInsensitive(
                postRoots,
                root);
        const auto live =
            addOnsRoot / root;
        const auto backup =
            transactionRoot /
            L"backup" /
            root;

        bool backupExists = false;
        if (!NormalDirectoryExists(
                backup,
                backupExists,
                error)) {
            return false;
        }

        if (backupExists) {
            if (!existedBefore) {
                error =
                    L"Addon transaction contains an unexpected rollback backup for " +
                    root +
                    L".";
                return false;
            }

            if (!RemoveAllWithRetries(
                    live,
                    error) ||
                !RenameWithRetries(
                    backup,
                    live,
                    error)) {
                return false;
            }
            continue;
        }

        if (existedBefore) {
            bool liveExists = false;
            if (!RootExists(
                    live,
                    liveExists,
                    error)) {
                return false;
            }
            if (!liveExists) {
                error =
                    L"Rollback evidence is incomplete for addon root " +
                    root +
                    L".";
                return false;
            }
            continue;
        }

        if (existsAfter &&
            !RemoveAllWithRetries(
                live,
                error)) {
            return false;
        }
    }

    return true;
}

bool VerifyPostStateFilesystem(
    const std::filesystem::path& addOnsRoot,
    const std::vector<std::wstring>& preExistingRoots,
    const std::vector<std::wstring>& postRoots,
    std::wstring& error) {
    error.clear();

    for (const auto& root : postRoots) {
        bool exists = false;
        if (!RootExists(
                addOnsRoot / root,
                exists,
                error)) {
            return false;
        }
        if (!exists) {
            error =
                L"Committed addon transaction is missing live root " +
                root +
                L".";
            return false;
        }
    }

    for (const auto& root : preExistingRoots) {
        if (ContainsInsensitive(
                postRoots,
                root)) {
            continue;
        }

        bool exists = false;
        if (!RootExists(
                addOnsRoot / root,
                exists,
                error)) {
            return false;
        }
        if (exists) {
            error =
                L"Committed addon transaction still contains obsolete live root " +
                root +
                L".";
            return false;
        }
    }

    return true;
}

bool MarkerExpectationMatches(
    const AddonTransactionStateMarker& expected,
    const std::vector<AddonTransactionStateMarker>& currentState) {
    std::size_t matches = 0;
    bool exact = false;

    for (const auto& current : currentState) {
        if (!current.present ||
            !EqualsInsensitive(
                current.packageId,
                expected.packageId)) {
            continue;
        }

        ++matches;
        exact =
            current.transactionId ==
            expected.transactionId;
    }

    if (!expected.present) {
        return matches == 0;
    }

    return matches == 1 && exact;
}

bool OtherIdentityAbsent(
    const AddonTransactionStateMarker& expected,
    const AddonTransactionStateMarker& other,
    const std::vector<AddonTransactionStateMarker>& currentState) {
    if (EqualsInsensitive(
            expected.packageId,
            other.packageId)) {
        return true;
    }

    return std::none_of(
        currentState.begin(),
        currentState.end(),
        [&](const AddonTransactionStateMarker& current) {
            return
                current.present &&
                EqualsInsensitive(
                    current.packageId,
                    other.packageId);
        });
}

} // namespace

bool BuildAddonInstallPlan(
    const std::filesystem::path& wowRoot,
    std::wstring_view packageId,
    const std::filesystem::path& extractedRoot,
    const std::vector<AddonCandidate>& candidates,
    const std::vector<std::wstring>& priorInstalledFiles,
    const std::vector<std::wstring>& otherInstalledFiles,
    AddonInstallPlan& plan,
    std::wstring& error) {
    plan = {};
    error.clear();

    if (wowRoot.empty() ||
        packageId.empty() ||
        extractedRoot.empty()) {
        error =
            L"Install planning requires WoW root, package id, and extracted archive root.";
        return false;
    }

    if (candidates.empty()) {
        error =
            L"The staged archive contains no addon roots to install.";
        return false;
    }

    std::set<std::wstring, InsensitiveLess> priorRoots;
    std::set<std::wstring, InsensitiveLess> otherRoots;
    if (!CollectOwnedRoots(
            priorInstalledFiles,
            priorRoots,
            error) ||
        !CollectOwnedRoots(
            otherInstalledFiles,
            otherRoots,
            error)) {
        return false;
    }

    for (const auto& root : priorRoots) {
        if (otherRoots.contains(root)) {
            error =
                L"Package ownership overlaps another managed package at addon root " +
                root +
                L".";
            return false;
        }
    }

    plan.packageId = std::wstring(packageId);
    plan.wowRoot = wowRoot;
    plan.addOnsRoot =
        wowRoot /
        L"Interface" /
        L"AddOns";
    plan.transactionRoot =
        wowRoot /
        L"Interface" /
        L"TocPilot" /
        L"transactions" /
        TransactionKey(packageId);

    std::set<std::wstring, InsensitiveLess> desiredRoots;
    std::set<std::wstring, InsensitiveLess> desiredFiles;
    const auto preparedRoot =
        plan.transactionRoot /
        L"prepared";

    for (const auto& candidate : candidates) {
        if (!SafeInstallFolderName(
                candidate.installFolder,
                error)) {
            return false;
        }

        if (!desiredRoots.insert(
                candidate.installFolder).second) {
            error =
                L"The archive maps more than one addon root to " +
                candidate.installFolder +
                L".";
            return false;
        }

        if (otherRoots.contains(
                candidate.installFolder)) {
            error =
                L"Addon root " +
                candidate.installFolder +
                L" is owned by another TocPilot package.";
            return false;
        }

        if (!SafeSourceRelativePath(
                candidate.sourceRelativePath)) {
            error =
                L"Addon candidate has an unsafe staged source path.";
            return false;
        }

        const auto source =
            extractedRoot /
            candidate.sourceRelativePath;

        std::error_code ec;
        const auto sourceStatus =
            std::filesystem::symlink_status(
                source,
                ec);
        if (ec ||
            std::filesystem::is_symlink(sourceStatus) ||
            !std::filesystem::is_directory(sourceStatus)) {
            error =
                L"Addon candidate source is missing or unsafe: " +
                source.wstring();
            return false;
        }

        bool hasToc = false;
        std::size_t regularFiles = 0;

        std::filesystem::recursive_directory_iterator it(
            source,
            std::filesystem::directory_options::none,
            ec);
        const std::filesystem::recursive_directory_iterator end;
        if (ec) {
            error =
                L"Could not enumerate addon candidate " +
                candidate.installFolder +
                L".";
            return false;
        }

        for (; it != end; it.increment(ec)) {
            if (ec) {
                error =
                    L"Could not enumerate addon candidate files.";
                return false;
            }

            const auto status =
                std::filesystem::symlink_status(
                    it->path(),
                    ec);
            if (ec) {
                error =
                    L"Could not inspect staged addon file.";
                return false;
            }

            if (std::filesystem::is_symlink(status)) {
                error =
                    L"Addon candidate contains a symbolic link.";
                return false;
            }

            if (std::filesystem::is_directory(status)) {
                continue;
            }

            if (!std::filesystem::is_regular_file(status)) {
                error =
                    L"Addon candidate contains an unsupported filesystem object.";
                return false;
            }

            const auto relative =
                it->path().lexically_relative(source);
            if (relative.empty() ||
                !SafeSourceRelativePath(relative)) {
                error =
                    L"Addon candidate contains an unsafe relative file path.";
                return false;
            }

            if (EqualsInsensitive(
                    relative.extension().wstring(),
                    L".toc")) {
                hasToc = true;
            }

            const std::wstring owned =
                OwnedFilePath(
                    candidate.installFolder,
                    relative);

            if (!desiredFiles.insert(owned).second) {
                error =
                    L"Addon install plan contains a case-insensitive file collision.";
                return false;
            }

            ++regularFiles;
        }

        if (!hasToc || regularFiles == 0) {
            error =
                L"Addon root " +
                candidate.installFolder +
                L" has no installable .toc package.";
            return false;
        }

        const auto live =
            plan.addOnsRoot /
            candidate.installFolder;

        bool liveExists = false;
        if (!RootExists(
                live,
                liveExists,
                error)) {
            return false;
        }

        if (liveExists &&
            !priorRoots.contains(
                candidate.installFolder)) {
            error =
                L"Refusing to replace existing unowned addon folder " +
                candidate.installFolder +
                L".";
            return false;
        }

        AddonInstallRoot root;
        root.sourceDirectory = source;
        root.installFolder =
            candidate.installFolder;
        root.liveDirectory = live;
        root.preparedDirectory =
            preparedRoot /
            candidate.installFolder;
        root.backupDirectory =
            BackupPath(
                plan,
                candidate.installFolder);
        plan.roots.push_back(std::move(root));
    }

    for (const auto& prior : priorRoots) {
        if (!desiredRoots.contains(prior)) {
            plan.obsoleteInstallFolders.push_back(prior);
        }
    }

    plan.desiredInstalledFiles.assign(
        desiredFiles.begin(),
        desiredFiles.end());

    std::sort(
        plan.roots.begin(),
        plan.roots.end(),
        [](const AddonInstallRoot& left,
           const AddonInstallRoot& right) {
            return InsensitiveLess{}(
                left.installFolder,
                right.installFolder);
        });
    std::sort(
        plan.obsoleteInstallFolders.begin(),
        plan.obsoleteInstallFolders.end(),
        InsensitiveLess{});

    return true;
}

bool BuildAddonRemovalPlan(
    const std::filesystem::path& wowRoot,
    std::wstring_view packageId,
    const std::vector<std::wstring>& installedFiles,
    const std::vector<std::wstring>& otherInstalledFiles,
    AddonInstallPlan& plan,
    std::wstring& error) {
    plan = {};
    error.clear();

    if (wowRoot.empty() || packageId.empty()) {
        error =
            L"Removal planning requires WoW root and package id.";
        return false;
    }

    if (installedFiles.empty()) {
        error =
            L"The selected package has no recorded installed files to remove.";
        return false;
    }

    std::set<std::wstring, InsensitiveLess> ownedRoots;
    std::set<std::wstring, InsensitiveLess> otherRoots;
    if (!CollectOwnedRoots(
            installedFiles,
            ownedRoots,
            error) ||
        !CollectOwnedRoots(
            otherInstalledFiles,
            otherRoots,
            error)) {
        return false;
    }

    if (ownedRoots.empty()) {
        error =
            L"The selected package has no owned addon roots to remove.";
        return false;
    }

    for (const auto& root : ownedRoots) {
        if (otherRoots.contains(root)) {
            error =
                L"Refusing to remove addon root " +
                root +
                L" because another TocPilot package also records ownership there.";
            return false;
        }
    }

    plan.packageId = std::wstring(packageId);
    plan.wowRoot = wowRoot;
    plan.addOnsRoot =
        wowRoot /
        L"Interface" /
        L"AddOns";
    plan.transactionRoot =
        wowRoot /
        L"Interface" /
        L"TocPilot" /
        L"transactions" /
        TransactionKey(packageId);
    plan.obsoleteInstallFolders.assign(
        ownedRoots.begin(),
        ownedRoots.end());

    return true;
}

bool PrepareAddonInstallTransaction(
    const AddonInstallPlan& plan,
    AddonInstallTransaction& transaction,
    std::wstring& error) {
    transaction = {};
    error.clear();

    const bool hasInstallRoots =
        !plan.roots.empty();
    const bool hasRemovalRoots =
        !plan.obsoleteInstallFolders.empty();

    if ((!hasInstallRoots && !hasRemovalRoots) ||
        (hasInstallRoots &&
         plan.desiredInstalledFiles.empty()) ||
        plan.transactionRoot.empty()) {
        error =
            L"Install transaction plan is incomplete.";
        return false;
    }

    std::error_code ec;
    const bool transactionExists =
        std::filesystem::exists(
            plan.transactionRoot,
            ec);
    if (ec) {
        error =
            L"Could not inspect install transaction directory.";
        return false;
    }
    if (transactionExists) {
        error =
            L"An unfinished TocPilot install transaction already exists for this package.";
        return false;
    }

    const auto preparedRoot =
        plan.transactionRoot /
        L"prepared";
    const auto backupRoot =
        plan.transactionRoot /
        L"backup";

    std::filesystem::create_directories(
        preparedRoot,
        ec);
    if (ec) {
        error =
            L"Could not create install transaction staging directory.";
        return false;
    }

    std::filesystem::create_directories(
        backupRoot,
        ec);
    if (ec) {
        std::wstring cleanupError;
        RemoveAllWithRetries(
            plan.transactionRoot,
            cleanupError);
        error =
            L"Could not create install transaction backup directory.";
        return false;
    }

    for (const auto& root : plan.roots) {
        if (!CopyTree(
                root.sourceDirectory,
                root.preparedDirectory,
                error)) {
            std::wstring cleanupError;
            RemoveAllWithRetries(
                plan.transactionRoot,
                cleanupError);
            return false;
        }
    }

    transaction.plan = plan;
    transaction.prepared = true;
    return true;
}

bool CommitAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error,
    const AddonInstallOptions& options) {
    error.clear();

    if (!transaction.prepared ||
        transaction.active ||
        (transaction.plan.roots.empty() &&
         transaction.plan.obsoleteInstallFolders.empty())) {
        error =
            L"Install transaction is not ready to commit.";
        return false;
    }

    auto& plan = transaction.plan;
    std::error_code ec;

    std::filesystem::create_directories(
        plan.addOnsRoot,
        ec);
    if (ec) {
        error =
            L"Could not create Interface\\AddOns.";
        std::wstring cleanupError;
        RollbackAddonInstallTransaction(
            transaction,
            cleanupError);
        return false;
    }

    transaction.active = true;

    std::vector<std::wstring> rootsToBackup;
    for (const auto& root : plan.roots) {
        rootsToBackup.push_back(
            root.installFolder);
    }
    rootsToBackup.insert(
        rootsToBackup.end(),
        plan.obsoleteInstallFolders.begin(),
        plan.obsoleteInstallFolders.end());
    std::sort(
        rootsToBackup.begin(),
        rootsToBackup.end(),
        InsensitiveLess{});
    rootsToBackup.erase(
        std::unique(
            rootsToBackup.begin(),
            rootsToBackup.end(),
            [](const std::wstring& left,
               const std::wstring& right) {
                return EqualsInsensitive(
                    left,
                    right);
            }),
        rootsToBackup.end());

    for (const auto& folder : rootsToBackup) {
        const auto live =
            plan.addOnsRoot /
            folder;

        bool exists = false;
        if (!RootExists(
                live,
                exists,
                error)) {
            const std::wstring commitError = error;
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        if (!exists) {
            continue;
        }

        const auto backup =
            BackupPath(plan, folder);
        std::filesystem::create_directories(
            backup.parent_path(),
            ec);
        if (ec) {
            const std::wstring commitError =
                L"Could not create install rollback directory.";
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        if (!RenameWithRetries(
                live,
                backup,
                error)) {
            const std::wstring commitError = error;
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        transaction.backedUpRoots.push_back(
            folder);

        if (transaction.backedUpRoots.size() >=
                options.failAfterRootBackups) {
            const std::wstring commitError =
                L"Injected install failure after live root backup.";
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }
    }

    for (const auto& root : plan.roots) {
        if (!RenameWithRetries(
                root.preparedDirectory,
                root.liveDirectory,
                error)) {
            const std::wstring commitError = error;
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }

        transaction.installedRoots.push_back(
            root.installFolder);

        if (transaction.installedRoots.size() >=
                options.failAfterNewRootCommits) {
            const std::wstring commitError =
                L"Injected install failure after live root commit.";
            std::wstring rollbackError;
            if (!RollbackAddonInstallTransaction(
                    transaction,
                    rollbackError)) {
                error =
                    commitError +
                    L" Rollback also failed: " +
                    rollbackError;
            } else {
                error = commitError;
            }
            return false;
        }
    }

    return true;
}

bool BeginAddonInstallTransaction(
    const AddonInstallPlan& plan,
    AddonInstallTransaction& transaction,
    std::wstring& error,
    const AddonInstallOptions& options) {
    if (!PrepareAddonInstallTransaction(
            plan,
            transaction,
            error)) {
        return false;
    }

    if (!CommitAddonInstallTransaction(
            transaction,
            error,
            options)) {
        std::wstring cleanupError;
        RollbackAddonInstallTransaction(
            transaction,
            cleanupError);
        return false;
    }

    return true;
}

bool RollbackAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.prepared) {
        return true;
    }

    if (transaction.active) {
        if (!RestoreBackups(
                transaction,
                error)) {
            return false;
        }

        transaction.active = false;
        transaction.backedUpRoots.clear();
        transaction.installedRoots.clear();
    }

    std::wstring cleanupError;
    if (!RemoveAllWithRetries(
            transaction.plan.transactionRoot,
            cleanupError)) {
        error = std::move(cleanupError);
        return false;
    }

    transaction.prepared = false;
    return true;
}

bool FinalizeAddonInstallTransaction(
    AddonInstallTransaction& transaction,
    std::wstring& error) {
    error.clear();

    if (!transaction.prepared) {
        return true;
    }

    // Once package state is saved, the new live files are authoritative.
    // Cleanup failure must never cause a later destructor/path to restore
    // the old backup over the committed installation.
    transaction.active = false;
    transaction.backedUpRoots.clear();
    transaction.installedRoots.clear();

    if (!RemoveAllWithRetries(
            transaction.plan.transactionRoot,
            error)) {
        return false;
    }

    transaction.prepared = false;
    return true;
}

} // namespace tp
