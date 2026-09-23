#include "archive.h"
#include "miniz.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

bool WriteFixtureZip(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string>>& files) {
    mz_zip_archive zip{};

    if (!mz_zip_writer_init_heap(
            &zip,
            0,
            0)) {
        return false;
    }

    bool ok = true;
    for (const auto& [name, body] : files) {
        if (!mz_zip_writer_add_mem(
                &zip,
                name.c_str(),
                body.data(),
                body.size(),
                MZ_BEST_SPEED)) {
            ok = false;
            break;
        }
    }

    void* data = nullptr;
    size_t size = 0;

    if (ok) {
        ok =
            mz_zip_writer_finalize_heap_archive(
                &zip,
                &data,
                &size) != 0;
    }

    mz_zip_writer_end(&zip);

    if (!ok || !data || size == 0) {
        if (data) {
            mz_free(data);
        }
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (output) {
        output.write(
            static_cast<const char*>(data),
            static_cast<std::streamsize>(size));
        output.flush();
    }

    const bool written =
        static_cast<bool>(output);

    mz_free(data);
    return written;
}

} // namespace

int main() {
    {
        std::filesystem::path path;
        std::wstring error;

        if (!tp::SafeArchiveRelativePath(
                "wrapper/AddOn/AddOn.toc",
                path,
                error) ||
            path.generic_wstring() !=
                L"wrapper/AddOn/AddOn.toc") {
            Fail("safe archive path was rejected");
        }
    }

    for (const char* unsafe : {
             "../evil.txt",
             "/absolute.txt",
             "C:/evil.txt",
             "wrapper/../evil.txt",
             "wrapper\\evil.txt",
             "wrapper/CON/file.txt",
             "wrapper/name:stream"}) {
        std::filesystem::path path;
        std::wstring error;

        if (tp::SafeArchiveRelativePath(
                unsafe,
                path,
                error)) {
            Fail("unsafe archive path was accepted");
        }
    }

    const auto temp =
        std::filesystem::temp_directory_path() /
        L"TocPilotArchiveTests";

    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp, ec);

    if (ec) {
        Fail("could not create archive test directory");
    } else {
        const auto zipPath =
            temp / L"fixture.zip";
        const auto extracted =
            temp / L"extracted";

        if (!WriteFixtureZip(
                zipPath,
                {
                    {"owner-repo-deadbeef/AddOn/AddOn.toc",
                     "## Interface: 11200\n## Title: AddOn\n"},
                    {"owner-repo-deadbeef/AddOn/main.lua",
                     "print('hello')\n"},
                    {"owner-repo-deadbeef/AddOn_Config/AddOn_Config.toc",
                     "## Interface: 11200\n## Title: AddOn Config\n"}
                })) {
            Fail("could not create ZIP fixture");
        } else {
            std::size_t entries = 0;
            std::uint64_t bytes = 0;
            std::wstring error;

            if (!tp::ExtractZipSecure(
                    zipPath,
                    extracted,
                    entries,
                    bytes,
                    error)) {
                Fail("secure ZIP extraction failed");
            } else {
                if (entries != 3 || bytes == 0) {
                    Fail("ZIP extraction stats were incorrect");
                }

                std::vector<tp::AddonCandidate> candidates;
                if (!tp::DetectAddonCandidates(
                        extracted,
                        candidates,
                        error)) {
                    Fail("addon candidate detection failed");
                } else if (
                    candidates.size() != 2 ||
                    candidates[0].installFolder != L"AddOn" ||
                    candidates[1].installFolder != L"AddOn_Config" ||
                    candidates[0].tocFiles.size() != 1 ||
                    candidates[1].tocFiles.size() != 1) {
                    Fail("addon candidate detection returned unexpected roots");
                }
            }
        }

        const auto rootAddonZipPath =
            temp / L"root-addon.zip";
        const auto rootAddonExtracted =
            temp / L"root-addon-extracted";

        if (!WriteFixtureZip(
                rootAddonZipPath,
                {
                    {"Shagu-pfUI-b2f6df8/pfUI.toc",
                     "## Interface: 11200\n## Title: pfUI\n"},
                    {"Shagu-pfUI-b2f6df8/pfUI-tbc.toc",
                     "## Interface: 20400\n## Title: pfUI TBC\n"},
                    {"Shagu-pfUI-b2f6df8/pfUI.lua",
                     "print('pfUI')\n"}
                })) {
            Fail("could not create GitHub root-addon ZIP fixture");
        } else {
            std::size_t entries = 0;
            std::uint64_t bytes = 0;
            std::wstring error;

            if (!tp::ExtractZipSecure(
                    rootAddonZipPath,
                    rootAddonExtracted,
                    entries,
                    bytes,
                    error)) {
                Fail("GitHub root-addon ZIP extraction failed");
            } else {
                std::vector<tp::AddonCandidate> candidates;
                if (!tp::DetectGitHubAddonCandidates(
                        rootAddonExtracted,
                        L"Shagu/pfUI",
                        {},
                        candidates,
                        error)) {
                    Fail("GitHub root-addon mapping failed");
                } else if (
                    candidates.size() != 1 ||
                    candidates[0].installFolder != L"pfUI" ||
                    candidates[0].sourceRelativePath !=
                        std::filesystem::path(L"Shagu-pfUI-b2f6df8") ||
                    candidates[0].tocFiles.size() != 2) {
                    Fail("GitHub root-addon mapping returned the wrapper name");
                }
            }
        }

        const auto gitLabRootZipPath =
            temp / L"gitlab-root-addon.zip";
        const auto gitLabRootExtracted =
            temp / L"gitlab-root-addon-extracted";

        if (!WriteFixtureZip(
                gitLabRootZipPath,
                {
                    {"pfUI-deadbeef/pfUI.toc",
                     "## Interface: 11200\n## Title: pfUI\n"},
                    {"pfUI-deadbeef/pfUI.lua",
                     "print('pfUI')\n"}
                })) {
            Fail("could not create GitLab root-addon ZIP fixture");
        } else {
            std::size_t entries = 0;
            std::uint64_t bytes = 0;
            std::wstring error;

            if (!tp::ExtractZipSecure(
                    gitLabRootZipPath,
                    gitLabRootExtracted,
                    entries,
                    bytes,
                    error)) {
                Fail("GitLab root-addon ZIP extraction failed");
            } else {
                std::vector<tp::AddonCandidate> candidates;
                if (!tp::DetectRepositoryAddonCandidates(
                        gitLabRootExtracted,
                        L"GitLab",
                        L"group/subgroup/pfUI",
                        {},
                        candidates,
                        error)) {
                    Fail("GitLab root-addon mapping failed");
                } else if (
                    candidates.size() != 1 ||
                    candidates[0].installFolder != L"pfUI") {
                    Fail("GitLab root-addon mapping returned the wrapper name");
                }
            }
        }

        const auto ambiguousZipPath =
            temp / L"ambiguous-root-addon.zip";
        const auto ambiguousExtracted =
            temp / L"ambiguous-root-addon-extracted";

        if (!WriteFixtureZip(
                ambiguousZipPath,
                {
                    {"Owner-Repo-deadbeef/Different.toc",
                     "## Interface: 11200\n## Title: Different\n"}
                })) {
            Fail("could not create ambiguous root-addon ZIP fixture");
        } else {
            std::size_t entries = 0;
            std::uint64_t bytes = 0;
            std::wstring error;

            if (!tp::ExtractZipSecure(
                    ambiguousZipPath,
                    ambiguousExtracted,
                    entries,
                    bytes,
                    error)) {
                Fail("ambiguous root-addon ZIP extraction failed");
            } else {
                std::vector<tp::AddonCandidate> candidates;
                if (!tp::DetectGitHubAddonCandidates(
                        ambiguousExtracted,
                        L"Owner/Repo",
                        {},
                        candidates,
                        error) ||
                    candidates.size() != 1 ||
                    candidates[0].installFolder !=
                        L"Repo") {
                    Fail("root addon with a differently named .toc was not mapped to the repository name");
                }
            }
        }

        const auto preservedRootZipPath =
            temp / L"preserved-root-addon.zip";
        const auto preservedRootExtracted =
            temp / L"preserved-root-addon-extracted";

        if (!WriteFixtureZip(
                preservedRootZipPath,
                {
                    {"Seraphic8x2244-pfUI-VendorTweaks-deadbeef/pfUI_VendorTweaks.toc",
                     "## Interface: 11200\n## Title: pfUI VendorTweaks\n"},
                    {"Seraphic8x2244-pfUI-VendorTweaks-deadbeef/pfUI_VendorTweaks.lua",
                     "print('vendor tweaks')\n"}
                })) {
            Fail("could not create preserved-root ZIP fixture");
        } else {
            std::size_t entries = 0;
            std::uint64_t bytes = 0;
            std::wstring error;

            if (!tp::ExtractZipSecure(
                    preservedRootZipPath,
                    preservedRootExtracted,
                    entries,
                    bytes,
                    error)) {
                Fail("preserved-root ZIP extraction failed");
            } else {
                std::vector<tp::AddonCandidate> candidates;
                if (!tp::DetectGitHubAddonCandidates(
                        preservedRootExtracted,
                        L"Seraphic8x2244/pfUI-VendorTweaks",
                        L"pfUI-VendorTweaks",
                        candidates,
                        error)) {
                    Fail("existing owned root was not preserved");
                } else if (
                    candidates.size() != 1 ||
                    candidates[0].installFolder !=
                        L"pfUI-VendorTweaks") {
                    Fail("root-addon update did not preserve owned install folder");
                }

                candidates.clear();
                error.clear();

                if (!tp::DetectGitHubAddonCandidates(
                        preservedRootExtracted,
                        L"Seraphic8x2244/pfUI-VendorTweaks",
                        {},
                        candidates,
                        error) ||
                    candidates.size() != 1 ||
                    candidates[0].installFolder !=
                        L"pfUI-VendorTweaks") {
                    Fail("fresh root addon was not mapped to the repository name");
                }
            }
        }

        const auto libraryZipPath =
            temp / L"repository-library.zip";
        const auto libraryExtracted =
            temp / L"repository-library-extracted";

        if (!WriteFixtureZip(
                libraryZipPath,
                {
                    {"Cabro-Atlas-deadbeef/Atlas/Atlas.toc",
                     "## Interface: 11200\n## Title: Atlas\n"},
                    {"Cabro-Atlas-deadbeef/Atlas/Atlas.lua",
                     "print('Atlas')\n"},
                    {"Cabro-Atlas-deadbeef/AtlasLoot/AtlasLoot.toc",
                     "## Interface: 11200\n## Title: AtlasLoot\n"},
                    {"Cabro-Atlas-deadbeef/AtlasQuest/AtlasQuest.toc",
                     "## Interface: 11200\n## Title: AtlasQuest\n"}
                })) {
            Fail("could not create repository-library ZIP fixture");
        } else {
            std::size_t entries = 0;
            std::uint64_t bytes = 0;
            std::wstring error;

            if (!tp::ExtractZipSecure(
                    libraryZipPath,
                    libraryExtracted,
                    entries,
                    bytes,
                    error)) {
                Fail("repository-library ZIP extraction failed");
            } else {
                std::vector<tp::AddonCandidate> candidates;
                if (!tp::DetectGitHubAddonCandidates(
                        libraryExtracted,
                        L"Cabro/Atlas",
                        {},
                        candidates,
                        error)) {
                    Fail("repository-library candidate detection failed");
                } else if (
                    candidates.size() != 3 ||
                    candidates[0].repositoryRelativePath !=
                        std::filesystem::path(L"Atlas") ||
                    candidates[1].repositoryRelativePath !=
                        std::filesystem::path(L"AtlasLoot") ||
                    candidates[2].repositoryRelativePath !=
                        std::filesystem::path(L"AtlasQuest") ||
                    !tp::IsRepositoryLibrary(candidates)) {
                    Fail("repository-library sibling roots were not classified correctly");
                } else if (
                    !tp::SelectRepositoryAddonCandidate(
                        L"AtlasLoot",
                        candidates,
                        error) ||
                    candidates.size() != 1 ||
                    candidates[0].installFolder !=
                        L"AtlasLoot" ||
                    candidates[0].repositoryRelativePath !=
                        std::filesystem::path(L"AtlasLoot")) {
                    Fail("repository-library child selection failed");
                }
            }
        }

        {
            const auto shallowExtracted1 =
                temp / L"shallow-root";
            const auto wrapper =
                shallowExtracted1 / L"Owner-Root-deadbeef";
            std::filesystem::create_directories(
                wrapper / L"Embedded" / L"Deep",
                ec);
            std::ofstream(wrapper / L"Root.toc")
                << "## Interface: 11200\n";
            std::ofstream(
                wrapper / L"Embedded" / L"Deep" / L"Deep.toc")
                << "## Interface: 11200\n";

            tp::RepositoryAddonLayout layout;
            std::wstring error;
            if (!tp::DetectShallowRepositoryAddonLayout(
                    shallowExtracted1,
                    L"GitHub",
                    L"Owner/Root",
                    layout,
                    error) ||
                layout.kind !=
                    tp::RepositoryAddonLayoutKind::RootAddon ||
                layout.candidates.size() != 1 ||
                !layout.candidates[0].repositoryRelativePath.empty() ||
                layout.candidates[0].installFolder != L"Root") {
                Fail("shallow root addon classification failed");
            }
        }

        {
            const auto shallowExtracted2 =
                temp / L"shallow-single";
            const auto wrapper =
                shallowExtracted2 / L"Owner-Collection-deadbeef";
            std::filesystem::create_directories(
                wrapper / L"Only" / L"Modules" / L"Deep",
                ec);
            std::ofstream(wrapper / L"Only" / L"Only.toc")
                << "## Interface: 11200\n";
            std::ofstream(
                wrapper / L"Only" / L"Modules" / L"Deep" / L"Deep.toc")
                << "## Interface: 11200\n";

            tp::RepositoryAddonLayout layout;
            std::wstring error;
            if (!tp::DetectShallowRepositoryAddonLayout(
                    shallowExtracted2,
                    L"GitHub",
                    L"Owner/Collection",
                    layout,
                    error) ||
                layout.kind !=
                    tp::RepositoryAddonLayoutKind::SingleNestedAddon ||
                layout.candidates.size() != 1 ||
                layout.candidates[0].repositoryRelativePath !=
                    std::filesystem::path(L"Only")) {
                Fail("shallow single nested addon classification failed");
            }
        }

        {
            const auto shallowExtracted3 =
                temp / L"shallow-library";
            const auto wrapper =
                shallowExtracted3 / L"Cabro-Atlas-deadbeef";
            std::filesystem::create_directories(
                wrapper / L"Atlas",
                ec);
            std::filesystem::create_directories(
                wrapper / L"AtlasLoot",
                ec);
            std::filesystem::create_directories(
                wrapper / L"AtlasQuest",
                ec);
            std::ofstream(wrapper / L"Atlas" / L"Atlas.toc")
                << "## Interface: 11200\n";
            std::ofstream(wrapper / L"AtlasLoot" / L"AtlasLoot.toc")
                << "## Interface: 11200\n";
            std::ofstream(wrapper / L"AtlasQuest" / L"AtlasQuest.toc")
                << "## Interface: 11200\n";

            tp::RepositoryAddonLayout layout;
            std::wstring error;
            if (!tp::DetectShallowRepositoryAddonLayout(
                    shallowExtracted3,
                    L"GitHub",
                    L"Cabro/Atlas",
                    layout,
                    error) ||
                layout.kind !=
                    tp::RepositoryAddonLayoutKind::RepositoryLibrary ||
                layout.candidates.size() != 3 ||
                layout.candidates[0].repositoryRelativePath !=
                    std::filesystem::path(L"Atlas") ||
                layout.candidates[1].repositoryRelativePath !=
                    std::filesystem::path(L"AtlasLoot") ||
                layout.candidates[2].repositoryRelativePath !=
                    std::filesystem::path(L"AtlasQuest")) {
                Fail("shallow repository library classification failed");
            }
        }

        {
            const auto shallowExtracted4 =
                temp / L"shallow-mixed";
            const auto wrapper =
                shallowExtracted4 / L"Owner-Mixed-deadbeef";
            std::filesystem::create_directories(
                wrapper / L"Child",
                ec);
            std::ofstream(wrapper / L"Mixed.toc")
                << "## Interface: 11200\n";
            std::ofstream(wrapper / L"Child" / L"Child.toc")
                << "## Interface: 11200\n";

            tp::RepositoryAddonLayout layout;
            std::wstring error;
            if (!tp::DetectShallowRepositoryAddonLayout(
                    shallowExtracted4,
                    L"GitHub",
                    L"Owner/Mixed",
                    layout,
                    error) ||
                layout.kind !=
                    tp::RepositoryAddonLayoutKind::MixedAmbiguous ||
                layout.candidates.size() != 2) {
                Fail("shallow mixed repository classification failed");
            }
        }

        {
            const auto shallowExtracted5 =
                temp / L"shallow-deep-only";
            const auto wrapper =
                shallowExtracted5 / L"Owner-Deep-deadbeef";
            std::filesystem::create_directories(
                wrapper / L"Top" / L"Deep",
                ec);
            std::ofstream(
                wrapper / L"Top" / L"Deep" / L"Deep.toc")
                << "## Interface: 11200\n";

            tp::RepositoryAddonLayout layout;
            std::wstring error;
            if (!tp::DetectShallowRepositoryAddonLayout(
                    shallowExtracted5,
                    L"GitHub",
                    L"Owner/Deep",
                    layout,
                    error) ||
                layout.kind !=
                    tp::RepositoryAddonLayoutKind::None ||
                !layout.candidates.empty()) {
                Fail("deep-only repository was not ignored by shallow classification");
            }
        }

        {
            std::vector<tp::AddonCandidate> candidates;

            tp::AddonCandidate root;
            root.repositoryRelativePath = {};
            root.installFolder = L"Root";
            candidates.push_back(root);

            tp::AddonCandidate embedded;
            embedded.repositoryRelativePath =
                std::filesystem::path(L"Embedded") /
                L"Deep";
            embedded.installFolder = L"Deep";
            candidates.push_back(embedded);

            std::wstring error;
            if (!tp::SelectRepositoryAddonCandidate(
                    L".",
                    candidates,
                    error) ||
                candidates.size() != 1 ||
                !candidates[0].repositoryRelativePath.empty() ||
                candidates[0].installFolder != L"Root") {
                Fail("explicit repository-root candidate selection failed");
            }
        }

        const auto badZipPath =
            temp / L"unsafe.zip";

        if (!WriteFixtureZip(
                badZipPath,
                {
                    {"../escape.txt", "bad"}
                })) {
            Fail("could not create unsafe ZIP fixture");
        } else {
            std::size_t entries = 0;
            std::uint64_t bytes = 0;
            std::wstring error;
            const auto unsafeExtracted =
                temp / L"unsafe-extracted";

            if (tp::ExtractZipSecure(
                    badZipPath,
                    unsafeExtracted,
                    entries,
                    bytes,
                    error)) {
                Fail("unsafe ZIP archive was extracted");
            }

            if (std::filesystem::exists(
                    temp /
                    L"escape.txt")) {
                Fail("unsafe ZIP escaped extraction root");
            }
        }
    }

    std::filesystem::remove_all(temp, ec);

    if (failures != 0) {
        std::cerr
            << failures
            << " archive inspection test(s) failed\n";
        return 1;
    }

    std::cout
        << "Archive inspection tests passed\n";
    return 0;
}
