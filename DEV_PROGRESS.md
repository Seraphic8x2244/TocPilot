# TocPilot Development Progress

> Live TocPilot development context for a fresh chat. Keep this current and concise. Git carries chronology; this file carries the current product contract and next work.

## Current

- Active branch: `fix/v0.3.6-runtime-ux`.
- Source/application version: `v0.3.6`.
- Current branch head before this documentation checkpoint: `b8399815a56e7dcf411f4b0425504860132f3262` (completed v0.3.6 version-source bump).
- Latest published release: `v0.3.5`.
- Release/source commit and tag target: `1d3050f4bc755e7b8be0762d3f6e6afe738f1525` (`Merge v0.3.5 version and status UX`).
- Current goal: close the remaining v0.3.5 runtime-discovered UX issues before beginning the next P5 feature.
- Current scope boundary: **do not start P5 import/export until the Update New transient-status, Advanced Version population, and single-branch selector UX are resolved and runtime-checked**.
- This branch is the focused post-v0.3.5 UX follow-up and is now versioned `v0.3.6` for CI/merge/release validation. It is **not yet published or runtime-tested**.

## Product Contract

TocPilot is a lightweight portable native Windows manager for Vanilla WoW addons and approved release assets.

Core product intent:

- one TocPilot instance lives beside `WoW.exe` and manages that WoW installation;
- no installer or separately installed runtime is required;
- state remains local to that installation;
- packages, not local Git repositories, are the central abstraction;
- branch/release tracking uses remote provider metadata rather than local `.git` directories;
- self-update is a first-class product feature and part of normal runtime testing;
- package ownership and install/update/remove operations must remain deterministic and rollback-minded.

Current non-goals include becoming a general Git client, general Windows package manager, multi-game manager, multi-WoW-profile manager, source editor, CurseForge/Wago client, or arbitrary executable/archive installer.

## Current Design / Development Contract

### Platform / Runtime

- C++20, native Win32 UI, CMake, MSVC, Windows x64.
- Static Microsoft runtime where practical.
- WinHTTP for HTTPS/API/download work.
- BCrypt/SHA-256 for integrity checks.
- Native Windows controls and APIs are preferred over framework-scale dependencies.
- Current vendored ZIP implementation is miniz.
- TocPilot is intentionally portable and runs directly from the WoW root.

### Local Layout / State

Normal layout:

```text
World of Warcraft/
├─ WoW.exe
├─ TocPilot.exe
├─ TocPilot.json
└─ Interface/
   └─ AddOns/
```

Startup resolves TocPilot's own directory, requires `WoW.exe` beside it, and ensures `Interface\AddOns` exists.

`TocPilot.json` is the local state file. Writes are atomic; schema changes must not destructively reset unknown/new fields. Installed state must never advance before the corresponding filesystem operation commits.

### Package / Provider Model

A package is independent of how its files are obtained.

Current supported source families:

- GitHub branch packages;
- public GitLab branch packages;
- GitHub latest-stable exact standalone DLL release assets;
- TocPilot's own GitHub Release self-update.

Branch packages store/compare the selected remote commit SHA and create no local Git repository.

Release/DLL packages track the exact release policy/asset/destination needed to update deterministically.

Private repositories/tokens and self-hosted GitLab remain deferred.

### Repository Classification

Add Git classification is intentionally shallow and deterministic.

After removing the provider archive wrapper, inspect only:

- direct `.toc` files in repository root;
- direct `.toc` files in each immediate child directory.

Classification:

1. root `.toc` -> root addon;
2. no root `.toc` + one immediate child addon -> single nested addon;
3. no root `.toc` + multiple immediate child addons -> repository library with selectable children;
4. root addon plus immediate-child addon roots -> mixed/ambiguous; stop rather than guess;
5. no addon roots on GitHub -> latest-stable exact standalone-DLL fallback;
6. no addon roots on GitLab -> concise no-addon result; GitLab release support is not implied.

Do not recursively search arbitrary repository depth to classify libraries. Once a specific addon root is selected, its subtree can be copied/validated normally.

Repository-library siblings are independent selectable install units unless explicit dependency work is added later.

### Ownership / Installation

Each package records the files/roots it owns.

Ownership invariants:

