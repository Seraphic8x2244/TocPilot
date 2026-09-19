#include "github_api.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

} // namespace

int main() {
    {
        std::wstring defaultBranch;
        std::wstring error;

        if (!tp::ParseGitHubRepositoryJson(
                R"({"id":1,"default_branch":"master","private":false})",
                defaultBranch,
                error) ||
            defaultBranch != L"master") {
            Fail("repository metadata parser failed");
        }
    }

    {
        const std::string json =
            R"([{"name":"master","commit":{"sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}},{"name":"development","commit":{"sha":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}}])";

        std::vector<tp::GitHubBranch> branches;
        std::wstring error;

        if (!tp::ParseGitHubBranchesJson(
                json,
                branches,
                error) ||
            branches.size() != 2 ||
            branches[0].name != L"master" ||
            branches[0].sha !=
                L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ||
            branches[1].name != L"development" ||
            branches[1].sha !=
                L"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") {
            Fail("branch parser failed");
        }
    }

    {
        std::vector<tp::GitHubBranch> branches;
        std::wstring error;

        if (!tp::ParseGitHubBranchesJson(
                R"([])",
                branches,
                error) ||
            !branches.empty()) {
            Fail("empty branch array parser failed");
        }
    }

    {
        std::vector<tp::GitHubBranch> branches;
        std::wstring error;

        if (tp::ParseGitHubBranchesJson(
                R"([{"name":"master","commit":{}}])",
                branches,
                error)) {
            Fail("missing branch SHA was accepted");
        }
    }

    {
        std::vector<tp::GitHubBranch> branches{
            {L"master", L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
            {L"vanillaplus", L"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}
        };
        std::wstring remoteSha;
        std::wstring error;

        if (!tp::FindGitHubBranchHead(
                branches,
                L"vanillaplus",
                remoteSha,
                error) ||
            remoteSha !=
                L"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") {
            Fail("tracked branch head lookup failed");
        }
    }

    {
        std::vector<tp::GitHubBranch> branches{
            {L"master", L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}
        };
        std::wstring remoteSha;
        std::wstring error;

        if (tp::FindGitHubBranchHead(
                branches,
                L"Master",
                remoteSha,
                error)) {
            Fail("branch lookup incorrectly ignored case");
        }
    }


    {
        std::wstring path;
        std::wstring error;

        if (!tp::BuildGitHubArchiveApiPath(
                L"Owner/Repo",
                L"feature/test branch",
                path,
                error) ||
            path !=
                L"/repos/Owner/Repo/zipball/feature%2Ftest%20branch") {
            Fail("GitHub archive API path encoding failed");
        }
    }

    {
        std::wstring path;
        std::wstring error;

        if (tp::BuildGitHubArchiveApiPath(
                L"Owner/Repo/Extra",
                L"main",
                path,
                error) ||
            tp::BuildGitHubArchiveApiPath(
                L"Owner/Repo",
                L"",
                path,
                error)) {
            Fail("invalid GitHub archive identity/ref was accepted");
        }
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " GitHub API parser test(s) failed\n";
        return 1;
    }

    std::cout
        << "GitHub API parser tests passed\n";
    return 0;
}
