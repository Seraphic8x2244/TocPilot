#include "mpq_package.h"
#include "state.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

constexpr char kPayloadOne[] =
    "mpq payload one\n";
constexpr char kPayloadTwo[] =
    "mpq payload two updated\n";
constexpr char kPayloadTwoCorrupt[] =
    "Mpq payload two updated\n";
constexpr char kUnmanaged[] =
    "unmanaged collision\n";

constexpr wchar_t kPayloadOneSha[] =
    L"753a3ee628a5048cf8d2562b7d5082750b2fab7d6e66d2dd554ceeffed180ed6";
constexpr wchar_t kPayloadTwoSha[] =
    L"7a069b118d371074c487d95547552952ae2a19a95261daae5d2a31cc921415fa";

void Fail(
    const char* message) {
    std::cerr
        << message
        << '\n';
    ++failures;
}

std::filesystem::path MakeTempRoot() {
    const auto root =
        std::filesystem::temp_directory_path() /
        (L"TocPilotMpqPackageTests-" +
         std::to_wstring(
             GetCurrentProcessId()));

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    ec.clear();

    std::filesystem::create_directories(
        root / L"Data",
        ec);

    if (ec) {
        return {};
    }

    return root;
}

bool WriteAll(
    const std::filesystem::path& path,
    std::string_view bytes) {
    std::ofstream stream(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!stream) {
        return false;
    }

    stream.write(
        bytes.data(),
        static_cast<std::streamsize>(
            bytes.size()));

    return stream.good();
}

std::string ReadAll(
    const std::filesystem::path& path) {
    std::ifstream stream(
        path,
        std::ios::binary);

    return std::string(
        std::istreambuf_iterator<char>(
            stream),
        std::istreambuf_iterator<char>());
}

tp::PackageRecord Package(
    std::wstring repository,
    std::wstring asset,
    std::wstring targetPath = {}) {
    tp::PackageRecord package;
    package.provider =
        L"github";
    package.repository =
        std::move(repository);
    package.asset =
        std::move(asset);
    package.id =
        package.provider + L":" +
        package.repository + L":release:" +
        package.asset;
    package.name =
        package.asset;
    package.mode =
        L"release";
    package.releasePolicy =
        L"latest_stable";
    package.target =
        L"data";
    package.targetPath =
        std::move(targetPath);
    return package;
}

tp::MpqRelease Release(
    const tp::PackageRecord& package,
    std::wstring tag,
    std::uint64_t size,
    std::wstring digest) {
    tp::MpqRelease release;
    release.kind =
        tp::ExactReleaseAssetKind::Mpq;
    release.tag =
        std::move(tag);
    release.asset.name =
        package.asset;
    release.asset.downloadUrl =
        L"https://example.invalid/" +
        package.asset;
    release.asset.size =
        size;
    release.expectedSha256 =
        std::move(digest);
    return release;
}

void TestAllocationAndPersistence(
    const std::filesystem::path& root) {
    const auto data =
        root / L"Data";

    if (!WriteAll(
            data / L"PATCH-a.MPQ",
            kUnmanaged)) {
        Fail(
            "could not create unmanaged allocation fixture");
        return;
    }

    auto reserved =
        Package(
            L"Owner/Reserved",
            L"Reserved.mpq",
            L"Data/patch-B.mpq");

    auto candidate =
        Package(
            L"Owner/WideScreens",
            L"WideScreens.mpq");

    std::vector<tp::PackageRecord>
        packages{reserved};

    std::wstring error;
    if (!tp::AssignMpqTargetPath(
            root,
            packages,
            candidate,
            error) ||
        candidate.targetPath !=
            L"Data/patch-C.mpq") {
        Fail(
            "MPQ allocation did not skip unmanaged and persisted slots deterministically");
        return;
    }

    packages.push_back(
        candidate);

    if (!tp::ValidateDurablePackageState(
            packages,
            error)) {
        Fail(
            "valid MPQ reservations failed durable validation");
        return;
    }

    tp::AppState state;
    bool created = false;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail(
            "could not create state for MPQ persistence test");
        return;
    }

    state.packages =
        packages;

    if (!tp::SaveState(
            root,
            state,
            error)) {
        Fail(
            "could not persist MPQ target assignment");
        return;
    }

    tp::AppState reloaded;
    created = true;

    if (!tp::LoadOrCreateState(
            root,
            reloaded,
            created,
            error) ||
        reloaded.packages.size() != 2 ||
        reloaded.packages[1].targetPath !=
            L"Data/patch-C.mpq") {
        Fail(
            "MPQ target assignment did not round-trip");
        return;
    }

    auto reused =
        reloaded.packages[1];

    if (!tp::AssignMpqTargetPath(
            root,
            reloaded.packages,
            reused,
            error) ||
        reused.targetPath !=
            L"Data/patch-C.mpq") {
        Fail(
            "persisted MPQ target was reassigned");
        return;
    }

    for (wchar_t letter = L'C';
         letter <= L'Z';
         ++letter) {
        if (!WriteAll(
                data /
                    (L"patch-" +
                     std::wstring(1, letter) +
                     L".mpq"),
                kUnmanaged)) {
            Fail(
                "could not fill MPQ allocation slots");
            return;
        }
    }

    auto exhausted =
        Package(
            L"Owner/Exhausted",
            L"Exhausted.mpq");

    if (tp::AssignMpqTargetPath(
            root,
            reloaded.packages,
            exhausted,
            error)) {
        Fail(
            "MPQ allocation succeeded with all patch letters occupied");
    }
}

