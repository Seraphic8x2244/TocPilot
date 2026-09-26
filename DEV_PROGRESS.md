# TocPilot Development Progress

> Live TocPilot development context for a fresh chat. Keep this current and concise. Git carries chronology; this file carries the current product contract and next work.

## Current

- Active branch: `main`.
- Source/application version: `v0.3.9`.
- Latest published release: `v0.3.9`.
- Release/source commit and tag target: `57feb4307058b484ff980bd30b60d7eafb4466af` (merge of PR #10).
- Latest runtime-confirmed release: `v0.3.8` at `17ce349273c6f2d76572c7107f3c6f7b139cf8d8` for the normal installed self-update path. v0.3.9 remains published/CI-checked with its updater/UI delta not separately confirmed.
- Latest verified `main` runtime/release head: `57feb4307058b484ff980bd30b60d7eafb4466af` (v0.3.9 WWW-column release; published/CI-checked).
- A1 runtime gate: **accepted for forward development** on 2026-09-26. Fresh install, reinstall, Update New and Remove Addon passed. Managed same-root replacement runtime validation is explicitly deferred rather than blocking A2. The deterministic crash-window tests remain the primary validation for restart-recovery semantics.
- Current goal: implement **A2 durable state semantic validation only**.
- Current scope boundary: do not mix A2 with ZIP bounds, updater hardening, async DLL discovery, warning cleanup or unrelated UI changes. The next UI follow-up is separately queued: rename `WWW` to lowercase `www`, and render repository links blue + underlined at all times, including selected/semantic rows.
- Audit continuity: `audit_dump.md` is a temporary scratch checkpoint for the interrupted broad audit only; this file remains the sole authoritative live development source of truth.
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

`Name | WWW | Branch | Version | Local SHA | Git SHA | Status`

`WWW` is derived from the package's existing provider/repository identity (GitHub/GitLab), displays the repository URL and opens that repository in the default browser on a single click. It adds no duplicate durable source field.

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
- selected rows keep the normal Windows selection background, but semantic row text colour should remain visible while selected;
- green session state clears on Refresh All or exit;
- Refresh All and Update New process packages in current visible list order;
- Update New returns the package list to the top when complete.

Advanced column widths/order are persisted. The dedicated Lock Columns UI/behaviour is removed in v0.3.6; Advanced columns remain directly resizable/reorderable, while Compact must not overwrite the saved Advanced layout. The legacy JSON `package_columns_locked` field is retained for state compatibility but no longer controls the UI.

Branch-selector target behaviour after the v0.3.6 runtime follow-up:

- a branch package whose remote repository advertises more than one branch should always show the Branch-cell arrow once that repository's branch metadata is known;
- a repository advertising exactly one branch should never show an actionable arrow or dropdown;
- branch affordance must be based on per-repository/package metadata rather than only the currently selected row;
- changing a branch may re-sort an addon to the update-available group, but the selection/branch interaction must remain anchored to that package identity after the list rebuild;
- remote branch lists are transient remote metadata and should not be duplicated into durable package state merely for UI rendering;
- Add Git already fetches the full Git smart-HTTP branch advertisement in the branch chooser; reuse that result to seed runtime metadata rather than immediately fetching it again;
- normal branch-head refresh also fetches the full advertisement internally, so retain/reuse that information to discover newly added/removed remote branches during ordinary startup/Refresh All checks without an extra branch-list request.

### Removal UX

Installed-addon Uninstall/Remove uses the native expandable TaskDialog:

- compact counts by default;
- No/cancel is the default;
- expandable details show exact TocPilot-owned addon roots and recorded files;
- Uninstall removes owned files but retains package/tracking;
- Remove deletes package state after owned files are removed;
- record-only removal clearly states that no addon files are being removed.

## Recent Relevant Commits / Release Provenance

- `57feb4307058b484ff980bd30b60d7eafb4466af` — merged PR #10 and exact v0.3.9 release/tag target.
- PR #10 head `06553ee63c8d6a645a42861ffe80bb2351b88c40` passed Build workflow run `36251399338` (#581), Windows x64 job `108429989143`, including **17/17 CTest tests** and the six-column -> seven-column persisted-layout migration case.
- v0.3.9 Release workflow run `36251599364` (#52), Windows x64 Release job `108430537496`, rebuilt exact merge commit `57feb4307058b484ff980bd30b60d7eafb4466af`, validated source version, passed **17/17 CTest tests**, generated SHA-256, created tag `v0.3.9`, and published direct release assets.
- Published v0.3.9 `TocPilot.exe`: 2,475,008 bytes, SHA-256 `0f31c5543155a515fd4af4a22a6dd4bea91e4d64e2babe65b9406adaeedfb58e`.
- Published v0.3.9 `TocPilot.exe.sha256` asset SHA-256: `a9f4c90767f275d5c2f709e2cedf5c664727d322475f6eaa25cb6ed37299e60d`.
- `17ce349273c6f2d76572c7107f3c6f7b139cf8d8` — merged PR #9 and exact v0.3.8 release/tag target.
- PR #9 head `8d56730e1079723c1541745b81e80b30e6d81a08` passed Build workflow run `36243060906` (#577), Windows x64 job `108407087603`, including **17/17 CTest tests**.
- v0.3.8 Release workflow run `36243263009` (#51), Windows x64 Release job `108407654961`, rebuilt exact merge commit `17ce349273c6f2d76572c7107f3c6f7b139cf8d8`, validated source version, passed **17/17 CTest tests**, generated SHA-256, created tag `v0.3.8`, and published direct release assets.
- Published v0.3.8 `TocPilot.exe`: 2,473,472 bytes, SHA-256 `82cd45ba4e060f1bd0915dd8af23dafce9bb516384677f4f1896fa37fecefaba`.
- Published v0.3.8 `TocPilot.exe.sha256` asset SHA-256: `6ae0cc32b33fde03aa5024ae58cbe2729cb216e70beb2597089367f52cd5e136`.
- `ec410df48787fa88a97d49489c4f99ea6893811a` — merged PR #7 and exact v0.3.7 release/tag target.
- PR #7 head `255a6439d788050ea034909e855851faeba78f4d` passed Build workflow run `36151542439` (#558), Windows x64 job `108125759401`, including **17/17 CTest tests**.
- v0.3.7 Release workflow run `36161737119` (#50), Windows x64 Release job `108159686091`, rebuilt exact merge commit `ec410df48787fa88a97d49489c4f99ea6893811a`, validated source version, passed **17/17 CTest tests**, generated SHA-256, created tag `v0.3.7`, and published direct release assets.
- Published v0.3.7 `TocPilot.exe`: 2,431,488 bytes, SHA-256 `81f6cd5b19a8952ea307bf9c004217d84baa5ebb95330488cae570292764d220`.
- Published v0.3.7 `TocPilot.exe.sha256` asset SHA-256: `81a9244b9a9a2af031e92ce98dfb0795a755e1db7ebc9c76079c12c66f0d4dc4`.
- `064cbef1707e0c6e58ed2664cba87158a1a14085` — retains full remote branch metadata even when the currently tracked branch disappears, so branch choices can recover from a deleted ref.
- `c1884287b1320f7d644f27fc3a220bcd4e7ba3ef` — implements v0.3.7 branch metadata cache, Add Git cache seeding, selected-row semantic colour preservation, package-ID selection anchoring and the v0.3.7 version bump.
- `430b3581b7b51ee9b1c45d64d06824984f1f4656` — main documentation checkpoint defining this focused follow-up.
- `94d15feb684bc10f13c35c75649f001f07ae1f55` — merged PR #6 and exact v0.3.6 release/tag target.
- `c253ce6e5c0c2d71df39f5fba718cbc3fdaf2638` — final v0.3.6 PR head/documented feature checkpoint.
- `a8a1f88604bf6c355fbdce7692fba6b9ed336c00` — focused runtime UX implementation.
- PR #6 Build workflow run `36030303266` (#552), Windows x64 job `107737068577`, passed Release build and **17/17 CTest tests**.
- v0.3.6 Release workflow run `36030677374` (#49), Windows x64 Release job `107738323507`, rebuilt exact merge commit `94d15feb684bc10f13c35c75649f001f07ae1f55`, validated source version, passed **17/17 CTest tests**, generated SHA-256, created tag `v0.3.6`, and published direct release assets.
- Published v0.3.6 `TocPilot.exe`: 2,425,344 bytes, SHA-256 `97367453f8df171aae57e856355ac56a84c4217cd8ea4c55bc4c0006682c8be7`.
- Published v0.3.6 `TocPilot.exe.sha256` asset SHA-256: `75a8c2ede9cd2280df1e8169db1a0519032150a851cdf747892ffe18d5d9a189`.
- `1d3050f4bc755e7b8be0762d3f6e6afe738f1525` — prior v0.3.5 release/source commit.

## Completed / Runtime-Confirmed

Published `v0.3.7` branch/list runtime matrix completed on 2026-09-25:

- branch arrows are correct for known multi-branch vs single-branch repositories;
- changing branch and triggering update-priority re-sort keeps selection/interaction on the same addon;
- selected orange/green rows retain their semantic text colour while keeping the Windows selection background;
- adding a remote branch while TocPilot remains open is discovered by Refresh All alone and updates the branch affordance/list;
- deleting that remote branch while TocPilot remains open is also discovered by Refresh All alone and removes the now-unneeded branch affordance;
- no close/restart or remove/re-add was needed for remote branch metadata refresh.

Published `v0.3.6` runtime-validation gate completed on 2026-09-24:

- Normal installed startup delivery to published `v0.3.6` is accepted as passed as part of the user's active release testing; do not separately ask whether the updater worked when the user is already testing that published version through the normal installed workflow unless a failure/manual replacement is reported.
- Entering Advanced without Refresh immediately populated Version for addons whose owned TOCs contain `## Version:`; observed blank/`—` cases were checked and their TOCs genuinely contained no Version field. DLL Version remains `—`.
- Update-available/update-session colours and Update New behaviour were otherwise correct through the focused matrix.
- Single-branch repositories expose no actionable branch selector after metadata loads; multi-branch repositories retain branch choices.
- Lock Columns is gone; Advanced columns remain resizable/reorderable and persist; Compact does not overwrite the Advanced layout.
- Healthy managed DLL/release rows sort normally.
- Refresh All/app restart green clearing and Update New final scroll-to-top passed.
- One UI issue remains: selecting an orange `Update Available` row causes its semantic orange text to revert to the normal selected-row text colour. Keep the Windows selection background, but preserve semantic orange/green text while selected in a future focused fix.

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

## Remaining Optional Runtime Checks

No v0.3.6 release-gate validation remains.

Optional validation debt where a real fixture is available:

- `Multiple` on a package with conflicting owned TOC Version values.
- Earlier removal/Add Git edge cases listed under Deferred Runtime Checks below.

### Deferred Runtime Checks

- Managed same-root replacement against a real installation (A1 runtime debt; explicitly deferred on 2026-09-26 and does not block A2).
- v0.3.2 removal confirmation UI.
- Single-nested Add Git case.
- Mixed root + child refusal.
- Terminal no-supported-content result.

## Static / Automated Checks

For A1 durable addon-transaction restart recovery (post-v0.3.7 source):

- PR #8 head `d09d27e4af05d3bafd1e024d5f40a8655357bb18` passed Windows x64 Release build and the complete **17/17 CTest** suite in Build workflow run `36241747072` (#574), job `108403455015`.
- The `addon-install-transaction` test now deterministically covers prepared-only interruption, live->backup rename before in-memory bookkeeping, all backups complete before new roots, prepared->live rename before bookkeeping, filesystem commit with durable pre-state, durable post-state before finalize, partial finalize cleanup, remove-to-absent post-state and fail-closed ambiguous durable state.
- PR #8 merged to `main` as `0396aaa17279da65f8ee0aea27ddff4458f481ac`.
- PR #9 release-prep head `8d56730e1079723c1541745b81e80b30e6d81a08` passed the complete **17/17 CTest** suite in Build workflow run `36243060906` (#577), job `108407087603`.
- v0.3.8 was published from exact merge commit `17ce349273c6f2d76572c7107f3c6f7b139cf8d8` by Release workflow run `36243263009` (#51), job `108407654961`; the release rebuild also passed **17/17 CTest tests**, created tag `v0.3.8` against that commit and published the direct EXE/checksum assets.
- v0.3.8's normal installed self-update path from v0.3.7 is runtime-confirmed.
- A1 package-operation runtime smoke: fresh install passed, reinstall passed, Update New passed and Remove Addon passed on 2026-09-26. Managed same-root replacement remains deferred.
- A1 is accepted for forward development with that explicit deferred runtime debt.
- v0.3.9 is **published/CI-checked but its updater/UI delta is not separately runtime-confirmed**.

For v0.3.7:

- PR head `255a6439d788050ea034909e855851faeba78f4d` passed Windows x64 Release build and complete **17/17 CTest** suite in Build run `36151542439` (#558), job `108125759401`.
- Merge/release commit `ec410df48787fa88a97d49489c4f99ea6893811a` was rebuilt by Release workflow run `36161737119` (#50), job `108159686091`.
- Release source-version validation passed for `v0.3.7`.
- Complete **17/17 CTest** suite passed in the release rebuild.
- SHA-256 sidecar generation passed.
- Tag `v0.3.7` was created against exact merge commit `ec410df48787fa88a97d49489c4f99ea6893811a`.
- Direct `TocPilot.exe` and `TocPilot.exe.sha256` assets were published successfully.

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

Published v0.3.9 contains the WWW-column usability slice on top of A1. A1 is accepted for forward development after fresh install, reinstall, Update New and Remove Addon passed; managed same-root replacement remains explicit deferred runtime debt. The v0.3.9 updater/UI delta is still not separately runtime-confirmed.

P6A is complete. A1 transaction restart recovery is implemented, CI-checked and published in v0.3.8 at `17ce349273c6f2d76572c7107f3c6f7b139cf8d8`, and accepted for forward development with the same-root replacement runtime check deferred. It now writes a versioned, flushed pre-mutation journal; arms it with package/transaction identity, affected-root intent and durable pre/post state markers before live renames; recovers unfinished transactions before normal package mutation; and preserves evidence rather than guessing when durable state is ambiguous.

Remaining audited implementation priority after A1 acceptance starts with:

- state load validates JSON shape but does not globally revalidate durable package semantics. It can accept duplicate package IDs, conflicting addon-root ownership, identity/provider/mode/target inconsistencies, incoherent installed state, unsafe ownership paths and direct-DLL ownership that normal mutation APIs would reject;
- the dedicated Add-Git branch dialog has no per-request generation/repository token. Closing and reopening it while its detached lookup is still running leaves a rare stale-result/HWND-reuse race; the main inline branch selector already has a generation + package-ID guard;
- Add Git latest-stable DLL discovery performs provider network I/O synchronously on the dialog thread, including the DLL-fallback dialog creation path, so provider timeout/failure can freeze that UI;
- self-update executable/checksum downloads lack explicit response-size limits/asset-size matching;
- self-update release asset/checksum transport does not reject an initial non-HTTPS URL even though the rulebook requires HTTPS provider/download traffic; normal GitHub metadata currently supplies HTTPS URLs and WinHTTP's default redirect policy blocks HTTPS -> HTTP downgrades;
- Update All does not treat branch-addon archive HTTP rate limiting as a queue-stop condition, unlike status refresh and direct-DLL update paths;
- ZIP extraction can allocate one very large uncompressed member fully in memory;
- provider/repository staging-directory sanitization can collide for distinct identities.

Lower-priority/test/build findings and contract-driven direct-DLL risks are retained in `audit_dump.md`. The async/UI pass found no high/medium GDI/icon ownership leak. It did confirm low-priority cleanup/debt: the old hidden branch COMBOBOX is now dead infrastructure after the list-cell popup redesign; main close can abandon non-install async work/staging; several rare Win32 control/subclass/timer/GetMessage failures are not surfaced.

Final severity/order:

1. **HIGH — transaction restart recovery:** **IMPLEMENTED / CI-CHECKED / PUBLISHED / ACCEPTED**, with managed same-root replacement runtime validation deferred.
2. **HIGH — durable state semantic validation:** **NEXT** — reject conflicting/impossible package records before runtime use.
3. **MEDIUM — ZIP member allocation bound:** reject a single oversized uncompressed member before allocating it.
4. **MEDIUM — self-update transport hardening:** carry/check exact asset size, cap checksum text and reject initial non-HTTPS asset/checksum URLs.
5. **MEDIUM — async latest-stable DLL discovery:** remove provider I/O from the dialog thread.
6. **MEDIUM/LOW — Add-Git branch-dialog request identity:** reject stale close/reopen completions.
7. **MEDIUM/LOW — Update All archive rate-limit propagation:** confirmed provider 429 should stop later provider-heavy queue work.
8. Remaining lower-priority work: separate live network smoke tests from deterministic default CTest; updater parent-wait handling; strict checksum-sidecar parsing; collision-proof staging identity; dead branch-combo removal; optional non-mutating shutdown cleanup; archive-test CRT/warning cleanup.

P6A bounded-pass status: **all four passes complete and documented**.

Build/test audit:
- `TocPilotArchiveTests` is currently built with CMake's default dynamic MSVC runtime while linked `TocPilotMiniz` is explicitly static-runtime, explaining the observed `LNK4098`; align the test target/runtime rather than masking the warning;
- `git-smart-http-live-github` and `self-update-live-latest` are ordinary CTest entries, so current Build and Release workflows make normal test success depend on live GitHub/network availability. Keep them as explicit live smoke coverage, but separate them from the deterministic default suite.

Power-loss durability of the addon directory rename sequence remains a verification gap: `TocPilot.json` is explicitly flushed/write-through, while addon directory moves currently use `std::filesystem::rename` without a separately verified durability guarantee.

The focused post-v0.3.6 branch/list issues remain runtime-confirmed fixed in v0.3.7.

## Testing

### Last Runtime Baselines

- Published `v0.3.8` normal installed self-update `v0.3.7 -> v0.3.8`: passed on 2026-09-26 through TocPilot's real self-updater; no manual EXE replacement was required.
- Published `v0.3.7` branch/list matrix: passed on 2026-09-25, including branch re-sort anchoring, correct arrow affordance, selected semantic row colours, and live remote branch add/delete discovery through Refresh All without restarting TocPilot.
- Published `v0.3.6` focused runtime matrix: passed on 2026-09-24.
- Advanced Version population without Refresh: passed; TOCs lacking a Version field correctly produce no Version value.
- Single-vs-multi-branch selector affordance: passed.
- Advanced resize/reorder persistence, Lock Columns removal and Compact isolation: passed.
- Healthy DLL/release row sorting: passed.
- Green clearing and Update New final scroll-to-top: passed.
- Normal installed startup self-update `v0.3.4 -> v0.3.5`: passed.
- v0.3.5 Advanced six-column presentation and column persistence/order: passed.
- v0.3.5 Compact mode preserving Advanced layout: passed.
- v0.3.5 DLL alignment/Version `—`: passed.
- v0.3.5 steady orange/green row presentation and update/green/normal ordering: passed.
- v0.3.5 branch switching correctly reaches `Update Available`; earlier colour concern did not reproduce.
- Earlier self-update baselines `v0.1.37 -> v0.3.0` and `v0.3.2 -> v0.3.3`: passed.
- Direct DLL path with ClassicAPI/Nampower and several Add Git root/library/DLL-discovery paths: passed.

### Next Runtime Test

No additional runtime test is required before starting A2. A1's managed same-root replacement check remains deferred. v0.3.9's WWW/updater UI delta can be checked opportunistically; do not block A2 on it.

The current product workflow has **Remove Addon**, not the old Uninstall flow. Do not reintroduce Uninstall as a required runtime gate.

Optional future checks remain the conflicting-TOC `Multiple` fixture and the deferred historical edge cases above.

## Planned / Next Work

### P6 — Hardening and polish

The original P5 backlog is no longer a strict sequence. Import/export and the remaining feature items are deferred while TocPilot's existing product surface is hardened and polished.

#### P6A — Robustness audit — COMPLETE

Completed as four bounded read-first passes. The audit covered:

- state load/save, schema compatibility and corruption/failure handling;
- package identity, ownership, replacement, uninstall/remove and rollback invariants;
- archive extraction/path safety and filesystem transaction boundaries;
- self-update integrity, updater handoff and release-state correctness;
- provider/network parsing, HTTP failure paths, rate limits and malformed/partial responses;
- background-thread/message lifetime, stale async results, cancellation/overlap and UI-thread state ownership;
- branch metadata caching, refresh freshness, sorting/view-order/selection identity and package reordering;
- direct DLL trust/integrity paths and file-lock/security-software failures;
- Win32 resource/handle lifetime, error propagation and silent-failure paths;
- test coverage gaps, duplicated/legacy code paths and assumptions that are no longer true.

Audit output distinguishes confirmed defects, robustness risks, cleanup opportunities and test debt. Detailed evidence remains in `audit_dump.md`; the priority and next step above/in this file are authoritative.

#### P6B — UI/UX pass

Once robustness findings are under control, exercise every normal workflow as a product rather than as isolated features: startup/self-update, Compact/Advanced, Add Git, branch selection, Refresh All, Update New, install/reinstall, DLL management, Remove/Uninstall and error/recovery paths.

Review consistency of labels, button state, selection/focus, keyboard/mouse behaviour, progress/status feedback, sorting/reordering, confirmations, empty/loading/error states, resize/DPI/text-scale behaviour and unnecessary friction. Prefer small coherent UX fixes over adding new capability.

#### P6C — Visual polish / skin

Only after interaction/layout behaviour is settled, define a restrained TocPilot visual treatment. Keep the native lightweight Windows application model and accessibility/clarity benefits; do not replace stable native controls with a framework-scale custom UI.

Possible scope includes a cleaner branded header, consistent iconography, spacing/typography cleanup, subtle panel/background treatment and purpose-built TocPilot artwork. The visual pass should make the existing UI feel deliberate and cohesive rather than heavily themed.

After P6 is accepted, reprioritize the deferred feature backlog rather than automatically returning to import/export.

## Deferred / Out of Scope

Current deferred work includes:

- P5 import/export;
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

- Current published release is `v0.3.9` at `57feb4307058b484ff980bd30b60d7eafb4466af`.
- Release workflow run `36251599364` (#52) is the authoritative v0.3.9 product build.
- Published v0.3.9 is CI/release-verified but not yet runtime-confirmed. v0.3.8's normal installed self-update path from v0.3.7 is runtime-confirmed; full A1 acceptance still awaits the focused package-operation smoke checks.
- Documentation-only commits after that release do not imply a new runtime build and require no version bump/release.
- Version remains local installed TOC metadata; do not persist a second Version value into JSON unless the product contract is deliberately changed.
- The legacy `package_columns_locked` JSON field remains accepted for compatibility but no longer controls v0.3.6 UI behaviour.

## Exact Next Step

Implement **A2 durable state semantic validation only**.

1. define a pure, centralized validator for the fully loaded durable package set before runtime use;
2. reject duplicate package IDs and ambiguous/conflicting addon-root ownership;
3. reject identity/provider/mode/target inconsistencies and incoherent installed-state combinations;
4. reject unsafe durable ownership paths that escape approved WoW/addon destinations;
5. revalidate direct-DLL durable ownership against the same narrow product contract used by normal mutation paths;
6. fail closed with a clear state-load error while preserving the original `TocPilot.json` for manual repair;
7. add deterministic state tests for every rejected semantic class plus valid branch/library/DLL fixtures;
8. keep JSON unknown-field preservation and existing legacy-layout migration behaviour intact;
9. do not begin A3/ZIP bounds, updater hardening or the queued `www` visual follow-up in this slice.

Keep the existing caveat explicit: A1 provides deterministic **process-restart** recovery, but full sudden-power-loss atomicity is not claimed until Windows directory-rename durability is separately verified.
