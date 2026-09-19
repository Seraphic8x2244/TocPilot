#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace tp {

struct AppSettings {
    double textScale = 1.0;
    bool checkAppUpdates = true;
};

struct PackageRecord {
    std::wstring id;
    std::wstring name;
    std::wstring provider;
    std::wstring repository;
    std::wstring mode = L"unconfigured";
    std::wstring target = L"addons";

    // Preserve the complete package object so fields from newer versions are
    // not discarded when this version updates unrelated state.
    std::string sourceJson;
};

struct AppState {
    int schema = 1;
    AppSettings settings;
    std::vector<PackageRecord> packages;

    // Preserve the complete top-level document so unknown/new fields survive.
    std::string sourceJson;
};

std::filesystem::path StatePath(const std::filesystem::path& wowRoot);

PackageRecord MakeRepositoryPackage(
    std::wstring provider,
    std::wstring repository);

bool AppendPackage(
    AppState& state,
    PackageRecord package,
    std::wstring& error);

bool LoadOrCreateState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    bool& created,
    std::wstring& error);

bool SaveState(
    const std::filesystem::path& wowRoot,
    AppState& state,
    std::wstring& error);

} // namespace tp