void TestValidationAndCollisions() {
    std::wstring error;

    auto package =
        Package(
            L"Owner/Valid",
            L"Valid.mpq",
            L"Data/patch-A.mpq");

    if (!tp::ValidateMpqPackage(
            package,
            error)) {
        Fail(
            "valid MPQ package was rejected");
        return;
    }

    {
        auto invalid =
            package;
        invalid.asset =
            L"Valid.dll";

        if (tp::ValidateMpqPackage(
                invalid,
                error)) {
            Fail(
                "DLL asset was accepted as MPQ package");
        }
    }

    {
        auto invalid =
            package;
        invalid.targetPath =
            L"Data/custom.mpq";

        if (tp::ValidateMpqPackage(
                invalid,
                error)) {
            Fail(
                "non patch-letter MPQ target was accepted");
        }
    }

    {
        auto invalid =
            package;
        invalid.targetPath =
            L"..\\patch-A.mpq";

        if (tp::ValidateMpqPackage(
                invalid,
                error)) {
            Fail(
                "unsafe MPQ target path was accepted");
        }
    }

    {
        auto invalid =
            package;
        invalid.sourcePath =
            L"release";

        if (tp::ValidateMpqPackage(
                invalid,
                error)) {
            Fail(
                "MPQ package mixed repository source-path tracking");
        }
    }

    if (tp::SetPackageInstalledState(
            package,
            L"v1",
            {L"Data/patch-B.mpq"},
            error)) {
        Fail(
            "MPQ installed state accepted ownership of another slot");
    }

    auto first =
        package;
    auto second =
        Package(
            L"Owner/Second",
            L"Second.mpq",
            L"data/PATCH-a.MPQ");

    if (tp::ValidateDurablePackageState(
            {first, second},
            error)) {
        Fail(
            "duplicate MPQ target reservation was accepted");
    }
}

