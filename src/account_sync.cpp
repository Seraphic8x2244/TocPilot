#include "account_sync.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <regex>
#include <system_error>
#include <utility>

namespace tp {
namespace {

struct PfUiNormalizedData {
    std::string profiles;
    std::string cache;
};

struct PfUiNode {
    std::string key;
    bool table = false;
    std::string value;
    std::vector<PfUiNode> children;
    std::size_t startLine = 0;
    std::size_t endLine = 0;
};

struct PfUiRoot {
    std::string name;
    std::vector<PfUiNode> children;
    std::size_t startLine = 0;
    std::size_t endLine = 0;
};

struct PfUiDocument {
    std::vector<std::string> lines;
    std::vector<PfUiRoot> roots;
    std::string newline = "\n";
    bool hadBom = false;
    bool endsWithNewline = true;
};

struct AccountCopy {
    std::wstring account;
    std::size_t index = 0;
    std::filesystem::path path;
    bool exists = false;
    std::filesystem::file_time_type modified{};
    std::string bytes;
};

struct PfUiFilePlan {
    AccountCopy copy;
    PfUiDocument document;
    bool profileSync = false;
    bool cacheMerge = false;
};

struct PfUiSyncPlan {
    bool noSource = false;
    std::size_t sourceFile = 0;
    AccountCopy source;
    std::vector<PfUiFilePlan> files;
    std::array<PfUiNode, 3> mergedCache{};
    std::array<bool, 3> mergedCachePresent{};
};

struct ItemAnalysis {
    AccountSyncItem item = AccountSyncItem::Macros;
    bool noSource = false;
    bool comparerFallback = false;
    AccountCopy source;
    std::vector<AccountCopy> automaticTargets;
    std::vector<AccountCopy> confirmationTargets;
};

std::size_t ItemIndex(AccountSyncItem item) {
    return static_cast<std::size_t>(item);
}

std::wstring FilesystemError(
    const std::error_code& error) {
    if (!error) {
        return L"Unknown filesystem error.";
    }

    const std::string message =
        error.message();

    if (message.empty()) {
        return
            L"Filesystem error " +
            std::to_wstring(error.value()) +
            L".";
    }

    const int required =
        MultiByteToWideChar(
            CP_UTF8,
            0,
            message.c_str(),
            static_cast<int>(message.size()),
            nullptr,
            0);

    if (required <= 0) {
        return
            L"Filesystem error " +
            std::to_wstring(error.value()) +
            L".";
    }

    std::wstring wide(
        static_cast<std::size_t>(required),
        L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        message.c_str(),
        static_cast<int>(message.size()),
        wide.data(),
        required);

    return wide;
}

bool EqualsInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < left.size();
         ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }

    return true;
}

bool LessInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    const std::size_t count =
        std::min(
            left.size(),
            right.size());

    for (std::size_t i = 0;
         i < count;
         ++i) {
        const wchar_t a =
            static_cast<wchar_t>(
                std::towlower(left[i]));
        const wchar_t b =
            static_cast<wchar_t>(
                std::towlower(right[i]));

        if (a < b) {
            return true;
        }
        if (a > b) {
            return false;
        }
    }

    return left.size() < right.size();
}

bool SafeAccountName(
    std::wstring_view account) {
    if (account.empty() ||
        account == L"." ||
        account == L".." ||
        account.find(L'\\') !=
            std::wstring::npos ||
        account.find(L'/') !=
            std::wstring::npos ||
        account.find(L':') !=
            std::wstring::npos) {
        return false;
    }

    return
        std::filesystem::path(
            std::wstring(account))
            .filename()
            .wstring() ==
        account;
}

bool NormalizeAccounts(
    const std::vector<std::wstring>& input,
    std::vector<std::wstring>& output,
    std::wstring& error) {
    output.clear();

    for (const auto& account :
         input) {
        if (!SafeAccountName(account)) {
            error =
                L"Account Sync contains an unsafe account name: " +
                account;
            return false;
        }

        const bool duplicate =
            std::any_of(
                output.begin(),
                output.end(),
                [&](const std::wstring& value) {
                    return
                        EqualsInsensitive(
                            value,
                            account);
                });

        if (!duplicate) {
            output.push_back(account);
        }
    }

    return true;
}

std::filesystem::path ItemRelativePath(
    AccountSyncItem item) {
    switch (item) {
    case AccountSyncItem::Macros:
        return L"macros-cache.txt";
    case AccountSyncItem::Keybindings:
        return L"bindings-cache.wtf";
    case AccountSyncItem::PfUi:
        return
            std::filesystem::path(
                L"SavedVariables") /
            L"pfUI.lua";
    }

    return {};
}

std::filesystem::path AccountItemPath(
    const std::filesystem::path& wowRoot,
    std::wstring_view account,
    AccountSyncItem item) {
    return
        wowRoot /
        L"WTF" /
        L"Account" /
        std::wstring(account) /
        ItemRelativePath(item);
}

std::string Trim(
    std::string value) {
    const auto whitespace =
        [](unsigned char value) {
            return
                value == ' ' ||
                value == '\t' ||
                value == '\r' ||
                value == '\n' ||
                value == '\f' ||
                value == '\v';
        };

    while (!value.empty() &&
           whitespace(
               static_cast<unsigned char>(
                   value.front()))) {
        value.erase(value.begin());
    }

    while (!value.empty() &&
           whitespace(
               static_cast<unsigned char>(
                   value.back()))) {
        value.pop_back();
    }

    return value;
}

std::string LowerAscii(
    std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            if (ch >= 'A' &&
                ch <= 'Z') {
                return
                    static_cast<char>(
                        ch - 'A' + 'a');
            }
            return
                static_cast<char>(ch);
        });

    return value;
}

std::string JoinStack(
    const std::vector<std::string>& stack) {
    std::string result;

    for (std::size_t i = 0;
         i < stack.size();
         ++i) {
        if (i != 0) {
            result.push_back(
                static_cast<char>(31));
        }
        result += stack[i];
    }

    return result;
}

const std::array<const char*, 3> kPfUiMergeCacheKeys{
    "libhealth",
    "gold",
    "prediction"
};

std::string PfUiKeyToken(
    const std::smatch& match) {
    return
        match[1].matched
            ? "string:" +
                match[1].str()
            : "number:" +
                match[2].str();
}

bool PfUiNodeKeyExists(
    const std::vector<PfUiNode>& nodes,
    const std::string& key) {
    return
        std::any_of(
            nodes.begin(),
            nodes.end(),
            [&](const PfUiNode& node) {
                return node.key == key;
            });
}

