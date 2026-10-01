#include "exact_release_asset.h"

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

tp::PackageRecord Package(
    std::wstring asset) {
    tp::PackageRecord package;
    package.id =
        L"github:Owner/Repo:release:" +
        asset;
    package.name =
        asset;
    package.provider =
        L"github";
    package.repository =
        L"Owner/Repo";
    package.mode =
        L"release";
    package.releasePolicy =
        L"latest_stable";
    package.asset =
        std::move(asset);
    return package;
}

tp::GitHubReleaseAsset Asset(
    std::wstring name,
    std::wstring digest) {
    tp::GitHubReleaseAsset asset;
    asset.name =
        std::move(name);
    asset.downloadUrl =
        L"https://github.com/Owner/Repo/releases/download/v1.2.3/" +
        asset.name;
    asset.digest =
        std::move(digest);
    asset.size = 12345;
    return asset;
}

void TestKindAwareValidation() {
    std::wstring error;

    auto dll =
        Package(L"ClassicAPI.dll");
    if (!tp::ValidateLatestStableExactReleaseAssetPackage(
            dll,
            tp::ExactReleaseAssetKind::Dll,
            error)) {
        Fail(
            "valid exact DLL source was rejected");
    }

    if (tp::ValidateLatestStableExactReleaseAssetPackage(
            dll,
            tp::ExactReleaseAssetKind::Mpq,
            error)) {
        Fail(
            "DLL source was accepted as MPQ kind");
    }

    auto mpq =
        Package(L"patch-X.mpq");
    if (!tp::ValidateLatestStableExactReleaseAssetPackage(
            mpq,
            tp::ExactReleaseAssetKind::Mpq,
            error)) {
        Fail(
            "valid exact MPQ source was rejected");
    }

    mpq.asset =
        L"Data\\patch-X.mpq";
    if (tp::ValidateLatestStableExactReleaseAssetPackage(
            mpq,
            tp::ExactReleaseAssetKind::Mpq,
            error)) {
        Fail(
            "nested MPQ release asset name was accepted");
    }

    dll.releasePolicy =
        L"latest_prerelease";
    if (tp::ValidateLatestStableExactReleaseAssetPackage(
            dll,
            tp::ExactReleaseAssetKind::Dll,
            error)) {
        Fail(
            "prerelease exact-asset policy was accepted");
    }
}

void TestExactMetadataResolution() {
    const std::wstring digest =
        L"sha256:ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";

    tp::GitHubReleaseInfo metadata;
    metadata.tag =
        L"v1.2.3";
    metadata.name =
        L"Stable 1.2.3";
    metadata.assets.push_back(
        Asset(
            L"ClassicAPI.dll",
            digest));
    metadata.assets.push_back(
        Asset(
            L"patch-X.mpq",
            digest));

    std::wstring error;

    {
        auto package =
            Package(L"ClassicAPI.dll");
        tp::ExactReleaseAssetRelease release;

        if (!tp::ResolveExactReleaseAssetMetadata(
                package,
                tp::ExactReleaseAssetKind::Dll,
                metadata,
                release,
                error) ||
            release.kind !=
                tp::ExactReleaseAssetKind::Dll ||
            release.tag !=
                L"v1.2.3" ||
            release.asset.name !=
                L"ClassicAPI.dll" ||
            release.expectedSha256 !=
                L"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789") {
            Fail(
                "exact DLL metadata resolution changed");
        }
    }

    {
        auto package =
            Package(L"patch-X.mpq");
        tp::ExactReleaseAssetRelease release;

        if (!tp::ResolveExactReleaseAssetMetadata(
                package,
                tp::ExactReleaseAssetKind::Mpq,
                metadata,
                release,
                error) ||
            release.kind !=
                tp::ExactReleaseAssetKind::Mpq ||
            release.asset.name !=
                L"patch-X.mpq") {
            Fail(
                "exact MPQ metadata resolution did not retain asset kind");
        }
    }

    {
        auto package =
            Package(L"Missing.dll");
        tp::ExactReleaseAssetRelease release;

        if (tp::ResolveExactReleaseAssetMetadata(
                package,
                tp::ExactReleaseAssetKind::Dll,
                metadata,
                release,
                error)) {
            Fail(
                "missing exact release asset was guessed");
        }
    }

    {
        auto duplicate =
            metadata;
        duplicate.assets.push_back(
            Asset(
                L"ClassicAPI.dll",
                digest));

        auto package =
            Package(L"ClassicAPI.dll");
        tp::ExactReleaseAssetRelease release;

        if (tp::ResolveExactReleaseAssetMetadata(
                package,
                tp::ExactReleaseAssetKind::Dll,
                duplicate,
                release,
                error)) {
            Fail(
                "ambiguous duplicate exact release asset was accepted");
        }
    }
}

void TestStableMetadataRequired() {
    auto package =
        Package(L"ClassicAPI.dll");

    tp::GitHubReleaseInfo metadata;
    metadata.tag =
        L"v1.2.3";
    metadata.assets.push_back(
        Asset(
            L"ClassicAPI.dll",
            L"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));

    std::wstring error;
    tp::ExactReleaseAssetRelease release;

    metadata.prerelease = true;
    if (tp::ResolveExactReleaseAssetMetadata(
            package,
            tp::ExactReleaseAssetKind::Dll,
            metadata,
            release,
            error)) {
        Fail(
            "prerelease metadata was accepted as latest stable");
    }

    metadata.prerelease = false;
    metadata.draft = true;
    if (tp::ResolveExactReleaseAssetMetadata(
            package,
            tp::ExactReleaseAssetKind::Dll,
            metadata,
            release,
            error)) {
        Fail(
            "draft metadata was accepted as latest stable");
    }
}

} // namespace

int main() {
    TestKindAwareValidation();
    TestExactMetadataResolution();
    TestStableMetadataRequired();

    if (failures != 0) {
        std::cerr
            << failures
            << " exact release-asset test(s) failed\n";
        return 1;
    }

    std::cout
        << "Exact release-asset tests passed\n";
    return 0;
}
