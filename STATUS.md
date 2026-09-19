# TocPilot status / handoff

## Current state

- Repository: `Seraphic8x2244/TocPilot`
- Branch: `p1-state-ui`
- Product stage: P0 self-update validated end-to-end; P1 state/UI foundation in progress
- License: MIT
- Intended platform: Windows x64
- Implementation: native C++20 / Win32 / CMake
- Highest priority: publish and runtime-validate `v0.1.4` through TocPilot self-update; this build introduces persistent repository package sources
- Current application version: `v0.1.4`; published and runtime-validated successfully through the installed `v0.1.3` self-updater

## Latest commits

- `f34be07` — Request v0.1.4 release
- `84fc7d2` — Prepare v0.1.4 persistent package test release
- `413b8c4` — Persist repository package sources
- `ff78443` — Record v0.1.3 runtime validation
- `4bf21f8` — Record automated v0.1.3 provider release

## Completed

- Native C++20 / Win32 / CMake application scaffold.
- Static MSVC runtime selected for portable Release builds.
- Compile-time application version `v0.1.0`.
- Executable-directory discovery.
- Startup validation requiring `WoW.exe` beside TocPilot.
- Startup creation/validation of `Interface\AddOns`.
- Minimal native window showing current version, WoW root, update status, and update action.
- Automatic non-blocking GitHub latest-release check.
- Semantic-version comparison against the compiled version.
- Discovery of direct `TocPilot.exe` release assets.
- SHA-256 verification using GitHub release asset `digest` when available.
- `TocPilot.exe.sha256` sidecar fallback supported.
- Streaming download to a unique Windows temp staging directory.
- Downloaded executable is preserved separately until validation succeeds.
- Downloaded/new executable doubles as updater helper mode.
- Helper waits on the old TocPilot process handle before replacement.
- Replacement is staged beside the target and uses `ReplaceFileW` with a backup.
- File replacement uses bounded retries for transient file/AV locks.
- Failed replacement leaves the old executable in place.
- Failed relaunch attempts automatic rollback to the previous executable.
- Successful restart receives cleanup arguments for backup/staging cleanup.
- GitHub Actions Windows x64 Release build workflow.
- GitHub Actions tag-release workflow that publishes:
  - `TocPilot.exe`
  - `TocPilot.exe.sha256`
- First Windows x64 CI build succeeded for commit `20c3ad5`.
- CI artifact `TocPilot-windows-x64` was uploaded successfully.
- P1 branch `p1-state-ui` created from the completed P0 line.
- Added native `src/state.h` / `src/state.cpp` state module.
- Missing `TocPilot.json` is created beside the EXE with schema 1 defaults.
- State writes use `TocPilot.json.tmp`, flush to disk, then replace/move atomically.
- Schema-1 package records are parsed into structured in-memory records while each raw package object is retained so unknown/future fields survive unrelated saves.
- Persisted settings currently include `text_scale` and `check_app_updates`.
- Invalid/unsupported state is surfaced read-only rather than overwritten.
- Main window now uses a resizable native ListView package table with Name, Source / Track, Installed, Latest, and Status columns.
- Package-action buttons are present but intentionally disabled until the package engine exists.
- Text-size selector persists to `TocPilot.json` and reapplies the native UI font.
- Existing application self-update status and action remain in the main window; updater implementation files were not changed.
- User confirmed the installed `v0.1.1` client detected, applied, and restarted into `v0.1.2` through TocPilot self-update.
- Added provider/repository URL normalization for public `github.com` and `gitlab.com` repositories.
- URL normalization accepts normal HTTPS URLs, scheme-less URLs, common SSH/scp-style clone URLs, `.git` suffixes, GitHub repository subpages, and GitLab nested groups/`/-/` routes.
- Unsupported hosts, incomplete repository paths, and unsafe path segments are rejected with a user-facing reason.
- Add Package now validates/normalizes a repository URL and can persist it as a source-only package record; it deliberately does not download or install anything yet.
- User runtime-validated `v0.1.3`: self-update from `v0.1.2` succeeded, GitHub and GitLab repository normalization behaved as expected, unsupported hosts were rejected cleanly, and existing state/text-size UI remained healthy.
- Added automated provider URL tests through CTest and required them in both normal Windows CI and release CI.
- Source-only package records use stable `provider:repository` IDs, default the visible name from the repository path, and begin with `mode: unconfigured` / `target: addons`.
- Duplicate repository sources are rejected before disk state is changed.
- Package saves remain atomic: the updated state is staged in memory, written through `TocPilot.json.tmp`, flushed, and only then replaces the live file.
- Persisted package records render in the main ListView immediately after save and after application restart; rows show Source only / Not configured until P2 tracking/install support exists.
- User runtime-validated `v0.1.4`: self-update from `v0.1.3` succeeded, saving a GitHub repository source created the expected row, the row survived restart, duplicate repository add was rejected without a second row, text-size persistence remained healthy, and no addon files were installed.
- Added state/package CTest coverage for default creation, package add/save/reload, duplicate rejection, setting persistence, and preservation of unknown top-level/package JSON fields.

