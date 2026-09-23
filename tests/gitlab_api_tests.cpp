#include "gitlab_api.h"

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

        if (!tp::BuildGitLabArchivePath(
                L"group/subgroup/project",
                L"0123456789abcdef0123456789abcdef01234567",
                path,
                error) ||
            path !=
                L"/api/v4/projects/group%2Fsubgroup%2Fproject/repository/archive.zip?sha=0123456789abcdef0123456789abcdef01234567&include_lfs_blobs=false") {
            Fail("nested GitLab project archive path was incorrect");
        }
    }

    {
        std::wstring path;
        std::wstring error;

        if (!tp::BuildGitLabArchivePath(
                L"group/project",
                L"feature/test",
                path,
                error) ||
            path.find(L"feature%2Ftest") ==
                std::wstring::npos) {
            Fail("GitLab archive ref was not URL encoded");
        }
    }

    for (const wchar_t* repository : {
             L"",
             L"group",
             L"/group/project",
             L"group/project/",
             L"group/../project"}) {
        std::wstring path;
        std::wstring error;

        if (tp::BuildGitLabArchivePath(
                repository,
                L"main",
                path,
                error)) {
            Fail("invalid GitLab repository identity was accepted");
        }
    }

    {
        std::wstring path;
        std::wstring error;

        if (tp::BuildGitLabArchivePath(
                L"group/project",
                L"",
                path,
                error)) {
            Fail("empty GitLab archive ref was accepted");
        }
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " GitLab archive path test(s) failed\n";
        return 1;
    }

    std::cout
        << "GitLab archive path tests passed\n";
    return 0;
}