bool ParsePfUiChildren(
    const std::vector<std::string>& lines,
    std::size_t& index,
    std::vector<PfUiNode>& children,
    std::wstring& error) {
    static const std::regex tablePattern(
        R"tp(^\[(?:"((?:[^"\\]|\\.)*)"|(\d+))\]\s*=\s*\{$)tp");
    static const std::regex valuePattern(
        R"tp(^\[(?:"((?:[^"\\]|\\.)*)"|(\d+))\]\s*=\s*(.+),$)tp");

    while (index < lines.size()) {
        const std::string line =
            Trim(lines[index]);

        if (line.empty()) {
            ++index;
            continue;
        }

        if (line == "}," ||
            line == "}") {
            ++index;
            return true;
        }

        std::smatch match;

        if (std::regex_match(
                line,
                match,
                tablePattern)) {
            PfUiNode node;
            node.key =
                PfUiKeyToken(match);
            node.table = true;
            node.startLine = index;

            if (PfUiNodeKeyExists(
                    children,
                    node.key)) {
                error =
                    L"pfUI contains a duplicate table key in one scope.";
                return false;
            }

            ++index;

            if (!ParsePfUiChildren(
                    lines,
                    index,
                    node.children,
                    error)) {
                return false;
            }

            node.endLine =
                index - 1;
            children.push_back(
                std::move(node));
            continue;
        }

        if (std::regex_match(
                line,
                match,
                valuePattern)) {
            PfUiNode node;
            node.key =
                PfUiKeyToken(match);
            node.table = false;
            node.value =
                Trim(match[3].str());
            node.startLine = index;
            node.endLine = index;

            if (PfUiNodeKeyExists(
                    children,
                    node.key)) {
                error =
                    L"pfUI contains a duplicate value key in one scope.";
                return false;
            }

            children.push_back(
                std::move(node));
            ++index;
            continue;
        }

        error =
            L"pfUI parser encountered an unexpected line format.";
        return false;
    }

    error =
        L"pfUI parser reached the end of the file before a table closed.";
    return false;
}

bool ParsePfUiDocument(
    PfUiDocument& document,
    std::wstring& error) {
    document.roots.clear();

    static const std::regex rootPattern(
        R"(^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\{$)");

    std::size_t index = 0;

    while (index <
           document.lines.size()) {
        const std::string line =
            Trim(document.lines[index]);

        if (line.empty()) {
            ++index;
            continue;
        }

        std::smatch match;

        if (!std::regex_match(
                line,
                match,
                rootPattern)) {
            error =
                L"pfUI parser encountered an unexpected top-level line.";
            return false;
        }

        PfUiRoot root;
        root.name =
            match[1].str();
        root.startLine = index;

        const bool duplicateRoot =
            std::any_of(
                document.roots.begin(),
                document.roots.end(),
                [&](const PfUiRoot& value) {
                    return
                        value.name ==
                        root.name;
                });

        if (duplicateRoot) {
            error =
                L"pfUI contains a duplicate top-level table.";
            return false;
        }

        ++index;

        if (!ParsePfUiChildren(
                document.lines,
                index,
                root.children,
                error)) {
            return false;
        }

        root.endLine =
            index - 1;
        document.roots.push_back(
            std::move(root));
    }

    return true;
}

bool ReadPfUiDocument(
    const std::filesystem::path& path,
    PfUiDocument& document,
    std::wstring& error) {
    document = {};

    std::ifstream stream(
        path,
        std::ios::binary);

    if (!stream) {
        error =
            L"Could not open pfUI file: " +
            path.wstring();
        return false;
    }

    stream.seekg(
        0,
        std::ios::end);
    const std::streamoff size =
        stream.tellg();

    if (size < 0) {
        error =
            L"Could not determine pfUI file size: " +
            path.wstring();
        return false;
    }

    stream.seekg(
        0,
        std::ios::beg);

    std::string bytes(
        static_cast<std::size_t>(size),
        '\0');

    if (!bytes.empty()) {
        stream.read(
            bytes.data(),
            static_cast<std::streamsize>(
                bytes.size()));
    }

    if (!stream) {
        error =
            L"Could not read pfUI file: " +
            path.wstring();
        return false;
    }

    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(
            bytes[0]) == 0xEF &&
        static_cast<unsigned char>(
            bytes[1]) == 0xBB &&
        static_cast<unsigned char>(
            bytes[2]) == 0xBF) {
        document.hadBom = true;
        bytes.erase(0, 3);
    }

    document.newline =
        bytes.find("\r\n") !=
                std::string::npos
            ? "\r\n"
            : "\n";
    document.endsWithNewline =
        !bytes.empty() &&
        bytes.back() == '\n';

    std::size_t start = 0;
    while (start < bytes.size()) {
        const std::size_t end =
            bytes.find(
                '\n',
                start);

        std::string line =
            end == std::string::npos
                ? bytes.substr(start)
                : bytes.substr(
                    start,
                    end - start);

        if (!line.empty() &&
            line.back() == '\r') {
            line.pop_back();
        }

        document.lines.push_back(
            std::move(line));

        if (end ==
            std::string::npos) {
            break;
        }

        start = end + 1;
    }

    return
        ParsePfUiDocument(
            document,
            error);
}

const PfUiRoot* FindPfUiRoot(
    const PfUiDocument& document,
    std::string_view name) {
    const auto it =
        std::find_if(
            document.roots.begin(),
            document.roots.end(),
            [&](const PfUiRoot& root) {
                return
                    root.name ==
                    name;
            });

    return
        it == document.roots.end()
            ? nullptr
            : &*it;
}

const PfUiNode* FindPfUiChild(
    const std::vector<PfUiNode>& children,
    std::string_view key) {
    const auto it =
        std::find_if(
            children.begin(),
            children.end(),
            [&](const PfUiNode& child) {
                return
                    child.key ==
                    key;
            });

    return
        it == children.end()
            ? nullptr
            : &*it;
}

int SupportedPfUiCacheIndex(
    std::string_view key) {
    for (std::size_t i = 0;
         i < kPfUiMergeCacheKeys.size();
         ++i) {
        if (key ==
            std::string("string:") +
                kPfUiMergeCacheKeys[i]) {
            return
                static_cast<int>(i);
        }
    }

    return -1;
}

const PfUiNode* SupportedPfUiCacheNode(
    const PfUiDocument& document,
    std::size_t cacheIndex,
    std::wstring& error) {
    const PfUiRoot* cache =
        FindPfUiRoot(
            document,
            "pfUI_cache");

    if (!cache) {
        return nullptr;
    }

    const std::string key =
        std::string("string:") +
        kPfUiMergeCacheKeys[
            cacheIndex];

    const PfUiNode* node =
        FindPfUiChild(
            cache->children,
            key);

    if (node &&
        !node->table) {
        error =
            L"pfUI cache section '" +
            std::wstring(
                kPfUiMergeCacheKeys[
                    cacheIndex],
                kPfUiMergeCacheKeys[
                    cacheIndex] +
                    std::char_traits<char>::
                        length(
                            kPfUiMergeCacheKeys[
                                cacheIndex])) +
            L"' is not a table.";
        return nullptr;
    }

    return node;
}

void AppendPfUiNodeSignature(
    const PfUiNode& node,
    std::string& output) {
    output += node.key;

    if (!node.table) {
        output += "=";
        output += node.value;
        output += ";";
        return;
    }

    output += "{";

    std::vector<const PfUiNode*> sorted;
    sorted.reserve(
        node.children.size());

    for (const auto& child :
         node.children) {
        sorted.push_back(
            &child);
    }

    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const PfUiNode* left,
           const PfUiNode* right) {
            return
                left->key <
                right->key;
        });

    for (const auto* child :
         sorted) {
        AppendPfUiNodeSignature(
            *child,
            output);
    }

    output += "}";
}

std::string PfUiRootSignature(
    const PfUiRoot* root) {
    if (!root) {
        return "<absent>";
    }

    PfUiNode wrapper;
    wrapper.key =
        "root:" +
        root->name;
    wrapper.table = true;
    wrapper.children =
        root->children;

    std::string output;
    AppendPfUiNodeSignature(
        wrapper,
        output);
    return output;
}

