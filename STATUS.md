# TocPilot status / handoff

## Continuation checkpoint — 2026-09-19

- Active branch: `p2-github-branches`.
- Current runtime-tested application version includes the `v0.1.13` adoption slice. The user successfully adopted existing Git-managed addons, then ran Update All across 21 installed packages: Queued 21 / Processed 21 / Updated 1 / Already current 20 / Failed 0. This validates adoption feeding the normal refresh/update transaction path. Restart persistence and continued GitAddonsManager visibility of retained `.git` metadata remain the final adoption runtime checks.
- v0.1.14 release request is committed at `dccec55`. Tag `v0.1.14` now exists and resolves to the 0.1.14 source, which confirms the release workflow passed source validation, Windows x64 build, and the full CTest suite before reaching its create-tag step. Release asset publication is the final workflow step; the current connector does not expose release assets directly.
- Post-v0.1.13 adoption follow-up commits:
  - `5e9d4c8` — Fix expandable dialog labels.
  - `a2a47a4` — Harden task dialog manifest declaration.
  - `04a6cac` — Use expandable adoption detail dialogs.
  - `cb8626b` — Build native dialog wrapper.
  - `3782839` — Implement expandable native dialogs.
  - `4433a5e` — Add native expandable dialog API.
  - `68543d3` — Upgrade source-only packages during adoption.
  - `a46d70a` — Test in-place source-only adoption.
  - `e895a1e` — Adopt source-only package records in place.
  - `7786f77` — Track in-place adoption targets.
  - `750af29` — Separate managed addons from adoption refusals.
- New GitAddonsManager-adoption implementation/release commits:
  - `57d9f3d` — Request v0.1.13 adoption release.
  - `248012a` — Set v0.1.13 application version.
  - `24f06ef` — Bump TocPilot to v0.1.13.
  - `dad2495` — Cover damaged Git adoption metadata.
  - `353c001` — Harden adoption test includes.
  - `d76d346` — Harden adoption standard includes.
  - `45e4603` — Refuse incomplete adoption scans.
  - `1fb09dd` — Add conservative Git install adoption UI.
  - `09b2848` — Wire Git addon adoption tests.
  - `f71895d` — Test conservative Git addon adoption.
  - `e148319` — Plan conservative GitAddonsManager adoption.
  - `19f266b` — Add Git addon adoption planning API.
- New uninstall implementation commits:
  - `f75f286` — Add transactional package uninstall UI.
  - `c4c1864` — Test uninstall state clearing.
  - `582b2a4` — Test transactional addon uninstall.
  - `3203e25` — Clear package ownership after uninstall.
  - `5ca70ee` — Add installed-state clear operation.
  - `ef0ddd2` — Reuse install transactions for addon removal.
  - `7a194fb` — Add removal transaction planning API.
- Completed in source: conservative adoption of simple existing Git-managed addon installs. The new **Adopt Git** flow scans only direct `Interface\\AddOns\\<folder>` repositories, reads local `.git` metadata without requiring Git/libgit2, requires a root-level `.toc`, an `origin` GitHub URL, attached local branch, matching upstream metadata, and a resolvable 40-character local HEAD; excludes `.git` from TocPilot file ownership; refuses duplicate repositories and addon roots already owned by TocPilot; stages all accepted records in memory and writes `TocPilot.json` once after user confirmation without reinstalling, deleting, or overwriting live addon files. GitLab, detached HEADs, linked worktrees/submodules, complex/unpacked multi-root layouts, symlinked content, and incomplete filesystem scans are refused.
- Completed in source: transactional uninstall for TocPilot-owned addon roots now reuses the existing install transaction/backup/rollback engine; shared ownership is refused; failed filesystem commits roll back; failed state saves restore removed roots; successful uninstall clears installed revision/file ownership while retaining repository/branch tracking; the existing **Forget** action remains explicitly non-destructive; deterministic install/state tests cover removal, shared ownership refusal, injected rollback, and installed-state clearing.
- Previously completed and runtime-validated: P0 self-update; P1 state/UI/provider foundation; GitHub branch selection/Refresh/Inspect; transactional single-package install/reinstall; unmanaged addon-root collision refusal, through `v0.1.10`.
- Runtime gate cleared for the uninstall slice: the user confirmed `v0.1.11` Uninstall behaved as expected through uninstall, restart, retained package state, and reinstall.
- Update All implementation head: `18d0f84` — Stop Update All after rollback failure. Supporting commits: `bc803fd` (sequential UI orchestration), `3df5659` (CTest wiring), `30989b0`/`c2d06a2` (test/source include hardening), `4e785ec` (queue tests), `642b536`/`d854cbb` (Update All queue module).
- Update All behavior now implemented: only installed TocPilot-owned GitHub branch packages are queued; each package is refreshed first; already-current packages are left untouched; changed branches reuse the existing single-package staged transaction; ordinary package failures are isolated so remaining packages continue; rollback failure stops the batch; not-installed packages are ignored; a final summary reports queued/processed/updated/current/failed counts.
- Deferred beyond the current adoption slice: crash-recovery journaling for unexpected process/power loss during live commit, GitHub release assets, GitLab support, DLL/direct-file installation, import/export, column persistence, and modification detection/backups.
- Exact next step: implement conservative adoption of existing GitAddonsManager-managed addon folders by reading local `.git` metadata, validating provider/repository/branch and addon-root ownership, then converting only safe matches into TocPilot package ownership without reinstalling or overwriting live addon files.
- Delivery rule: publish runtime-test builds only after their exact source version passes Windows CI.

