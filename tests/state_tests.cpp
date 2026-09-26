#include "state.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int failures = 0;

void Fail(const std::string& message) {
    std::cerr << message << '\n';
    ++failures;
}

std::filesystem::path MakeTempRoot() {
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD length =
        GetTempPathW(MAX_PATH, temp);
    if (length == 0 || length > MAX_PATH) {
        return {};
    }

    const std::filesystem::path root =
        std::filesystem::path(temp) /
        (L"TocPilotStateTest-" +
         std::to_wstring(GetCurrentProcessId()) +
         L"-" +
         std::to_wstring(GetTickCount64()));

    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return ec ? std::filesystem::path{} : root;
}

std::string ReadAll(
    const std::filesystem::path& path) {
    std::ifstream stream(
        path,
        std::ios::binary);
    std::ostringstream output;
    output << stream.rdbuf();
    return output.str();
}

bool WriteAll(
    const std::filesystem::path& path,
    const std::string& content) {
    std::ofstream stream(
        path,
        std::ios::binary | std::ios::trunc);
    stream.write(
        content.data(),
        static_cast<std::streamsize>(content.size()));
    return stream.good();
}

void TestCreateAddRoundTrip(
    const std::filesystem::path& root) {
    tp::AppState state;
    bool created = false;
    std::wstring error;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("initial LoadOrCreateState failed");
        return;
    }

    if (!created || !state.packages.empty()) {
        Fail("default state was not created empty");
        return;
    }

    tp::PackageRecord package =
        tp::MakeRepositoryPackage(
            L"github",
            L"Shagu/pfUI");

    if (!tp::AppendPackage(
            state,
            package,
            error)) {
        Fail("AppendPackage failed");
        return;
    }

    if (tp::AppendPackage(
            state,
            package,
            error)) {
        Fail("duplicate package was accepted");
        return;
    }

    if (!tp::SetPackageBranch(
            state.packages[0],
            L"master",
            L"0123456789abcdef0123456789abcdef01234567",
            error)) {
        Fail("SetPackageBranch failed");
        return;
    }

    if (!tp::SetPackageLatestRevision(
            state.packages[0],
            L"89abcdef0123456789abcdef0123456789abcdef",
            error) ||
        state.packages[0].ref != L"master" ||
        !state.packages[0].installedRevision.empty()) {
        Fail("SetPackageLatestRevision failed or changed installed tracking");
        return;
    }

    const std::wstring installedSha =
        L"fedcba9876543210fedcba9876543210fedcba98";
    const std::vector<std::wstring> installedFiles{
        L"Interface/AddOns/pfUI/pfUI.toc",
        L"Interface/AddOns/pfUI/core.lua"
    };

    if (!tp::SetPackageInstalledState(
            state.packages[0],
            installedSha,
            installedFiles,
            error) ||
        state.packages[0].installedRevision != installedSha ||
        state.packages[0].latestRevision != installedSha ||
        state.packages[0].installedFiles != installedFiles) {
        Fail("SetPackageInstalledState failed");
        return;
    }

    state.packages[0].installTransaction =
        L"transaction-roundtrip-marker";

    state.settings.textScale = 1.25;
    state.settings.packageSortColumn = 6;
    state.settings.packageSortAscending = false;
    state.settings.packageColumnWidths = {
        260,
        275,
        310,
        105,
        125,
        130,
        170
    };
    state.settings.packageColumnOrder = {
        6,
        0,
        1,
        2,
        3,
        4,
        5
    };
    state.settings.packageColumnsLocked = true;

    if (!tp::SaveState(root, state, error)) {
        Fail("SaveState failed");
        return;
    }

    tp::AppState loaded;
    created = true;
    if (!tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error)) {
        Fail("round-trip LoadOrCreateState failed");
        return;
    }

    if (created ||
        loaded.packages.size() != 1 ||
        loaded.packages[0].name != L"pfUI" ||
        loaded.packages[0].provider != L"github" ||
        loaded.packages[0].repository != L"Shagu/pfUI" ||
        loaded.packages[0].mode != L"branch" ||
        loaded.packages[0].ref != L"master" ||
        loaded.packages[0].installedRevision != installedSha ||
        loaded.packages[0].latestRevision != installedSha ||
        loaded.packages[0].installedFiles != installedFiles ||
        loaded.packages[0].installTransaction !=
            L"transaction-roundtrip-marker" ||
        loaded.settings.textScale != 1.25 ||
        loaded.settings.packageSortColumn != 6 ||
        loaded.settings.packageSortAscending ||
        loaded.settings.packageColumnWidths !=
            std::array<int, tp::kPackageColumnCount>{
                260, 275, 310, 105, 125, 130, 170} ||
        loaded.settings.packageColumnOrder !=
            std::array<int, tp::kPackageColumnCount>{
                6, 0, 1, 2, 3, 4, 5} ||
        !loaded.settings.packageColumnsLocked) {
        Fail("round-trip state values did not match");
    }
}

