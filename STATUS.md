# TocPilot status / handoff

## Current state

- Repository: `Seraphic8x2244/TocPilot`
- Branch: `p0-self-update`
- Product stage: P0 implementation started
- License: MIT
- Intended platform: Windows x64
- Implementation: native C++20 / Win32 / CMake
- Highest priority: self-update bootstrap before addon-management features
- Current application version: `v0.1.0` (implementation target; not yet released/tested)

## Latest commits

- `49c8e0b` — Add TocPilot implementation handoff
- `3d5c9f2` — Expand TocPilot project overview
- `63c1910` — Add TocPilot development plan

## Completed

- Repository created.
- Product named **TocPilot**.
- Core product scope defined.
- Full development/architecture document added as `DEVELOPMENT.md`.
- README updated with project direction.
- Decision made to use one TocPilot instance per WoW directory rather than multi-directory profiles.
- Decision made to manage remote packages/snapshots rather than local Git repositories.
- GitHub/GitLab branches and releases are first-class source types.
- Direct release assets such as DLLs are part of the package model.
- Native Windows implementation selected to avoid the large Qt deployment used by GitAddonsManager.
- Self-update architecture designed around a direct `TocPilot.exe` release asset and a two-process Windows-safe replacement flow.
- Development milestones and test strategy documented.
- P0 implementation branch `p0-self-update` created.

## In progress — P0 self-update bootstrap

This branch is implementing only the P0 milestone:

- native Win32 application skeleton;
- own-directory / `WoW.exe` validation;
- visible compile-time version `v0.1.0`;
- asynchronous GitHub latest-release discovery;
- direct `TocPilot.exe` download;
- SHA-256 validation;
- updater-helper mode using the downloaded executable;
- wait-for-parent-exit plus bounded replacement retries;
- rollback-safe executable replacement and restart;
- GitHub Actions x64 Release build/release workflow.

## Untested / unimplemented

P0 is not yet compiled or run on Windows. The following still require validation after the first implementation commit:

- MSVC/CMake compile;
- real WoW-directory startup validation;
- GitHub API/release parsing against an actual TocPilot release;
- download and SHA-256 verification;
- updater wait/retry behaviour with a live locked executable;
- rollback behaviour on replacement failure;
- restart into the installed version;
- GitHub Actions build artifacts;
- end-to-end `v0.1.0 -> v0.1.1` update.

Everything after P0 also remains unimplemented:

- local JSON state;
- package engine;
- ZIP extraction;
- GitHub branch/release package support;
- GitLab support;
- addon layout detection;
- DLL/direct-file installation;
- remove/update file ownership;
- import/export;
- text-size and column/layout preferences.

## Important design decisions

1. Put `TocPilot.exe` beside `WoW.exe`.
2. If another WoW install needs management, copy TocPilot there too.
3. No local `.git` repositories.
4. No Qt/libgit2/Git runtime dependency.
5. Use provider APIs to follow branch commit SHAs and releases.
6. A managed **package** may install addon folders, multiple addon folders, a release ZIP, a DLL, or another safe relative file.
7. Self-update comes before addon management so later builds can be tested without manual copy-over.
8. Self-update publishes/downloads a direct `TocPilot.exe` asset.
9. The updater waits for the old process to fully terminate before replacing the EXE and uses retries/rollback rather than reproducing GitAddonsManager's Windows file-lock false failure.
10. Local state is portable with the WoW install, initially planned as `TocPilot.json`.

## Priority roadmap

### P0 — Self-update bootstrap

Build `v0.1.0` with:

- native Win32 app/window;
- own-directory detection;
- `WoW.exe` validation;
- visible compiled version;
- GitHub latest-release check;
- direct `TocPilot.exe` download;
- SHA-256 verification;
- updater-helper mode using the downloaded/new executable;
- wait-for-parent-exit and bounded replacement retries;
- safe rollback/failure behaviour;
- restart into the new executable;
- GitHub Actions x64 Release build.

Then publish a minimal `v0.1.1` and prove `v0.1.0 -> v0.1.1` through the in-app updater.

**Do not move on to addon/package implementation until this succeeds.**

### P1 — State and basic UI

- `TocPilot.json`;
- package list;
- text-size preference;
- URL/provider parsing.

### P2 — GitHub branch packages

- list branches;
- follow selected branch SHA;
- download archive;
- extract securely;
- detect `.toc` addon roots;
- install/update/remove without `.git`.

### P3 — GitHub releases/direct assets

- browse releases/assets;
- latest stable/prerelease tracking;
- ZIP assets;
- direct DLL/file assets.

### P4 — GitLab

- equivalent public branch/release support.

### P5 — UX/safety refinement

- compact expandable removal details;
- columns/layout persistence;
- import/export;
- local modification detection/backups;
- diagnostics.

## Deferred

- private repositories/authentication;
- code signing;
- self-hosted GitLab;
- rollback history UI;
- delta updates;
- CLI/headless mode;
- scheduled/background updating;
- multi-directory profile management.

## Exact next step

Scaffold the P0 CMake/native source tree on `p0-self-update`, implement the `v0.1.0` application and updater state machine, add Windows CI/release workflows, then validate the branch with GitHub Actions before preparing the `v0.1.0` release test.