bool PfUiCacheSignature(
    const PfUiDocument& document,
    std::string& output,
    std::wstring& error) {
    output.clear();

    for (std::size_t i = 0;
         i < kPfUiMergeCacheKeys.size();
         ++i) {
        const PfUiNode* node =
            SupportedPfUiCacheNode(
                document,
                i,
                error);

        if (!error.empty()) {
            return false;
        }

        output +=
            kPfUiMergeCacheKeys[i];
        output += ":";

        if (!node) {
            output += "<absent>;";
            continue;
        }

        AppendPfUiNodeSignature(
            *node,
            output);
        output += ";";
    }

    return true;
}

bool NormalizePfUi(
    const std::filesystem::path& path,
    PfUiNormalizedData& data,
    std::wstring& error) {
    data = {};
    error.clear();

    PfUiDocument document;

    if (!ReadPfUiDocument(
            path,
            document,
            error)) {
        return false;
    }

    data.profiles =
        PfUiRootSignature(
            FindPfUiRoot(
                document,
                "pfUI_profiles"));

    return
        PfUiCacheSignature(
            document,
            data.cache,
            error);
}

void MergePfUiTable(
    PfUiNode& destination,
    const PfUiNode& incoming) {
    for (const auto& child :
         incoming.children) {
        auto it =
            std::find_if(
                destination.children.begin(),
                destination.children.end(),
                [&](const PfUiNode& existing) {
                    return
                        existing.key ==
                        child.key;
                });

        if (it ==
            destination.children.end()) {
            destination.children.push_back(
                child);
            continue;
        }

        if (it->table &&
            child.table) {
            MergePfUiTable(
                *it,
                child);
            continue;
        }

        *it = child;
    }
}

std::string PfUiKeyExpression(
    const std::string& key) {
    static constexpr
        std::string_view stringPrefix =
            "string:";
    static constexpr
        std::string_view numberPrefix =
            "number:";

    if (key.rfind(
            stringPrefix,
            0) == 0) {
        return
            "[\"" +
            key.substr(
                stringPrefix.size()) +
            "\"]";
    }

    if (key.rfind(
            numberPrefix,
            0) == 0) {
        return
            "[" +
            key.substr(
                numberPrefix.size()) +
            "]";
    }

    return "[]";
}

std::string LeadingWhitespace(
    const std::string& line) {
    std::size_t count = 0;

    while (count < line.size() &&
           (line[count] == ' ' ||
            line[count] == '\t')) {
        ++count;
    }

    return
        line.substr(
            0,
            count);
}

void RenderPfUiNode(
    const PfUiNode& node,
    const std::string& indent,
    std::vector<std::string>& output) {
    const std::string key =
        PfUiKeyExpression(
            node.key);

    if (!node.table) {
        output.push_back(
            indent +
            key +
            " = " +
            node.value +
            ",");
        return;
    }

    output.push_back(
        indent +
        key +
        " = {");

    std::vector<const PfUiNode*> sorted;
    sorted.reserve(
        node.children.size());

    for (const auto& child :
         node.children) {
        sorted.push_back(
            &child);
    }

    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const PfUiNode* left,
           const PfUiNode* right) {
            return
                left->key <
                right->key;
        });

    for (const auto* child :
         sorted) {
        RenderPfUiNode(
            *child,
            indent + "\t",
            output);
    }

    output.push_back(
        indent + "},");
}

std::vector<std::string> PfUiRootRawLines(
    const PfUiDocument& document,
    const PfUiRoot* root) {
    if (!root) {
        return {};
    }

    return
        std::vector<std::string>(
            document.lines.begin() +
                static_cast<std::ptrdiff_t>(
                    root->startLine),
            document.lines.begin() +
                static_cast<std::ptrdiff_t>(
                    root->endLine + 1));
}

std::vector<std::string> RenderMergedPfUiCache(
    const PfUiDocument& target,
    const PfUiSyncPlan& plan) {
    const PfUiRoot* cache =
        FindPfUiRoot(
            target,
            "pfUI_cache");

    std::vector<std::string> output;

    if (!cache) {
        bool any = false;
        for (bool present :
             plan.mergedCachePresent) {
            any =
                any ||
                present;
        }

        if (!any) {
            return output;
        }

        output.push_back(
            "pfUI_cache = {");

        for (std::size_t i = 0;
             i < plan.mergedCache.size();
             ++i) {
            if (!plan.mergedCachePresent[i]) {
                continue;
            }

            RenderPfUiNode(
                plan.mergedCache[i],
                "\t",
                output);
        }

        output.push_back("}");
        return output;
    }

    output.push_back(
        target.lines[
            cache->startLine]);

    std::array<bool, 3> rendered{};
    std::size_t cursor =
        cache->startLine + 1;

    std::string defaultIndent = "\t";
    if (!cache->children.empty()) {
        defaultIndent =
            LeadingWhitespace(
                target.lines[
                    cache->children
                        .front()
                        .startLine]);
    }

    for (const auto& child :
         cache->children) {
        while (cursor <
               child.startLine) {
            output.push_back(
                target.lines[
                    cursor++]);
        }

        const int supported =
            SupportedPfUiCacheIndex(
                child.key);

        if (supported >= 0) {
            const std::size_t index =
                static_cast<std::size_t>(
                    supported);
            rendered[index] = true;

            if (plan.mergedCachePresent[
                    index]) {
                RenderPfUiNode(
                    plan.mergedCache[
                        index],
                    LeadingWhitespace(
                        target.lines[
                            child.startLine]),
                    output);
            }
        } else {
            for (std::size_t line =
                     child.startLine;
                 line <=
                     child.endLine;
                 ++line) {
                output.push_back(
                    target.lines[
                        line]);
            }
        }

        cursor =
            child.endLine + 1;
    }

    for (std::size_t i = 0;
         i < plan.mergedCache.size();
         ++i) {
        if (plan.mergedCachePresent[i] &&
            !rendered[i]) {
            RenderPfUiNode(
                plan.mergedCache[i],
                defaultIndent,
                output);
        }
    }

    while (cursor <=
           cache->endLine) {
        output.push_back(
            target.lines[
                cursor++]);
    }

    return output;
}

std::string SerializePfUiLines(
    const PfUiDocument& style,
    const std::vector<std::string>& lines) {
    std::string output;

    if (style.hadBom) {
        output +=
            "\xEF\xBB\xBF";
    }

    for (std::size_t i = 0;
         i < lines.size();
         ++i) {
        output += lines[i];

        if (i + 1 <
                lines.size() ||
            style.endsWithNewline) {
            output +=
                style.newline;
        }
    }

    return output;
}

