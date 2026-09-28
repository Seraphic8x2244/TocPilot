# TocPilot Development Progress

> Live TocPilot development context for a fresh chat. Keep this current and concise. Git carries chronology; this file carries the current product contract and next work.

## Current

- Active branch: `main`.
- Source/application version: `v0.3.21`.
- Latest published release: `v0.3.21`.
- Release/source commit and tag target: `06f1d253ab7c16efb34c75ea85aa43f890fa2e84` (`v0.3.21`).
- Latest fully runtime-accepted release is now `v0.3.17` at `a636d1eb0d1a3c213bb96c84ecbce2b22ca09d82`. The combined v0.3.14-v0.3.17 Compact fill/split gate passed on 2026-09-27: no Compact horizontal scrollbar under divider reversal, real Name/Status minimums hold in both orders, the far-right edge stays locked, whole-window resizing distributes width correctly, and Advanced resize/reorder behavior remains intact.
- Latest verified `main` source-changing head: `5dd02e0c87f87c226f433e6edb50bc5e7a5879dc` (P6B icon toolbar). Release/version commit `928b26797d8d1b14bdbb07c2e2baf49ea896b2dc` sits above it.
- A1 runtime gate: **accepted for forward development** on 2026-09-26. Fresh install, reinstall, Update New and Remove Addon passed. Managed same-root replacement runtime validation is explicitly deferred rather than blocking later work. The deterministic crash-window tests remain the primary validation for restart-recovery semantics.
- A2 durable state semantic validation: **implemented / CI-checked / merged / published in v0.3.10 / runtime-accepted** as part of the combined v0.3.10 gate.
- A3 + queued `www` presentation delta: **implemented / CI-checked / merged / published / runtime-accepted in v0.3.10**. ZIP members stream through miniz's extraction callback directly into staged files instead of allocating one full-member buffer; the existing 256 MiB per-entry and 1 GiB total policy limits remain. Deterministic archive coverage forges an oversized central-directory member size in a tiny fixture and verifies policy rejection before extraction staging. Advanced repository URLs are custom-drawn always blue + underlined and the header is lowercase `www`; Compact remained `Name | Status` in the accepted gate.
- v0.3.11 self-update transport hardening + constrained/shared package-column layout: **implemented / CI-checked / merged / published; requested runtime matrix passed**. Installed update/startup-state passed. `Name`/`Status` stay in the first two positions and swap only with each other; Advanced-only columns stay to their right and reorder normally; Name/Status width sharing passed in both directions; persisted layout passed. Initial executable/checksum URLs must be HTTPS; GitHub release executable size is carried into the self-update decision and the streamed download must match it exactly; checksum text is capped at 64 KiB. The one adjacent Compact main-window-width regression found during this gate is fixed in v0.3.12.
- v0.3.12 Compact main-window-width follow-up: **implemented / CI-checked / merged / published; partial runtime pass**. Compact width is captured before entering Advanced and restored on return. The `v0.3.11 -> v0.3.12` self-update/restart passed, but runtime testing exposed that the restore path incorrectly treated the 590 px startup default as a hard Compact minimum.
- v0.3.13 default-vs-minimum width fix: **implemented / CI-checked / merged / published / runtime-accepted**. `ToggleAdvanced` uses the same content-derived Compact minimum as normal manual resizing; `kCompactWindowWidth` remains only the startup default. Narrower- and wider-than-default Compact widths now survive Compact -> Advanced -> Compact.
- v0.3.14 Compact two-column fill/split UX: **implemented / CI-checked / merged / published; runtime gate failed on divider drag**. Window resizing works and the right-hand outer divider is locked, but dragging the middle divider can still grow the left column without atomically shrinking the right column, pushing column 2 out of view and creating a horizontal scrollbar. Fix this as a true give-and-take transaction before accepting v0.3.14.
- v0.3.15 Compact divider transaction fix: **implemented / CI-checked / merged / published; partial runtime pass**. Expanding the left Compact column no longer creates a horizontal scrollbar immediately, but the divider transaction still clamps both primary columns only to the generic 40 px floor. This lets the right visible column shrink below its intended Compact minimum; after entering that invalid state, dragging back left can expose a horizontal scrollbar. Fix by enforcing the real per-column Compact minima throughout fitting and divider transactions.
- v0.3.16 Compact minimum-width fix: **implemented / CI-checked / merged / published; partial runtime pass**. Self-update passed; the companion column now stops at its intended minimum in both column orders; whole-window resize and Advanced regression checks passed. Remaining failure: when dragging the middle divider back left after pushing it right, a horizontal scrollbar still appears. This is now isolated to reverse-direction transaction ordering rather than width limits.
- v0.3.17 reverse-direction divider fix: **implemented / CI-checked / merged / published / runtime-accepted**. Compact divider dragging is a fully owned two-column transaction: the shrinking column is applied first, the growing column second, and the native one-column commit is cancelled under a re-entrancy guard. User runtime confirmed the rightward clamp, leftward reversal, both Name/Status orders, window resizing and Advanced regression checks all pass with no horizontal scrollbar.
- Current goal: runtime-check published **v0.3.21** at normal 125% Windows scaling for DPI sharpness, clean disabled icons, and persistent Icons/Words toolbar presentation. The Refresh All viewport jump remains queued immediately after acceptance.
- Current scope boundary: only the P6B sizing/group-spacing follow-up is in scope. Do not mix the queued Refresh All viewport-jump fix, async latest-stable DLL discovery, Add-Git stale-request work, warning cleanup, rate-limit propagation, staging-name cleanup or P6C art/skin work into this gate.
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

