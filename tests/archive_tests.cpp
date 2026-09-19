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
                    temp.parent_path() /
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