bool BuildPfUiOutput(
    const PfUiSyncPlan& plan,
    const PfUiFilePlan& targetFile,
    bool replaceProfiles,
    bool mergeCache,
    std::string& output,
    std::wstring& error) {
    error.clear();

    const PfUiDocument& source =
        plan.files[
            plan.sourceFile]
            .document;

    PfUiDocument target =
        targetFile.document;

    if (!targetFile.copy.exists) {
        target.newline =
            source.newline;
        target.hadBom =
            source.hadBom;
        target.endsWithNewline =
            source.endsWithNewline;
    }

    const PfUiRoot* targetProfiles =
        FindPfUiRoot(
            target,
            "pfUI_profiles");
    const PfUiRoot* targetCache =
        FindPfUiRoot(
            target,
            "pfUI_cache");

    const PfUiRoot* sourceProfiles =
        FindPfUiRoot(
            source,
            "pfUI_profiles");

    const auto profileReplacement =
        replaceProfiles
            ? PfUiRootRawLines(
                source,
                sourceProfiles)
            : std::vector<std::string>{};
    const auto cacheReplacement =
        mergeCache
            ? RenderMergedPfUiCache(
                target,
                plan)
            : std::vector<std::string>{};

    std::vector<std::string> lines;
    std::size_t index = 0;
    bool profilesHandled = false;
    bool cacheHandled = false;

    while (index <
           target.lines.size()) {
        if (replaceProfiles &&
            targetProfiles &&
            index ==
                targetProfiles->startLine) {
            lines.insert(
                lines.end(),
                profileReplacement.begin(),
                profileReplacement.end());
            index =
                targetProfiles->endLine + 1;
            profilesHandled = true;
            continue;
        }

        if (mergeCache &&
            targetCache &&
            index ==
                targetCache->startLine) {
            lines.insert(
                lines.end(),
                cacheReplacement.begin(),
                cacheReplacement.end());
            index =
                targetCache->endLine + 1;
            cacheHandled = true;
            continue;
        }

        lines.push_back(
            target.lines[
                index++]);
    }

    if (replaceProfiles &&
        !profilesHandled &&
        !profileReplacement.empty()) {
        lines.insert(
            lines.end(),
            profileReplacement.begin(),
            profileReplacement.end());
    }

    if (mergeCache &&
        !cacheHandled &&
        !cacheReplacement.empty()) {
        lines.insert(
            lines.end(),
            cacheReplacement.begin(),
            cacheReplacement.end());
    }

    output =
        SerializePfUiLines(
            target,
            lines);
    return true;
}

