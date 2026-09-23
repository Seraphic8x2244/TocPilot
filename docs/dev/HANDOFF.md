# TocPilot development handoff

## 2026-09-23 v0.3.3 release preparation

- Active branch: `main`.
- Current published/source version before this release: `v0.3.2`.
- Current main head entering release preparation: `5f140fe3fc4260ba4133abaab19c4769e05db400` (`Document P5 column layout completion`).
- P5 column persistence/reordering/locking implementation merge: `291b8a4617fc11503cec103f43bf7420ba13bb6e`.
- Exact CI-tested feature head: `e7185c320fc9d212367e43ede88f8a2b81a2a100`; PR Build #522 passed Windows x64 Release build and all 17/17 tests.
- Completed work being released:
  - persisted Advanced package-list column widths;
  - persisted display order;
  - validated/clamped persisted column settings;
  - persisted Advanced-only Lock columns setting;
  - unlocked Advanced mode supports native resize/reorder;
  - locked mode blocks resize/reorder/divider autosizing;
  - compact mode is layout-read-only and does not overwrite the saved Advanced layout;
  - existing sort persistence remains unchanged.
- Runtime-untested/deferred for this feature: resize/reorder persistence across restart; lock/unlock interaction; compact/Advanced toggling after custom layout; inline Branch selector positioning after reorder.
- Previously deferred runtime checks remain: v0.3.2 removal confirmation UI; single-nested Add Git; same-root overwrite/cancel; mixed root+child refusal; terminal no-supported-content cases.
- Deferred/out of scope remains: P5 import/export, package-edit flow, ambiguous archive mapping improvements, local-modification detection/backups, richer diagnostics, GitLab release support, release-archive executable discovery, arbitrary-depth catalogue discovery, dependency resolution, broader collection UX.
- Release target: `v0.3.3`.
- Exact next step: bump `CMakeLists.txt`, `src/version.h`, and `.github/release-version` to 0.3.3/v0.3.3 on `main`. The `.github/release-version` change must trigger the Release workflow. Verify source-version validation, Release build, all tests, checksum generation, tag creation, and publication of direct `TocPilot.exe` + `TocPilot.exe.sha256`. Then update the handoff to the published release and stop before starting P5 import/export.