## Current state

- pfUI test source direction: use `brues-code/pfUI` for future runtime testing, not `Shagu/pfUI`. The brues fork also stores `pfUI.toc` at repository root, so it exercises the same GitHub wrapper mapping fix.

- Repository: `Seraphic8x2244/TocPilot`
- Branch: `p2-github-branches`
- Product stage: P0/P1 validated; P2 GitHub branch install/update/uninstall and Update All runtime-validated; conservative Git adoption is runtime-validated through adoption plus a 21-package Update All pass, with restart persistence / retained-Git compatibility still to confirm
- License: MIT
- Intended platform: Windows x64
- Implementation: native C++20 / Win32 / CMake
- Highest priority: runtime-test v0.1.14 in-place adoption and expandable details once the release asset/self-update is visible
- Current adoption runtime result: 21 managed packages completed Update All with 1 updated / 20 current / 0 failed after existing Git installs were adopted.

## Latest commits

- `dccec55` — Request v0.1.14 adoption UX release
- `1b5bdad` — Set v0.1.14 application version
- `21bc962` — Bump TocPilot to v0.1.14
- `834c4fa` — Record launcher and compact UI roadmap
- `5e9d4c8` — Fix expandable dialog labels
- `a2a47a4` — Harden task dialog manifest declaration
- `04a6cac` — Use expandable adoption detail dialogs
- `cb8626b` — Build native dialog wrapper
- `3782839` — Implement expandable native dialogs
- `4433a5e` — Add native expandable dialog API
- `68543d3` — Upgrade source-only packages during adoption
- `a46d70a` — Test in-place source-only adoption
- `e895a1e` — Adopt source-only package records in place
- `7786f77` — Track in-place adoption targets
- `750af29` — Separate managed addons from adoption refusals
- `57d9f3d` — Request v0.1.13 adoption release
- `248012a` — Set v0.1.13 application version
- `24f06ef` — Bump TocPilot to v0.1.13
- `dad2495` — Cover damaged Git adoption metadata
- `353c001` — Harden adoption test includes
- `d76d346` — Harden adoption standard includes
- `45e4603` — Refuse incomplete adoption scans
- `1fb09dd` — Add conservative Git install adoption UI
- `09b2848` — Wire Git addon adoption tests
- `f71895d` — Test conservative Git addon adoption
- `e148319` — Plan conservative GitAddonsManager adoption
- `19f266b` — Add Git addon adoption planning API
- `9dd56e4` — Polish uninstall development controls
- `f75f286` — Add transactional package uninstall UI
- `c4c1864` — Test uninstall state clearing
- `582b2a4` — Test transactional addon uninstall
- `3203e25` — Clear package ownership after uninstall
- `5ca70ee` — Add installed-state clear operation
- `ef0ddd2` — Reuse install transactions for addon removal
- `7a194fb` — Add removal transaction planning API

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
- Install/update-all file actions remain disabled; branch selection and metadata Refresh are enabled only where their provider/state supports them.
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
- Created P2 branch `p2-github-branches` from the validated P1 line and enabled normal/release CI on it.
- Added a GitHub provider API module using WinHTTP with explicit GitHub API headers, timeouts, public-repository metadata lookup, branch pagination, and rate/not-found error handling.
- GitHub repository metadata resolves the default branch; branch responses capture branch names and each remote head commit SHA.
- Added an asynchronous native Set Branch dialog so GitHub network lookup does not block the main window.
- Existing GitHub source records can now save `mode: branch`, `ref`, and `latest_revision` without downloading or installing files.
- Package JSON updates merge known tracking fields into the existing raw package object so unknown/future package fields remain preserved.
- Main package rows display the selected branch under Source / Track and a short remote SHA under Latest; Installed remains empty until an install transaction exists.
- GitLab source records remain persistent but branch browsing is intentionally deferred to P4.
- Added deterministic GitHub repository/branch JSON parser CTests and extended the state round-trip test to verify branch/ref/remote-SHA persistence.
- User runtime-validated `v0.1.5`: self-update from `v0.1.4` succeeded, the existing pfUI source loaded real GitHub branches, branch selection persisted the selected ref and remote SHA, the row survived restart, and `Interface\\AddOns` remained untouched.
- Added a separate asynchronous Refresh action for configured GitHub branch packages while retaining Set Branch as a distinct action.
- Refresh re-resolves the tracked branch head from GitHub and updates only `latest_revision`; it never writes `installed_revision`.
- Refresh, Set Branch, and Add Package are gated while a package refresh is active; selection changes update action availability.
- Per-package Refresh shows `Checking...`, a row-level failure state plus detailed hint on error, and a successful branch/SHA hint without modal spam.
- Failed remote refreshes leave the previously saved remote SHA untouched.
- Added deterministic tracked-branch lookup tests (including case-sensitive branch matching) and state tests proving Refresh metadata updates do not create an installed revision.
- User runtime-validated `v0.1.6`: the tracked-branch Refresh slice works as intended; this clears the gate for the archive-inspection release.
- Vendored miniz 3.1.2, pinned to upstream commit `77d0dce8627735138c51770d1799a1ef48f2117d`, and linked it statically for ZIP inspection/extraction.
- Added a disposable per-package staging area under `Interface\TocPilot\staging`; archive inspection resets only its package staging directory and does not write to `Interface\AddOns`.
- Added exact-ref GitHub archive download support. Inspect resolves the selected branch to a commit SHA first, percent-encodes the API path safely, streams the ZIP with a 256 MiB download limit, checks the HTTP result and ZIP signature, and deletes partial downloads on failure.
- Added secure ZIP pre-validation before extraction: absolute/traversal paths, backslash separators, empty path segments, invalid UTF-8, Windows-invalid/ADS characters, trailing dot/space names, reserved device names, case-insensitive duplicate paths, encrypted entries, unsupported compression, excessive entry counts, and excessive extracted sizes are rejected.
- Added bounded ZIP extraction into the staging directory only. Partial extracted content is cleaned on extraction failure.
- Added deterministic addon-layout inspection that finds directories containing `.toc` files and reports candidate addon folder names/source paths without copying them into the live AddOns directory.
- Added a separate asynchronous **Inspect** action for configured GitHub branch packages. The preview reports the exact resolved SHA, staging path, archive/extracted sizes, and detected addon roots while leaving package installed state unchanged.
- Inspect, Refresh, Set Branch, and Add Package are mutually gated while package network/staging work is active.
- Added archive/path/layout CTest coverage, including a malicious `../` ZIP fixture, plus GitHub archive-ref URL encoding tests.
- Versioned the archive-inspection slice as `v0.1.7` after user confirmation that `v0.1.6` Refresh works.
- Updated release-version validation so it checks the TocPilot project/version independently of the CMake language list; this was required after vendored miniz added C to `LANGUAGES C CXX`.
- Corrected the release validator regex escaping after CI exposed the first regex form as over-escaped.
- Automated `v0.1.7` release run `35459015449` completed successfully: source validation, Release build, CTest, checksum sidecar, tag creation, and asset publishing all passed.
- GitHub release `v0.1.7` points to commit `2112d040e822360b0dbf17c58edd832117ccec8f` and publishes `TocPilot.exe` plus `TocPilot.exe.sha256`.
- User runtime-validated `v0.1.7`: self-update succeeded; real pfUI Inspect produced the expected staging/archive/extracted preview behavior; repeated Inspect remained healthy; Refresh/Set Branch remained healthy; `Interface\AddOns` stayed untouched and Installed remained unset.

