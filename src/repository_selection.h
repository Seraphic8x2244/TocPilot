#pragma once

#include "repository_discovery.h"

#include <cstddef>
#include <string>
#include <vector>

namespace tp {

struct RepositorySelectionRow {
    std::size_t candidateIndex = 0;
    std::wstring type;
    std::wstring name;
    std::wstring source;
    std::wstring destination;
    bool requiresDllTrust = false;
    bool selectable = true;
    std::wstring unavailableReason;
};

bool BuildRepositorySelectionRows(
    const RepositoryDiscoveryResult& discovery,
    const std::vector<std::wstring>& mpqDestinations,
    const std::vector<std::wstring>& mpqErrors,
    std::vector<RepositorySelectionRow>& rows,
    std::wstring& error);

bool FindSingleSelectableRepositoryCandidate(
    const std::vector<RepositorySelectionRow>& rows,
    std::size_t& candidateIndex);

} // namespace tp
