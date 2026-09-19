# TocPilot status / handoff

## Current state

- Repository: `Seraphic8x2244/TocPilot`
- Branch: `main`
- Product stage: design complete, implementation not yet started
- License: MIT
- Intended platform: Windows x64
- Planned implementation: native C++20 / Win32 / CMake
- Highest priority: self-update bootstrap before addon-management features
- Current application version: none yet

## Latest commits

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

## Untested / unimplemented

Everything below still requires implementation:

- native application skeleton;
- WoW-directory detection;
- UI;
- HTTP/API layer;
- self-update discovery/download/replacement;
- GitHub Actions Windows build;
- local JSON state;
- package engine;
- ZIP extraction;
- GitHub branch/release support;
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
8. Self-update should publish/download a direct `TocPilot.exe`, not a ZIP.
9. The updater must wait for the old process to fully terminate before replacing the EXE and must use retries/rollback rather than reproducing GitAddonsManager's Windows file-lock false failure.
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

## Exact next step for the next chat

Read `DEVELOPMENT.md` and this file first.

Then implement **P0 only**:

1. scaffold `CMakeLists.txt` and native C++ source layout;
2. create a small Win32 `TocPilot.exe`;
3. add compile-time version `v0.1.0`;
4. validate that `WoW.exe` is beside TocPilot;
5. add GitHub latest-release checking for `Seraphic8x2244/TocPilot`;
6. implement direct-EXE download and SHA-256 validation;
7. implement the Windows-safe two-process replacement flow;
8. add GitHub Actions Windows x64 build;
9. produce/test `v0.1.0`;
10. create `v0.1.1` solely to validate self-update end-to-end.

Do not spend the first implementation session on addon management or UI polish beyond what is necessary to test the updater.
