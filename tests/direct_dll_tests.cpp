#include "direct_dll.h"

#include <filesystem>
#include <fstream>
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

std::string ReadBytes(
    const std::filesystem::path& path) {
    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        return {};
    }

    input.seekg(
        0,
        std::ios::end);
    const auto length =
        input.tellg();
    input.seekg(
        0,
        std::ios::beg);

    if (length < 0) {
        return {};
    }

    std::string bytes(
        static_cast<std::size_t>(
            length),
        '\0');

    if (!bytes.empty()) {
        input.read(
            bytes.data(),
            static_cast<std::streamsize>(
                bytes.size()));
    }

    return bytes;
}

bool WriteBytes(
    const std::filesystem::path& path,
    std::string_view bytes) {
    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!output) {
        return false;
    }

    output.write(
        bytes.data(),
        static_cast<std::streamsize>(
            bytes.size()));
    output.flush();
    return
        static_cast<bool>(
            output);
}

tp::PackageRecord ValidPackage() {
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
    return package;
}

void ExpectValid() {
    auto package =
        ValidPackage();
    std::wstring error;

    if (!tp::IsDirectDllPackage(
            package) ||
        !tp::ValidateDirectDllPackage(
            package,
            error)) {
        Fail(
            "valid direct DLL package was rejected");
    }

    std::filesystem::path target;
    if (!tp::DirectDllTargetPath(
            L"C:\\Games\\World of Warcraft",
            package,
            target,
            error) ||
        target !=
            std::filesystem::path(
                L"C:\\Games\\World of Warcraft\\ClassicAPI.dll")) {
        Fail(
            "direct DLL target path was not exact");
    }
}

void ExpectExactRemoval() {
    const auto root =
        std::filesystem::temp_directory_path() /
        L"TocPilotDirectDllRemovalTests";

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    ec.clear();
    std::filesystem::create_directories(
        root,
        ec);

    if (ec) {
        Fail(
            "could not create direct DLL removal test root");
        return;
    }

    const auto target =
        root / L"ClassicAPI.dll";

    {
        std::ofstream file(
            target,
            std::ios::binary);
        file << "dll";
    }

    auto package =
        ValidPackage();
    bool removed = false;
    std::wstring error;

    if (!tp::RemoveDirectDll(
            package,
            root,
            removed,
            error) ||
        !removed ||
        std::filesystem::exists(
            target)) {
        Fail(
            "exact direct DLL removal failed");
    }

    removed = true;
    error.clear();

    if (!tp::RemoveDirectDll(
            package,
            root,
            removed,
            error) ||
        removed) {
        Fail(
            "missing direct DLL was not treated as already removed");
    }

    std::filesystem::remove_all(
        root,
        ec);
}

void ExpectDllsTxtRegistration() {
    const auto root =
        std::filesystem::temp_directory_path() /
        L"TocPilotDirectDllLoaderTests";

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    ec.clear();
    std::filesystem::create_directories(
        root,
        ec);

    if (ec) {
        Fail(
            "could not create direct DLL loader test root");
        return;
    }

    const auto loader =
        root / L"dlls.txt";
    auto package =
        ValidPackage();
    bool changed = false;
    std::wstring error;

    if (!tp::EnsureDirectDllLoaderEntry(
            root,
            package,
            changed,
            error) ||
        !changed ||
        ReadBytes(loader) !=
            "ClassicAPI.dll\r\n") {
        Fail(
            "missing dlls.txt was not created with the exact DLL entry");
    }

    const std::string existing =
        "# keep this comment\r\n"
        "nampower.dll\r\n"
        "  another.dll  \r\n"
        "# ClassicAPI.dll is only mentioned here\r\n"
        "last.dll";

    if (!WriteBytes(
            loader,
            existing)) {
        Fail(
            "could not prepare dlls.txt preservation fixture");
    } else {
        changed = false;
        error.clear();

        if (!tp::EnsureDirectDllLoaderEntry(
                root,
                package,
                changed,
                error) ||
            !changed ||
            ReadBytes(loader) !=
                existing +
                "\r\nClassicAPI.dll\r\n") {
            Fail(
                "dlls.txt append did not preserve existing content exactly");
        }
    }

    const std::string duplicate =
        "# untouched\n"
        "\tclassicapi.DLL  \n"
        "OtherHook.dll\n";

    if (!WriteBytes(
            loader,
            duplicate)) {
        Fail(
            "could not prepare dlls.txt duplicate fixture");
    } else {
        changed = true;
        error.clear();

        if (!tp::EnsureDirectDllLoaderEntry(
                root,
                package,
                changed,
                error) ||
            changed ||
            ReadBytes(loader) !=
                duplicate) {
            Fail(
                "existing dlls.txt entry was duplicated or rewritten");
        }
    }

    const std::string utf16 =
        std::string("\xFF\xFE", 2) +
        "C\0l\0a\0s\0s\0i\0c\0A\0P\0I\0.\0d\0l\0l\0\r\0\n\0";

    if (!WriteBytes(
            loader,
            utf16)) {
        Fail(
            "could not prepare UTF-16 dlls.txt fixture");
    } else {
        changed = true;
        error.clear();

        if (tp::EnsureDirectDllLoaderEntry(
                root,
                package,
                changed,
                error) ||
            changed ||
            ReadBytes(loader) !=
                utf16) {
            Fail(
                "UTF-16 dlls.txt was modified instead of being left untouched");
        }
    }

    std::filesystem::remove_all(
        root,
        ec);
}

void ExpectInvalidVariants() {
    std::wstring error;

    {
        auto package =
            ValidPackage();
        package.releasePolicy =
            L"latest_prerelease";
        if (tp::ValidateDirectDllPackage(
                package,
                error)) {
            Fail(
                "prerelease DLL policy was accepted");
        }
    }

    {
        auto package =
            ValidPackage();
        package.asset =
            L"ClassicAPI.mpq";
        package.targetPath =
            package.asset;
        if (tp::ValidateDirectDllPackage(
                package,
                error)) {
            Fail(
                "MPQ release asset was accepted as a direct DLL");
        }
    }

    {
        auto package =
            ValidPackage();
        package.asset =
            L"ClassicAPI.zip";
        package.targetPath =
            package.asset;
        if (tp::ValidateDirectDllPackage(
                package,
                error)) {
            Fail(
                "non-DLL release asset was accepted");
        }
    }

    {
        auto package =
            ValidPackage();
        package.targetPath =
            L"renamed.dll";
        if (tp::ValidateDirectDllPackage(
                package,
                error)) {
            Fail(
                "renamed DLL destination was accepted");
        }
    }

    {
        auto package =
            ValidPackage();
        package.asset =
            L"bin\\ClassicAPI.dll";
        package.targetPath =
            package.asset;
        if (tp::ValidateDirectDllPackage(
                package,
                error)) {
            Fail(
                "nested DLL destination was accepted");
        }
    }

    {
        auto package =
            ValidPackage();
        package.target =
            L"addons";
        if (tp::IsDirectDllPackage(
                package) ||
            tp::ValidateDirectDllPackage(
                package,
                error)) {
            Fail(
                "addon target was accepted as direct DLL");
        }
    }
}

} // namespace

int main() {
    ExpectValid();
    ExpectExactRemoval();
    ExpectDllsTxtRegistration();
    ExpectInvalidVariants();

    if (failures != 0) {
        std::cerr
            << failures
            << " direct DLL policy test(s) failed\n";
        return 1;
    }

    std::cout
        << "Direct DLL policy tests passed\n";
    return 0;
}