- unrelated files must not be removed;
- two durable package records must not ambiguously own the same addon root;
- unmanaged live addon roots are not silently overwritten;
- a multi-root package is not partially replaced;
- same-root managed replacement is explicit and transactional.

Managed same-root replacement keeps the old package authoritative while the replacement is staged/validated. Ownership/state swaps only after the live install commits. State-save or install failure must roll the filesystem/state back to the old package.

Archive installs use staging/validation/commit semantics:

1. resolve desired revision;
2. download;
3. validate;
4. securely extract to staging;
5. determine install mapping/ownership;
6. reject unsafe paths/collisions;
7. prepare rollback state;
8. install;
9. remove obsolete files owned by that package;
10. persist state;
11. clean staging/backup.

Path traversal, absolute paths, drive-qualified archive paths and writes outside approved roots are invalid.

### Direct DLL Policy

Direct DLL management is deliberately narrow:

- GitHub latest stable only at present;
- exact standalone `.dll` release asset;
- exact approved filename in WoW root;
- future releases must retain the configured exact asset name;
- first-manage trust warning identifies repository, asset and destination;
- TocPilot does not inspect ZIP/7z/RAR/installer/bundle release assets to discover executable payloads;
- TocPilot does not alter antivirus exclusions/settings or terminate WoW automatically.

The direct DLL path intentionally writes the release payload to the configured final DLL path rather than creating staged/temp/renamed/backup DLLs. After completion, verify against GitHub asset digest when available or exact `<asset>.sha256`; if verification cannot be established, installation is refused and installed state stays unchanged.

### Self-Update

Self-update is the normal TocPilot delivery/test path.

Startup:

1. query latest stable GitHub Release for TocPilot;
2. compare release tag to compiled semantic version;
3. find direct `TocPilot.exe`;
4. obtain expected SHA-256;
5. when newer, automatically download/verify/apply the update before addon/package scanning continues.

Manual update checks report availability and wait for user action.

Replacement uses the existing two-process updater handoff:

- current process downloads/verifies the new EXE to Windows temp;
- launches updater mode with current PID/real target path/working directory;
- old process exits;
- updater waits for confirmed process termination;
- performs bounded filesystem-lock retries;
- replaces the real executable atomically while retaining rollback safety;
- launches the installed new TocPilot;
- cleans staging after success.

A failed update must leave the old TocPilot runnable and clearly report failure.

### Concurrency

- UI network/download work must not freeze the window.
- The same package must not update concurrently with itself.
- Sequential install commits are acceptable.
- Application self-update must not proceed through replacement while a package install commit is active.
- Prefer correctness over speculative parallelism.

### Current UI / Status Contract

Compact mode:

`Name | Status`

Advanced mode:

`Name | Branch | Version | Local SHA | Git SHA | Status`

Version semantics are local-only:

- read only installed package-owned `.toc` files;
- one unique nonblank `## Version:` -> display it;
- none -> `—`;
- disagreement -> `Multiple`;
- DLL package -> `—`;
- no Git-tag inference, archive fetch or network work is used for Version.

Steady status text:

- `Up To Date`
- `Update Available`
- `Not Installed`
- `Not Configured`
- `Needs Attention`

Row semantics retained from v0.3.4:

- update-available rows are orange;
- successfully updated packages are green for the current session;
- orange rows sort/group ahead of green, then normal rows under the selected sort;
- selected rows use normal Windows selection colours;
- green session state clears on Refresh All or exit;
- Refresh All and Update New process packages in current visible list order;
- Update New returns the package list to the top when complete.

Advanced column widths/order are persisted. The dedicated Lock Columns UI/behaviour is removed in v0.3.6; Advanced columns remain directly resizable/reorderable, while Compact must not overwrite the saved Advanced layout. The legacy JSON `package_columns_locked` field is retained for state compatibility but no longer controls the UI.

### Removal UX

Installed-addon Uninstall/Remove uses the native expandable TaskDialog:

- compact counts by default;
- No/cancel is the default;
- expandable details show exact TocPilot-owned addon roots and recorded files;
- Uninstall removes owned files but retains package/tracking;
- Remove deletes package state after owned files are removed;
- record-only removal clearly states that no addon files are being removed.

## Recent Relevant Commits / Release Provenance