void TestLegacySixColumnLayoutMigration(
    const std::filesystem::path& root) {
    const std::string json =
        "{\n"
        "  \"schema\": 1,\n"
        "  \"settings\": {"
        "\"text_scale\": 1.0,"
        "\"check_app_updates\": true,"
        "\"package_sort_column\":5,"
        "\"package_sort_ascending\":false,"
        "\"package_column_widths\":[260,310,105,125,130,170],"
        "\"package_column_order\":[5,0,1,2,3,4],"
        "\"package_columns_locked\":true"
        "},\n"
        "  \"packages\": []\n"
        "}\n";

    if (!WriteAll(tp::StatePath(root), json)) {
        Fail("could not write six-column layout fixture");
        return;
    }

    tp::AppState state;
    bool created = true;
    std::wstring error;
    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("six-column layout fixture did not load");
        return;
    }

    if (created ||
        state.settings.packageSortColumn != 6 ||
        state.settings.packageSortAscending ||
        state.settings.packageColumnWidths !=
            std::array<int, tp::kPackageColumnCount>{
                260, 260, 310, 105, 125, 130, 170} ||
        state.settings.packageColumnOrder !=
            std::array<int, tp::kPackageColumnCount>{
                6, 0, 1, 2, 3, 4, 5} ||
        !state.settings.packageColumnsLocked) {
        Fail("six-column layout did not migrate");
    }
}

void TestLegacyFiveColumnLayoutMigration(
    const std::filesystem::path& root) {
    const std::string json =
        "{\n"
        "  \"schema\": 1,\n"
        "  \"settings\": {"
        "\"text_scale\": 1.0,"
        "\"check_app_updates\": true,"
        "\"package_sort_column\":4,"
        "\"package_sort_ascending\":false,"
        "\"package_column_widths\":[260,310,125,130,170],"
        "\"package_column_order\":[4,0,1,2,3],"
        "\"package_columns_locked\":true"
        "},\n"
        "  \"packages\": []\n"
        "}\n";

    if (!WriteAll(tp::StatePath(root), json)) {
        Fail("could not write legacy column-layout fixture");
        return;
    }

    tp::AppState state;
    bool created = true;
    std::wstring error;
    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("legacy column-layout fixture did not load");
        return;
    }

    if (created ||
        state.settings.packageSortColumn != 6 ||
        state.settings.packageSortAscending ||
        state.settings.packageColumnWidths !=
            std::array<int, tp::kPackageColumnCount>{
                260, 260, 310, 110, 125, 130, 170} ||
        state.settings.packageColumnOrder !=
            std::array<int, tp::kPackageColumnCount>{
                6, 0, 1, 2, 3, 4, 5} ||
        !state.settings.packageColumnsLocked) {
        Fail("legacy five-column layout did not migrate");
    }
}

void TestColumnLayoutValidation(
    const std::filesystem::path& root) {
    const std::string json =
        "{\n"
        "  \"schema\": 1,\n"
        "  \"settings\": {"
        "\"text_scale\": 1.0,"
        "\"check_app_updates\": true,"
        "\"package_column_widths\":[10,300,5000,110,115,115,150],"
        "\"package_column_order\":[0,0,1,2,3,4,5],"
        "\"package_columns_locked\":true"
        "},\n"
        "  \"packages\": []\n"
        "}\n";

    if (!WriteAll(tp::StatePath(root), json)) {
        Fail("could not write column-layout validation fixture");
        return;
    }

    tp::AppState state;
    bool created = true;
    std::wstring error;
    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("column-layout validation fixture did not load");
        return;
    }

    if (created ||
        state.settings.packageColumnWidths !=
            std::array<int, tp::kPackageColumnCount>{
                40, 300, 2000, 110, 115, 115, 150} ||
        state.settings.packageColumnOrder !=
            tp::kDefaultPackageColumnOrder ||
        !state.settings.packageColumnsLocked) {
        Fail("column-layout validation did not normalize persisted values");
    }
}