bool InspectCopy(
    const std::filesystem::path& path,
    bool& exists,
    std::filesystem::file_time_type& modified,
    std::wstring& error) {
    std::error_code ec;

    exists =
        std::filesystem::is_regular_file(
            path,
            ec);

    if (ec ==
        std::errc::no_such_file_or_directory) {
        ec.clear();
        exists = false;
        return true;
    }

    if (ec) {
        error =
            L"Could not inspect Account Sync file '" +
            path.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    if (!exists) {
        return true;
    }

    modified =
        std::filesystem::last_write_time(
            path,
            ec);

    if (ec) {
        error =
            L"Could not read Account Sync modified time for '" +
            path.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    return true;
}


bool ReadSyncBytes(AccountCopy& copy, std::wstring& error) {
    if (!copy.exists) {
        return true;
    }

    std::ifstream file(copy.path, std::ios::binary);
    if (!file) {
        error = L"Could not read Account Sync contents for '" + copy.path.wstring() + L"'.";
        return false;
    }
    copy.bytes.assign(std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>());
    if (file.bad()) {
        error = L"Account Sync file read failed for '" + copy.path.wstring() + L"'.";
        return false;
    }
    return true;
}

bool SameSyncPlan(const ItemAnalysis& a, const ItemAnalysis& b) {
    if (a.item != b.item || a.noSource != b.noSource ||
        a.source.account != b.source.account ||
        a.source.exists != b.source.exists ||
        a.source.modified != b.source.modified ||
        a.source.bytes != b.source.bytes ||
        a.confirmationTargets.size() != b.confirmationTargets.size() ||
        a.automaticTargets.size() != b.automaticTargets.size()) {
        return false;
    }
    const auto equalCopy = [](const AccountCopy& x, const AccountCopy& y) {
        return x.account == y.account && x.exists == y.exists &&
               x.modified == y.modified && x.bytes == y.bytes;
    };
    for (std::size_t i = 0; i < a.confirmationTargets.size(); ++i) {
        if (!equalCopy(a.confirmationTargets[i], b.confirmationTargets[i])) return false;
    }
    for (std::size_t i = 0; i < a.automaticTargets.size(); ++i) {
        if (!equalCopy(a.automaticTargets[i], b.automaticTargets[i])) return false;
    }
    return true;
}

bool BuildPfUiSyncPlan(
    const std::filesystem::path& wowRoot,
    const std::vector<std::wstring>& accounts,
    PfUiSyncPlan& plan,
    std::wstring& error) {
    plan = {};
    error.clear();
    plan.files.reserve(
        accounts.size());

    std::vector<std::size_t> existing;

    for (std::size_t i = 0;
         i < accounts.size();
         ++i) {
        PfUiFilePlan file;
        file.copy.account =
            accounts[i];
        file.copy.index = i;
        file.copy.path =
            AccountItemPath(
                wowRoot,
                file.copy.account,
                AccountSyncItem::PfUi);

        if (!InspectCopy(
                file.copy.path,
                file.copy.exists,
                file.copy.modified,
                error)) {
            return false;
        }

        if (file.copy.exists) {
            std::wstring parseError;

            if (!ReadPfUiDocument(
                    file.copy.path,
                    file.document,
                    parseError)) {
                error =
                    L"Could not safely parse pfUI for account '" +
                    file.copy.account +
                    L"': " +
                    parseError;
                return false;
            }

            existing.push_back(i);
        }

        plan.files.push_back(
            std::move(file));
    }

    if (existing.empty()) {
        plan.noSource = true;
        return true;
    }

    std::sort(
        existing.begin(),
        existing.end(),
        [&](std::size_t left,
            std::size_t right) {
            const auto& a =
                plan.files[left].copy;
            const auto& b =
                plan.files[right].copy;

            if (a.modified !=
                b.modified) {
                return
                    a.modified >
                    b.modified;
            }

            return
                a.index <
                b.index;
        });

    plan.sourceFile =
        existing.front();
    plan.source =
        plan.files[
            plan.sourceFile]
            .copy;

    // Merge from lowest to highest precedence. Newer files therefore win
    // same-key conflicts. For equal timestamps, later selected accounts are
    // merged first so the earlier selected account wins, matching source
    // selection elsewhere in Account Sync.
    std::vector<std::size_t> precedence =
        existing;

    std::sort(
        precedence.begin(),
        precedence.end(),
        [&](std::size_t left,
            std::size_t right) {
            const auto& a =
                plan.files[left].copy;
            const auto& b =
                plan.files[right].copy;

            if (a.modified !=
                b.modified) {
                return
                    a.modified <
                    b.modified;
            }

            return
                a.index >
                b.index;
        });

    for (const std::size_t fileIndex :
         precedence) {
        auto& document =
            plan.files[
                fileIndex]
                .document;

        for (std::size_t cacheIndex = 0;
             cacheIndex <
                 kPfUiMergeCacheKeys.size();
             ++cacheIndex) {
            std::wstring cacheError;
            const PfUiNode* node =
                SupportedPfUiCacheNode(
                    document,
                    cacheIndex,
                    cacheError);

            if (!cacheError.empty()) {
                error =
                    L"Could not safely merge pfUI for account '" +
                    plan.files[
                        fileIndex]
                        .copy.account +
                    L"': " +
                    cacheError;
                return false;
            }

            if (!node) {
                continue;
            }

            if (!plan.mergedCachePresent[
                    cacheIndex]) {
                plan.mergedCache[
                    cacheIndex] =
                    *node;
                plan.mergedCachePresent[
                    cacheIndex] = true;
            } else {
                MergePfUiTable(
                    plan.mergedCache[
                        cacheIndex],
                    *node);
            }
        }
    }

    const PfUiDocument& sourceDocument =
        plan.files[
            plan.sourceFile]
            .document;
    const std::string sourceProfiles =
        PfUiRootSignature(
            FindPfUiRoot(
                sourceDocument,
                "pfUI_profiles"));

    std::string mergedCacheSignature;
    for (std::size_t cacheIndex = 0;
         cacheIndex <
             kPfUiMergeCacheKeys.size();
         ++cacheIndex) {
        mergedCacheSignature +=
            kPfUiMergeCacheKeys[
                cacheIndex];
        mergedCacheSignature += ":";

        if (!plan.mergedCachePresent[
                cacheIndex]) {
            mergedCacheSignature +=
                "<absent>;";
            continue;
        }

        AppendPfUiNodeSignature(
            plan.mergedCache[
                cacheIndex],
            mergedCacheSignature);
        mergedCacheSignature += ";";
    }

    for (std::size_t i = 0;
         i < plan.files.size();
         ++i) {
        auto& file =
            plan.files[i];

        if (!file.copy.exists) {
            file.profileSync = true;
            continue;
        }

        std::string cacheSignature;
        std::wstring cacheError;

        if (!PfUiCacheSignature(
                file.document,
                cacheSignature,
                cacheError)) {
            error =
                L"Could not safely compare pfUI for account '" +
                file.copy.account +
                L"': " +
                cacheError;
            return false;
        }

        file.cacheMerge =
            cacheSignature !=
            mergedCacheSignature;

        if (i ==
            plan.sourceFile) {
            continue;
        }

        if (!(file.copy.modified <
              plan.source.modified)) {
            continue;
        }

        const std::string profiles =
            PfUiRootSignature(
                FindPfUiRoot(
                    file.document,
                    "pfUI_profiles"));

        file.profileSync =
            profiles !=
            sourceProfiles;
    }

    return true;
}

void FillPfUiAnalysis(
    const PfUiSyncPlan& plan,
    ItemAnalysis& analysis) {
    analysis = {};
    analysis.item =
        AccountSyncItem::PfUi;
    analysis.noSource =
        plan.noSource;

    if (plan.noSource) {
        return;
    }

    analysis.source =
        plan.source;

    for (const auto& file :
         plan.files) {
        if (file.copy.exists &&
            file.cacheMerge) {
            analysis.automaticTargets
                .push_back(
                    file.copy);
        }

        if (!file.copy.exists ||
            file.profileSync) {
            analysis.confirmationTargets
                .push_back(
                    file.copy);
        }
    }
}

void SetDisabledResult(
    AccountSyncItem item,
    AccountSyncItemResult& result) {
    result = {};
    result.item = item;
    result.status = L"Disabled";
    result.action = L"—";
}

void SetNotConfiguredResult(
    AccountSyncItem item,
    AccountSyncItemResult& result) {
    result = {};
    result.item = item;
    result.status = L"Not configured";
    result.action =
        L"Select 2+ accounts";
}

bool AnalyzeItem(
    const std::filesystem::path& wowRoot,
    const std::vector<std::wstring>& accounts,
    AccountSyncItem item,
    ItemAnalysis& analysis,
    std::wstring& error) {
    analysis = {};
    analysis.item = item;

    if (item ==
        AccountSyncItem::PfUi) {
        PfUiSyncPlan plan;

        if (!BuildPfUiSyncPlan(
                wowRoot,
                accounts,
                plan,
                error)) {
            return false;
        }

        FillPfUiAnalysis(
            plan,
            analysis);
        return true;
    }

    std::vector<AccountCopy> copies;
    copies.reserve(accounts.size());

    for (std::size_t i = 0;
         i < accounts.size();
         ++i) {
        AccountCopy copy;
        copy.account = accounts[i];
        copy.index = i;
        copy.path =
            AccountItemPath(
                wowRoot,
                copy.account,
                item);

        if (!InspectCopy(
                copy.path,
                copy.exists,
                copy.modified,
                error)) {
            return false;
        }

        if (!ReadSyncBytes(copy, error)) {
            return false;
        }

        copies.push_back(
            std::move(copy));
    }

    std::vector<AccountCopy> existing;

    for (const auto& copy :
         copies) {
        if (copy.exists) {
            existing.push_back(copy);
        }
    }

    if (existing.empty()) {
        analysis.noSource = true;
        return true;
    }

    std::sort(
        existing.begin(),
        existing.end(),
        [](const AccountCopy& left,
           const AccountCopy& right) {
            if (left.modified !=
                right.modified) {
                return
                    left.modified >
                    right.modified;
            }

            return
                left.index <
                right.index;
        });

    analysis.source =
        existing.front();

    for (std::size_t i = 1;
         i < existing.size();
         ++i) {
        const auto& candidate =
            existing[i];

        if (!(candidate.modified <
              analysis.source.modified) ||
            candidate.bytes == analysis.source.bytes) {
            continue;
        }

        analysis.confirmationTargets
            .push_back(candidate);
    }

    for (const auto& copy :
         copies) {
        if (!copy.exists) {
            analysis.confirmationTargets
                .push_back(copy);
        }
    }

    return true;
}

void FillPreviewResult(
    const ItemAnalysis& analysis,
    AccountSyncItemResult& result) {
    result = {};
    result.item = analysis.item;

    if (analysis.noSource) {
        result.status = L"No source";
        result.action = L"Skipped";
        return;
    }

    result.sourceAccount =
        analysis.source.account;
    result.comparerFallback =
        analysis.comparerFallback;

    for (const auto& target :
         analysis.automaticTargets) {
        result.automaticTargets.push_back(
            target.account);
    }

    for (const auto& target :
         analysis.confirmationTargets) {
        result.confirmationTargets
            .push_back(
                target.account);
    }

    result.confirmationRequired =
        !analysis.confirmationTargets.empty();

    const std::size_t automatic =
        analysis.automaticTargets.size();
    const std::size_t confirmation =
        analysis.confirmationTargets.size();

    if (confirmation != 0) {
        result.status =
            L"Confirmation required";

        if (analysis.comparerFallback) {
            result.action =
                L"pfUI safe fallback; ";
        }

        result.action +=
            std::to_wstring(confirmation) +
            (confirmation == 1
                ? L" target"
                : L" targets");

        if (automatic != 0) {
            result.action +=
                L"; " +
                std::to_wstring(automatic) +
                L" cache auto";
        }

        return;
    }

    if (automatic != 0) {
        result.status = L"Cache differs";
        result.action =
            std::to_wstring(automatic) +
            (automatic == 1
                ? L" automatic target"
                : L" automatic targets");
        return;
    }

    result.status = analysis.item == AccountSyncItem::PfUi
        ? L"Up to date" : L"Already synced";
    result.action = L"No changes";
}

bool EnsureBackupRunFolder(
    const std::filesystem::path& wowRoot,
    std::filesystem::path& runFolder,
    std::wstring& error) {
    if (!runFolder.empty()) {
        return true;
    }

    const std::filesystem::path backupRoot =
        wowRoot /
        L"WTF" /
        L"tocpilot";

    std::error_code ec;

    std::filesystem::create_directories(
        backupRoot,
        ec);

    if (ec) {
        error =
            L"Could not create Account Sync backup root '" +
            backupRoot.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);

    wchar_t timestamp[32]{};

    swprintf_s(
        timestamp,
        L"%02u-%02u-%04u-%02u%02u",
        static_cast<unsigned>(now.wDay),
        static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wYear),
        static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute));

    std::filesystem::path candidate =
        backupRoot /
        timestamp;

    int suffix = 2;

    for (;;) {
        ec.clear();

        const bool exists =
            std::filesystem::exists(
                candidate,
                ec);

        if (ec) {
            error =
                L"Could not inspect Account Sync backup path '" +
                candidate.wstring() +
                L"': " +
                FilesystemError(ec);
            return false;
        }

        if (!exists) {
            break;
        }

        wchar_t suffixed[40]{};

        swprintf_s(
            suffixed,
            L"%s-%02d",
            timestamp,
            suffix++);

        candidate =
            backupRoot /
            suffixed;
    }

    std::filesystem::create_directory(
        candidate,
        ec);

    if (ec) {
        error =
            L"Could not create Account Sync backup run folder '" +
            candidate.wstring() +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    runFolder =
        std::move(candidate);
    return true;
}

bool CopyTarget(
    const std::filesystem::path& wowRoot,
    AccountSyncItem item,
    const AccountCopy& source,
    const AccountCopy& target,
    std::filesystem::path& backupRunFolder,
    std::wstring& error) {
    std::error_code ec;

    std::filesystem::create_directories(
        target.path.parent_path(),
        ec);

    if (ec) {
        error =
            L"Could not create destination folder for account '" +
            target.account +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    if (target.exists) {
        if (!EnsureBackupRunFolder(
                wowRoot,
                backupRunFolder,
                error)) {
            return false;
        }

        const auto backupPath =
            backupRunFolder /
            target.account /
            ItemRelativePath(item);

        std::filesystem::create_directories(
            backupPath.parent_path(),
            ec);

        if (ec) {
            error =
                L"Could not create backup folder for account '" +
                target.account +
                L"': " +
                FilesystemError(ec);
            return false;
        }

        ec.clear();

        const bool backupCopied =
            std::filesystem::copy_file(
                target.path,
                backupPath,
                std::filesystem::copy_options::
                    overwrite_existing,
                ec);

        if (ec ||
            !backupCopied) {
            error =
                L"Backup failed for account '" +
                target.account +
                L"'. The destination was not overwritten.";

            if (ec) {
                error +=
                    L" " +
                    FilesystemError(ec);
            }

            return false;
        }
    }

    ec.clear();

    const bool copied =
        std::filesystem::copy_file(
            source.path,
            target.path,
            std::filesystem::copy_options::
                overwrite_existing,
            ec);

    if (ec ||
        !copied) {
        error =
            L"Copy failed for account '" +
            target.account +
            L"'.";

        if (target.exists) {
            error +=
                L" The backup remains intact.";
        }

        if (ec) {
            error +=
                L" " +
                FilesystemError(ec);
        }

        return false;
    }

    return true;
}

void FinalizeRunItem(
    const ItemAnalysis& analysis,
    std::size_t automaticCopied,
    std::size_t confirmedCopied,
    std::size_t modifiedTargets,
    bool confirmationDeclined,
    AccountSyncItemResult& result) {
    if (result.fatal) {
        result.status = L"Error";
        result.action =
            L"Sync failed";
        return;
    }

    const std::size_t totalCopied =
        modifiedTargets;

    result.copiedTargets =
        modifiedTargets;
    result.confirmationDeclined =
        confirmationDeclined;

    if (confirmationDeclined) {
        if (automaticCopied != 0) {
            result.status =
                L"Partially synced";
            result.action =
                L"Cache auto; confirmation declined";
        } else {
            result.status = L"Skipped";
            result.action =
                L"Confirmation declined";
        }
        return;
    }

    if (totalCopied != 0) {
        result.status = L"Synced";

        if (automaticCopied != 0 &&
            confirmedCopied != 0) {
            result.action =
                L"Confirmed + automatic cache";
        } else if (confirmedCopied != 0) {
            result.action = L"Confirmed";
        } else {
            result.action =
                L"Automatic cache";
        }
        return;
    }

    FillPreviewResult(
        analysis,
        result);
}


bool BackupPfUiTarget(
    const std::filesystem::path& wowRoot,
    const AccountCopy& target,
    std::filesystem::path& backupRunFolder,
    std::wstring& error) {
    if (!target.exists) {
        return true;
    }

    if (!EnsureBackupRunFolder(
            wowRoot,
            backupRunFolder,
            error)) {
        return false;
    }

    const auto backupPath =
        backupRunFolder /
        target.account /
        ItemRelativePath(
            AccountSyncItem::PfUi);

    std::error_code ec;
    std::filesystem::create_directories(
        backupPath.parent_path(),
        ec);

    if (ec) {
        error =
            L"Could not create pfUI backup folder for account '" +
            target.account +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    const bool copied =
        std::filesystem::copy_file(
            target.path,
            backupPath,
            std::filesystem::copy_options::
                overwrite_existing,
            ec);

    if (ec ||
        !copied) {
        error =
            L"Backup failed for account '" +
            target.account +
            L"'. The destination was not modified.";

        if (ec) {
            error +=
                L" " +
                FilesystemError(ec);
        }

        return false;
    }

    return true;
}

bool AtomicWritePfUi(
    const AccountCopy& target,
    const std::string& content,
    std::wstring& error) {
    std::error_code ec;
    std::filesystem::create_directories(
        target.path.parent_path(),
        ec);

    if (ec) {
        error =
            L"Could not create pfUI destination folder for account '" +
            target.account +
            L"': " +
            FilesystemError(ec);
        return false;
    }

    std::filesystem::path temporary;
    unsigned long long suffix = 0;

    for (;;) {
        temporary =
            target.path.wstring() +
            L".tocpilot.tmp-" +
            std::to_wstring(
                GetCurrentProcessId()) +
            L"-" +
            std::to_wstring(
                suffix++);

        HANDLE file =
            CreateFileW(
                temporary.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

        if (file ==
            INVALID_HANDLE_VALUE) {
            const DWORD code =
                GetLastError();

            if (code ==
                    ERROR_FILE_EXISTS ||
                code ==
                    ERROR_ALREADY_EXISTS) {
                continue;
            }

            error =
                L"Could not create temporary pfUI file for account '" +
                target.account +
                L"'.";
            return false;
        }

        bool ok = true;
        std::size_t offset = 0;

        while (offset <
               content.size()) {
            const std::size_t remaining =
                content.size() -
                offset;
            const DWORD chunk =
                static_cast<DWORD>(
                    std::min<std::size_t>(
                        remaining,
                        1024u * 1024u * 1024u));
            DWORD written = 0;

            if (!WriteFile(
                    file,
                    content.data() +
                        offset,
                    chunk,
                    &written,
                    nullptr) ||
                written != chunk) {
                ok = false;
                break;
            }

            offset += written;
        }

        if (ok &&
            !FlushFileBuffers(file)) {
            ok = false;
        }

        CloseHandle(file);

        if (!ok) {
            DeleteFileW(
                temporary.c_str());
            error =
                L"Could not write temporary pfUI file for account '" +
                target.account +
                L"'. The destination was not modified.";
            return false;
        }

        if (!MoveFileExW(
                temporary.c_str(),
                target.path.c_str(),
                MOVEFILE_REPLACE_EXISTING |
                    MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(
                temporary.c_str());
            error =
                L"Could not atomically replace pfUI for account '" +
                target.account +
                L"'. The destination was not modified.";
            return false;
        }

        return true;
    }
}

bool WritePfUiTarget(
    const std::filesystem::path& wowRoot,
    const PfUiFilePlan& target,
    const std::string& content,
    std::filesystem::path& backupRunFolder,
    std::wstring& error) {
    if (!BackupPfUiTarget(
            wowRoot,
            target.copy,
            backupRunFolder,
            error)) {
        return false;
    }

    if (!AtomicWritePfUi(
            target.copy,
            content,
            error)) {
        if (target.copy.exists) {
            error +=
                L" The backup remains intact.";
        }

        return false;
    }

    return true;
}

struct PendingPfUiWrite {
    std::size_t fileIndex = 0;
    bool profiles = false;
    bool cache = false;
    std::string content;
};

void RunPfUiSyncItem(
    const std::filesystem::path& wowRoot,
    const std::vector<std::wstring>& accounts,
    const AccountSyncConfirmFn& confirm,
    AccountSyncRunResult& runResult,
    AccountSyncItemResult& itemResult,
    std::filesystem::path& backupRunFolder,
    std::wstring& error) {
    PfUiSyncPlan plan;
    std::wstring planError;

    if (!BuildPfUiSyncPlan(
            wowRoot,
            accounts,
            plan,
            planError)) {
        itemResult = {};
        itemResult.item =
            AccountSyncItem::PfUi;
        itemResult.fatal = true;
        itemResult.status = L"Error";
        itemResult.action =
            L"Sync failed";
        itemResult.error =
            planError;
        runResult.fatal = true;

        if (error.empty()) {
            error = planError;
        }

        return;
    }

    ItemAnalysis analysis;
    FillPfUiAnalysis(
        plan,
        analysis);
    FillPreviewResult(
        analysis,
        itemResult);

    if (plan.noSource) {
        return;
    }

    std::vector<std::wstring>
        confirmationAccounts;

    for (const auto& file :
         plan.files) {
        if (!file.copy.exists ||
            file.profileSync) {
            confirmationAccounts.push_back(
                file.copy.account);
        }
    }

    bool confirmationDeclined = false;
    bool accepted = true;

    if (!confirmationAccounts.empty()) {
        accepted =
            confirm &&
            confirm(
                AccountSyncItem::PfUi,
                plan.source.account,
                confirmationAccounts,
                false);
        confirmationDeclined =
            !accepted;
    }

    bool anyMergedCache = false;
    for (bool present :
         plan.mergedCachePresent) {
        anyMergedCache =
            anyMergedCache ||
            present;
    }

    std::vector<PendingPfUiWrite>
        pending;

    for (std::size_t i = 0;
         i < plan.files.size();
         ++i) {
        const auto& file =
            plan.files[i];

        const bool needsConfirmation =
            !file.copy.exists ||
            file.profileSync;
        const bool profiles =
            needsConfirmation &&
            accepted;
        const bool cache =
            file.copy.exists
                ? file.cacheMerge
                : accepted &&
                    anyMergedCache;

        if (!profiles &&
            !cache) {
            continue;
        }

        PendingPfUiWrite write;
        write.fileIndex = i;
        write.profiles = profiles;
        write.cache = cache;

        std::wstring buildError;

        if (!BuildPfUiOutput(
                plan,
                file,
                profiles,
                cache,
                write.content,
                buildError)) {
            itemResult.fatal = true;
            itemResult.status = L"Error";
            itemResult.action =
                L"Sync failed";
            itemResult.error =
                buildError;
            runResult.fatal = true;

            if (error.empty()) {
                error = buildError;
            }

            return;
        }

        pending.push_back(
            std::move(write));
    }

    std::size_t automaticCopied = 0;
    std::size_t confirmedCopied = 0;
    std::size_t modifiedTargets = 0;

    for (const auto& write :
         pending) {
        const auto& file =
            plan.files[
                write.fileIndex];
        std::wstring writeError;

        if (!WritePfUiTarget(
                wowRoot,
                file,
                write.content,
                backupRunFolder,
                writeError)) {
            itemResult.fatal = true;
            runResult.fatal = true;

            if (itemResult.error.empty()) {
                itemResult.error =
                    writeError;
            }

            if (error.empty()) {
                error = writeError;
            }

            continue;
        }

        ++modifiedTargets;

        if (write.cache) {
            ++automaticCopied;
        }

        if (write.profiles) {
            ++confirmedCopied;
        }
    }

    FinalizeRunItem(
        analysis,
        automaticCopied,
        confirmedCopied,
        modifiedTargets,
        confirmationDeclined,
        itemResult);
}

} // namespace

const wchar_t* AccountSyncItemLabel(
    AccountSyncItem item) {
    switch (item) {
    case AccountSyncItem::Macros:
        return L"Macros";
    case AccountSyncItem::Keybindings:
        return L"Keybindings";
    case AccountSyncItem::PfUi:
        return L"pfUI";
    }

    return L"";
}

bool AccountSyncItemEnabled(
    const AccountSyncConfig& config,
    AccountSyncItem item) {
    switch (item) {
    case AccountSyncItem::Macros:
        return config.macros;
    case AccountSyncItem::Keybindings:
        return config.keybindings;
    case AccountSyncItem::PfUi:
        return config.pfUi;
    }

    return false;
}

bool AccountSyncConfigured(
    const AccountSyncConfig& config) {
    return
        config.accounts.size() >= 2 &&
        (config.macros ||
         config.keybindings ||
         config.pfUi);
}

bool RunAccountSyncLaunchFlow(
    bool syncBeforeLaunch,
    bool console,
    const AccountSyncPreLaunchFn& sync,
    const AccountSyncLaunchFn& launch) {
    if (syncBeforeLaunch &&
        (!sync ||
         !sync())) {
        return false;
    }

    if (!launch) {
        return false;
    }

    launch(console);
    return true;
}

bool DiscoverAccountNames(
    const std::filesystem::path& wowRoot,
    std::vector<std::wstring>& accounts,
    std::wstring& error) {
    accounts.clear();
    error.clear();

    const auto accountRoot =
        wowRoot /
        L"WTF" /
        L"Account";

    std::error_code ec;

    if (!std::filesystem::exists(
            accountRoot,
            ec)) {
        if (ec) {
            error =
                L"Could not inspect WTF\\Account: " +
                FilesystemError(ec);
            return false;
        }

        return true;
    }

    std::filesystem::directory_iterator it(
        accountRoot,
        ec);
    const std::filesystem::directory_iterator end;

    if (ec) {
        error =
            L"Could not enumerate WTF\\Account: " +
            FilesystemError(ec);
        return false;
    }

    for (;
         it != end;
         it.increment(ec)) {
        if (ec) {
            error =
                L"Could not enumerate WTF\\Account: " +
                FilesystemError(ec);
            return false;
        }

        const auto status =
            it->symlink_status(ec);

        if (ec) {
            error =
                L"Could not inspect an account directory: " +
                FilesystemError(ec);
            return false;
        }

        if (!std::filesystem::is_directory(
                status) ||
            std::filesystem::is_symlink(
                status)) {
            continue;
        }

        const std::wstring name =
            it->path()
                .filename()
                .wstring();

        if (SafeAccountName(name)) {
            accounts.push_back(name);
        }
    }

    std::sort(
        accounts.begin(),
        accounts.end(),
        [](const std::wstring& left,
           const std::wstring& right) {
            return
                LessInsensitive(
                    left,
                    right);
        });

    return true;
}

bool ComparePfUiFiles(
    const std::filesystem::path& pathA,
    const std::filesystem::path& pathB,
    PfUiComparison& comparison,
    std::wstring& error) {
    comparison = {};
    error.clear();

    PfUiNormalizedData dataA;
    PfUiNormalizedData dataB;

    if (!NormalizePfUi(
            pathA,
            dataA,
            error) ||
        !NormalizePfUi(
            pathB,
            dataB,
            error)) {
        return false;
    }

    const bool profilesEqual =
        dataA.profiles ==
        dataB.profiles;
    const bool cacheEqual =
        dataA.cache ==
        dataB.cache;

    comparison.equivalent =
        profilesEqual &&
        cacheEqual;
    comparison.cacheOnly =
        profilesEqual &&
        !cacheEqual;
    comparison.settingsChanged =
        !profilesEqual;
    comparison.cacheChanged =
        !cacheEqual;

    return true;
}

bool InspectAccountSync(
    const std::filesystem::path& wowRoot,
    const AccountSyncConfig& config,
    std::array<AccountSyncItemResult, kAccountSyncItemCount>& items,
    std::wstring& error) {
    error.clear();

    const std::array<
        AccountSyncItem,
        kAccountSyncItemCount>
        displayOrder{
            AccountSyncItem::Macros,
            AccountSyncItem::Keybindings,
            AccountSyncItem::PfUi
        };

    std::vector<std::wstring> accounts;

    if (!NormalizeAccounts(
            config.accounts,
            accounts,
            error)) {
        return false;
    }

    for (const auto item :
         displayOrder) {
        auto& result =
            items[ItemIndex(item)];

        if (!AccountSyncItemEnabled(
                config,
                item)) {
            SetDisabledResult(
                item,
                result);
            continue;
        }

        if (accounts.size() < 2) {
            SetNotConfiguredResult(
                item,
                result);
            continue;
        }

        ItemAnalysis analysis;

        if (!AnalyzeItem(
                wowRoot,
                accounts,
                item,
                analysis,
                error)) {
            return false;
        }

        FillPreviewResult(
            analysis,
            result);
    }

    return true;
}

bool RunAccountSync(
    const std::filesystem::path& wowRoot,
    const AccountSyncConfig& config,
    const AccountSyncConfirmFn& confirm,
    AccountSyncRunResult& result,
    std::wstring& error) {
    result = {};
    error.clear();

    std::vector<std::wstring> accounts;

    if (!NormalizeAccounts(
            config.accounts,
            accounts,
            error)) {
        return false;
    }

    const std::array<
        AccountSyncItem,
        kAccountSyncItemCount>
        runOrder{
            AccountSyncItem::PfUi,
            AccountSyncItem::Keybindings,
            AccountSyncItem::Macros
        };

    std::filesystem::path backupRunFolder;

    for (const auto item :
         runOrder) {
        auto& itemResult =
            result.items[
                ItemIndex(item)];

        if (!AccountSyncItemEnabled(
                config,
                item)) {
            SetDisabledResult(
                item,
                itemResult);
            continue;
        }

        if (accounts.size() < 2) {
            SetNotConfiguredResult(
                item,
                itemResult);
            continue;
        }

        if (item ==
            AccountSyncItem::PfUi) {
            RunPfUiSyncItem(
                wowRoot,
                accounts,
                confirm,
                result,
                itemResult,
                backupRunFolder,
                error);
            continue;
        }

        ItemAnalysis analysis;
        std::wstring analysisError;

        if (!AnalyzeItem(
                wowRoot,
                accounts,
                item,
                analysis,
                analysisError)) {
            itemResult = {};
            itemResult.item = item;
            itemResult.fatal = true;
            itemResult.status = L"Error";
            itemResult.action =
                L"Sync failed";
            itemResult.error =
                analysisError;
            result.fatal = true;

            if (error.empty()) {
                error = analysisError;
            }

            continue;
        }

        FillPreviewResult(
            analysis,
            itemResult);

        std::size_t automaticCopied = 0;
        std::size_t confirmedCopied = 0;

        for (const auto& target :
             analysis.automaticTargets) {
            std::wstring copyError;

            if (CopyTarget(
                    wowRoot,
                    item,
                    analysis.source,
                    target,
                    backupRunFolder,
                    copyError)) {
                ++automaticCopied;
            } else {
                itemResult.fatal = true;
                result.fatal = true;

                if (itemResult.error.empty()) {
                    itemResult.error =
                        copyError;
                }

                if (error.empty()) {
                    error = copyError;
                }
            }
        }

        bool confirmationDeclined = false;

        if (!analysis.confirmationTargets
                 .empty()) {
            std::vector<std::wstring>
                targetAccounts;

            targetAccounts.reserve(
                analysis.confirmationTargets
                    .size());

            for (const auto& target :
                 analysis.confirmationTargets) {
                targetAccounts.push_back(
                    target.account);
            }

            const bool accepted =
                confirm &&
                confirm(
                    item,
                    analysis.source.account,
                    targetAccounts,
                    analysis.comparerFallback);

            if (accepted) {
                ItemAnalysis fresh;
                std::wstring freshnessError;
                if (!AnalyzeItem(wowRoot, accounts, item, fresh, freshnessError) ||
                    !SameSyncPlan(analysis, fresh)) {
                    itemResult.fatal = true;
                    result.fatal = true;
                    itemResult.status = L"Changed during confirmation";
                    itemResult.action = L"Sync cancelled; review and confirm again";
                    itemResult.error = freshnessError.empty()
                        ? L"Account Sync files or sync direction changed after approval."
                        : freshnessError;
                    if (error.empty()) error = itemResult.error;
                    continue;
                }
                for (const auto& target :
                     analysis.confirmationTargets) {
                    std::wstring copyError;

                    ItemAnalysis latest;
                    std::wstring latestError;
                    if (!AnalyzeItem(wowRoot, accounts, item, latest, latestError) ||
                        !SameSyncPlan(analysis, latest)) {
                        itemResult.fatal = true;
                        result.fatal = true;
                        itemResult.error = latestError.empty()
                            ? L"Account Sync files changed before the write; confirm again."
                            : latestError;
                        if (error.empty()) error = itemResult.error;
                        break;
                    }

                    if (CopyTarget(
                            wowRoot,
                            item,
                            analysis.source,
                            target,
                            backupRunFolder,
                            copyError)) {
                        ++confirmedCopied;
                    } else {
                        itemResult.fatal = true;
                        result.fatal = true;

                        if (itemResult.error.empty()) {
                            itemResult.error =
                                copyError;
                        }

                        if (error.empty()) {
                            error = copyError;
                        }
                    }
                }
            } else {
                confirmationDeclined = true;
            }
        }

        FinalizeRunItem(
            analysis,
            automaticCopied,
            confirmedCopied,
            automaticCopied +
                confirmedCopied,
            confirmationDeclined,
            itemResult);
    }

    return true;
}

} // namespace tp