- `8272778adf0c67191c0525459cc026b41f50db30` — documentation baseline immediately before this workflow migration.
- `1d3050f4bc755e7b8be0762d3f6e6afe738f1525` — v0.3.5 merge commit and release tag target.
- `13b71f7a8cbfa628338e161c452cf7caaa5d6978` — final v0.3.5 feature head before merge; PR Build run `35931674431` (#541) passed Windows x64 Release build and 17/17 CTest tests.
- v0.3.5 Release workflow run `35932008997` (#48), Windows x64 job `107420500944`, rebuilt the exact merged `main` commit, validated source version, passed 17/17 tests, generated SHA-256, created/verified tag `v0.3.5`, and published the release assets.
- Published v0.3.5 `TocPilot.exe`: 2,425,856 bytes, SHA-256 `7b6756d80cd115dec2c6ec38c258adb10a6cb5b6e8d91ad3dca883ca1b29db9b`.
- Published `TocPilot.exe.sha256` asset SHA-256: `e2b459a74ff0f449efbd1eac201a0c372ba7f3b0305316019cba0bfd4b7fd6c5`.

## Completed / Runtime-Confirmed

Previously runtime-confirmed foundations include:

- Normal installed startup self-update `v0.3.4 -> v0.3.5` passed.
- Advanced six-column presentation is correct; column persistence/order is good in runtime use.
- Compact mode remains `Name | Status` and does not overwrite the Advanced layout.
- DLL rows/Version presentation are aligned correctly; DLL Version is `—`.
- Orange/green row presentation and update/green/normal priority ordering were reported correct in steady state.
- Changing a package branch correctly changes its steady status to `Update Available` when the selected branch head differs from the installed revision; the earlier colour concern has not reproduced.
- TocPilot automatic self-update works end-to-end; notably installed `v0.1.37 -> v0.3.0` passed.
- Installed `v0.3.2 -> v0.3.3` normal startup self-update passed before the v0.3.4 release.
- Direct DLL management has been runtime-proven with ClassicAPI and Nampower.
- GitHub Add Git root-addon handling has been exercised.
- Repository-library / multi-addon selection has been exercised.
- GitHub no-addon -> standalone-DLL fallback/discovery has been exercised.
- Core package ownership, secure archive installation and normal branch tracking are established product behaviour.

Do not infer runtime confirmation for the newer v0.3.4/v0.3.5 UX deltas listed below.

## Published / Awaiting Runtime Test

### v0.3.5 delta still needing confirmation or follow-up

- Real local TOC Version values are correct after a refresh, but entering Advanced initially shows `—` until Refresh; this should populate immediately from the installed package files.
- `Multiple` on a real package with conflicting owned TOC versions remains untested.
- Lock Columns is no longer considered clearly useful; reassess/remove the dedicated toggle in the follow-up UX slice rather than preserving it by inertia.
- Single-branch packages should not present an actionable branch arrow/menu once TocPilot knows there is only one branch.
- Healthy managed DLL/release packages sorting normally rather than as attention rows should remain covered by the follow-up runtime pass.

### Retained v0.3.4 UX / follow-up

- During Update New, rows currently turn black and temporarily show `Up To Date` while the update is actually in progress, then turn green after completion. This transient state is misleading and should be corrected.
- Refresh All / Update New ordering and final green steady-state behaviour otherwise appeared good in the current runtime pass.
- Green clearing on Refresh All/app exit and Update New final scroll-to-top can be rechecked with the follow-up build.

### Earlier deferred runtime checks

- v0.3.2 removal confirmation UI.
- Single-nested Add Git case.
- Same-root managed-addon overwrite/cancel against a real installation.
- Mixed root + child refusal.
- Terminal no-supported-content result.

## Implemented / Awaiting CI and Runtime Test

On `fix/v0.3.6-runtime-ux`:

- `a8a1f88604bf6c355fbdce7692fba6b9ed336c00` implements the focused UX follow-up:
  - active Update New item status is derived from authoritative batch state so list rebuilds retain `Updating...` and orange semantics until the item completes;
  - entering Advanced immediately rebuilds local Version values from installed owned TOCs, with no network refresh and no duplicate Version field persisted to JSON;
  - branch arrows are shown only after branch metadata confirms more than one branch, and single-branch rows do not open a branch menu;
  - the Lock Columns checkbox/locking behaviour is removed while column resize/reorder persistence remains.
- Version sources were advanced to `v0.3.6` through branch head `b8399815a56e7dcf411f4b0425504860132f3262`.
- No v0.3.6 CI result, merge, release, self-update or runtime result should be inferred yet.

## Static / Automated Checks

For v0.3.5:

- Final feature head `13b71f7a8cbfa628338e161c452cf7caaa5d6978` passed the Windows x64 Release build and complete 17/17 CTest suite in Build run `35931674431`.
- Release commit `1d3050f4bc755e7b8be0762d3f6e6afe738f1525` was rebuilt by Release workflow run `35932008997`.
- Release workflow source-version validation passed.
- Complete 17/17 CTest suite passed.
- SHA-256 sidecar generation passed.
- Tag creation/verification and direct release-asset publication passed.
- GitHub latest stable reports `v0.3.5`.

## Current Issues

Runtime-discovered v0.3.5 follow-up issues:

- Update New transient UI is misleading: rows can display black `Up To Date` while their install is still running, before becoming green on success.
- Advanced Version values are correct after Refresh, but entering Advanced can initially show `—`; Version should be populated immediately without requiring network refresh.
- A single-branch package should not expose an actionable branch arrow/menu once branch metadata confirms there is no alternative.
- The earlier branch-change colour concern is currently non-reproducible and is not treated as a defect.

The dedicated Lock Columns toggle is also being reconsidered as unnecessary UI; any removal should be handled deliberately in the same focused UX slice rather than as an unrelated refactor.

## Testing

### Last Runtime Baselines

- Automatic startup self-update `v0.1.37 -> v0.3.0`: passed.
- Automatic startup self-update `v0.3.2 -> v0.3.3`: passed.
- Direct DLL path with ClassicAPI/Nampower: passed.
- Several Add Git root/library/DLL-discovery paths: passed as listed above.

### Next Runtime Test

After the focused follow-up build is published, self-update to it normally and verify:

1. entering Advanced immediately shows installed TOC Version values without requiring Refresh;
2. Update New keeps an honest in-progress status/semantic presentation until each package actually commits, then turns green on success;
3. packages with only one discovered branch show no actionable branch arrow/menu;
4. Advanced six-column persistence and Compact isolation remain intact;
5. healthy DLL/release rows still sort normally;
6. green clearing on Refresh All/app exit and Update New final scroll-to-top still behave as documented.

## Planned / Next Work

After the focused v0.3.5 follow-up slice is runtime-confirmed:

- begin P5 import/export as its own isolated slice;
- define its state/ownership semantics before implementation;
- keep package editing and the other deferred P5 work out of that slice unless explicitly reopened.

## Deferred / Out of Scope

Current deferred work includes:

- P5 package-edit flow;
- better ambiguous archive mapping;
- optional local-modification detection/backups;
- richer diagnostics;
- GitLab release/direct-asset support;
- release-archive executable discovery;
- arbitrary-depth repository catalogue discovery;
- dependency resolution between repository-library children;
- broader collection UX;
- prerelease/direct-file expansion beyond the current exact-DLL path;
- private repository authentication/tokens;
- self-hosted GitLab;
- code signing;
- rollback-history UI;
- repository/search discovery;
- changelog viewer;
- headless/CLI mode;
- scheduled/background updating;
- Windows notifications.

Do not add support for user-uploaded ZIP/7z/RAR/installer/bundle release assets merely to discover executable/DLL payloads unless that product policy is explicitly revisited.

## Release / Documentation Notes

- Current published runtime baseline remains `v0.3.5` at `1d3050f4bc755e7b8be0762d3f6e6afe738f1525`.
- Documentation-only commits after that release do not imply a new runtime build.
- This workflow migration intentionally does **not** change `CMakeLists.txt`, `src/version.h` or `.github/release-version`.
- No tag or GitHub Release should be created for this documentation migration.

## Exact Next Step

Open/validate the v0.3.6 feature PR from `fix/v0.3.6-runtime-ux`, run the Windows x64 Release build and full CTest suite, review any CI failures, then merge only if green. After merge, publish `v0.3.6` through the normal Release workflow and runtime-test normal `v0.3.5 -> v0.3.6` self-update plus the focused Version/Update New/single-branch/column-persistence regression matrix.

**Do not begin P5 import/export until v0.3.6 is runtime-confirmed.**