void TestRemovePackageRecord(
    const std::filesystem::path& root) {
    tp::AppState state;
    bool created = false;
    std::wstring error;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("remove-record fixture did not load");
        return;
    }

    state.packages.clear();

    auto first =
        tp::MakeRepositoryPackage(
            L"github",
            L"Shagu/pfUI");
    auto second =
        tp::MakeRepositoryPackage(
            L"github",
            L"brues-code/pfUI");

    if (!tp::AppendPackage(
            state,
            first,
            error) ||
        !tp::AppendPackage(
            state,
            second,
            error)) {
        Fail("remove-record fixture add failed");
        return;
    }

    if (!tp::RemovePackageRecord(
            state,
            L"github:Shagu/pfUI",
            error) ||
        state.packages.size() != 1 ||
        state.packages[0].repository !=
            L"brues-code/pfUI") {
        Fail("RemovePackageRecord removed the wrong record");
        return;
    }

    if (tp::RemovePackageRecord(
            state,
            L"github:missing/repo",
            error)) {
        Fail("RemovePackageRecord accepted a missing package");
        return;
    }

    if (!tp::SaveState(
            root,
            state,
            error)) {
        Fail("remove-record fixture save failed");
        return;
    }

    tp::AppState loaded;
    created = true;
    if (!tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error) ||
        loaded.packages.size() != 1 ||
        loaded.packages[0].repository !=
            L"brues-code/pfUI") {
        Fail("removed package record returned after reload");
    }
}

void TestPackageOwnerReplacement(
    const std::filesystem::path& root) {
    tp::AppState state;
    bool created = false;
    std::wstring error;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("package-owner replacement fixture did not load");
        return;
    }

    state.packages.clear();

    auto oldPackage =
        tp::MakeRepositoryPackage(
            L"github",
            L"OldOwner/Shared");
    auto otherPackage =
        tp::MakeRepositoryPackage(
            L"github",
            L"Owner/Other");

    const std::wstring oldRevision =
        L"1111111111111111111111111111111111111111";
    const std::wstring otherRevision =
        L"2222222222222222222222222222222222222222";

    if (!tp::SetPackageBranch(
            oldPackage,
            L"main",
            oldRevision,
            error) ||
        !tp::SetPackageInstalledState(
            oldPackage,
            oldRevision,
            {
                L"Interface/AddOns/Shared/Shared.toc",
                L"Interface/AddOns/Shared/main.lua",
                L"Interface/AddOns/SharedExtra/SharedExtra.toc"
            },
            error) ||
        !tp::SetPackageBranch(
            otherPackage,
            L"main",
            otherRevision,
            error) ||
        !tp::SetPackageInstalledState(
            otherPackage,
            otherRevision,
            {L"Interface/AddOns/Other/Other.toc"},
            error) ||
        !tp::AppendPackage(
            state,
            oldPackage,
            error) ||
        !tp::AppendPackage(
            state,
            otherPackage,
            error)) {
        Fail("package-owner replacement fixture setup failed");
        return;
    }

    if (tp::FindPackageOwningAddonRoot(
            state,
            L"shared") != 0 ||
        tp::FindPackageOwningAddonRoot(
            state,
            L"SharedExtra") != 0 ||
        tp::FindPackageOwningAddonRoot(
            state,
            L"OTHER") != 1 ||
        tp::FindPackageOwningAddonRoot(
            state,
            L"Missing") !=
            state.packages.size()) {
        Fail("addon-root owner lookup returned the wrong package");
        return;
    }

    auto replacement =
        tp::MakeRepositoryPackage(
            L"github",
            L"NewOwner/Shared");
    replacement.sourcePath = L".";

    const std::wstring newRevision =
        L"3333333333333333333333333333333333333333";

    if (!tp::SetPackageBranch(
            replacement,
            L"master",
            newRevision,
            error) ||
        !tp::SetPackageInstalledState(
            replacement,
            newRevision,
            {
                L"Interface/AddOns/Shared/Shared.toc",
                L"Interface/AddOns/Shared/new.lua"
            },
            error) ||
        !tp::ReplacePackageRecord(
            state,
            oldPackage.id,
            replacement,
            error)) {
        Fail("managed addon owner could not be replaced");
        return;
    }

    if (state.packages.size() != 2 ||
        state.packages[0].repository !=
            L"NewOwner/Shared" ||
        state.packages[0].installedRevision !=
            newRevision ||
        tp::FindPackageOwningAddonRoot(
            state,
            L"Shared") != 0 ||
        tp::FindPackageOwningAddonRoot(
            state,
            L"SharedExtra") !=
            state.packages.size()) {
        Fail("replacement did not transfer addon-root ownership cleanly");
        return;
    }

    auto duplicate =
        tp::MakeRepositoryPackage(
            L"github",
            L"Owner/Other");

    if (tp::ReplacePackageRecord(
            state,
            replacement.id,
            std::move(duplicate),
            error)) {
        Fail("replacement accepted an id already owned by another package");
        return;
    }

    if (!tp::SaveState(
            root,
            state,
            error)) {
        Fail("replacement state could not be saved");
        return;
    }

    tp::AppState loaded;
    created = true;
    if (!tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error) ||
        loaded.packages.size() != 2 ||
        loaded.packages[0].repository !=
            L"NewOwner/Shared" ||
        loaded.packages[0].installedRevision !=
            newRevision ||
        loaded.packages[1].repository !=
            L"Owner/Other") {
        Fail("replacement state did not round-trip");
    }
}