void TestInstallUpdateIntegrityAndRemoval(
    const std::filesystem::path& root) {
    const auto data =
        root / L"Data";

    auto package =
        Package(
            L"Owner/WideScreens",
            L"WideScreens.mpq",
            L"Data/patch-A.mpq");

    const auto payloadOne =
        root / L"payload-one.mpq";
    const auto payloadTwo =
        root / L"payload-two.mpq";

    if (!WriteAll(
            payloadOne,
            kPayloadOne) ||
        !WriteAll(
            payloadTwo,
            kPayloadTwo)) {
        Fail(
            "could not write MPQ install payload fixtures");
        return;
    }

    auto releaseOne =
        Release(
            package,
            L"v1.0.0",
            sizeof(kPayloadOne) - 1,
            kPayloadOneSha);

    tp::MpqInstallTransaction
        install;
    std::wstring actual;
    std::wstring error;

    std::vector<tp::PackageRecord>
        packages{package};

    if (!tp::BeginMpqInstallFromFile(
            package,
            releaseOne,
            root,
            packages,
            payloadOne,
            install,
            actual,
            error) ||
        !install.active ||
        install.hadPreviousTarget ||
        ReadAll(data / L"patch-A.mpq") !=
            kPayloadOne) {
        Fail(
            "fresh MPQ install transaction failed");
        return;
    }

    if (!tp::SetPackageInstalledState(
            package,
            releaseOne.tag,
            {package.targetPath},
            error) ||
        !tp::FinalizeMpqInstall(
            install,
            error)) {
        Fail(
            "fresh MPQ install could not commit installed state");
        return;
    }

    packages[0] =
        package;

    if (!tp::VerifyInstalledMpq(
            package,
            releaseOne,
            root,
            actual,
            error)) {
        Fail(
            "fresh installed MPQ failed integrity verification");
        return;
    }

    auto releaseMismatch =
        releaseOne;
    releaseMismatch.asset.name =
        L"Other.mpq";

    if (tp::BeginMpqInstallFromFile(
            package,
            releaseMismatch,
            root,
            packages,
            payloadOne,
            install,
            actual,
            error)) {
        Fail(
            "release-asset mismatch was accepted for MPQ install");
        return;
    }

    auto releaseTwo =
        Release(
            package,
            L"v2.0.0",
            sizeof(kPayloadTwo) - 1,
            kPayloadTwoSha);

    if (!tp::BeginMpqInstallFromFile(
            package,
            releaseTwo,
            root,
            packages,
            payloadTwo,
            install,
            actual,
            error) ||
        !install.active ||
        !install.hadPreviousTarget ||
        ReadAll(data / L"patch-A.mpq") !=
            kPayloadTwo) {
        Fail(
            "MPQ update did not replace the persisted slot in place");
        return;
    }

    if (!tp::RollbackMpqInstall(
            install,
            error) ||
        ReadAll(data / L"patch-A.mpq") !=
            kPayloadOne) {
        Fail(
            "MPQ update rollback did not restore the prior file");
        return;
    }

    if (!tp::BeginMpqInstallFromFile(
            package,
            releaseTwo,
            root,
            packages,
            payloadTwo,
            install,
            actual,
            error) ||
        !tp::SetPackageInstalledState(
            package,
            releaseTwo.tag,
            {package.targetPath},
            error) ||
        package.targetPath !=
            L"Data/patch-A.mpq" ||
        !tp::FinalizeMpqInstall(
            install,
            error)) {
        Fail(
            "MPQ update commit changed or failed the persisted slot");
        return;
    }

    packages[0] =
        package;

    if (!tp::VerifyInstalledMpq(
            package,
            releaseTwo,
            root,
            actual,
            error)) {
        Fail(
            "updated MPQ failed integrity verification");
        return;
    }

    if (!WriteAll(
            data / L"patch-A.mpq",
            kPayloadTwoCorrupt)) {
        Fail(
            "could not corrupt MPQ integrity fixture");
        return;
    }

    if (tp::VerifyInstalledMpq(
            package,
            releaseTwo,
            root,
            actual,
            error)) {
        Fail(
            "MPQ integrity verification accepted modified payload");
        return;
    }

    if (!WriteAll(
            data / L"patch-A.mpq",
            kPayloadTwo)) {
        Fail(
            "could not restore MPQ after integrity test");
        return;
    }

    auto collision =
        Package(
            L"Owner/Collision",
            L"Collision.mpq",
            L"Data/patch-B.mpq");
    auto collisionRelease =
        Release(
            collision,
            L"v1",
            sizeof(kPayloadOne) - 1,
            kPayloadOneSha);

    if (!WriteAll(
            data / L"patch-B.mpq",
            kUnmanaged)) {
        Fail(
            "could not write unmanaged collision fixture");
        return;
    }

    tp::MpqInstallTransaction
        collisionInstall;

    if (tp::BeginMpqInstallFromFile(
            collision,
            collisionRelease,
            root,
            {package, collision},
            payloadOne,
            collisionInstall,
            actual,
            error) ||
        ReadAll(data / L"patch-B.mpq") !=
            kUnmanaged) {
        Fail(
            "unmanaged MPQ collision was overwritten");
        return;
    }

    tp::MpqRemovalTransaction
        removal;

    if (!tp::BeginMpqRemoval(
            package,
            root,
            packages,
            removal,
            error) ||
        !removal.active ||
        std::filesystem::exists(
            data / L"patch-A.mpq")) {
        Fail(
            "MPQ removal transaction did not stage the managed file");
        return;
    }

    if (!tp::RollbackMpqRemoval(
            removal,
            error) ||
        ReadAll(data / L"patch-A.mpq") !=
            kPayloadTwo) {
        Fail(
            "MPQ removal rollback did not restore the managed file");
        return;
    }

    if (!tp::BeginMpqRemoval(
            package,
            root,
            packages,
            removal,
            error) ||
        !tp::ClearPackageInstalledState(
            package,
            error) ||
        package.targetPath !=
            L"Data/patch-A.mpq" ||
        !tp::FinalizeMpqRemoval(
            removal,
            error) ||
        std::filesystem::exists(
            data / L"patch-A.mpq")) {
        Fail(
            "MPQ removal did not preserve reservation while clearing installed state");
        return;
    }

    if (!tp::ValidateMpqPackage(
            package,
            error)) {
        Fail(
            "uninstalled MPQ package lost its persisted target reservation");
    }
}

} // namespace

int main() {
    const auto root =
        MakeTempRoot();

    if (root.empty()) {
        Fail(
            "could not create MPQ test root");
    } else {
        TestAllocationAndPersistence(
            root);

        std::error_code ec;
        std::filesystem::remove_all(
            root,
            ec);
        ec.clear();
        std::filesystem::create_directories(
            root / L"Data",
            ec);

        if (ec) {
            Fail(
                "could not reset MPQ test root");
        } else {
            TestValidationAndCollisions();
            TestInstallUpdateIntegrityAndRemoval(
                root);
        }

        std::filesystem::remove_all(
            root,
            ec);
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " MPQ backend test(s) failed\n";
        return 1;
    }

    std::cout
        << "MPQ backend tests passed\n";
    return 0;
}