## CI validation

Latest persistent-package Windows build: Actions run `35449693734` for commit `413b8c4` completed successfully.

- x64 Release compile/link: success;
- provider URL normalization CTest: success;
- state/package round-trip and unknown-field preservation CTest: success;
- executable artifact upload: success;
- artifact ID: `10585779366`;
- artifact name: `TocPilot-windows-x64`;
- artifact ZIP size: 194,444 bytes;
- artifact SHA-256: `832c3cb24518a765a05304796a1832b0eaba6fcfb0bd4c0c4107cd7f653d5b5d`.

Latest provider/parser Windows build: Actions run `35447864601` for commit `a7f5aba` completed successfully.

- x64 Release compile/link: success;
- provider URL normalization CTest: success;
- executable artifact upload: success;
- artifact ID: `10586536172`;
- artifact name: `TocPilot-windows-x64`;
- artifact ZIP size: 183,149 bytes;
- artifact SHA-256: `0d686da42841fdf3cf7879b9ef8f6e4ad2ee65ed728c8e87464122e044a88130`.

The first provider/dialog build attempt failed only on a Win32 `LONG`/integer type mismatch in dialog centering; commit `a7f5aba` corrected it and the next build/test run passed.

Latest P1 Windows build: Actions run `35446732391` for commit `698b984` completed successfully.

- CMake configure: success;
- x64 Release compile/link: success;
- executable artifact upload: success;
- artifact ID: `10584424934`;
- artifact name: `TocPilot-windows-x64`;
- artifact ZIP size: 175,045 bytes;
- artifact SHA-256: `1e3ccad4e611b9611552b7fbb839e808aa2f1a6b69089c4d20996fb3d2b9026e`.

The first P1 CI attempt failed only on Win32 helper macro/type issues in `main.cpp`; commit `698b984` corrected them and the next run passed.

The `v0.1.1` branch build also succeeded in Actions run `35445252119`; artifact `TocPilot-windows-x64` was uploaded successfully.

GitHub Actions run `35443134276` completed the important build steps successfully:

- checkout: success;
- CMake configure: success;
- x64 Release compile/link: success;
- executable artifact upload: success.

Artifact metadata:

- artifact ID: `10584660938`;
- artifact name: `TocPilot-windows-x64`;
- ZIP artifact size: 101,139 bytes;
- artifact SHA-256: `21cb498988c178f999db64d8735ac42fa76bcea5c9da4316b93d8c82aa4e358c`.

This validates compilation and CI packaging only. It does not validate runtime updater behaviour.

## Release workflow finding

- The user created GitHub release `v0.1.0` successfully.
- Tag `v0.1.0` points to commit `1386f41`.
- Release workflow run `35444784363` built `TocPilot.exe` successfully and wrote the SHA-256 sidecar successfully.
- The workflow failed only at the final publish step because it used `gh release create` even though the release already existed.
- The permanent release workflow now uploads to an existing release when present and can create one when absent.
- A one-off repair workflow checked out the exact `v0.1.0` tag and successfully attached `TocPilot.exe` plus `TocPilot.exe.sha256` to the existing release.
- The one-off repair workflow was then removed.

## Development delivery rule

During active development, CI artifacts are for compile validation only. Any build handed to the user for runtime testing should be version-bumped and published through the automated GitHub release path so the installed TocPilot exercises self-update on every test cycle. Manual EXE replacement is a fallback only when the updater itself is under repair.