void TestClearInstalledState(
    const std::filesystem::path& root) {
    tp::AppState state;
    bool created = false;
    std::wstring error;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("clear-installed-state fixture did not load");
        return;
    }

    state.packages.clear();
    auto package =
        tp::MakeRepositoryPackage(
            L"github",
            L"Owner/Addon");

    if (!tp::AppendPackage(
            state,
            std::move(package),
            error) ||
        !tp::SetPackageBranch(
            state.packages[0],
            L"main",
            L"1111111111111111111111111111111111111111",
            error) ||
        !tp::SetPackageInstalledState(
            state.packages[0],
            L"1111111111111111111111111111111111111111",
            {
                L"Interface/AddOns/Addon/Addon.toc",
                L"Interface/AddOns/Addon/main.lua"
            },
            error)) {
        Fail("clear-installed-state fixture setup failed");
        return;
    }

    const std::wstring latest =
        state.packages[0].latestRevision;

    if (!tp::ClearPackageInstalledState(
            state.packages[0],
            error) ||
        !state.packages[0].installedRevision.empty() ||
        !state.packages[0].installedFiles.empty() ||
        state.packages[0].latestRevision != latest) {
        Fail("ClearPackageInstalledState changed the wrong tracking fields");
        return;
    }

    if (!tp::SaveState(
            root,
            state,
            error)) {
        Fail("cleared installed state did not save");
        return;
    }

    tp::AppState loaded;
    created = true;
    if (!tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error) ||
        loaded.packages.size() != 1 ||
        !loaded.packages[0].installedRevision.empty() ||
        !loaded.packages[0].installedFiles.empty() ||
        loaded.packages[0].latestRevision != latest) {
        Fail("cleared installed state did not round-trip");
    }
}


void TestGitLabBranchMutators() {
    tp::PackageRecord package =
        tp::MakeRepositoryPackage(
            L"gitlab",
            L"group/subgroup/Addon");

    std::wstring error;
    const std::wstring installed =
        L"1111111111111111111111111111111111111111";
    const std::wstring latest =
        L"2222222222222222222222222222222222222222";

    if (!tp::SetPackageBranch(
            package,
            L"main",
            installed,
            error) ||
        package.mode != L"branch" ||
        package.ref != L"main" ||
        package.latestRevision != installed) {
        Fail("GitLab branch state could not be configured");
        return;
    }

    if (!tp::SetPackageInstalledState(
            package,
            installed,
            {L"Interface/AddOns/Addon/Addon.toc"},
            error) ||
        package.installedRevision != installed) {
        Fail("GitLab branch installed state could not be recorded");
        return;
    }

    if (!tp::SetPackageLatestRevision(
            package,
            latest,
            error) ||
        package.installedRevision != installed ||
        package.latestRevision != latest) {
        Fail("GitLab branch latest revision could not be refreshed");
    }
}