- Added persisted `installed_revision` and `installed_files` ownership state while preserving schema-1 backward compatibility and unknown package fields.
- Installed ownership is stored as WoW-root-relative file paths such as `Interface/AddOns/Example/Example.toc`; state round-trip tests cover installed revision and file ownership.
- Added a transaction planner that validates addon roots/files, rejects duplicate or case-insensitive mappings, rejects other-package ownership collisions, and refuses to overwrite an existing live addon folder that TocPilot does not already own.
- Updates back up current package-owned roots, swap in the fully prepared roots, and remove previously owned roots absent from the new package as obsolete.
- Split installation into an off-thread **prepare** phase and a short live **commit** phase. Download, secure extraction, mapping, validation, and full file copying all complete before `Interface\AddOns` is changed.
- Added rollback that removes newly committed roots and restores prior backups, with idempotent retry behaviour for partially completed rollback attempts.
- Added deterministic install CTests for first install, multi-root packages, update replacement, obsolete-root cleanup, unowned collision rejection, other-package ownership collision rejection, explicit rollback, injected mid-commit failure, and proof that prepare-only work leaves live AddOns untouched.
- Added a temporary single-package **Install / Update / Reinstall** test control. Package state is saved only after the filesystem commit succeeds; if state save fails, the old live addon state is restored and old package state remains authoritative.
- Successful install state records the exact installed SHA plus complete file ownership. Transaction backups and package download/extraction staging are cleaned after the state commit; cleanup failures become warnings rather than restoring obsolete content over authoritative new state.
- Normal window close is blocked while a package install is active, and application self-replacement is gated against package installation.
- Core install transaction CI run `35460118275` passed; prepare-only safety run `35460454903` passed; exact implementation run `35460533602` passed Release build, full CTest, and artifact upload.

