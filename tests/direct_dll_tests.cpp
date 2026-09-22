#include "direct_dll.h"

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