Starting with v0.3.11, self-update additionally requires the initial executable/checksum URLs to be HTTPS, carries GitHub's exact executable asset size into the download, rejects a streamed executable that is shorter or longer than that size before replacement, and caps checksum text at 64 KiB. SHA-256 verification remains mandatory.

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

`Name/Status first | advanced-only columns after`

Column layout rule implemented in v0.3.11 (runtime-verified on 2026-09-27):

- `Name` and `Status` must always occupy columns 1 and 2;
- users may swap `Name` and `Status` with each other, but neither may move behind an Advanced-only column;
- all Advanced-only columns (`www`, Branch, Version, Local SHA, Git SHA) must always remain to the right of those first two columns, while retaining their own reorderability within that right-hand group;
- `Name` and `Status` widths are shared between Compact and Advanced: resizing either in one mode must be inherited by the other mode in both directions;
- switching modes must not create separate divergent widths for `Name` or `Status`.
- next Compact UX slice: the two visible columns must always exactly fill the list client width, so Compact never shows a horizontal scrollbar; only the divider between the two visible columns may be dragged; the outer/right edge of the right-hand visible column is not resizable; whichever of Name/Status is currently on the left owns the draggable divider; resizing the Compact window distributes the width delta equally between both visible columns while preserving the user-set split as closely as practical; Advanced behavior remains unchanged.

Starting with v0.3.12, a manually resized Compact **main window width** is preserved across Compact -> Advanced -> Compact. This is separate from persisted Name/Status column widths.

`www` is derived from the package's existing provider/repository identity (GitHub/GitLab), displays the repository URL and opens that repository in the default browser on a single click. It adds no duplicate durable source field.

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

Advanced column widths/order are persisted. The dedicated Lock Columns UI/behaviour is removed in v0.3.6. Starting with v0.3.11, persistence enforces the two-column invariant: `Name` and `Status` are always the first two columns and may only swap with each other; Advanced-only columns stay to their right. `Name`/`Status` widths are one shared pair of values across Compact and Advanced, so resizing in either mode updates what the other mode inherits. Advanced-only widths/order remain Advanced-specific. The legacy JSON `package_columns_locked` field is retained for state compatibility but no longer controls the UI.

Branch-selector target behaviour after the v0.3.6 runtime follow-up:

- a branch package whose remote repository advertises more than one branch should always show the Branch-cell arrow once that repository's branch metadata is known;
- a repository advertising exactly one branch should never show an actionable arrow or dropdown;
- branch affordance must be based on per-repository/package metadata rather than only the currently selected row;
- changing a branch may re-sort an addon to the update-available group, but the selection/branch interaction must remain anchored to that package identity after the list rebuild;
- remote branch lists are transient remote metadata and should not be duplicated into durable package state merely for UI rendering;
- Add Git already fetches the full Git smart-HTTP branch advertisement in the branch chooser; reuse that result to seed runtime metadata rather than immediately fetching it again;
- normal branch-head refresh also fetches the full advertisement internally, so retain/reuse that information to discover newly added/removed remote branches during ordinary startup/Refresh All checks without an extra branch-list request.

### Removal UX

The current product workflow is **Remove Addon**; the old Uninstall flow is not part of the active UI/runtime gate.

Remove Addon uses the native expandable TaskDialog:

- compact counts by default;
- No/cancel is the default;
- expandable details show exact TocPilot-owned addon roots and recorded files;
- installed managed addon removal deletes TocPilot-owned files and then removes package state;
- record-only removal clearly states that no addon files are being removed.

Legacy Uninstall implementation code may remain as cleanup debt, but do not treat it as a current user workflow or required runtime test.

## Recent Relevant Commits / Release Provenance

- `11314087e0adacefe35d3a10da7fa6c092820f5f` — merged release-prep PR #17 and exact `v0.3.12` release/tag target.
- `eea6e2b30e9ba8323b1e282660e75f39caea8d78` — merged PR #16, focused Compact main-window-width round-trip fix.
- PR #16 head `0758c36c538dc0fbcc1c43fda3371e821aca845a` passed Build workflow run `36311247881`, Windows x64 job `108597463779`, including **18/18 CTest tests** and the new `window-layout-roundtrip` test.
- PR #17 head `41e1fd4219484b61f4e0eb65ca8d23cab9639b44` changed only release/version metadata and passed Build workflow run `36311440134`, Windows x64 job `108598023371`, including **18/18 CTest tests**.
- Main push Build workflow run `36311630542`, Windows x64 job `108598564433`, passed **18/18 CTest tests**.
- v0.3.12 Release workflow run `36311630529`, Windows x64 Release job `108598564280`, validated exact source version, passed **18/18 CTest tests**, generated the SHA-256 sidecar, created tag `v0.3.12` and published the direct assets.
- Published v0.3.12 `TocPilot.exe`: 2,496,512 bytes, SHA-256 `dbf1eca9e65e35ef89de7dd60ff8c06d2f793c9605d27e47cf5662fa39c63ed3`.
- Published v0.3.12 `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `54e9f6fa249a19d28bb441d71fc3ae5de91020344a355177f22b67040c7f9119`.

- `a6fc2bdb7721b85f58c6d0e42bc9eeaba16c2b1d` — merged release-prep PR #15 and exact `v0.3.11` release/tag target.
- `78bec799ae3e611a58fcd33d10a0e4d9d0f90d2e` — merged PR #14, self-update transport hardening + constrained/shared package-column layout.
- PR #14 head `a261b1b5bbde7644f57b371c67c7c9c177364adb` passed Build workflow run `36309719299` (#597), Windows x64 job `108593188173`.
- PR #15 head `c9acffe37b72cec348c5e4e6cde230ea7516a042` passed Build workflow run `36309932860` (#599), Windows x64 job `108593782235`.
- v0.3.11 Release workflow run `36310168201` (#54), Windows x64 Release job `108594444924`, rebuilt exact commit `a6fc2bdb7721b85f58c6d0e42bc9eeaba16c2b1d`, validated source version, passed **17/17 CTest tests**, generated the SHA-256 sidecar, created tag `v0.3.11` and published the direct assets.
- Published v0.3.11 `TocPilot.exe`: 2,496,512 bytes, SHA-256 `fa1b0a097fa9afb346e7f9eff07708df37b46b062a73330ea6a15a0b3ceaf424`.
- Published v0.3.11 `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `ace81b8f2c66747722531f8e032eed67e865c3d35126b8026e1830d11dfee6e6`.

