# TocPilot development handoff

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
