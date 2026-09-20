#include "git_refs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

std::string Pkt(const std::string& payload) {
    char prefix[5]{};
    std::snprintf(
        prefix,
        sizeof(prefix),
        "%04x",
        static_cast<unsigned>(payload.size() + 4));
    return std::string(prefix, 4) + payload;
}

std::string Advertisement() {
    const std::string a(40, 'a');
    const std::string b(40, 'b');

    std::string first = a + " HEAD";
    first.push_back('\0');
    first += "multi_ack thin-pack symref=HEAD:refs/heads/main\n";

    return
        Pkt("# service=git-upload-pack\n") +
        "0000" +
        Pkt(first) +
        Pkt(a + " refs/heads/main\n") +
        Pkt(b + " refs/heads/feature/test\n") +
        "0000";
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--live-github") {
        std::wstring sha;
        std::wstring error;
        if (!tp::ResolvePublicGitBranchHead(
                L"github.com",
                L"Seraphic8x2244/TocPilot",
                L"main",
                sha,
                error)) {
            std::wcerr
                << L"live GitHub smart-HTTP lookup failed: "
                << error
                << L'\n';
            return 1;
        }

        const bool valid =
            (sha.size() == 40 || sha.size() == 64) &&
            std::all_of(
                sha.begin(),
                sha.end(),
                [](wchar_t ch) {
                    return
                        (ch >= L'0' && ch <= L'9') ||
                        (ch >= L'a' && ch <= L'f') ||
                        (ch >= L'A' && ch <= L'F');
                });

        if (!valid) {
            std::wcerr
                << L"live GitHub smart-HTTP lookup returned invalid SHA: "
                << sha
                << L'\n';
            return 1;
        }

        std::wcout
            << L"Live GitHub smart-HTTP lookup passed: "
            << sha
            << L'\n';
        return 0;
    }

    {
        std::string sha;
        std::wstring error;
        if (!tp::ParseGitSmartHttpBranchAdvertisement(
                Advertisement(),
                "main",
                sha,
                error) ||
            sha != std::string(40, 'a')) {
            Fail("main branch smart-HTTP ref lookup failed");
        }
    }

    {
        std::string sha;
        std::wstring error;
        if (!tp::ParseGitSmartHttpBranchAdvertisement(
                Advertisement(),
                "feature/test",
                sha,
                error) ||
            sha != std::string(40, 'b')) {
            Fail("slash-containing branch smart-HTTP lookup failed");
        }
    }

    {
        std::string sha;
        std::wstring error;
        if (tp::ParseGitSmartHttpBranchAdvertisement(
                Advertisement(),
                "missing",
                sha,
                error) ||
            error.find(L"not found") == std::wstring::npos) {
            Fail("missing branch was not rejected");
        }
    }

    {
        std::string sha;
        std::wstring error;
        if (tp::ParseGitSmartHttpBranchAdvertisement(
                "zzzzbroken",
                "main",
                sha,
                error)) {
            Fail("invalid pkt-line length was accepted");
        }
    }

    {
        std::string sha;
        std::wstring error;
        if (tp::ParseGitSmartHttpBranchAdvertisement(
                "0010abc",
                "main",
                sha,
                error)) {
            Fail("truncated pkt-line was accepted");
        }
    }

    {
        const std::string body =
            Pkt("# service=git-upload-pack\n") +
            "0000" +
            Pkt("version 2\n");

        std::string sha;
        std::wstring error;
        if (tp::ParseGitSmartHttpBranchAdvertisement(
                body,
                "main",
                sha,
                error) ||
            error.find(L"protocol v2") == std::wstring::npos) {
            Fail("unsupported protocol v2 response was not rejected clearly");
        }
    }

    {
        const std::string body =
            Pkt("# service=git-upload-pack\n") +
            "0000" +
            Pkt(std::string(39, 'a') + " refs/heads/main\n") +
            "0000";

        std::string sha;
        std::wstring error;
        if (tp::ParseGitSmartHttpBranchAdvertisement(
                body,
                "main",
                sha,
                error)) {
            Fail("invalid object ID length was accepted");
        }
    }

    {
        const std::string sha256(64, 'c');
        const std::string body =
            Pkt("# service=git-upload-pack\n") +
            "0000" +
            Pkt(sha256 + " refs/heads/main\n") +
            "0000";

        std::string sha;
        std::wstring error;
        if (!tp::ParseGitSmartHttpBranchAdvertisement(
                body,
                "main",
                sha,
                error) ||
            sha != sha256) {
            Fail("64-character object ID was not accepted");
        }
    }

    {
        const std::string body =
            Pkt(std::string(40, 'a') + " refs/heads/main\n") +
            "0000";

        std::string sha;
        std::wstring error;
        if (tp::ParseGitSmartHttpBranchAdvertisement(
                body,
                "main",
                sha,
                error)) {
            Fail("non-smart-HTTP response without service header was accepted");
        }
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " smart-HTTP ref parser test(s) failed\n";
        return 1;
    }

    std::cout << "Smart-HTTP ref parser tests passed\n";
    return 0;
}
