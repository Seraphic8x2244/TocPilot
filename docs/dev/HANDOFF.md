# TocPilot development handoff

## 2026-09-23 v0.3.5 version/status UX implementation

- Active branch: `ux/version-status-v035`.
- Branch base/main head at start: `4611b68d1ea56107cef27a32a45fb5c21cbbc00c`.
- Published version: `v0.3.4`; target release: `v0.3.5`.
- Latest released feature merge/tag target: `d658b8eb409cb5e8d08f2bf647acbe33676a12a6`; v0.3.4 Release workflow #47 passed 17/17 tests.
- v0.3.5 scope: Advanced-only local TOC `Version`; columns become `Name | Branch | Version | Local SHA | Git SHA | Status`; Compact remains `Name | Status`; steady Status wording becomes title-cased with `Current -> Up To Date`; healthy managed DLL/release rows must sort normally instead of being promoted by the branch-only attention predicate.
- Version semantics must stay fast/unambiguous: read only local package-owned `.toc` files; one unique nonblank `## Version:` value displays it; no value displays `—`; conflicting values across owned TOCs display `Multiple`; DLL packages display `—`. No tag inference and no network work.
- Untested: six-column state migration/persistence, TOC parsing/version aggregation, Advanced reorder/lock behavior with new column, branch-selector geometry, compact switching, DLL sort correction, status wording.
- Deferred: existing runtime backlog and P5 import/export; do not broaden this release.
- Exact next step: implement the TOC helper and six-column persistence migration first, add tests, then wire the Win32 list and release as v0.3.5 after green CI.

## 2026-09-23 v0.3.4 published

- Active branch: `main`.
- Published/source version: `v0.3.4`.
- PR #4 merge and release tag target: `d658b8eb409cb5e8d08f2bf647acbe33676a12a6`.
- Release workflow run `35926103920` (#47), Windows x64 job `107401454846`, passed version validation, Release build, **17/17** CTest tests, checksum generation, tag creation, and publication.
- Latest-stable GitHub Release is `v0.3.4`, draft=false, prerelease=false.
- Assets:
  - `TocPilot.exe` — 2,417,664 bytes; SHA-256 `1cc3429c28eae36c0b99a77e48cfe083fb9b81af3e0f1f7c9c0cf0f0592afcf0`;
  - `TocPilot.exe.sha256` — 78 bytes; release-asset SHA-256 `527a2721bc26d14365d55765347eae371a940c3b7bce0505448b0902cb4ebddf`.
- Completed/released: orange update rows; green session-updated rows; update/green/normal priority grouping; Refresh All and Update New use visible list order; green clears on refresh/exit; Update New returns to top; no JSON schema/storage-order change.
- Runtime-confirmed before publication: `v0.3.2 -> v0.3.3` normal startup self-update.
- Runtime-untested: `v0.3.3 -> v0.3.4` startup self-update; orange/green presentation and live movement; visible-order scan/update; green clearing; final scroll-to-top.
- Earlier deferred runtime checks remain: column persistence/locking/compact toggle/Branch selector and the listed removal/Add Git edge cases.
- Deferred/out of scope remains P5 import/export and the broader roadmap items already documented below.
- Exact next step: launch installed `v0.3.3`, confirm normal automatic update to `v0.3.4`, then runtime-test this UX slice. Do not begin P5 import/export.

## 2026-09-23 v0.3.4 update-visibility release candidate

- Active branch: `ux/update-status-visibility`; PR #4 targets `main`.
- Published version is still `v0.3.3`; intended release is `v0.3.4`.
- Latest CI-tested code head before this documentation checkpoint: `c688906f67a5acf0f15a4f99ef48161eb448314b`.
- PR Build run `35924738730` (#533), Windows x64 job `107397001537`, passed Release build and all **17/17** CTest tests.
- Completed: orange update-available row text; green session-only successfully-updated row text; update/green/normal priority grouping; Refresh All and Update New queueing from visible order; defensive preservation of Update New candidates; Update New scroll-to-top completion; no JSON schema or persisted package-order changes.
- Untested/runtime-gated: installed `v0.3.3 -> v0.3.4` automatic startup self-update; row colours; live movement from orange to green during Update New; visible-order processing; green clearing on Refresh All/app exit; final scroll-to-top.
- Previously deferred runtime checks remain unchanged: package-list column persistence/locking/compact toggle/Branch selector, removal confirmation, Add Git collision/library edge cases.
- Deferred/out of scope remains: P5 import/export and the broader roadmap items already listed below.
- Exact next step: bump `CMakeLists.txt`, `src/version.h`, and `.github/release-version` to `v0.3.4`; require green PR CI; merge PR #4; let the existing push-to-main Release workflow publish `v0.3.4`; then runtime-test via normal self-update.

## 2026-09-23 v0.3.3 published

- Active branch: `main`.
- Published/source version: `v0.3.3`.
- P5 column persistence/reordering/locking implementation merge: `291b8a4617fc11503cec103f43bf7420ba13bb6e`.
- Release commit/tag target: `60f8cf680821db5ea88789c15c1f35f94f014d74` (`Publish v0.3.3`).
- Release workflow: run `35920885122` (#46), Windows x64 job `107384232317`; source-version validation, Release build, **17/17** CTest tests, checksum generation, tag creation, and release publication all passed.
- Published latest-stable release: `v0.3.3`, draft=false, prerelease=false.
- Published assets:
  - `TocPilot.exe` — 2,416,128 bytes; SHA-256 `df3a2c1d39f9378a9e8174f90e29078db394e6ff6c41276449b8d48a7d27bb14`;
  - `TocPilot.exe.sha256` — 78 bytes; release-asset SHA-256 `7fa14dc10b2645c0ce4d68b70b9a23d690c313f2a307a580bf33050385edc25e`.
- Completed/released: persisted Advanced package-list widths/order, validated layout state, persisted Lock columns, unlocked native resize/reorder, locked resize/reorder/autosize blocking, compact-mode protection of saved Advanced layout, unchanged sort persistence.
- Untested/runtime-deferred: automatic `v0.3.2 -> v0.3.3` self-update; resize/reorder persistence across restart; lock/unlock interaction; compact/Advanced toggling after custom layout; inline Branch selector positioning after reorder.
- Previously deferred runtime checks remain: v0.3.2 removal confirmation UI; single-nested Add Git; same-root overwrite/cancel; mixed root+child refusal; terminal no-supported-content cases.
- Deferred/out of scope: P5 import/export, package-edit flow, ambiguous archive mapping improvements, local-modification detection/backups, richer diagnostics, GitLab release support, release-archive executable discovery, arbitrary-depth catalogue discovery, dependency resolution, broader collection UX.
- Documentation commits after the release: `5e9620ea1ff40a40dbd58f51e8be548af34f415e` (`Document v0.3.3 release`), then `2b03acfd9bba30f2c5095f1eadf685c6cbe4d735` (`Mark P5 column layout released`).
- Exact next step: runtime-test normal startup self-update from installed v0.3.2 to v0.3.3, then test the four column-layout interactions. Do not begin P5 import/export before that runtime pass.
