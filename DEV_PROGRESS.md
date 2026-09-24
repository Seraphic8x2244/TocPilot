# TocPilot Development Progress

> Live TocPilot development context for a fresh chat. Keep this current and concise. Git carries chronology; this file carries the current product contract and next work.

## Current

- Active branch: `main`.
- Source/application version: `v0.3.5`.
- Documentation migration baseline head: `8272778adf0c67191c0525459cc026b41f50db30` (`Document retained v0.3.5 row colours`).
- Latest published release: `v0.3.5`.
- Release/source commit and tag target: `1d3050f4bc755e7b8be0762d3f6e6afe738f1525` (`Merge v0.3.5 version and status UX`).
- Current goal: finish runtime validation of published `v0.3.5` and its retained v0.3.4 UX behaviour before beginning the next P5 feature.
- Current scope boundary: **do not start P5 import/export until the pending v0.3.5 runtime pass is complete**.
- This documentation migration does not change runtime code, source version, `.github/release-version`, tag or release.

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

Advanced column widths/order and Lock Columns are persisted. Compact mode must not overwrite the saved Advanced layout.

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

- During the current v0.3.5 runtime pass, orange/green row presentation and update/green/normal priority ordering were reported correct.
- Changing a package branch correctly changes its steady status to `Update Available` when the selected branch head differs from the installed revision.
- TocPilot automatic self-update works end-to-end; notably installed `v0.1.37 -> v0.3.0` passed.
- Installed `v0.3.2 -> v0.3.3` normal startup self-update passed before the v0.3.4 release.
- Direct DLL management has been runtime-proven with ClassicAPI and Nampower.
- GitHub Add Git root-addon handling has been exercised.
- Repository-library / multi-addon selection has been exercised.
- GitHub no-addon -> standalone-DLL fallback/discovery has been exercised.
- Core package ownership, secure archive installation and normal branch tracking are established product behaviour.

Do not infer runtime confirmation for the newer v0.3.4/v0.3.5 UX deltas listed below.

## Published / Awaiting Runtime Test

### v0.3.5 delta

- Automatic startup self-update `v0.3.4 -> v0.3.5`.
- Real local TOC Version display.
- `Multiple` on a real package with conflicting owned TOC versions.
- Five-to-six-column persisted-state migration.
- Six-column width/order persistence.
- Lock Columns behavior with six columns.
- Compact/Advanced toggling without Compact corrupting Advanced layout.
- Inline Branch selector geometry after column reorder/addition.
- New steady status wording.
- Healthy managed DLL/release packages sorting normally rather than as attention rows.

### Retained v0.3.4 UX still needing runtime confirmation

- Live orange -> green transition during Update New.
- Refresh All visible-order processing.
- Update New visible-order processing.
- Green clearing on Refresh All/app exit.
- Update New final scroll-to-top.

### Earlier deferred runtime checks

- v0.3.2 removal confirmation UI.
- Single-nested Add Git case.
- Same-root managed-addon overwrite/cancel against a real installation.
- Mixed root + child refusal.
- Terminal no-supported-content result.

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

No confirmed source defect is currently documented for v0.3.5.

Current runtime observation: changing a selected package's branch correctly moves its status to `Update Available`, but the row does not visibly show orange/green while it remains selected. Static review shows this is expected under the documented selected-row contract: custom draw intentionally uses normal Windows selection colours for selected rows, and the branch-change path reselects the package after saving. Verify the semantic colour after selecting a different row before classifying this as a defect.

The remaining active issue is **runtime validation debt** for the rest of the published v0.3.5 UX/update matrix. Do not interpret publication or green CI as that runtime pass.

## Testing

### Last Runtime Baselines

- Automatic startup self-update `v0.1.37 -> v0.3.0`: passed.
- Automatic startup self-update `v0.3.2 -> v0.3.3`: passed.
- Direct DLL path with ClassicAPI/Nampower: passed.
- Several Add Git root/library/DLL-discovery paths: passed as listed above.

### Next Runtime Test

Continue the active installed v0.3.5 runtime pass. First, after changing a package branch and seeing `Update Available`, select a different row and confirm the branch-switched row becomes orange once it is no longer selected. If it remains uncoloured after deselection, record that as a real row-colour defect.

Then verify the remaining matrix:

1. automatic startup self-update `v0.3.4 -> v0.3.5` result, if not already explicitly recorded;
2. Advanced shows `Name | Branch | Version | Local SHA | Git SHA | Status`;
3. real addon Version values are correct and DLL Version is `—`;
4. steady statuses use the current wording;
5. healthy DLL/release rows sort normally;
6. existing Advanced widths/order migrate to six columns and persist;
7. Lock Columns still blocks resize/reorder/autosize;
8. Compact remains `Name | Status` and does not overwrite Advanced layout;
9. Branch selector remains correctly positioned after reorder;
10. live orange -> green transition during Update New, visible-order Refresh All/Update New, green clearing and final scroll-to-top behave as documented.

Record partial results accurately if the whole matrix is not completed.

## Planned / Next Work

After the pending runtime pass succeeds:

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

Continue the published v0.3.5 runtime pass. First verify that a branch-switched `Update Available` row turns orange after selecting a different row; selected rows intentionally use normal Windows selection colours. Then complete the remaining self-update, Version/Local SHA/Git SHA/Status, healthy DLL sorting, six-column persistence/Lock/Compact, Update New transition/order, clearing, and scroll-to-top checks.

**Do not begin P5 import/export until that runtime pass is complete.**
