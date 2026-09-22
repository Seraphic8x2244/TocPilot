#include "github_release.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Fail(
    const char* message) {
    std::cerr
        << message
        << '\n';
    ++failures;
}

const std::string kStableReleaseJson =
    "{"
    "\"tag_name\":\"v1.2.3\","
    "\"name\":\"Stable 1.2.3\","
    "\"draft\":false,"
    "\"prerelease\":false,"
    "\"assets\":["
      "{"
        "\"name\":\"ClassicAPI.dll\","
        "\"browser_download_url\":\"https://github.com/Owner/Repo/releases/download/v1.2.3/ClassicAPI.dll\","
        "\"digest\":\"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
        "\"size\":12345"
      "},"
      "{"
        "\"name\":\"ClassicAPI.dll.sha256\","
        "\"browser_download_url\":\"https://github.com/Owner/Repo/releases/download/v1.2.3/ClassicAPI.dll.sha256\","
        "\"digest\":null,"
        "\"size\":80"
      "}"
    "]"
    "}";

void TestStableReleaseParsing() {
    tp::GitHubReleaseInfo release;
    std::wstring error;

    if (!tp::ParseGitHubReleaseJson(
            kStableReleaseJson,
            release,
            error)) {
        Fail(
            "valid stable release JSON was rejected");
        return;
    }

    if (release.tag != L"v1.2.3" ||
        release.name != L"Stable 1.2.3" ||
        release.draft ||
        release.prerelease ||
        release.assets.size() != 2) {
        Fail(
            "stable release metadata did not parse correctly");
        return;
    }

    tp::GitHubReleaseAsset asset;
    if (!tp::FindExactGitHubReleaseAsset(
            release,
            L"ClassicAPI.dll",
            asset,
            error) ||
        asset.name != L"ClassicAPI.dll" ||
        asset.size != 12345 ||
        asset.digest !=
            L"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") {
        Fail(
            "exact DLL release asset was not resolved");
    }
}

void TestMissingAssetFailsClosed() {
    tp::GitHubReleaseInfo release;
    std::wstring error;

    if (!tp::ParseGitHubReleaseJson(
            kStableReleaseJson,
            release,
            error)) {
        Fail(
            "missing-asset fixture did not parse");
        return;
    }

    tp::GitHubReleaseAsset asset;
    if (tp::FindExactGitHubReleaseAsset(
            release,
            L"RenamedClassicAPI.dll",
            asset,
            error)) {
        Fail(
            "missing exact asset name was guessed");
    }
}

void TestDuplicateAssetFailsClosed() {
    const std::string json =
        "{"
        "\"tag_name\":\"v2.0.0\","
        "\"name\":null,"
        "\"draft\":false,"
        "\"prerelease\":false,"
        "\"assets\":["
          "{"
            "\"name\":\"Same.dll\","
            "\"browser_download_url\":\"https://example.invalid/one\","
            "\"digest\":null,"
            "\"size\":1"
          "},"
          "{"
            "\"name\":\"Same.dll\","
            "\"browser_download_url\":\"https://example.invalid/two\","
            "\"digest\":null,"
            "\"size\":2"
          "}"
        "]"
        "}";

    tp::GitHubReleaseInfo release;
    std::wstring error;
    if (!tp::ParseGitHubReleaseJson(
            json,
            release,
            error)) {
        Fail(
            "duplicate-asset fixture did not parse");
        return;
    }

    tp::GitHubReleaseAsset asset;
    if (tp::FindExactGitHubReleaseAsset(
            release,
            L"Same.dll",
            asset,
            error)) {
        Fail(
            "ambiguous duplicate asset name was accepted");
    }
}

void TestMalformedReleaseFails() {
    tp::GitHubReleaseInfo release;
    std::wstring error;

    if (tp::ParseGitHubReleaseJson(
            "{\"draft\":false,\"prerelease\":false,\"assets\":[]}",
            release,
            error)) {
        Fail(
            "release without tag_name was accepted");
    }

    if (tp::ParseGitHubReleaseJson(
            "{\"tag_name\":\"v1\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"A.dll\",\"size\":1}]}",
            release,
            error)) {
        Fail(
            "asset without browser_download_url was accepted");
    }
}

} // namespace

int main() {
    TestStableReleaseParsing();
    TestMissingAssetFailsClosed();
    TestDuplicateAssetFailsClosed();
    TestMalformedReleaseFails();

    if (failures != 0) {
        std::cerr
            << failures
            << " GitHub release metadata test(s) failed\n";
        return 1;
    }

    std::cout
        << "GitHub release metadata tests passed\n";
    return 0;
}
