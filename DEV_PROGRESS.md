# TocPilot Development Progress

> Live TocPilot development context for a fresh chat. Keep this current and concise. Git carries chronology; this file carries the current product contract and next work.

## Current

- Active branch: `main`.
- Source/application version: `v0.3.6`.
- Latest runtime source/release commit: `94d15feb684bc10f13c35c75649f001f07ae1f55` (merge of PR #6, `v0.3.6 runtime UX follow-up`).
- Latest published release: `v0.3.6`.
- Release/source commit and tag target: `94d15feb684bc10f13c35c75649f001f07ae1f55`.
- Current goal: runtime-validate published `v0.3.6` through the normal installed self-update path and focused UX regression matrix.
- Current scope boundary: **do not start P5 import/export until v0.3.6 is runtime-confirmed**.
- Documentation-only commits after the release do not change the published runtime baseline.

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

- `94d15feb684bc10f13c35c75649f001f07ae1f55` — merged PR #6 and exact v0.3.6 release/tag target.
- `c253ce6e5c0c2d71df39f5fba718cbc3fdaf2638` — final v0.3.6 PR head/documented feature checkpoint.
- `a8a1f88604bf6c355fbdce7692fba6b9ed336c00` — focused runtime UX implementation.
- PR #6 Build workflow run `36030303266` (#552), Windows x64 job `107737068577`, passed Release build and **17/17 CTest tests**.
- v0.3.6 Release workflow run `36030677374` (#49), Windows x64 Release job `107738323507`, rebuilt exact merge commit `94d15feb684bc10f13c35c75649f001f07ae1f55`, validated source version, passed **17/17 CTest tests**, generated SHA-256, created tag `v0.3.6`, and published direct release assets.
- Published v0.3.6 `TocPilot.exe`: 2,425,344 bytes, SHA-256 `97367453f8df171aae57e856355ac56a84c4217cd8ea4c55bc4c0006682c8be7`.
- Published v0.3.6 `TocPilot.exe.sha256` asset SHA-256: `75a8c2ede9cd2280df1e8169db1a0519032150a851cdf747892ffe18d5d9a189`.
- `1d3050f4bc755e7b8be0762d3f6e6afe738f1525` — prior v0.3.5 release/source commit.

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

### v0.3.6 focused delta

Published but not yet runtime-confirmed:

- Normal installed startup self-update `v0.3.5 -> v0.3.6`.
- Entering Advanced immediately reads installed package-owned TOC Version values without requiring Refresh; this remains local filesystem metadata and is intentionally not duplicated into JSON.
- During Update New, the active item derives `Updating...` from authoritative batch state and remains orange across list rebuilds until completion; successful updates then become green for the session.
- Once branch metadata confirms only one branch, the Branch cell exposes no actionable arrow/menu; multi-branch packages retain the selector.
- The dedicated Lock Columns checkbox/locking behaviour is removed. Advanced columns remain directly resizable/reorderable and persist; Compact must continue not to overwrite the saved Advanced layout.
- Healthy managed DLL/release rows should continue to sort normally and DLL Version should remain `—`.
- Green clearing on Refresh All/app exit and Update New final scroll-to-top should be rechecked as regressions.

Still optional validation debt where a real fixture is available:

- `Multiple` on a package with conflicting owned TOC Version values.
- Earlier removal/Add Git edge cases listed under Deferred Runtime Checks below.

### Deferred Runtime Checks

- v0.3.2 removal confirmation UI.
- Single-nested Add Git case.
- Same-root managed-addon overwrite/cancel against a real installation.
- Mixed root + child refusal.
- Terminal no-supported-content result.

## Static / Automated Checks

For v0.3.6:

- PR head `c253ce6e5c0c2d71df39f5fba718cbc3fdaf2638` passed Windows x64 Release build and complete **17/17 CTest** suite in Build run `36030303266` (#552), job `107737068577`.
- Merge/release commit `94d15feb684bc10f13c35c75649f001f07ae1f55` was rebuilt by Release workflow run `36030677374` (#49), job `107738323507`.
- Release source-version validation passed for `v0.3.6`.
- Complete **17/17 CTest** suite passed in the release rebuild.
- SHA-256 sidecar generation passed.
- Tag `v0.3.6` creation and direct `TocPilot.exe` / `TocPilot.exe.sha256` publication passed.
- GitHub release `v0.3.6` targets exact merge commit `94d15feb684bc10f13c35c75649f001f07ae1f55`.

The prior v0.3.5 release also passed its documented 17/17 Release workflow validation; retain it only as the inherited runtime baseline.

## Current Issues

No known source defect is currently documented for v0.3.6.

The active issue is **runtime validation debt**: v0.3.6 is published and CI/release-validated but has not yet been exercised through the real installed `v0.3.5 -> v0.3.6` self-update path or its focused UX regression matrix.

The earlier branch-change colour concern did not reproduce and is not considered a defect.

Runtime observation to carry forward without acting on it yet: when a selected/targeted addon becomes `Update Available`, the Windows selection background highlight is desirable, but its text remains the normal selected-row colour instead of changing to the semantic state colour. Desired UX is to keep the selection background while allowing the text colour to reflect the row state (for example orange for `Update Available`, green for session-updated).

## Testing

### Last Runtime Baselines

- Normal installed startup self-update `v0.3.4 -> v0.3.5`: passed.
- v0.3.5 Advanced six-column presentation and column persistence/order: passed.
- v0.3.5 Compact mode preserving Advanced layout: passed.
- v0.3.5 DLL alignment/Version `—`: passed.
- v0.3.5 steady orange/green row presentation and update/green/normal ordering: passed.
- v0.3.5 branch switching correctly reaches `Update Available`; earlier colour concern did not reproduce.
- Earlier self-update baselines `v0.1.37 -> v0.3.0` and `v0.3.2 -> v0.3.3`: passed.
- Direct DLL path with ClassicAPI/Nampower and several Add Git root/library/DLL-discovery paths: passed.

### Next Runtime Test

Start from the currently installed `v0.3.5` and launch TocPilot normally.

Verify:

1. normal startup self-update reaches published `v0.3.6` without manual EXE replacement;
2. open Advanced **without pressing Refresh** and confirm installed addon Version values appear immediately; DLL Version remains `—`;
3. create one or more known updates, press Update New, and confirm the active package stays orange with `Updating...` until its install actually completes, then turns green; queued updates must not falsely become black `Up To Date`;
4. on a known single-branch repository, allow branch metadata to load and confirm there is no actionable arrow/menu; confirm a multi-branch repository still offers its branch choices;
5. confirm the Lock Columns checkbox is gone, Advanced columns can be resized/reordered and persist after restart, and Compact still does not overwrite that Advanced layout;
6. confirm healthy DLL/release rows still sort normally;
7. if practical, confirm Refresh All/app restart clears green session state and Update New finishes scrolled to the top.

Record partial results accurately if the entire matrix is not completed.

## Planned / Next Work

After v0.3.6 runtime confirmation:

- begin P5 import/export as its own isolated slice;
- define import/export state, identity, ownership and conflict semantics before implementation;
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

- Current published runtime baseline is `v0.3.6` at `94d15feb684bc10f13c35c75649f001f07ae1f55`.
- Release workflow run `36030677374` (#49) is the authoritative v0.3.6 product build.
- Documentation-only commits after that release do not imply a new runtime build and require no version bump/release.
- Version remains local installed TOC metadata; do not persist a second Version value into JSON unless the product contract is deliberately changed.
- The legacy `package_columns_locked` JSON field remains accepted for compatibility but no longer controls v0.3.6 UI behaviour.

## Exact Next Step

Runtime-test normal installed `v0.3.5 -> published v0.3.6` through TocPilot's startup self-update path, then verify immediate Advanced Version population, honest orange `Updating...` Update New state through commit, single-vs-multi-branch selector affordance, direct Advanced column resize/reorder persistence without the Lock Columns checkbox, Compact isolation, healthy DLL sorting, and the green-clearing/scroll-to-top regressions where practical.

**Do not begin P5 import/export until v0.3.6 is runtime-confirmed.**
