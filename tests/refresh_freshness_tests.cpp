#include "refresh_freshness.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Fail(const char* message) {
    std::cerr << message << '\n';
    ++failures;
}

tp::PackageRecord MakePackage() {
    tp::PackageRecord package;
    package.id = L"github:owner/addon";
    package.provider = L"github";
    package.repository = L"owner/addon";
    package.mode = L"branch";
    package.ref = L"main";
    package.target = L"addons";
    package.installedRevision =
        L"1111111111111111111111111111111111111111";
    package.latestRevision =
        L"2222222222222222222222222222222222222222";
    package.installedFiles = {
        L"Interface/AddOns/Addon/Addon.toc"
    };
    return package;
}

void TestFreshness() {
    auto package = MakePackage();

    std::vector<tp::PackageRefreshStamp>
        stamps;

    if (tp::IsPackageRefreshFresh(
            stamps,
            package,
            1000)) {
        Fail("missing refresh stamp was treated as fresh");
    }

    tp::RecordPackageRefresh(
        stamps,
        package,
        1000);

    if (!tp::IsPackageRefreshFresh(
            stamps,
            package,
            1000) ||
        !tp::IsPackageRefreshFresh(
            stamps,
            package,
            1000 +
                tp::kPackageRefreshFreshnessMs)) {
        Fail("fresh refresh stamp was not reusable");
    }

    if (tp::IsPackageRefreshFresh(
            stamps,
            package,
            1001 +
                tp::kPackageRefreshFreshnessMs)) {
        Fail("expired refresh stamp was treated as fresh");
    }

    package.ref = L"dev";
    if (tp::IsPackageRefreshFresh(
            stamps,
            package,
            1001)) {
        Fail("branch change did not invalidate freshness");
    }

    package.ref = L"main";
    package.latestRevision =
        L"3333333333333333333333333333333333333333";

    if (tp::IsPackageRefreshFresh(
            stamps,
            package,
            1001)) {
        Fail("latest revision change did not invalidate freshness");
    }
}

void TestRecordUpdate() {
    auto package = MakePackage();

    std::vector<tp::PackageRefreshStamp>
        stamps;

    tp::RecordPackageRefresh(
        stamps,
        package,
        1000);

    package.latestRevision =
        L"3333333333333333333333333333333333333333";

    tp::RecordPackageRefresh(
        stamps,
        package,
        2000);

    if (stamps.size() != 1 ||
        stamps[0].latestRevision !=
            package.latestRevision ||
        stamps[0].refreshedAtMs != 2000 ||
        !tp::IsPackageRefreshFresh(
            stamps,
            package,
            2000)) {
        Fail("refresh stamp update was not in place");
    }
}

} // namespace

int main() {
    TestFreshness();
    TestRecordUpdate();

    if (failures != 0) {
        std::cerr
            << failures
            << " refresh freshness test(s) failed\n";
        return 1;
    }

    std::cout
        << "refresh freshness tests passed\n";
    return 0;
}
