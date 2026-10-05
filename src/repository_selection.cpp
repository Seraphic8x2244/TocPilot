#include "repository_selection.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace tp {
namespace {

std::wstring AddonType(
    const RepositoryDiscoveryResult& discovery) {
    return
        discovery.addonLayout.kind ==
                RepositoryAddonLayoutKind::RepositoryLibrary
            ? L"Library addon"
            : L"Addon";
}

} // namespace

bool BuildRepositorySelectionRows(
    const RepositoryDiscoveryResult& discovery,
    const std::vector<std::wstring>& mpqDestinations,
    const std::vector<std::wstring>& mpqErrors,
    std::vector<RepositorySelectionRow>& rows,
    std::wstring& error) {
    rows.clear();
    error.clear();

    if (mpqDestinations.size() !=
            discovery.candidates.size() ||
        mpqErrors.size() !=
            discovery.candidates.size()) {
        error =
            L"Repository selection destination metadata does not match the discovered candidates.";
        return false;
    }

    rows.reserve(
        discovery.candidates.size());

    for (std::size_t i = 0;
         i < discovery.candidates.size();
         ++i) {
        const auto& candidate =
            discovery.candidates[i];

        RepositorySelectionRow row;
        row.candidateIndex = i;

        switch (candidate.kind) {
        case RepositoryCandidateKind::Addon:
            row.type =
                AddonType(discovery);
            row.name =
                candidate.addon.installFolder;
            row.source =
                L"Branch: " +
                discovery.branch;
            row.destination =
                L"Interface\\AddOns\\" +
                candidate.addon.installFolder;
            break;

        case RepositoryCandidateKind::Dll:
            row.type =
                L"DLL";
            row.name =
                candidate.releaseAsset.name;
            row.source =
                L"Latest stable release: " +
                candidate.releaseTag;
            row.destination =
                L"WoW root\\" +
                candidate.releaseAsset.name;
            row.requiresDllTrust =
                true;
            break;

        case RepositoryCandidateKind::Mpq:
            row.type =
                L"MPQ";
            row.name =
                candidate.releaseAsset.name;
            row.source =
                L"Latest stable release: " +
                candidate.releaseTag;

            if (!mpqErrors[i].empty()) {
                row.selectable = false;
                row.destination =
                    L"Unavailable";
                row.unavailableReason =
                    mpqErrors[i];
            } else if (
                mpqDestinations[i].empty()) {
                error =
                    L"Repository selection is missing a proposed MPQ destination.";
                rows.clear();
                return false;
            } else {
                row.destination =
                    mpqDestinations[i];
            }
            break;
        }

        if (row.name.empty() ||
            row.source.empty() ||
            row.destination.empty()) {
            error =
                L"Repository discovery produced an incomplete selection row.";
            rows.clear();
            return false;
        }

        rows.push_back(
            std::move(row));
    }

    return true;
}

bool FindSingleSelectableRepositoryCandidate(
    const std::vector<RepositorySelectionRow>& rows,
    std::size_t& candidateIndex) {
    bool found = false;
    candidateIndex = 0;

    for (const auto& row : rows) {
        if (!row.selectable) {
            continue;
        }

        if (found) {
            candidateIndex = 0;
            return false;
        }

        candidateIndex =
            row.candidateIndex;
        found = true;
    }

    return found;
}


} // namespace tp