## CI validation

v0.1.13 adoption/update integration runtime validation passed: after adopting existing Git-managed addon folders (including pfUI after forgetting its prior source-only record), Update All reported Queued 21 / Processed 21 / Updated 1 / Already current 20 / Failed 0. This confirms adopted installed revisions/file ownership are accepted by Update All and a changed adopted branch can flow through the normal transactional update path. Remaining adoption checks are restart persistence and confirming GitAddonsManager can still use the untouched local `.git` metadata.

Runtime UX findings from the same adoption test:
- the first adoption scan found 18 safe candidates and 3 genuine refusals;
- rescanning after adoption mixed already-managed packages into the refusal list, obscuring the genuine refusal reasons; commit `750af29` now separates those categories;
- an existing same-repository TocPilot row that is still Not installed (observed with `brues-code/pfUI`) previously required Forget + rescan; commits `7786f77` through `68543d3` now upgrade that record in place while preserving its existing package/source JSON;
- oversized adoption MessageBoxes have been replaced in source by a minimal native `TaskDialogIndirect` wrapper with compact summary text and collapsed **Show details** information; it falls back to `MessageBoxW` if TaskDialog is unavailable.

v0.1.12 Update All runtime validation passed. The earlier current/current run reported Queued 2 / Processed 2 / Updated 0 / Current 2 / Failed 0, confirming multi-package enumeration, sequential processing, current-package skip behavior, and summary accounting. The user subsequently confirmed the changed-branch Update All path was also tested successfully, clearing the remaining runtime gate.


v0.1.12 self-update runtime validation passed: the user confirmed TocPilot detected and successfully took the update from v0.1.11 to v0.1.12.


v0.1.11 transactional uninstall/reinstall runtime validation passed: the user confirmed Uninstall removed the managed addon as expected, TocPilot restart preserved the retained package record/state, and Reinstall restored the addon successfully.


Published `v0.1.10` install-failure-dialog release: Actions release run `35468630162` completed successfully for commit `fdb5f75d1e3b11ff62d281b3d3c6b05bd420fcb9`.

- x64 Release configure/build: success;
- complete Release CTest suite: success;
- release publication: success;
- preparation failures now show the exact error in a MessageBox and state that no live addon files changed;
- commit failures and state-save rollback outcomes also show explicit dialogs.

