#include "provider.h"

#include <algorithm>
#include <cwctype>
#include <vector>

namespace tp {
namespace {

std::wstring Trim(std::wstring_view value) {
    std::size_t first = 0;
    while (first < value.size() && std::iswspace(value[first])) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && std::iswspace(value[last - 1])) {
        --last;
    }

    return std::wstring(value.substr(first, last - first));
}

std::wstring Lower(std::wstring value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
    return value;
}

bool StartsWithInsensitive(
    std::wstring_view value,
    std::wstring_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::towlower(value[i]) != std::towlower(prefix[i])) {
            return false;
        }
    }
    return true;
}

void StripKnownScheme(std::wstring& value) {
    constexpr std::wstring_view schemes[]{
        L"https://",
        L"http://",
        L"ssh://",
        L"git://"
    };

    for (const auto scheme : schemes) {
        if (StartsWithInsensitive(value, scheme)) {
            value.erase(0, scheme.size());
            return;
        }
    }
}

std::vector<std::wstring> SplitPath(std::wstring_view path) {
    std::vector<std::wstring> segments;
    std::size_t start = 0;

    while (start <= path.size()) {
        const std::size_t slash = path.find(L'/', start);
        const std::size_t end = slash == std::wstring_view::npos
            ? path.size()
            : slash;

        if (end > start) {
            segments.emplace_back(path.substr(start, end - start));
        }

        if (slash == std::wstring_view::npos) {
            break;
        }
        start = slash + 1;
    }

    return segments;
}

bool ValidSegment(std::wstring_view segment) {
    if (segment.empty() || segment == L"." || segment == L"..") {
        return false;
    }

    for (const wchar_t ch : segment) {
        if (std::iswspace(ch) ||
            std::iswcntrl(ch) ||
            ch == L'\\') {
            return false;
        }
    }
    return true;
}

std::wstring JoinSegments(
    const std::vector<std::wstring>& segments,
    std::size_t count) {
    std::wstring result;
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0) {
            result += L'/';
        }
        result += segments[i];
    }
    return result;
}

} // namespace

const wchar_t* ProviderName(ProviderKind provider) {
    switch (provider) {
    case ProviderKind::GitHub:
        return L"GitHub";
    case ProviderKind::GitLab:
        return L"GitLab";
    }
    return L"Unknown";
}

bool NormalizeRepositoryUrl(
    std::wstring_view input,
    RepositoryIdentity& identity,
    std::wstring& error) {
    identity = {};
    error.clear();

    std::wstring value = Trim(input);
    if (value.empty()) {
        error = L"Enter a GitHub or GitLab repository URL.";
        return false;
    }

    const std::size_t query = value.find_first_of(L"?#");
    if (query != std::wstring::npos) {
        value.resize(query);
    }

    StripKnownScheme(value);

    const std::size_t slash = value.find(L'/');
    const std::size_t colon = value.find(L':');
    const std::size_t at = value.find(L'@');

    if (at != std::wstring::npos &&
        colon != std::wstring::npos &&
        (slash == std::wstring::npos || colon < slash) &&
        at < colon) {
        value.erase(0, at + 1);
        const std::size_t hostColon = value.find(L':');
        if (hostColon != std::wstring::npos) {
            value[hostColon] = L'/';
        }
    } else {
        const std::size_t firstSlash = value.find(L'/');
        const std::size_t userAt = value.find(L'@');
        if (userAt != std::wstring::npos &&
            (firstSlash == std::wstring::npos ||
             userAt < firstSlash)) {
            value.erase(0, userAt + 1);
        }
    }

    while (!value.empty() && value.back() == L'/') {
        value.pop_back();
    }

    const std::size_t hostEnd = value.find(L'/');
    if (hostEnd == std::wstring::npos ||
        hostEnd == 0 ||
        hostEnd + 1 >= value.size()) {
        error =
            L"The URL must include both a repository owner/group "
            L"and repository name.";
        return false;
    }

    std::wstring host = Lower(value.substr(0, hostEnd));
    if (StartsWithInsensitive(host, L"www.")) {
        host.erase(0, 4);
    }
    if (host.ends_with(L":443")) {
        host.resize(host.size() - 4);
    }

    ProviderKind provider{};
    if (host == L"github.com") {
        provider = ProviderKind::GitHub;
    } else if (host == L"gitlab.com") {
        provider = ProviderKind::GitLab;
    } else {
        error =
            L"Only public github.com and gitlab.com repositories "
            L"are supported in this version.";
        return false;
    }

    std::wstring path = value.substr(hostEnd + 1);
    while (!path.empty() && path.front() == L'/') {
        path.erase(path.begin());
    }

    if (provider == ProviderKind::GitLab) {
        const std::size_t dashRoute = path.find(L"/-/");
        if (dashRoute != std::wstring::npos) {
            path.resize(dashRoute);
        }
    }

    auto segments = SplitPath(path);
    if (provider == ProviderKind::GitHub &&
        segments.size() > 2) {
        segments.resize(2);
    }

    if (segments.size() < 2) {
        error =
            L"The URL must include both a repository owner/group "
            L"and repository name.";
        return false;
    }

    if (segments.back().size() > 4 &&
        Lower(segments.back().substr(
            segments.back().size() - 4)) == L".git") {
        segments.back().resize(segments.back().size() - 4);
    }

    for (const auto& segment : segments) {
        if (!ValidSegment(segment)) {
            error =
                L"The repository path contains an invalid segment.";
            return false;
        }
    }

    identity.provider = provider;
    identity.host = host;
    identity.repository =
        JoinSegments(segments, segments.size());
    identity.canonicalUrl =
        L"https://" + host + L"/" + identity.repository;
    return true;
}

} // namespace tp
