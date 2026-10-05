#include "repository_install.h"

#include "mpq_package.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace tp {
namespace {

bool EqualsInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < left.size();
         ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }

    return true;
}

PackageRecord MakeReleasePackage(
    const RepositoryDiscoveryResult& discovery,
    const RepositoryCandidate& candidate,
    std::wstring target) {
    PackageRecord package =
        MakeRepositoryPackage(
            L"github",
            discovery.repository);

    package.id =
        L"github:" +
        discovery.repository +
        L":release:" +
        candidate.releaseAsset.name;
    package.name =
        candidate.releaseAsset.name;
    package.mode =
        L"release";
    package.ref.clear();
    package.releasePolicy =
        L"latest_stable";
    package.asset =
        candidate.releaseAsset.name;
    package.sourcePath.clear();
    package.target =
        std::move(target);
    package.targetPath.clear();
    package.installedRevision.clear();
    package.latestRevision =
        candidate.releaseTag;
    package.installedFiles.clear();
    package.installTransaction.clear();
    package.sourceJson.clear();
    return package;
}

} // namespace

bool BuildRepositoryInstallItems(
    const RepositoryDiscoveryResult& discovery,
    const std::vector<std::size_t>& selectedIndices,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& existingPackages,
    std::vector<RepositoryInstallItem>& items,
    std::wstring& error) {
    items.clear();
    error.clear();

    if (selectedIndices.empty()) {
        error =
            L"Select at least one discovered component.";
        return false;
    }

    std::vector<std::size_t> ordered =
        selectedIndices;
    std::sort(
        ordered.begin(),
        ordered.end());

    if (std::adjacent_find(
            ordered.begin(),
            ordered.end()) !=
        ordered.end()) {
        error =
            L"The repository selection contains a duplicate component.";
        return false;
    }

    std::vector<PackageRecord> reserved =
        existingPackages;
    items.reserve(ordered.size());

    for (const auto candidateIndex :
         ordered) {
        if (candidateIndex >=
            discovery.candidates.size()) {
            error =
                L"The repository selection changed unexpectedly.";
            items.clear();
            return false;
        }

        const auto& candidate =
            discovery.candidates[
                candidateIndex];

        RepositoryInstallItem item;
        item.candidateIndex =
            candidateIndex;
        item.kind =
            candidate.kind;

        switch (candidate.kind) {
        case RepositoryCandidateKind::Addon: {
            if ((discovery.provider != L"github" &&
                 discovery.provider != L"gitlab") ||
                discovery.repository.empty() ||
                discovery.branch.empty() ||
                discovery.branchRevision.empty() ||
                candidate.addon.installFolder.empty()) {
                error =
                    L"Addon discovery metadata is incomplete.";
                items.clear();
                return false;
            }

            const auto repositoryRelativePath =
                candidate.addon.repositoryRelativePath
                    .generic_wstring();
            const bool repositoryRoot =
                repositoryRelativePath.empty() ||
                repositoryRelativePath == L".";

            if (repositoryRoot) {
                item.package =
                    MakeRepositoryPackage(
                        discovery.provider,
                        discovery.repository);
                item.package.sourcePath =
                    L".";
            } else {
                item.package =
                    MakeRepositoryAddonPackage(
                        discovery.provider,
                        discovery.repository,
                        candidate.addon.repositoryRelativePath
                            .generic_wstring(),
                        candidate.addon.installFolder);
            }

            if (!SetPackageBranch(
                    item.package,
                    discovery.branch,
                    discovery.branchRevision,
                    error)) {
                items.clear();
                return false;
            }
            break;
        }

        case RepositoryCandidateKind::Dll: {
            if (discovery.provider != L"github" ||
                candidate.releaseAsset.name.empty() ||
                candidate.releaseTag.empty()) {
                error =
                    L"DLL discovery metadata is incomplete.";
                items.clear();
                return false;
            }

            item.package =
                MakeReleasePackage(
                    discovery,
                    candidate,
                    L"wow_root");
            item.package.targetPath =
                candidate.releaseAsset.name;

            if (!ValidateDirectDllPackageRecord(
                    item.package,
                    error)) {
                items.clear();
                return false;
            }

            for (const auto& other :
                 reserved) {
                if (other.mode == L"release" &&
                    other.target == L"wow_root" &&
                    !other.targetPath.empty() &&
                    EqualsInsensitive(
                        other.targetPath,
                        item.package.targetPath)) {
                    error =
                        L"DLL destination '" +
                        item.package.targetPath +
                        L"' is already reserved by package '" +
                        other.id +
                        L"'.";
                    items.clear();
                    return false;
                }
            }
            break;
        }

        case RepositoryCandidateKind::Mpq: {
            if (discovery.provider != L"github" ||
                candidate.releaseAsset.name.empty() ||
                candidate.releaseTag.empty()) {
                error =
                    L"MPQ discovery metadata is incomplete.";
                items.clear();
                return false;
            }

            item.package =
                MakeReleasePackage(
                    discovery,
                    candidate,
                    L"data");

            if (!AssignMpqTargetPath(
                    wowRoot,
                    reserved,
                    item.package,
                    error) ||
                !ValidateMpqPackageRecord(
                    item.package,
                    error)) {
                items.clear();
                return false;
            }
            break;
        }
        }

        reserved.push_back(
            item.package);
        items.push_back(
            std::move(item));
    }

    return true;
}

bool BuildRepositoryInstallQueue(
    const AppState& state,
    const std::vector<std::size_t>& indices,
    RepositoryInstallQueue& queue,
    std::wstring& error) {
    queue = {};
    error.clear();

    if (indices.empty()) {
        error =
            L"The install queue is empty.";
        return false;
    }

    queue.packageIds.reserve(
        indices.size());

    for (const auto index :
         indices) {
        if (index >= state.packages.size()) {
            error =
                L"The install queue references a package that no longer exists.";
            queue = {};
            return false;
        }

        const auto& package =
            state.packages[index];

        if (package.id.empty()) {
            error =
                L"The install queue contains a package with no identity.";
            queue = {};
            return false;
        }

        const bool duplicate =
            std::any_of(
                queue.packageIds.begin(),
                queue.packageIds.end(),
                [&](const std::wstring& existing) {
                    return EqualsInsensitive(
                        existing,
                        package.id);
                });

        if (duplicate) {
            error =
                L"The install queue contains a duplicate package.";
            queue = {};
            return false;
        }

        queue.packageIds.push_back(
            package.id);
    }

    return true;
}

bool RepositoryInstallQueueHasCurrent(
    const RepositoryInstallQueue& queue) {
    return
        queue.position <
            queue.packageIds.size();
}

std::wstring_view RepositoryInstallQueueCurrentPackageId(
    const RepositoryInstallQueue& queue) {
    if (!RepositoryInstallQueueHasCurrent(
            queue)) {
        return {};
    }

    return queue.packageIds[
        queue.position];
}

bool AdvanceRepositoryInstallQueue(
    RepositoryInstallQueue& queue,
    std::wstring_view completedPackageId,
    std::wstring& error) {
    error.clear();

    if (!RepositoryInstallQueueHasCurrent(
            queue)) {
        error =
            L"The install queue has no active package.";
        return false;
    }

    if (!EqualsInsensitive(
            RepositoryInstallQueueCurrentPackageId(
                queue),
            completedPackageId)) {
        error =
            L"The completed package does not match the active install queue item.";
        return false;
    }

    ++queue.position;
    return true;
}

} // namespace tp