## P0 end-to-end validation

User confirmed the production-style self-update flow succeeded:

- local client was `v0.1.0`;
- it detected `v0.1.1` as available;
- clicking `Update now` closed TocPilot;
- TocPilot replaced itself and reopened automatically;
- the update completed successfully with no reported error dialog.

This proves the core P0 self-update path end-to-end.

## Release automation direction

Routine releases no longer require manually drafting a GitHub release. The repository uses `.github/release-version`; changing it to a new validated version triggers Actions to validate source/version consistency, build, create or verify the tag, create/update the release, and publish the direct EXE plus checksum. Manual dispatch remains a fallback once the workflow is on the default branch.

This was validated with `v0.1.1`, and again with the first P1 test release `v0.1.2`. Release workflow run `35447238830` created tag `v0.1.2` at commit `95deab4`, published the release, and attached `TocPilot.exe` (358,400 bytes; SHA-256 `1517ff2a81d26689d6c7764c6ca196251e9f480a7866b6c6960acf5e7dbf49d7`) plus `TocPilot.exe.sha256`.

Provider-normalization test release `v0.1.3` was also published automatically. Release workflow run `35448024199` validated source/version consistency, built Release x64, passed the provider URL CTest, created tag `v0.1.3` at commit `dd5d77d`, and published `TocPilot.exe` (373,248 bytes; SHA-256 `bb1eb5dea85fdf208542155eaaeecff9be339bd3e4de9dc0d65f6aa3fd5942df`) plus `TocPilot.exe.sha256`.

Persistent-package test release `v0.1.4` was published automatically. Release workflow run `35449831222` validated source/version consistency, built Release x64, passed both provider URL and state/package CTests, created tag `v0.1.4` at commit `f34be07`, and published `TocPilot.exe` (398,336 bytes; SHA-256 `0c0257a1781c9d9184a21c0d1079b5885817853a6a71f2ba3c1f5e19ee142780`) plus `TocPilot.exe.sha256`.

## Untested / remaining validation

P1 runtime testing still required:

- first startup creates `TocPilot.json` beside the EXE;
- generated JSON contains schema 1, settings, and an empty packages array;
- reopening loads the existing state without rewriting it;
- text-size selection persists across restart;
- package ListView renders/resizes correctly at supported text sizes;
- malformed/unsupported `TocPilot.json` leaves the file unchanged and shows the state error in the UI.

P0 edge/failure paths not yet deliberately forced:


- missing-`WoW.exe` error path;
- `Interface\AddOns` creation;
- direct release EXE download;
- SHA-256 metadata-digest path;
- SHA-256 sidecar fallback path;
- deliberate digest mismatch refusal;
- helper waiting while the original executable remains locked/running;
- transient replacement retry behaviour;
- rollback after a forced replacement/relaunch failure;
- successful restart and cleanup;
- paths containing spaces;
- non-system drive such as `D:\Games\WoW`;

Deferred beyond the current P1 slice:

- package editing/installation engine;
- ZIP extraction;
- GitHub branch/release package support;
- GitLab support;
- addon layout detection;
- DLL/direct-file installation;
- remove/update file ownership;
- import/export;
- persisted column order/width and layout locking.

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
11. P0 release builds also publish a small `TocPilot.exe.sha256` sidecar as a fallback if GitHub does not expose an asset digest.

## Priority roadmap

### P0 — Self-update bootstrap

Complete. A real `v0.1.0 -> v0.1.1` in-app self-update succeeded on Windows beside a real `WoW.exe`: update detected, downloaded, verified, old process closed, executable replaced, and new version restarted successfully.

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

1. Begin P2 with GitHub branch discovery for persisted GitHub repository sources; do not install or modify addon files yet.
2. Add a provider/API path that resolves repository metadata, lists branches, and captures each branch head commit SHA.
3. Extend the package definition flow so a saved GitHub source can select a branch and persist `mode: branch`, `ref`, and the resolved remote SHA.
4. Render branch tracking and the resolved SHA/status in the main package table.
5. Keep Update All/Refresh/install/remove file actions disabled until the remote tracking slice is runtime-validated.
6. Add deterministic tests around GitHub API response parsing/state persistence; use Windows CI before publishing the next self-update build.