Published `v0.1.9` wrapper-layout/Forget release: Actions release run `35463284080` completed successfully for commit `1d337c6bc2903bf3341ef2dfe2c0750eade88a80`.

- source/version validation: success;
- x64 Release configure/build: success;
- complete Release CTest suite: success;
- GitHub root-addon wrapper mapping test: success;
- package-record removal tests: success;
- release publication: success;
- `TocPilot.exe`: 704,512 bytes; SHA-256 `76955dccef2829cb815722334a90dd125bc047e223dc567aeab617c2ecef7fac`.

Published transactional-install release: Actions release run `35460888124` completed successfully for commit `0ef5a0a93490ba5bdf6348b4e0ef08cb3d141532`.

- source/version validation: success;
- x64 Release configure/build: success;
- complete Release CTest suite: success;
- SHA-256 sidecar generation: success;
- tag `v0.1.8` creation: success;
- release asset publication: success;
- `TocPilot.exe`: 699,904 bytes; SHA-256 `284bfda3628ddf09f8e9d3f29facda16dda81fba1d924116c4fdcc5ffa67ce7e`.

Published archive-inspection release: Actions release run `35459015449` completed successfully at commit `2112d04`.

- release source/version validation: success;
- x64 Release configure/build: success;
- complete Release CTest suite: success;
- SHA-256 sidecar generation: success;
- tag `v0.1.7` creation: success;
- release asset publication: success;
- `TocPilot.exe`: 587,264 bytes; SHA-256 `136ff21c3c58f972f62ca886ebd11e10b8cef6d15b225c02db37291351e4c0f1`;
- `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `b103a8b6155b55cbf482aed62e16d6ddd9eaf2c51665f5bd7f350710f7817ba3`.

Two earlier `v0.1.7` release attempts failed safely at the pre-build source-version validation step because the validator still assumed `LANGUAGES CXX`, then because the first replacement regex was over-escaped. Neither failed run created a release tag or published assets; the validator was fixed before the successful release.

Latest archive-inspection Windows build: Actions run `35457055183` for implementation head `6803330` completed successfully.

- x64 Release configure/build: success;
- complete Release CTest step: success, including provider URL, state/package, GitHub API/ref encoding, and archive-inspection tests;
- executable artifact upload: success;
- artifact ID: `10588118973`;
- artifact name: `TocPilot-windows-x64`;
- artifact ZIP size: 286,398 bytes;
- artifact digest: SHA-256 `7a308ef61af96d13496c0e0ea1b5da97df146b7b3d5ad95850a77cce61c4125a`.

The archive-inspection implementation has therefore passed Windows compile/link and deterministic tests, but it has not yet been published as a runtime-test release because the `v0.1.6` Refresh runtime gate is still pending.

Latest tracked-branch Refresh Windows build: Actions run `35453670639` for commit `b6a6793` completed successfully.

- x64 Release compile/link: success;
- provider URL normalization CTest: success;
- state/package branch/refresh round-trip and unknown-field preservation CTest: success;
- GitHub repository/branch API parsing and tracked-branch lookup CTest: success;
- executable artifact upload: success;
- artifact ID: `10587671685`;
- artifact name: `TocPilot-windows-x64`;
- artifact ZIP size: 215,307 bytes;
- artifact SHA-256: `36a8a365d60dad7a08582cfb68e94a221c76d3d851e2eb94f75739e578baeb02`.

Latest P2 GitHub branch-tracking Windows build: Actions run `35450961396` for commit `ac740c7` completed successfully.

- x64 Release compile/link: success;
- provider URL normalization CTest: success;
- state/package branch round-trip and unknown-field preservation CTest: success;
- GitHub repository/branch API parsing CTest: success;
- executable artifact upload: success;
- artifact ID: `10586721132`;
- artifact name: `TocPilot-windows-x64`;
- artifact ZIP size: 210,443 bytes;
- artifact SHA-256: `be5790d819c08b7e710de970f79379dfc0d0f4460b5cf31a6f4d8fbcfc09cc56`.

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

First P2 GitHub branch-tracking test release `v0.1.5` was published automatically. Release workflow run `35451120184` validated source/version consistency, built Release x64, passed provider URL, state/package, and GitHub API parser CTests, created tag `v0.1.5` at commit `6d8fcbf`, and published `TocPilot.exe` (433,152 bytes; SHA-256 `0449a725ef0ca2b1226c3e54c987d65923d1451906523b7086b80f32fe92b0a9`) plus `TocPilot.exe.sha256`.

Tracked-branch Refresh test release `v0.1.6` was published automatically. Release workflow run `35453833168` validated source/version consistency, built Release x64, passed provider URL, state/package Refresh, and GitHub API/tracked-branch CTests, created tag `v0.1.6` at commit `45b87a1`, and published `TocPilot.exe` (444,928 bytes; SHA-256 `80b6c33368a3287a95b9725c61532f92d64d2e3fa64ded42c2c12b4ec1e78ece`) plus `TocPilot.exe.sha256`.

## Untested / remaining validation

- Adoption detection and the adoption -> Update All integration path are runtime-validated. Still unverified: restart persistence after adoption, explicit confirmation that live addon files / `.git` were untouched, and GitAddonsManager continuing to recognize/update retained repositories.
- Post-v0.1.13 follow-ups are not yet Windows/runtime validated: in-place adoption of a matching Not-installed TocPilot record, the new TaskDialog/Common-Controls-v6 wrapper, expanded/collapsed detail labels, and the compact adoption summary/refusal presentation.
- The adoption pass intentionally does not yet claim GitLab repositories, detached HEADs, linked worktrees/submodules, or Git repository containers without a root-level `.toc`. GitAddonsManager has no unique ownership marker, so the UI explicitly warns that an eligible normal Git clone is indistinguishable from a GitAddonsManager-created clone.
- Runtime/repository investigation identified the GAM container layout for the two remaining refusals. `satan666/_LP` tracks the loadable addon under `_LazyPig/_LazyPig.toc`, while the user's live `_LP` folder contains only `.git` plus repository metadata; GAM has therefore separated the tracked addon root into a sibling live folder while retaining the Git container. `Cabro/Atlas` similarly tracks three loadable roots: `Atlas/Atlas.toc`, `AtlasLoot/AtlasLoot.toc`, and `AtlasQuest/AtlasQuest.toc`. Future complex-GAM adoption should resolve the container's exact local SHA/origin, inspect that exact repository revision using the existing archive-inspection path, map detected addon roots to sibling live `Interface\\AddOns` folders, and adopt those roots as one TocPilot package without touching the Git container.
- Current update-readiness UX limitation: package rows already derive **Current** vs **Update available** by comparing `installed_revision` and `latest_revision`, and the selected Install button changes to **Update** when they differ. However, TocPilot does not automatically refresh all remote branch heads at startup, so `latest_revision` may be stale until manual Refresh or Update All; automatic status refresh is therefore required for trustworthy at-a-glance update indicators.
- Current package ListView has no `LVN_COLUMNCLICK` handling or sort state, so clicking Name / Source / Installed / Latest / Status headers is intentionally inert in the current implementation; add stable ascending/descending sorting later.

- `v0.1.10` successful-path runtime validation passed with `Shellyoung/AdvancedTradeSkillWindow2` on its default `main` branch: first install succeeded, TocPilot still showed the package as Current after restart, and Reinstall completed successfully. This validates the normal transactional install/reinstall path and persisted ownership/state at runtime.

- `v0.1.10` unmanaged pfUI collision runtime test passed: `brues-code/pfUI` / `master` correctly refused to replace the existing unmanaged `Interface\AddOns\pfUI`, showed the failure popup, and created no generated GitHub wrapper folder.
- Runtime finding on `v0.1.9`: expected unmanaged pfUI collision reached `Install failed`, but no popup was shown. Diagnosis: the install-complete handler only wrote preparation/commit errors to the row/hint and returned; it did not show a MessageBox for ordinary install failures. Add explicit user-facing failure dialogs before further runtime testing.
- Runtime finding on `v0.1.8`: Shagu/pfUI reported a successful install while the pre-existing `Interface\\AddOns\\pfUI` remained unchanged. Root cause identified: GitHub ZIPs wrap repository contents in a generated top-level directory, and root-level addon repositories such as Shagu/pfUI (`pfUI.toc` at repo root) were incorrectly mapped to that generated wrapper name instead of the real addon folder name. Exact runtime confirmation: `Interface\\AddOns\\shagu-pfUI-b2f6df8` was created. The unowned-root collision guard itself is intact, but it was checking the wrong destination. Pause live-install testing until this archive-layout bug is fixed and released.

Current P2 runtime gates:

- Published `v0.1.6` Refresh is runtime-validated successfully.
- Published `v0.1.7` archive inspection is runtime-validated successfully.
- Transactional single-package install/update is CI-green but not yet published/runtime-tested. The next runtime gate is expected `v0.1.8`.
- Runtime validation must cover a first install into an addon root that is not already present, exact Installed/Latest SHA persistence across restart, repeat reinstall/update, unrelated addon preservation, and transaction/staging cleanup.
- An existing live addon folder without TocPilot ownership must be refused rather than overwritten; explicit adoption of unmanaged addons is deferred.
- In-process commit/state-save failures are rollback-covered and CI-tested. Crash/power-loss recovery during the short live-commit/state-save window is not yet journaled and remains deferred.

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
- paths containing spaces;
- non-system drive such as `D:\Games\WoW`.

Deferred beyond the current adoption slice:
- crash-recovery journal/reconciliation after unexpected process or power loss during live commit;
- GitHub release package/assets support;
- GitLab support;
- DLL/direct-file installation;
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
12. P2 Inspect remains non-destructive under `Interface\\TocPilot\\staging`; Install may touch `Interface\\AddOns` only after the exact revision is fully downloaded, securely extracted, mapped, collision-checked, and copied into transaction preparation.

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

### Later UI direction

- Add **Launch WoW.exe** and **Launch VanillaFixes.exe** buttons. Launch targets should resolve beside TocPilot/WoW and fail cleanly if the executable is absent.
- Default to a compact/small main window focused on addon name + status.
- Keep the normal path automatic/minimal:
  - automatically check TocPilot for updates and show a simple Yes/No prompt when one is available;
  - automatically refresh managed GitHub branch heads so update readiness is current without requiring manual Refresh/Update All first;
  - make update readiness visually obvious in compact mode (at minimum a clear **Update available** status/action; later consider row/icon emphasis without relying on colour alone);
  - automatically scan for newly discoverable GitAddonsManager/Git installs and show a simple Yes/No adoption prompt only when new candidates exist.
- Compact-mode primary actions: **Add**, **Update**, **Remove**, **Advanced**.
- **Advanced** expands the window to the right and exposes the detailed addon columns plus lower-frequency controls such as branch/refresh/inspect/reinstall/ownership diagnostics.
- Add click-to-sort behavior for package-list columns, with ascending/descending toggle and a visible sort indicator. Current Win32 ListView headers do not implement `LVN_COLUMNCLICK`.
- Add an Advanced/details folder scan that classifies immediate children of `Interface\\AddOns` instead of silently ignoring non-managed folders: managed addon, unmanaged addon/root-level `.toc`, Git repository container, Blizzard/system/local addon, and non-addon/no-`.toc` folder.
- For Git repository containers, inspect only the repository root and **one directory level down** for addon roots; do not recursively hunt arbitrary repository depth.
- For repositories containing multiple addon roots, present each detected root as an explicit Yes/No choice and persist that selection as package configuration so future Install/Update respects excluded roots. If a later revision adds a new addon root, surface it as a new choice instead of installing it silently.
- Keep the repository/package as the update unit while allowing selected addon roots within it. This should support GAM layouts such as `_LP -> _LazyPig` and `Atlas.repo -> Atlas + AtlasLoot + AtlasQuest` after exact-SHA/sibling-root mapping is validated.
- Treat this as a later UI-wrapper/layout pass; do not mix it into the current v0.1.14 adoption-fix release.

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

1. Let the installed v0.1.13 client detect/self-update to v0.1.14 once the release asset is visible.
2. Runtime-test two focused cases: (a) a matching TocPilot **Not installed** record is adopted in place without Forget, and (b) adoption dialogs show compact counts with **Show details** exposing exact candidates/refusal reasons.
3. Confirm restart persistence after adoption.
4. Confirm retained `.git` metadata remains usable by GitAddonsManager.
5. After those gates pass, begin the later compact-UI/launcher pass recorded under **Later UI direction**.