void TestRepositoryChildPackages(
    const std::filesystem::path& root) {
    tp::AppState state;
    bool created = false;
    std::wstring error;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("repository-child fixture did not load");
        return;
    }

    state.packages.clear();

    auto atlas =
        tp::MakeRepositoryAddonPackage(
            L"github",
            L"Cabro/Atlas",
            L"Atlas",
            L"Atlas");
    auto loot =
        tp::MakeRepositoryAddonPackage(
            L"github",
            L"Cabro/Atlas",
            L"AtlasLoot",
            L"AtlasLoot");

    if (atlas.id == loot.id ||
        atlas.sourcePath != L"Atlas" ||
        loot.sourcePath != L"AtlasLoot") {
        Fail("repository child package identities were not unique");
        return;
    }

    const std::wstring revision =
        L"0123456789abcdef0123456789abcdef01234567";

    if (!tp::SetPackageBranch(
            atlas,
            L"master",
            revision,
            error) ||
        !tp::SetPackageBranch(
            loot,
            L"master",
            revision,
            error) ||
        !tp::AppendPackage(
            state,
            std::move(atlas),
            error) ||
        !tp::AppendPackage(
            state,
            std::move(loot),
            error) ||
        !tp::SaveState(
            root,
            state,
            error)) {
        Fail("repository child packages could not be saved");
        return;
    }

    tp::AppState loaded;
    created = true;
    if (!tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error) ||
        loaded.packages.size() != 2 ||
        loaded.packages[0].sourcePath != L"Atlas" ||
        loaded.packages[1].sourcePath != L"AtlasLoot" ||
        loaded.packages[0].repository != L"Cabro/Atlas" ||
        loaded.packages[1].repository != L"Cabro/Atlas") {
        Fail("repository child package paths did not round-trip");
        return;
    }

    const std::string saved =
        ReadAll(tp::StatePath(root));
    if (saved.find("\"source_path\":\"Atlas\"") ==
            std::string::npos ||
        saved.find("\"source_path\":\"AtlasLoot\"") ==
            std::string::npos) {
        Fail("repository child source paths were not serialized");
    }
}

void TestRepositoryRootSourcePath(
    const std::filesystem::path& root) {
    tp::AppState state;
    bool created = false;
    std::wstring error;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("repository-root source-path fixture did not load");
        return;
    }

    state.packages.clear();

    auto package =
        tp::MakeRepositoryPackage(
            L"github",
            L"Owner/RootAddon");
    package.sourcePath = L".";

    if (!tp::SetPackageBranch(
            package,
            L"main",
            L"0123456789abcdef0123456789abcdef01234567",
            error) ||
        !tp::AppendPackage(
            state,
            std::move(package),
            error) ||
        !tp::SaveState(
            root,
            state,
            error)) {
        Fail("repository-root source-path fixture could not be saved");
        return;
    }

    tp::AppState loaded;
    created = true;

    if (!tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error) ||
        loaded.packages.size() != 1 ||
        loaded.packages[0].id !=
            L"github:Owner/RootAddon" ||
        loaded.packages[0].sourcePath !=
            L"." ||
        loaded.packages[0].ref !=
            L"main") {
        Fail("explicit repository-root source path did not round-trip");
    }
}

void TestReleasePackageRoundTrip(
    const std::filesystem::path& root) {
    tp::AppState state;
    bool created = false;
    std::wstring error;

    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("release-package fixture did not load");
        return;
    }

    state.packages.clear();

    auto package =
        tp::MakeRepositoryPackage(
            L"github",
            L"Owner/ClassicAPI");

    package.mode =
        L"release";
    package.releasePolicy =
        L"latest_stable";
    package.asset =
        L"ClassicAPI.dll";
    package.target =
        L"wow_root";
    package.targetPath =
        L"ClassicAPI.dll";
    package.installedRevision =
        L"v1.2.3";
    package.latestRevision =
        L"v1.2.4";
    package.installedFiles = {
        L"ClassicAPI.dll"
    };

    if (!tp::AppendPackage(
            state,
            std::move(package),
            error) ||
        !tp::SaveState(
            root,
            state,
            error)) {
        Fail("release-package fixture did not save");
        return;
    }

    tp::AppState loaded;
    created = true;

    if (!tp::LoadOrCreateState(
            root,
            loaded,
            created,
            error) ||
        loaded.packages.size() != 1) {
        Fail("release-package fixture did not reload");
        return;
    }

    const auto& actual =
        loaded.packages[0];

    if (actual.mode != L"release" ||
        actual.releasePolicy != L"latest_stable" ||
        actual.asset != L"ClassicAPI.dll" ||
        actual.target != L"wow_root" ||
        actual.targetPath != L"ClassicAPI.dll" ||
        actual.installedRevision != L"v1.2.3" ||
        actual.latestRevision != L"v1.2.4" ||
        actual.installedFiles !=
            std::vector<std::wstring>{L"ClassicAPI.dll"}) {
        Fail("release/direct-file package fields did not round-trip");
        return;
    }

    const std::string saved =
        ReadAll(tp::StatePath(root));

    if (saved.find("\"release_policy\":\"latest_stable\"") ==
            std::string::npos ||
        saved.find("\"asset\":\"ClassicAPI.dll\"") ==
            std::string::npos ||
        saved.find("\"target_path\":\"ClassicAPI.dll\"") ==
            std::string::npos) {
        Fail("release/direct-file package fields were not serialized");
    }
}


