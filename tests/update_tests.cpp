#include "update.h"

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

bool LooksLikeVersionTag(const std::wstring& tag) {
    if (tag.size() < 6 || (tag[0] != L'v' && tag[0] != L'V')) {
        return false;
    }

    int dots = 0;
    for (std::size_t i = 1; i < tag.size(); ++i) {
        if (tag[i] == L'.') {
            ++dots;
            continue;
        }
        if (!std::iswdigit(tag[i])) {
            return false;
        }
    }
    return dots == 2;
}

void TestSelfUpdateTransportPolicy() {
    const std::wstring digest =
        L"sha256:0123456789abcdef0123456789abcdef"
        L"0123456789abcdef0123456789abcdef";

    tp::ReleaseInfo release;
    release.tag = L"v9.9.9";
    release.assetUrl =
        L"https://example.invalid/TocPilot.exe";
    release.assetDigest = digest;
    release.assetSize = 1234;
    release.checksumUrl =
        L"https://example.invalid/TocPilot.exe.sha256";
    release.checksumSize = 80;

    std::wstring error;
    if (!tp::ValidateSelfUpdateRelease(
            release,
            error)) {
        Fail("valid HTTPS self-update metadata was rejected");
    }

    auto invalid = release;
    invalid.assetUrl =
        L"http://example.invalid/TocPilot.exe";
    if (tp::ValidateSelfUpdateRelease(
            invalid,
            error)) {
        Fail("HTTP self-update executable URL was accepted");
    }

    invalid = release;
    invalid.checksumUrl =
        L"http://example.invalid/TocPilot.exe.sha256";
    if (tp::ValidateSelfUpdateRelease(
            invalid,
            error)) {
        Fail("HTTP self-update checksum URL was accepted");
    }

    invalid = release;
    invalid.checksumSize =
        tp::kMaxSelfUpdateChecksumBytes + 1;
    if (tp::ValidateSelfUpdateRelease(
            invalid,
            error)) {
        Fail("oversized self-update checksum metadata was accepted");
    }

    invalid = release;
    invalid.assetSize = 0;
    if (tp::ValidateSelfUpdateRelease(
            invalid,
            error)) {
        Fail("zero-byte self-update executable metadata was accepted");
    }

    if (!tp::ValidateSelfUpdateDownloadSize(
            1234,
            1234,
            error)) {
        Fail("matching self-update asset size was rejected");
    }
    if (tp::ValidateSelfUpdateDownloadSize(
            1234,
            1233,
            error)) {
        Fail("short self-update asset size was accepted");
    }
    if (tp::ValidateSelfUpdateDownloadSize(
            1234,
            1235,
            error)) {
        Fail("oversized self-update asset size was accepted");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--live-latest") {
        std::wstring tag;
        std::wstring error;
        if (!tp::ResolveLatestReleaseTag(tag, error)) {
            std::wcerr
                << L"live latest-release redirect failed: "
                << error
                << L'\n';
            return 1;
        }

        if (!LooksLikeVersionTag(tag)) {
            std::wcerr
                << L"live latest-release redirect returned invalid tag: "
                << tag
                << L'\n';
            return 1;
        }

        tp::ReleaseInfo release;
        tp::ReleaseCheckState state =
            tp::ReleaseCheckState::NoRelease;

        if (!tp::CheckLatestRelease(
                release,
                state,
                error)) {
            std::wcerr
                << L"live latest-release check failed: "
                << error
                << L'\n';
            return 1;
        }

        const std::wstring expectedBase =
            L"https://github.com/Seraphic8x2244/TocPilot/releases/download/" +
            release.tag +
            L"/";

        if (release.tag != tag ||
            release.assetUrl != expectedBase + L"TocPilot.exe" ||
            release.assetSize == 0 ||
            release.checksumUrl !=
                expectedBase + L"TocPilot.exe.sha256" ||
            release.checksumSize == 0 ||
            release.checksumSize >
                tp::kMaxSelfUpdateChecksumBytes) {
            std::wcerr
                << L"live latest-release asset URLs were not constructed correctly\n";
            return 1;
        }

        std::wcout
            << L"Live latest-release redirect passed: "
            << tag
            << L'\n';
        return 0;
    }

    TestSelfUpdateTransportPolicy();

    {
        std::wstring tag;
        std::wstring error;
        if (!tp::ParseLatestReleaseTagFromUrl(
                L"https://github.com/Seraphic8x2244/TocPilot/releases/tag/v1.2.3",
                tag,
                error) ||
            tag != L"v1.2.3") {
            Fail("valid latest-release redirect was not parsed");
        }
    }

    {
        std::wstring tag;
        std::wstring error;
        if (!tp::ParseLatestReleaseTagFromUrl(
                L"https://github.com/Seraphic8x2244/TocPilot/releases/tag/v1.2.3/?ignored=1",
                tag,
                error) ||
            tag != L"v1.2.3") {
            Fail("latest-release redirect suffix handling failed");
        }
    }

    {
        std::wstring tag;
        std::wstring error;
        if (tp::ParseLatestReleaseTagFromUrl(
                L"https://api.github.com/repos/Seraphic8x2244/TocPilot/releases/latest",
                tag,
                error)) {
            Fail("GitHub REST URL was accepted as latest-release redirect");
        }
    }

    {
        std::wstring tag;
        std::wstring error;
        if (tp::ParseLatestReleaseTagFromUrl(
                L"https://github.com/Seraphic8x2244/TocPilot/releases/latest",
                tag,
                error)) {
            Fail("unresolved latest-release URL was accepted");
        }
    }

    {
        std::wstring tag;
        std::wstring error;
        if (tp::ParseLatestReleaseTagFromUrl(
                L"https://github.com/Seraphic8x2244/TocPilot/releases/tag/not-a-version",
                tag,
                error)) {
            Fail("invalid release tag was accepted");
        }
    }

    {
        std::wstring tag;
        std::wstring error;
        if (tp::ParseLatestReleaseTagFromUrl(
                L"https://example.com/Seraphic8x2244/TocPilot/releases/tag/v1.2.3",
                tag,
                error)) {
            Fail("non-GitHub release redirect was accepted");
        }
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " self-update discovery test(s) failed\n";
        return 1;
    }

    std::cout << "Self-update discovery tests passed\n";
    return 0;
}
