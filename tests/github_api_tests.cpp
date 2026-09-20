#include "github_api.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

} // namespace

int main() {
    {
        std::wstring path;
        std::wstring error;

        if (!tp::BuildGitHubCodeloadPath(
                L"Owner/Repo",
                L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                path,
                error) ||
            path !=
                L"/Owner/Repo/zip/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") {
            Fail("GitHub codeload exact-SHA path failed");
        }
    }

    {
        std::wstring path;
        std::wstring error;

        if (!tp::BuildGitHubCodeloadPath(
                L"Owner/Repo",
                L"feature/test branch",
                path,
                error) ||
            path !=
                L"/Owner/Repo/zip/feature%2Ftest%20branch") {
            Fail("GitHub codeload ref encoding failed");
        }
    }

    {
        std::wstring path;
        std::wstring error;

        if (tp::BuildGitHubCodeloadPath(
                L"Owner/Repo/Extra",
                L"main",
                path,
                error) ||
            tp::BuildGitHubCodeloadPath(
                L"Owner/Repo",
                L"",
                path,
                error)) {
            Fail("invalid GitHub codeload identity/ref was accepted");
        }
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " GitHub transport test(s) failed\n";
        return 1;
    }

    std::cout
        << "GitHub transport tests passed\n";
    return 0;
}
