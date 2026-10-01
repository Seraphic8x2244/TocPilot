#pragma once

#include "repository_discovery.h"
#include "state.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp {

struct RepositoryInstallItem {
    std::size_t candidateIndex = 0;
    RepositoryCandidateKind kind =
        RepositoryCandidateKind::Addon;
    PackageRecord package;
};

struct RepositoryInstallQueue {
    std::vector<std::wstring> packageIds;
    std::size_t position = 0;
};

bool BuildRepositoryInstallItems(
    const RepositoryDiscoveryResult& discovery,
    const std::vector<std::size_t>& selectedIndices,
    const std::filesystem::path& wowRoot,
    const std::vector<PackageRecord>& existingPackages,
    std::vector<RepositoryInstallItem>& items,
    std::wstring& error);

bool BuildRepositoryInstallQueue(
    const AppState& state,
    const std::vector<std::size_t>& indices,
    RepositoryInstallQueue& queue,
    std::wstring& error);

bool RepositoryInstallQueueHasCurrent(
    const RepositoryInstallQueue& queue);

std::wstring_view RepositoryInstallQueueCurrentPackageId(
    const RepositoryInstallQueue& queue);

bool AdvanceRepositoryInstallQueue(
    RepositoryInstallQueue& queue,
    std::wstring_view completedPackageId,
    std::wstring& error);

} // namespace tp
