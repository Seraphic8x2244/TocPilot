#include "provider.h"

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void ExpectOk(
    std::wstring_view input,
    tp::ProviderKind provider,
    std::wstring_view repository,
    std::wstring_view canonical) {
    tp::RepositoryIdentity identity;
    std::wstring error;
    if (!tp::NormalizeRepositoryUrl(
            input,
            identity,
            error)) {
        std::wcerr
            << L"Expected success for: "
            << input
            << L" -> "
            << error
            << L'\n';
        ++failures;
        return;
    }

    if (identity.provider != provider ||
        identity.repository != repository ||
        identity.canonicalUrl != canonical) {
        std::wcerr
            << L"Unexpected normalization for: "
            << input
            << L'\n'
            << L"  repository: "
            << identity.repository
            << L'\n'
            << L"  canonical: "
            << identity.canonicalUrl
            << L'\n';
        ++failures;
    }
}

void ExpectFail(std::wstring_view input) {
    tp::RepositoryIdentity identity;
    std::wstring error;
    if (tp::NormalizeRepositoryUrl(
            input,
            identity,
            error)) {
        std::wcerr
            << L"Expected failure for: "
            << input
            << L'\n';
        ++failures;
    }
}

} // namespace

int main() {
    ExpectOk(
        L"https://github.com/Shagu/pfUI",
        tp::ProviderKind::GitHub,
        L"Shagu/pfUI",
        L"https://github.com/Shagu/pfUI");
    ExpectOk(
        L"github.com/Shagu/pfUI.git/",
        tp::ProviderKind::GitHub,
        L"Shagu/pfUI",
        L"https://github.com/Shagu/pfUI");
    ExpectOk(
        L"git@github.com:Shagu/pfUI.git",
        tp::ProviderKind::GitHub,
        L"Shagu/pfUI",
        L"https://github.com/Shagu/pfUI");
    ExpectOk(
        L"https://github.com/Shagu/pfUI/tree/master",
        tp::ProviderKind::GitHub,
        L"Shagu/pfUI",
        L"https://github.com/Shagu/pfUI");
    ExpectOk(
        L"https://gitlab.com/group/subgroup/project.git",
        tp::ProviderKind::GitLab,
        L"group/subgroup/project",
        L"https://gitlab.com/group/subgroup/project");
    ExpectOk(
        L"git@gitlab.com:group/subgroup/project.git",
        tp::ProviderKind::GitLab,
        L"group/subgroup/project",
        L"https://gitlab.com/group/subgroup/project");
    ExpectOk(
        L"https://gitlab.com/group/subgroup/project/-/tree/main?ref_type=heads",
        tp::ProviderKind::GitLab,
        L"group/subgroup/project",
        L"https://gitlab.com/group/subgroup/project");
    ExpectOk(
        L"  https://www.github.com/Owner/Repo.git#readme  ",
        tp::ProviderKind::GitHub,
        L"Owner/Repo",
        L"https://github.com/Owner/Repo");

    ExpectFail(L"");
    ExpectFail(L"https://github.com/owner");
    ExpectFail(L"https://bitbucket.org/owner/repo");
    ExpectFail(L"https://github.com/../repo");
    ExpectFail(L"https://gitlab.example.com/group/repo");

    if (failures != 0) {
        std::cerr
            << failures
            << " provider URL test(s) failed\n";
        return 1;
    }

    std::cout << "provider URL tests passed\n";
    return 0;
}