- `dcdd5058f50c83c427310feba083437db6368dcd` — merged PR #11, A2 durable state semantic validation; source version remains v0.3.9 and this commit is not yet published.
- PR #11 head `262ad8806639d9b2c4461bb8fec687bb82a35cc1` passed Build workflow run `36255044602` (#585), Windows x64 job `108440109791`, including **17/17 CTest tests**.
- A2 load-time tests now cover duplicate IDs, overlapping addon-root ownership, identity/provider/mode/target inconsistencies, missing branch ref, incoherent installed revision/files state, unsafe addon ownership paths, invalid direct-DLL target/ownership and conflicting direct-DLL destinations; invalid loads preserve the original state file. Valid GitHub root, GitLab child/library and GitHub direct-DLL fixtures also load.
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

For v0.3.11 self-update transport + column-layout release:
- PR #14 head `a261b1b5bbde7644f57b371c67c7c9c177364adb` passed Windows x64 Release build and the complete **17/17 CTest** suite in Build workflow run `36309719299` (#597), job `108593188173`.
- PR #14 merged to `main` as `78bec799ae3e611a58fcd33d10a0e4d9d0f90d2e`; normal push Build workflow run `36309871572` (#598) completed successfully on that exact merge.
- Release-prep PR #15 head `c9acffe37b72cec348c5e4e6cde230ea7516a042` changed only `CMakeLists.txt`, `src/version.h` and `.github/release-version`, and passed Build workflow run `36309932860` (#599), job `108593782235`.
- PR #15 merged to exact release/source commit `a6fc2bdb7721b85f58c6d0e42bc9eeaba16c2b1d`; normal push Build workflow run `36310168226` (#600), job `108594445019`, completed successfully.
- Release workflow run `36310168201` (#54), job `108594444924`, rebuilt exact commit `a6fc2bdb7721b85f58c6d0e42bc9eeaba16c2b1d`, validated source version, passed the complete **17/17 CTest** suite, generated the SHA-256 sidecar, created tag `v0.3.11` and published `TocPilot.exe` / `TocPilot.exe.sha256`.
- v0.3.11 is therefore **published and CI/release-verified; runtime acceptance is pending**.

For v0.3.10 A3 + `www` combined release:
- PR #12 head `21930b18ca8dfbb5f4b81664361e45340a2c8f1a` passed Windows x64 Release build and the complete **17/17 CTest** suite in Build workflow run `36262546306` (#589), job `108460997745`.
- PR #12 merged to `main` as `8ff352e58890399f95bf633ccfe58a32580f2be1`.
- Release-prep PR #13 head `2999ca4d2996bc8d2a8553566b6273a81434245c` changed only `CMakeLists.txt`, `src/version.h` and `.github/release-version`, and passed the complete **17/17 CTest** suite in Build workflow run `36262852969` (#591), job `108461843800`.
- PR #13 merged to exact release/source commit `be5a767a79b20751646fbd5f88c8da4a39c4697e`; normal push Build workflow run `36263029991` (#592) also completed successfully.
- Release workflow run `36263030432` (#53), job `108462335143`, rebuilt exact commit `be5a767a79b20751646fbd5f88c8da4a39c4697e`, passed source-version validation and the complete **17/17 CTest** suite, generated the SHA-256 sidecar, created tag `v0.3.10` against that exact commit and published direct `TocPilot.exe` / `TocPilot.exe.sha256` assets.
- v0.3.10 is therefore **published, CI/release-verified and runtime-accepted**. The user confirmed the installed v0.3.9 -> v0.3.10 self-update/restart/state-load path, lowercase always-blue/underlined `www` links including semantic/selected rows, correct repository opening, Compact `Name | Status`, and a working update operation on 2026-09-27. Remove Addon was not retested and is deferred/non-blocking.

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

Published v0.3.11 added the focused self-update transport hardening and constrained/shared package-column layout on top of the runtime-accepted v0.3.10 baseline. On 2026-09-27 its installed self-update/startup-state path and requested column-layout matrix passed: Compact/Advanced primary-column constraints, Advanced-only boundary/reordering, bidirectional Name/Status width sharing, and persisted layout all passed. The adjacent Compact main-window-width round-trip regression found during that gate is fixed and published in v0.3.12. v0.3.12 is CI/release-verified and awaits only the focused runtime gate below; v0.3.10 remains the latest fully runtime-accepted release until that check passes. A1 remains accepted for forward development after fresh install, reinstall, Update New and Remove Addon passed in its earlier gate; managed same-root replacement remains explicit deferred runtime debt. Remove Addon was not repeated for the v0.3.10 gate and is non-blocking.

P6A is complete. A1 transaction restart recovery is implemented, CI-checked and published in v0.3.8 at `17ce349273c6f2d76572c7107f3c6f7b139cf8d8`, and accepted for forward development with the same-root replacement runtime check deferred. It now writes a versioned, flushed pre-mutation journal; arms it with package/transaction identity, affected-root intent and durable pre/post state markers before live renames; recovers unfinished transactions before normal package mutation; and preserves evidence rather than guessing when durable state is ambiguous.

A2 durable state semantic validation is implemented, CI-checked, merged at `dcdd5058f50c83c427310feba083437db6368dcd` and published as part of v0.3.10. State load now fails closed before runtime use when durable packages violate understood identity, mode/target, ownership, installed-state or direct-DLL invariants; the original JSON is preserved for manual repair. The user accepted A2 for forward development without a separate A2-only runtime gate.

A3 ZIP single-member allocation hardening is implemented, CI-checked, merged at `8ff352e58890399f95bf633ccfe58a32580f2be1` and published as part of v0.3.10. ZIP members are streamed to staged files rather than buffered wholly in RAM; the existing archive/per-entry/total limits remain in force, and deterministic oversized-entry metadata coverage rejects policy violations before extraction staging.

Remaining audited implementation priority after the v0.3.11 runtime gate starts with:

- the dedicated Add-Git branch dialog has no per-request generation/repository token. Closing and reopening it while its detached lookup is still running leaves a rare stale-result/HWND-reuse race; the main inline branch selector already has a generation + package-ID guard;
- Add Git latest-stable DLL discovery performs provider network I/O synchronously on the dialog thread, including the DLL-fallback dialog creation path, so provider timeout/failure can freeze that UI;
- Update All does not treat branch-addon archive HTTP rate limiting as a queue-stop condition, unlike status refresh and direct-DLL update paths;
- provider/repository staging-directory sanitization can collide for distinct identities.

Lower-priority/test/build findings and contract-driven direct-DLL risks are retained in `audit_dump.md`.

New deferred UI observation from 2026-09-27: after **Refresh All**, the addon package list appears to jump to an arbitrary mid-list position. Code review points to `RefreshPackageStateUi()` preserving the identity of the previous top/selected package across repopulate/re-sort; when `ClearSessionUpdatedPackages()` changes ordering, that same package can move to a different display row and the viewport follows it. This is likely not 'first addon in JSON'. Keep this queued separately; do not mix it into the Compact two-column fill/split slice. The async/UI pass found no high/medium GDI/icon ownership leak. It did confirm low-priority cleanup/debt: the old hidden branch COMBOBOX is now dead infrastructure after the list-cell popup redesign; main close can abandon non-install async work/staging; several rare Win32 control/subclass/timer/GetMessage failures are not surfaced.

Final severity/order:

1. **HIGH — transaction restart recovery:** **IMPLEMENTED / CI-CHECKED / PUBLISHED / ACCEPTED**, with managed same-root replacement runtime validation deferred.
2. **HIGH — durable state semantic validation:** **IMPLEMENTED / CI-CHECKED / MERGED / ACCEPTED FOR FORWARD DEVELOPMENT** at `dcdd5058f50c83c427310feba083437db6368dcd`; no standalone A2-only release gate required.
3. **MEDIUM — ZIP member allocation bound:** **IMPLEMENTED / CI-CHECKED / PUBLISHED / RUNTIME-ACCEPTED in v0.3.10** — per-member extraction streams to the staged file rather than allocating the full uncompressed member in RAM; the existing 256 MiB per-entry policy remains enforced during archive inspection.
4. **SELF-UPDATE TRANSPORT + COLUMN LAYOUT:** **IMPLEMENTED / CI-CHECKED / MERGED / PUBLISHED in v0.3.11; REQUESTED RUNTIME MATRIX PASSED** — installed update/startup-state and the constrained/shared column behavior passed on 2026-09-27.
5. **COMPACT MAIN-WINDOW WIDTH FOLLOW-UP:** v0.3.12 implemented capture/restore; v0.3.13 fixed the startup-default-vs-real-minimum edge. **v0.3.13 PUBLISHED / CI-CHECKED / RUNTIME-ACCEPTED**.
6. **CURRENT NARROW UI FOLLOW-UP — icon-toolbar pass:** replace the wide text toolbar presentation with compact icon buttons while preserving existing command behavior; details are locked under P6B below.
7. **NEXT NARROW UI FOLLOW-UP AFTER TOOLBAR PASS — Refresh All viewport jump:** investigate preserved top/selection identity across re-sort/repopulate.
8. **NEXT ROBUSTNESS SOURCE WORK AFTER UI FOLLOW-UPS — async latest-stable DLL discovery:** remove provider I/O from the dialog thread.
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

- Published `v0.3.10` normal installed self-update `v0.3.9 -> v0.3.10`: passed on 2026-09-27 through TocPilot's real self-updater. Restart and existing-state load passed; lowercase `www`, blue + underlined repository links across normal/selected/orange/green row states, correct repo opening, Compact `Name | Status`, and a representative update operation passed. Remove Addon was not retested and is deferred/non-blocking.
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

Published `v0.3.17` runtime gate passed on 2026-09-27:
- self-update/startup: passed;
- pushing the Compact middle divider hard right to the minimum clamp: passed;
- reversing it hard left: passed with **no horizontal scrollbar**;
- both Name/Status orders: passed;
- Name/Status minimums: passed;
- far-right outer divider lock: passed;
- whole-window resize distribution: passed;
- Advanced resize/reorder regression smoke: passed.

The combined v0.3.14-v0.3.17 Compact two-column fill/split work is runtime-accepted.

Next runtime gate will first cover the focused **P6B icon-toolbar pass**. The separately queued **Refresh All viewport jump** remains the next narrow UI follow-up after that pass.

A1's managed same-root replacement check remains deferred. The current product workflow has **Remove Addon**, not the old Uninstall flow.

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

Once robustness findings are under control, exercise every normal workflow as a product rather than as isolated features: startup/self-update, Compact/Advanced, Add Git, branch selection, Refresh All, Update New, install/reinstall, DLL management, Remove Addon and error/recovery paths.

Review consistency of labels, button state, selection/focus, keyboard/mouse behaviour, progress/status feedback, sorting/reordering, confirmations, empty/loading/error states, resize/DPI/text-scale behaviour and unnecessary friction. Prefer small coherent UX fixes over adding new capability.

##### Current icon-toolbar slice — implemented / CI-checked

This is a functional/spacing pass only, not the later art-skin pass.

- Use **32 x 32 px native buttons** with **16 x 16 px icons**.
- Use the real **Font Awesome Free Solid** icons for standard actions; keep source icon resources under `resources/icons/fa-solid/` with the appropriate Font Awesome license/notice.
- The one custom icon is **Reinstall Repository**: use the Font Awesome Solid `folder-plus` folder silhouette, replace the plus with a repeat symbol, and preserve the source icon's inner spacing/weight. Keep it under `resources/icons/custom/`.
- **Launch** keeps its existing context/executable icon (WoW.exe or VanillaFixes.exe); do not replace it with Font Awesome.
- Agreed icon mapping:
  - Update -> `cloud-arrow-down`
  - Refresh -> `arrows-rotate`
  - Launch -> existing executable/context icon
  - Add Repository -> `folder-plus`
  - Reinstall Repository -> custom `folder-repeat`
  - Remove Repository -> `folder-minus`
  - Inspect -> `magnifying-glass`
  - Scan -> `folder-open`
  - TocPilot -> `circle-info`
  - Advanced -> `screwdriver-wrench`
- Agreed full relative order: **Update - Refresh - Launch - Add Repository - Reinstall Repository - Remove Repository - Inspect - Scan - TocPilot - Advanced**.
- **Compact** shows only: **Update, Refresh, Launch, Add Repository, Remove Repository, Advanced**.
- Compact grouping: **[Update Refresh] [Launch] [Add Repository Remove Repository] [Advanced]**.
- Compact spacing: **6 px normal gap**; each group boundary adds **16 px extra** (22 px total at a group break). Centre the complete Compact button strip within the available toolbar content width.
- **Advanced** shows all ten actions in the same relative order; Reinstall Repository, Inspect, Scan and TocPilot are Advanced-only.
- Icon buttons are visually icon-only, but mouseover tooltips/accessibility text use the new names exactly: **Update, Refresh, Launch, Add Repository, Reinstall Repository, Remove Repository, Inspect, Scan, TocPilot, Advanced**.
- Preserve the useful existing button behavior by reusing the current HWND globals/control IDs/WM_COMMAND paths and state logic. The new icon controls should be deliberately dumb presentation wrappers over those handlers.
- Do **not** keep a second set of hidden live legacy buttons with duplicate IDs. Retain the old text-toolbar creation/layout code only as clearly commented legacy/reference code for the later art/UI pass, then create the icon controls into the existing globals.

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

- Current published release is `v0.3.18` at `928b26797d8d1b14bdbb07c2e2baf49ea896b2dc`; latest fully runtime-accepted release remains `v0.3.17` until the P6B self-update/runtime gate is checked.
- PR #26 head `bf9ee839f6d7ab722c6841648a642a4c4b5073aa` passed Build workflow run `36325300444`, Windows x64 job `108636801237`, including **18/18 CTest tests** with explicit shrink-first direction coverage.
- PR #26 merged the fully owned direction-safe divider transaction as `2c7dd510e04be7cd28263fa004b65d32033a9056`.
- PR #27 head `7d4256d7bb5a0a715ab52e8de89655f018221f5b` changed only release/version metadata and passed Build workflow run `36325571382`, Windows x64 job `108637574892`, including **18/18 CTest tests**.
- Main push Build workflow run `36325809455`, Windows x64 job `108638240899`, passed **18/18 CTest tests**.
- v0.3.17 Release workflow run `36325809412`, Windows x64 Release job `108638240745`, validated exact source version, passed **18/18 CTest tests**, generated the SHA-256 sidecar, created tag `v0.3.17` and published direct assets.
- Published v0.3.17 `TocPilot.exe`: 2,498,048 bytes, SHA-256 `d073217456c7d9b4dabf488947fbecf2424f4655da145b51632d5f48fe8f42c8`.
- Published v0.3.17 `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `343f45497193901851be3364c9f968b2246618527e54a8e2668d43eb2956fdf9`.
- The combined v0.3.14-v0.3.17 Compact fill/split UX is runtime-accepted.
- The 2026-09-28 P6B icon-toolbar implementation is CI-checked at `5dd02e0c87f87c226f433e6edb50bc5e7a5879dc` and published in `v0.3.18`.
- v0.3.18 Release workflow run `36360720566`, Windows x64 Release job `108737243385`, validated the exact source version, passed the Release build and **18/18 CTest tests**, generated the SHA-256 sidecar, created tag `v0.3.18`, and published direct assets.
- Published v0.3.18 `TocPilot.exe`: 2,927,616 bytes, SHA-256 `b082c6af0cbb05af6f172470cfb48cf312606a4d4dfb7c6681e35217486c70c8`.
- Published v0.3.18 `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `cf3ef06099ea1b479e2669b1aea138d14b86d9946d7b4d8a6fc19e1d26052592`.
- v0.3.18 runtime exposed a P6B rendering regression: every Font Awesome toolbar glyph rendered as the same narrow fallback bar, while the executable-derived Launch icon rendered correctly. Root cause is Font Awesome face substitution at runtime; the corrective v0.3.19 source explicitly resolves the Solid face and verifies all required PUA glyphs with `GetGlyphIndicesW` before accepting a font.
- Corrective v0.3.19 commit `347d76a289b229c56c4c789c9badec6f76d2980d` is published. Release workflow run `36361372404`, Windows x64 Release job `108739089189`, passed source-version validation, Release build, **18/18 CTest tests**, checksum generation, tag creation and asset publication.
- Published v0.3.19 `TocPilot.exe`: 2,928,128 bytes, SHA-256 `863035585cff4c9e2549b9862f5f5791941289c2de093e2ec09a4390b1fe3b51`.
- Published v0.3.19 `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `92a3f5fdcf4de9e6843ad6bcc036fa509b786cd2d56784b65f4b30d68eef907d`.
- The Refresh All viewport jump remains queued immediately after the corrected toolbar runtime gate.
- P6B sizing/group-spacing source commit `8f768825221513021fc23d58ca5fe3ec03cd2d8c` changes toolbar buttons to **48 x 48**, renders/extracts icons at **36 x 36**, preserves Compact grouping, adds Advanced grouping **[Update Refresh] [Launch] [Add Reinstall Remove] [Inspect Scan] [TocPilot] [Advanced]**, and moves the list top to preserve the existing 14 px toolbar/list gap. Compact toolbar width is 366 px versus the unchanged 370 px Name + Status minimum, so Compact minimum width remains list-driven.
- v0.3.20 release commit `6bbc7c505bd6ed2643b9c25a7e9f8dab7a78fecc` is published. Release workflow run `36405469945`, Windows x64 Release job `108873033011`, passed exact-version validation, Release build, **18/18 CTest tests**, checksum generation, tag creation and asset publication.
- Published v0.3.20 `TocPilot.exe`: 2,928,128 bytes, SHA-256 `1c38a0186fbc30245e4b6876833704e7d5fc3245a95388e166ae922258b72c6f`.
- Published v0.3.20 `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `7509b88569f884e8c67d73406bbc75fa7e4b679593f8bdcc4b8651900ec15b1e`.

## Exact Next Step

Runtime-check the published DPI/presentation release and runtime-check at the user's normal **125% Windows display scaling**.

Release workflow run `36409359486`, Windows x64 Release job `108885633236`, passed source-version validation, Release build, **18/18 CTest tests**, checksum generation, tag creation and asset publication.

Published `TocPilot.exe`: 2,937,856 bytes, SHA-256 `f555849aa6e7786167e5bfb9aff7bd9b2b1b9e722d85e79eb2831867b9f6cebe`.

Acceptance:
- list/header text is natively sharp rather than bitmap-stretched;
- 48 x 48 toolbar buttons and 36 x 36 icon design size retain the intended apparent size through DPI scaling;
- Font Awesome and Launch icons are rerendered/extracted at the actual monitor pixel size;
- disabled icon buttons remain clean and recognisable, not dithered/corrupt;
- TocPilot/info popup exposes persistent **Toolbar buttons: Icons / Words**;
- Words mode uses the existing command controls and current Compact/Advanced action ordering/grouping.

After this runtime gate, return to the separately queued **Refresh All viewport jump**. Do not mix async DLL, Add-Git stale-request, cleanup, or P6C art/skin work into this pass.
