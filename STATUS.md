# TocPilot status / handoff

## Current state

- Repository: `Seraphic8x2244/TocPilot`
- Branch: `p1-state-ui`
- Product stage: P0 self-update validated end-to-end; P1 state/UI foundation in progress
- License: MIT
- Intended platform: Windows x64
- Implementation: native C++20 / Win32 / CMake
- Highest priority: publish and validate `v0.1.3` through TocPilot self-update, then continue P1 package-record/add-package work
- Current source version: `v0.1.3` prepared for the next user-test build; latest published and runtime-validated version is `v0.1.2`

## Latest commits

- `a7f5aba` — Fix Add Package dialog coordinate types
- `d699bd0` — Add provider URL normalization
- `a49b98a` — Record automated v0.1.2 P1 release
- `95deab4` — Request v0.1.2 release
- `701a2ce` — Prepare v0.1.2 P1 test release

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
- Existing package-array JSON is preserved verbatim while package editing is not yet implemented.
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
- Added a minimal Add Package source-check dialog. It validates and normalizes a repository URL but deliberately does not install or save a package yet.
- Added automated provider URL tests through CTest and required them in both normal Windows CI and release CI.

## CI validation

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
- full package-record JSON parsing/writing;
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

1. Publish `v0.1.3` through the automated release workflow.
2. Keep the user's installed `v0.1.2` and let TocPilot detect `v0.1.3`; use `Update app` rather than manually replacing the EXE.
3. In `v0.1.3`, click `Add Package` and confirm a GitHub repository URL is recognized and normalized without installing anything.
4. Repeat with a public GitLab repository URL; verify an unsupported/non-GitHub/GitLab URL is rejected cleanly.
5. Also recheck that the existing package table/state UI and text-size persistence remain healthy.
6. After that smoke test, continue P1 with real package-record parsing/writing and turn Add Package from source validation into the first persistent package-definition flow.