void TestReleaseTrackingMutators(
    const std::filesystem::path&) {
    tp::PackageRecord package;
    package.id =
        L"github:Owner/ClassicAPI:release:ClassicAPI.dll";
    package.name =
        L"ClassicAPI.dll";
    package.provider =
        L"github";
    package.repository =
        L"Owner/ClassicAPI";
    package.mode =
        L"release";
    package.releasePolicy =
        L"latest_stable";
    package.asset =
        L"ClassicAPI.dll";
    package.target =
        L"wow_root";
    package.targetPath =
        L"ClassicAPI.dll";

    std::wstring error;
    if (!tp::SetPackageLatestRevision(
            package,
            L"v2.0.0",
            error) ||
        package.latestRevision !=
            L"v2.0.0") {
        Fail("release latest revision could not be recorded");
        return;
    }

    if (!tp::SetPackageInstalledState(
            package,
            L"v2.0.0",
            {L"ClassicAPI.dll"},
            error) ||
        package.installedRevision !=
            L"v2.0.0" ||
        package.latestRevision !=
            L"v2.0.0" ||
        package.installedFiles !=
            std::vector<std::wstring>{
                L"ClassicAPI.dll"}) {
        Fail("release installed state could not be recorded");
        return;
    }

    if (tp::SetPackageInstalledState(
            package,
            L"v2.0.1",
            {L"renamed.dll"},
            error)) {
        Fail("release installed state accepted wrong owned target");
    }
}

void TestUnknownFieldPreservation(
    const std::filesystem::path& root) {
    const std::string json =
        "{\n"
        "  \"schema\": 1,\n"
        "  \"settings\": {"
        "\"text_scale\": 1.0,"
        "\"check_app_updates\": true"
        "},\n"
        "  \"future_top\": {\"enabled\": true},\n"
        "  \"packages\": ["
        "{"
        "\"id\":\"gitlab:group/project\","
        "\"name\":\"project\","
        "\"provider\":\"gitlab\","
        "\"repository\":\"group/project\","
        "\"mode\":\"unconfigured\","
        "\"target\":\"addons\","
        "\"future_package\":{\"answer\":42}"
        "}"
        "]\n"
        "}\n";

    if (!WriteAll(tp::StatePath(root), json)) {
        Fail("could not write preservation fixture");
        return;
    }

    tp::AppState state;
    bool created = false;
    std::wstring error;
    if (!tp::LoadOrCreateState(
            root,
            state,
            created,
            error)) {
        Fail("preservation fixture did not load");
        return;
    }

    state.settings.textScale = 1.10;
    if (!tp::SaveState(root, state, error)) {
        Fail("preservation fixture did not save");
        return;
    }

    const std::string saved =
        ReadAll(tp::StatePath(root));
    if (saved.find("\"future_top\"") ==
            std::string::npos ||
        saved.find("\"future_package\"") ==
            std::string::npos) {
        Fail("unknown fields were not preserved");
    }
}

} // namespace

int main() {
    const std::filesystem::path root =
        MakeTempRoot();
    if (root.empty()) {
        Fail("could not create temporary test directory");
    } else {
        TestCreateAddRoundTrip(root);
        TestLegacySixColumnLayoutMigration(root);
        TestLegacyFiveColumnLayoutMigration(root);
    TestColumnLayoutValidation(root);
        TestRemovePackageRecord(root);
        TestPackageOwnerReplacement(root);
        TestClearInstalledState(root);
        TestGitLabBranchMutators();
        TestRepositoryChildPackages(root);
        TestRepositoryRootSourcePath(root);
        TestReleasePackageRoundTrip(root);
        TestReleaseTrackingMutators(root);
        TestUnknownFieldPreservation(root);

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " state test(s) failed\n";
        return 1;
    }

    std::cout << "state tests passed\n";
    return 0;
}
