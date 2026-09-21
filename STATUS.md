# TocPilot status / handoff

## v0.1.30 splash position adjustment — 2026-09-21

- Active branch: `main`.
- Runtime feedback after v0.1.29 tuning: move the plaque up 20 px and the words up 40 px.
- Implemented exactly from the current geometry:
  - plaque Y `238 -> 218`;
  - status text Y `395 -> 355`.
- Logo geometry remains unchanged.
- Plaque size remains unchanged.
- Animated-dot behavior remains unchanged and previously runtime-confirmed stable.
- Temporary **Click to continue!** remains in place for inspection.
- Latest implementation commit: `3ff7d38` — Raise splash plaque and status text.
- Exact next step: publish the next tuning release from `main`, runtime-check the new positions, and only then decide whether to remove the click gate.

## v0.1.29 final splash tuning release request — 2026-09-21

- Active branch: `main`.
- Runtime-confirmed from `v0.1.28`:
  - animated dots are fixed and no longer cause horizontal text jiggle;
  - logo geometry is approved and locked;
  - plaque geometry/overlap is approved and locked.
- Final visual adjustment implemented:
  - status text rectangle moved from Y=405 to Y=395;
  - no other splash geometry, animation, or click-gate behavior changed.
- Source version bumped to `v0.1.29`.
- Latest relevant commits:
  - `5c7d97c` — Request v0.1.29 final splash tuning release
  - `f48db62` — Bump TocPilot to v0.1.29
  - `7acf1ec` — Bump TocPilot build version to v0.1.29
  - `4b81771` — Raise splash status text
  - `7188025` — Record v0.1.28 splash runtime approval
- Release workflow has been triggered from `main` by changing `.github/release-version` to `v0.1.29`.
- Untested/runtime pending:
  - release publication confirmation;
  - final screenshot approval of text placement.
- Deferred until final screenshot approval:
  - remove temporary **Click to continue!**;
  - restore automatic splash close after addon scanning.
- Exact next step: once `v0.1.29` is published, self-update to it, relaunch, let the splash reach **Click to continue!**, and inspect/send one final screenshot. If approved, remove the click gate and restore automatic close with no further splash geometry changes.

## v0.1.28 runtime splash approval checkpoint — 2026-09-21

- Active branch: `main`.
- Runtime-tested version: `v0.1.28`.
- Approved and now locked:
  - logo position/scale;
  - plaque size/position/overlap;
  - fixed-origin animated-dot rendering; the status text no longer jiggles horizontally as dots change.
- Remaining visual issue: runtime status text is still slightly low within the wooden plaque.
- Exact next change: move the status text rectangle upward by another 10 px, from Y=405 to Y=395, with no other splash geometry or animation changes.
- Keep temporary **Click to continue!** for one final screenshot after that adjustment.
- Deferred until that screenshot is approved: remove click gate and restore automatic splash close after addon scanning.

## v0.1.28 / main consolidation completed — 2026-09-21

- `main` is now the authoritative TocPilot development branch.
- `main` was fast-forwarded to the full former `p2-github-branches` history with no merge conflict or lost work.
- At consolidation, `main` and `p2-github-branches` were identical.
- Source version: `v0.1.28`.
- Release tag `v0.1.28` now exists and points exactly to `8bbeede23ca477ef2eef434df19108ce810a80f7`, the original P2 release-request commit. The first release workflow was still running when the tag was initially checked; the later main consolidation did not create this tag.
- Tag creation occurs only after release source-version validation, Windows x64 Release build, complete CTest suite, and SHA-256 generation, so those stages passed for this release run.
- The final GitHub release-asset publication step could not be independently queried through the connected GitHub API in this chat; verify the Releases page exposes `TocPilot.exe` and `TocPilot.exe.sha256`.
- Branch policy: develop on `main` by default; create a temporary branch only for risky/destructive/long-running work. Consider a persistent `dev` branch later once `main` should remain release-stable for regular users.
- Old milestone branches are obsolete and contain no unique work outside main. They can be deleted once convenient.
- Runtime next step: update to `v0.1.28`, relaunch, inspect the splash/dot animation, and provide screenshot feedback before removing the temporary **Click to continue!** gate.

## Main-branch consolidation checkpoint — 2026-09-21

- Branch policy is changing from milestone branches to a simple main-first workflow.
- `main` is the normal TocPilot development branch from this point forward.
- Create a temporary feature/dev branch only for genuinely risky, destructive, or long-running work that should not sit directly on the normal development line.
- Once TocPilot reaches a broadly used/stable release point, consider keeping `main` release-stable and using a persistent `dev` branch for ongoing development.
- The old milestone branches `p0-self-update`, `p1-state-ui`, `p2-github-branches`, and `runtime-update-fixture` contain no unique commits outside the current P2 history; their work is fully contained in the current line.
- `p2-github-branches` is a clean fast-forward descendant of the old `main` history: 389+ commits ahead and 0 behind before this consolidation.
- Build/release workflows have been simplified to watch `main`.
- Source version is `v0.1.28`; splash tuning is implemented, but the first P2 release request did not create a `v0.1.28` tag/release.
- Exact next step: fast-forward `main` to this consolidated P2 head. That main push should run the normal Build workflow and, because `.github/release-version` is `v0.1.28`, the Release workflow. Verify the `v0.1.28` tag/release exists before runtime testing.
- After consolidation, new development chats should resume TocPilot from `main`; no special P2 branch instruction should be necessary.

## v0.1.28 release request checkpoint — 2026-09-21

- Active branch: `p2-github-branches`.
- Source version is now `v0.1.28`.
- Latest commits:
  - `8bbeede` — Request v0.1.28 splash tuning release
  - `61d9817` — Bump TocPilot to v0.1.28
  - `7e8b9ad` — Bump TocPilot build version to v0.1.28
  - `0debb04` — Record P2 splash tuning implementation
  - `fe4f17f` — Tune splash plaque and stabilize status text
- The existing release workflow has been triggered by changing `.github/release-version` from `v0.1.27` to `v0.1.28`.
- That workflow performs source-version validation, Windows x64 Release configure/build, the complete CTest suite, SHA-256 generation, tag creation, and release asset publication in sequence; failed build/tests prevent the tag/release steps from succeeding.
- Completed in source: smaller/higher plaque, status text moved upward, fixed-origin animated dot rendering, unchanged logo geometry, temporary click gate retained.
- Untested/runtime pending: confirmation that the release workflow completed successfully; runtime screenshot/visual approval of the new splash; confirmation that dot animation no longer jiggles.
- Deferred: removing **Click to continue!** and restoring automatic splash close until runtime approval; human-readable Installed/Latest versions; provider expansion.
- Exact next step: once `v0.1.28` is published, self-update from `v0.1.27`, relaunch, inspect the splash through both animated status states and **Click to continue!**, then provide a screenshot/feedback before any further geometry or click-gate changes.

## P2 splash tuning implementation checkpoint — 2026-09-21

- Active branch: `p2-github-branches`.
- Published/version baseline remains `v0.1.27`; no version bump has been made.
- Latest commits:
  - `fe4f17f` — Tune splash plaque and stabilize status text
  - `116da41` — Checkpoint P2 splash tuning resume
  - `8166a8c` — Request v0.1.27 splash asset repair release
  - `28d7dbb` — Bump TocPilot to v0.1.27
- Completed in source:
  - logo rectangle remains unchanged at `(20, 0, 780, 394)`;
  - plaque changed from `(95, 250, 630, 315)` to centered `(115, 238, 590, 295)`, shrinking it about 6.35% and moving it up 12 px;
  - status text moved from Y=415 to Y=405;
  - animated statuses now measure the three-dot form, center that fixed-width region, and left-align the live one/two/three-dot text inside it so the text origin stays fixed;
  - non-animated **Click to continue!** remains centered;
  - temporary click gate remains intact.
- Untested:
  - Windows x64 Release compile for `fe4f17f`;
  - complete CTest suite for `fe4f17f`;
  - runtime visual balance of the smaller/higher plaque;
  - runtime verification that the animated dot origin no longer jiggles;
  - runtime screenshot approval.
- CI note: `.github/workflows/build.yml` is configured to run on pushes to `p2-github-branches`, but the available GitHub connector in this chat does not expose push-triggered workflow-run listings/check-runs, so this checkpoint does not claim a CI result.
- Deferred: version bump/release `v0.1.28` until build/tests are verified green; removing **Click to continue!**; restoring automatic splash close; human-readable Installed/Latest versions; provider expansion.
- Exact next step: verify the Build workflow for commit `fe4f17f`; if Windows x64 Release + full CTest are green, bump source/release version to expected `v0.1.28`, publish it through the existing release workflow, then runtime-test the splash and obtain a screenshot before removing the click gate.

## Resume checkpoint — 2026-09-21 P2 splash tuning

- Active branch: `p2-github-branches`.
- Published/version baseline: `v0.1.27`.
- Branch/release baseline commit: `8166a8c` — **Request v0.1.27 splash asset repair release**.
- Latest relevant commits:
  - `8166a8c` — Request v0.1.27 splash asset repair release
  - `28d7dbb` — Bump TocPilot to v0.1.27
  - `34946fa` — Repair splash logo resource embedding
  - `dd40ec6` — Record splash asset recovery checkpoint
- Completed/runtime-confirmed: clean resource-backed logo rendering; v0.1.27 release/build/tests green; logo position and scale accepted; temporary **Click to continue!** inspection gate retained.
- Untested active work: shrink plaque approximately 5–8%; move plaque upward approximately 8–15 px; move status text upward to about Y=405; stabilize animated-dot text origin using a centered fixed-width region with left-aligned live text.
- Deferred: remove click gate/restore automatic splash close; human-readable Installed/Latest versions; GitLab/Gitea/OctoWoW/provider expansion; unrelated broader P2 work.
- Exact next step: edit only the splash plaque/status geometry and animated status text layout, leave the logo rectangle unchanged, then build Windows x64 Release and run the complete CTest suite before preparing the next tuning release.

## v0.1.27 runtime splash review / handoff — 2026-09-21

- Active branch: `p2-github-branches`.
- Current branch head entering this handoff: `8166a8c` — **Request v0.1.27 splash asset repair release**.
- Published version: `v0.1.27`.
- Release tag `v0.1.27` points exactly to `8166a8c6b163b16d53630a2df46f750ebd49ff19`.
- Release workflow `35620287102` completed successfully:
  - source-version validation passed;
  - Windows x64 Release build passed;
  - complete CTest suite passed;
  - SHA-256 sidecar generation passed;
  - tag creation passed;
  - release asset publication passed.
- Published `TocPilot.exe`: 2,257,920 bytes; SHA-256 `b90f4a347f3b5539d6b058eebcc3d82d03ac4d5f8b9f628811a050ece89b75f2`.
- Latest relevant commits:
  - `8166a8c` — Request v0.1.27 splash asset repair release
  - `28d7dbb` — Bump TocPilot to v0.1.27
  - `34946fa` — Repair splash logo resource embedding
  - `dd40ec6` — Record splash asset recovery checkpoint
  - `ab5527c` — Add files via upload
  - `4dfb20a` — Record splash runtime feedback handoff

### Completed / runtime-confirmed

- The authoritative cutout logo is now embedded from `resources/TocPilot_logo_cutout.png` as a Windows executable resource instead of the truncated base64 chunks.
- The previous large brown corruption block is gone in the real v0.1.27 runtime screenshot.
- The logo itself renders cleanly and its current position/scale remains acceptable.
- The existing startup flow still reaches the temporary **Click to continue!** inspection state.
- The first text adjustment moved the status rectangle from Y=425 to Y=415.
- v0.1.27 is a known-good release baseline for the next visual tuning slice.

### Runtime feedback to address next

- The plaque currently feels a little too large / too far below the logo, more like it is hanging beneath the logo than tucked into it.
- Preferred direction for the next pass:
  - make the plaque slightly smaller, approximately 5–8% as a starting range;
  - move the plaque upward enough to increase the overlap under the TocPilot logo, approximately 8–15 px as a starting range;
  - do **not** move or rescale the logo unless runtime evidence requires it.
- The gold status text still looks slightly low. Move it upward by about another 10 px from the current Y=415 position (initial target Y≈405).
- Animated trailing dots currently make the status line visibly jiggle because the whole string is center-justified and its width changes between `.`, `..`, and `...`.
- Preferred anti-jiggle rendering:
  - reserve/measure a fixed-width region based on the longest animated form;
  - center that fixed region on the plaque;
  - render the live text left-aligned within that fixed centered region;
  - this should preserve the apparent centered composition while keeping the text origin stable as dots are added.
- Keep the temporary **Click to continue!** state for the next screenshot/inspection pass.

### Untested work

- Reduced plaque scale and increased logo/plaque overlap.
- Additional ~10 px upward status-text move.
- Fixed-origin / left-aligned-within-centered-box animated text rendering.
- Final visual balance after those three changes.
- Runtime behavior of both animated status messages after the alignment change.

### Deferred

- Removing temporary **Click to continue!**.
- Restoring automatic splash close immediately after addon scanning.
- Human-readable Installed/Latest addon versions.
- GitLab/Gitea/OctoWoW provider expansion.
- Any broader P2 feature work until the splash is visually approved.

### Exact next step

1. Resume from `p2-github-branches` at `8166a8c` / published `v0.1.27`.
2. Treat the v0.1.27 runtime screenshot as the approved clean-logo baseline.
3. Leave the logo draw rectangle unchanged.
4. Make one conservative plaque-tuning pass: shrink it roughly 5–8% and move it upward roughly 8–15 px to increase overlap under the logo.
5. Move the status text upward another ~10 px (initial target Y≈405).
6. Replace whole-string center justification for animated statuses with a fixed centered region and left-aligned live text so `.` / `..` / `...` no longer causes horizontal jiggle.
7. Keep **Click to continue!** for inspection.
8. Build Windows x64 Release and run the complete CTest suite.
9. If green, prepare the next tuning release (expected `v0.1.28`) and obtain another runtime screenshot before removing the click gate.
10. Do not start provider expansion or other unrelated feature work until this splash pass is approved.

## Asset recovery checkpoint — 2026-09-21

- Active branch: `p2-github-branches`.
- Branch head before this checkpoint: `ab5527c` — **Add files via upload**.
- Published version remains `v0.1.26`; source version remains `v0.1.26` until the corrected splash build passes CI.
- The authoritative approved cutout logo has now been uploaded as `resources/TocPilot_logo_cutout.png` (1,315,364 bytes).
- The current four-chunk embedded logo was validated structurally and is definitely truncated: its PNG IDAT chunk is incomplete and no IEND chunk is present. This confirms the brown rectangle seen at runtime is caused by incomplete embedded image data rather than splash geometry.
- Approved geometry remains locked: do not move or rescale the logo or plaque.
- The only intended visual geometry change is a small upward move of the gold runtime status line.
- Temporary **Click to continue!** remains required for the next runtime screenshot pass.

### Completed

- Exact original cutout asset recovered in the repository.
- Runtime corruption cause confirmed from PNG structure rather than inferred from the screenshot alone.
- Existing v0.1.26 splash flow, animation, click gate, logo placement and plaque placement remain unchanged.

### Untested / next implementation slice

- Replace the fragile truncated base64 logo embedding with the complete uploaded PNG embedded as a Windows executable resource.
- Load that resource into GDI+ without altering the current logo draw rectangle.
- Move the status text upward slightly while leaving all other geometry unchanged.
- Build Windows x64 Release and run the complete CTest suite.
- Only after a clean build/test pass, prepare expected `v0.1.27` for runtime screenshot validation.

### Deferred

- Removing the temporary click gate and restoring automatic splash close.
- Human-readable Installed/Latest versions.
- GitLab/Gitea/OctoWoW provider expansion.

### Exact next step

1. Add `TocPilot_logo_cutout.png` to the executable resources and load it directly from the module resource bytes.
2. Remove the truncated logo base64 include dependency from `src/splash.cpp` while keeping the plaque embedding unchanged.
3. Keep splash size, logo rectangle `(20, 0, 780, 394)`, plaque rectangle `(95, 250, 630, 315)`, overlap and scale unchanged.
4. Move only the runtime text rectangle upward slightly.
5. Push the implementation, require Windows x64 Release + full CTest to pass, then prepare the next version only after CI is green.

## Runtime splash feedback checkpoint — 2026-09-21

- Active branch: `p2-github-branches`.
- Current branch head entering this handoff: `28b3ada` — **Record v0.1.26 splash tuning release**.
- Published version: `v0.1.26`.
- Published release commit/tag target: `af147e708660c7ced00f70ab622acc90264cbff2` — **Request v0.1.26 splash tuning release**.
- Latest relevant commits:
  - `28b3ada` — Record v0.1.26 splash tuning release
  - `af147e7` — Request v0.1.26 splash tuning release
  - `077cfb3` — Bump TocPilot to v0.1.26
  - `5a0c48f` — Bump TocPilot build version to v0.1.26
  - `f01ee2a` — Fix layered splash position pointer
  - `b58275e` — Fix GDI+ COM include order
  - `e1d3dc2` — Complete embedded splash asset set
- v0.1.26 release pipeline passed Windows x64 Release build, complete CTest, checksum generation, tag creation and publication.
- Runtime screenshot feedback from the first real splash test:
  - splash flow works and reaches temporary **Click to continue!** hold correctly;
  - click-gated inspection state is useful and should remain for the next tuning build;
  - **logo location is approved — do not move it**;
  - **plaque location is approved — do not move it**;
  - overall logo/plaque overlap and splash placement are approved;
  - runtime gold status text is **slightly too low** on the wooden panel and should be moved upward a small amount;
  - the logo image is visibly corrupted by a large solid brown rectangle below the goblin/head area;
  - this corruption is an implementation/asset-embedding fault, not a design/layout issue;
  - do not regenerate, redraw or reinterpret the locked logo while fixing it.
- Likely corruption cause identified during implementation:
  - the splash source originally referenced six embedded logo chunks;
  - only four logo chunk include files were ultimately retained in `src/splash.cpp`;
  - the resulting embedded PNG data is incomplete/truncated and renders partially before corrupting.
- Completed work:
  - layered transparent Win32 splash exists;
  - separate logo/plaque composition exists;
  - runtime one-line gold status text exists;
  - trailing dot animation exists;
  - app-update -> addon-scan -> temporary click-to-continue state wiring works;
  - no artificial startup delay is used.
- Untested after the next fix:
  - clean full-logo rendering with no corruption;
  - exact upward text adjustment;
  - status-line placement during both animated messages;
  - final visual proportions after corruption is removed.
- Deferred until the corrected splash screenshot is approved:
  - removing temporary **Click to continue!**;
  - automatic splash close immediately after addon scan;
  - human-readable Installed/Latest versions;
  - provider expansion remains frozen.

## Exact next step

1. Fix the embedded logo asset using the complete approved transparent TocPilot logo bytes; do **not** regenerate the artwork.
2. Keep the current splash window size, logo coordinates, plaque coordinates, scale and overlap unchanged.
3. Move the runtime gold status text upward slightly on the wooden panel; make no other geometry change unless required by the repaired asset.
4. Keep the temporary **Click to continue!** state for another screenshot/inspection pass.
5. Build Windows x64 Release and run the complete CTest suite.
6. Publish the corrected tuning build as the next version (expected `v0.1.27`) only after build/tests pass.
7. Runtime-test the corrected splash and obtain another screenshot.
8. Once the corrected logo and text placement are approved, remove the temporary click gate and restore automatic splash close after addon scanning.

## Release checkpoint — 2026-09-21 v0.1.26 splash tuning build published

- Active branch: `p2-github-branches`.
- Published version: `v0.1.26`.
- Release tag `v0.1.26` points exactly to `af147e708660c7ced00f70ab622acc90264cbff2` — **Request v0.1.26 splash tuning release**.
- Release workflow run `35548054383` completed successfully:
  - release tag resolution passed;
  - source/version validation passed;
  - Windows x64 Release configure/build passed;
  - complete CTest suite passed;
  - SHA-256 sidecar generation passed;
  - tag creation passed;
  - release asset publication passed.
- Published `TocPilot.exe`: 1,004,032 bytes.
- Splash implementation completed:
  - locked TocPilot logo is embedded as a faithful transparent/resized asset; it is not AI-regenerated;
  - blank wood/brass plaque is embedded separately for independent positioning;
  - splash is a borderless transparent layered Win32 window;
  - plaque is drawn first and logo overlaps it, hiding the plaque's top ornament;
  - one centered runtime gold status line is drawn over the wooden panel;
  - active status trailing dots animate `. -> .. -> ... -> .` at 420 ms;
  - startup begins with **Checking for TocPilot Update**;
  - after the app-update check completes it switches to **Scanning for Addon Updates**;
  - after the startup addon scan completes it switches to temporary **Click to continue!** and remains visible indefinitely;
  - clicking anywhere on the splash, or pressing Enter/Space, reveals the main TocPilot window and closes the splash;
  - no artificial splash delay was added.
- Build/debug notes:
  - initial GDI+ build failures were corrected by fixing COM/GDI+ include order;
  - layered-window position pointer constness was corrected;
  - implementation commit `f01ee2a` passed Windows x64 Release compile + complete CTest before the v0.1.26 version bump;
  - versioned-source build `35547887191` also passed before release publication.
- Runtime/visual testing still required:
  - logo/plaque scale and overlap;
  - gold text size, placement and legibility;
  - dot animation feel;
  - transition timing between update check and addon scan;
  - temporary **Click to continue!** hold and click-to-reveal behaviour;
  - screenshots should be used to tune geometry before removing the temporary hold.
- Deferred until splash geometry is approved:
  - automatic splash close immediately after addon scan;
  - human-readable Installed/Latest versions;
  - provider expansion.

## Exact next runtime step

1. Launch an existing `v0.1.25` TocPilot and self-update normally to `v0.1.26`.
2. Relaunch `v0.1.26`.
3. Observe the splash through:
   - **Checking for TocPilot Update. / .. / ...**
   - **Scanning for Addon Updates. / .. / ...**
   - **Click to continue!**
4. Leave it on **Click to continue!**, inspect the logo/plaque/text layout and take a screenshot.
5. Click anywhere on the splash and confirm the normal TocPilot window appears.
6. Send the screenshot/layout feedback before any geometry tweaks.
7. Once the splash is visually locked, remove the temporary **Click to continue!** state and auto-close after the addon scan.

## Splash implementation checkpoint — 2026-09-21

- Active branch: `p2-github-branches`.
- Published baseline: `v0.1.25`.
- Current branch head entering this slice: `a3cab59` — **Record v0.1.25 behavior refinement release**.
- Splash design now locked for implementation:
  - use the approved transparent TocPilot logo cutout unchanged;
  - use the approved blank wood/brass plaque as a separate asset beneath the logo;
  - overlap the logo over the plaque so the plaque's top ornament is hidden;
  - render one centered gold status line at runtime on the wooden panel;
  - animate only trailing dots as `. -> .. -> ... -> .`;
  - startup status sequence is **Checking for TocPilot Update** then **Scanning for Addon Updates**;
  - temporary tuning state after the addon scan is **Click to continue!** with no animation;
  - while tuning, the splash remains open indefinitely at **Click to continue!** and any click closes it/reveals the main TocPilot window;
  - no artificial delay during update checking or addon scanning.
- Completed before this slice:
  - v0.1.25 published successfully;
  - normal startup checks TocPilot release, then scans addon status;
  - smart launch, popup branch menu, and known-update-only Update All are implemented.
- Untested/deferred:
  - v0.1.25 runtime interaction validation remains useful, but splash work is now the active slice;
  - human-readable Installed/Latest versions remain deferred;
  - provider expansion remains frozen.

## Exact next step

1. Add the approved logo and blank plaque as embedded executable resources.
2. Implement a borderless transparent Win32 splash with per-pixel artwork composition and runtime gold status text.
3. Wire splash state transitions to the existing app-update check and startup addon-status scan.
4. Keep the main window hidden until the temporary **Click to continue!** state is clicked.
5. Add timer-driven trailing-dot animation with no fake startup delay.
6. Build Windows x64 Release and run the complete CTest suite before preparing a tuning release.
7. Do not remove the temporary click gate until runtime screenshots/layout feedback are complete.

## Release checkpoint — 2026-09-20 v0.1.25 published

- Active branch: `p2-github-branches`.
- Published version: `v0.1.25`.
- Release tag `v0.1.25` points exactly to `c9e0b195bad05d26919d49bd9926379f371ed7e5` — **Request v0.1.25 behavior refinement release**.
- Release workflow run `35536264813` completed successfully:
  - source/version validation passed;
  - Windows x64 Release configure/build passed;
  - complete CTest suite passed;
  - SHA-256 sidecar generation passed;
  - tag creation passed;
  - release asset publication passed.
- Published `TocPilot.exe`: 894,464 bytes.
- v0.1.25 behaviour changes:
  - TocPilot now always checks its own latest release at normal startup, then continues into addon status refresh;
  - **Update All** only queues addons already known to be **Update available** and applies their saved latest revision without rescanning Current addons;
  - startup/**Refresh All** remain responsible for discovering remote addon updates;
  - the WoW/VanillaFixes controls are now one smart launch button: `VanillaFixes.exe` + its icon when present, otherwise `WoW.exe` + its icon;
  - Branch selection uses a transient native popup menu instead of an overlaid combobox, removing the main source of selector jitter/alignment problems;
  - zero/one-branch repositories do not open a pointless branch popup; the selected one-branch row drops its arrow after discovery.
- Installed/Latest still display abbreviated commit SHAs in v0.1.25:
  - Smart HTTP can expose Git refs/tags but cannot read a remote addon TOC `## Version`;
  - a future slice can map exact revisions to Git tags opportunistically and fall back to SHA;
  - true TOC version values require remote content inspection.
- Runtime testing still required for all v0.1.25 interaction changes.

## Exact next runtime step

1. Start the existing `v0.1.24` TocPilot and confirm it discovers `v0.1.25` automatically on opening.
2. Self-update normally to `v0.1.25`.
3. Confirm the single launch icon targets VanillaFixes when `VanillaFixes.exe` exists and WoW otherwise.
4. In Advanced mode click Branch cells:
   - multi-branch repositories should open a popup menu anchored to the cell;
   - one-branch repositories should not open a one-item menu.
5. Let startup status refresh complete, then press **Update All**:
   - only addons already marked **Update available** should be queued/applied;
   - Current addons should not be rescanned by Update All.
6. Report runtime feedback before starting the human-readable version/tag slice or any provider expansion.

## Implementation checkpoint — 2026-09-20 post-v0.1.24 runtime-feedback slice

- Active branch: `p2-github-branches`.
- Published baseline: `v0.1.24`.
- Source version remains `v0.1.24` until release preparation.
- Current branch head entering release preparation: `67ad8a2` — **Replace branch combo overlay with popup menu**.
- Latest implementation commits:
  - `67ad8a2` — **Replace branch combo overlay with popup menu**.
  - `87a0c05` — **Refine startup updates and smart launch behavior**.
  - `d7e80c1` — **Test known-update Update All filtering**.
  - `dca126e` — **Queue only known addon updates**.
  - `450de7e` — **Checkpoint post-v0.1.24 runtime feedback**.
- Completed in source:
  - normal startup always checks the latest TocPilot release, then continues into addon status refresh;
  - **Update All** now queues only installed packages already known to have `installedRevision != latestRevision` and applies the saved latest revision without scanning Current addons;
  - startup/**Refresh All** remain the discovery path for new addon branch heads;
  - the two WoW/VanillaFixes launch controls are collapsed into one smart launch button that prefers `VanillaFixes.exe` and its icon when present, otherwise `WoW.exe` and its icon;
  - Branch selection no longer opens the overlaid Win32 combobox; clicking a Branch cell uses a transient popup menu anchored to the cell;
  - repositories with zero/one discovered branch do not open a pointless branch menu, and the selected one-branch row drops the arrow after discovery.
- Validation:
  - build run `35535889855` for `67ad8a2` completed successfully with Windows x64 Release compile + full CTest suite;
  - implementation-only Update All commit `dca126e` failed the previous test expectation as expected;
  - corrected test commit `d7e80c1` then passed the Windows build/test workflow;
  - `87a0c05` also passed Windows build + full CTest.
- Installed/Latest version display:
  - still shows abbreviated commit SHAs in this slice;
  - Git Smart HTTP can expose refs/tags but cannot read a remote addon TOC `## Version`;
  - a later refinement can opportunistically map exact SHAs to Git tags, while true TOC versions require remote content inspection.
- Runtime testing still required:
  - startup app-update indication and handoff into addon status refresh;
  - smart launch target/icon with and without VanillaFixes;
  - popup-menu branch interaction, including one-branch repositories;
  - Update All applying only rows already marked **Update available**.
- Deferred:
  - human-readable version/tag display;
  - GitLab/Gitea/OctoWoW provider expansion;
  - complex multi-root adoption.

## Exact next step

1. Prepare source version `v0.1.25`.
2. Require a clean Windows x64 Release build + full CTest on the versioned source.
3. Trigger the normal release workflow only by updating `.github/release-version` to `v0.1.25`.
4. Require the release workflow to pass source-version validation, build, tests, checksum generation, tag creation and asset publication.
5. Self-update a runtime `v0.1.24` install to `v0.1.25` and test the four runtime behaviours above before starting another feature slice.

## Continuation checkpoint — 2026-09-20 post-v0.1.24 runtime feedback

- Active branch: `p2-github-branches`.
- Published baseline: `v0.1.24`.
- Current branch head entering this slice: `9a4d118` — **Record v0.1.24 branch-cell UX release**.
- Latest release tag: `v0.1.24` -> `ffdc961e323f6ec276ca1562a86cc59342f4e003`.
- Runtime feedback to address:
  - TocPilot should always check its own published release on startup rather than depending on the saved `checkAppUpdates` setting.
  - the overlaid native Win32 branch combobox feels visually janky; reduce unnecessary dropdown interaction, especially for one-branch repositories.
  - **Installed / Latest** currently display abbreviated commit SHAs; investigate human-readable addon versions and document Smart-HTTP limits.
  - replace the two launch buttons with one smart launch button: prefer `VanillaFixes.exe` + its icon when present, otherwise `WoW.exe` + its icon.
  - **Update All** should apply only addons already known to need an update; status discovery belongs to startup/**Refresh All**, not the Update All action.
- Completed before this slice:
  - v0.1.24 published successfully with Windows x64 Release build + complete CTest pass.
  - startup already performs app-release checking when `checkAppUpdates` is enabled, then runs addon status refresh.
  - startup/manual addon status refresh already populates saved latest branch SHAs without changing addon files.
- Untested work from v0.1.24 remains:
  - branch-cell visual alignment while scrolling/sorting/resizing;
  - branch one-click/deferred-open feel;
  - Advanced toolbar geometry;
  - runtime branch switching.
- Deferred unless naturally solved in this slice:
  - human-readable remote addon versions when they require downloading/inspecting addon content rather than Smart-HTTP refs;
  - GitLab/Gitea/OctoWoW provider expansion;
  - complex multi-root adoption.

## Exact next step

1. Make the app-release check unconditional at normal startup, then continue into addon status refresh.
2. Change Update All to queue/apply only packages whose saved `installedRevision != latestRevision`; do not use Update All as a discovery scan.
3. Collapse WoW/VanillaFixes into one smart launch button with the executable's icon.
4. Suppress branch dropdown opening/affordance when the repository exposes no alternative branch.
5. Evaluate whether a less janky branch-picker interaction is warranted beyond the native overlaid Win32 combobox.
6. Run/adjust unit tests and Windows CI before preparing a new release.
7. Keep the version at v0.1.24 until this slice is validated; bump only for release preparation.

## Release checkpoint — 2026-09-20 v0.1.24 published

- Active branch: `p2-github-branches`.
- Published version: `v0.1.24`.
- Release tag `v0.1.24` points exactly to `ffdc961e323f6ec276ca1562a86cc59342f4e003` — **Request v0.1.24 branch-cell UX release**.
- Release workflow run `35533964804` completed successfully:
  - source/version validation passed;
  - Windows x64 Release configure/build passed;
  - complete CTest suite passed;
  - SHA-256 sidecar generation passed;
  - tag creation passed;
  - release asset publication passed.
- Published `TocPilot.exe`: 898,048 bytes; SHA-256 `e9244e39ebd80907ce01a9de0d383f99e93241c3cb0a6f4aaea6c4da479afb79`.
- v0.1.24 UI changes:
  - **Remove** -> **Remove Addon**;
  - **Reinstall** -> **Reinstall Addon** and uninstalled selections show **Install Addon**;
  - **Source / Track** -> **Branch**;
  - Branch values/dropdown entries are branch names rather than `GitHub / branch`;
  - Advanced toolbar now uses equal-width buttons with a minimum sized for **Scan Existing Addons**, and distributes extra width evenly;
  - Advanced minimum window width is derived from toolbar/visible-column requirements instead of the old fixed +520px expansion;
  - expanding/collapsing preserves additional user-resized width while respecting mode minimums;
  - every GitHub Branch cell in Advanced draws a visible dropdown arrow;
  - clicking an unselected Branch cell selects that addon and opens its dropdown in one interaction;
  - if branches are still loading, the dropdown automatically opens when discovery completes instead of requiring a second click.
- Runtime testing still required for the visual branch-arrow affordance, one-click timing, Advanced width/spacing, and selector alignment while scrolling/sorting/resizing.

## Exact next runtime step

1. Self-update TocPilot to `v0.1.24`.
2. Confirm compact shows **Update All / Add Git / Remove Addon / Advanced**.
3. Confirm all eight Advanced buttons are equal width and expand evenly with the window.
4. Confirm **Branch** is the second column and every GitHub addon row visibly shows a dropdown arrow.
5. Click the Branch cell of an unselected addon once; the branch list should open directly, or open automatically once the branch lookup finishes.
6. Scroll, resize columns/window, and sort while a row is selected; verify the real selector stays aligned.
7. Change a test branch and confirm tracking changes without addon files changing until an explicit **Update All** or **Reinstall Addon**.
8. Continue from this checkpoint in a fresh chat; the current conversation is heavily tool-loaded.

## Release-prep checkpoint — 2026-09-20 v0.1.24 Advanced toolbar + branch-cell UX

- Active branch: `p2-github-branches`.
- Published baseline: `v0.1.23`.
- New source version: `v0.1.24`.
- Latest implementation commits:
  - `05643cd` — **Bump TocPilot to v0.1.24**.
  - `ce478e2` — **Bump TocPilot build version to v0.1.24**.
  - `5649ff4` — **Complete branch selector forward declarations**.
  - `70fa018` — **Make Advanced branch cells one-click dropdowns**.
  - `32bf8cf` — **Refine Advanced toolbar and addon labels**.
  - `9582ebc` — **Checkpoint v0.1.24 branch-cell UX slice**.
- Completed in source:
  - **Remove** renamed to **Remove Addon** in compact and Advanced;
  - **Reinstall** renamed to **Reinstall Addon**; uninstalled selected packages show **Install Addon**;
  - Advanced list column **Source / Track** renamed to **Branch**;
  - Branch column values are now branch-centric (`main`, `dev`, etc.) rather than `GitHub / branch`;
  - compact remains four equal-width controls;
  - Advanced now uses the same equal-width auto-fill layout strategy across all eight buttons;
  - Advanced minimum button width is sized for the longest label (**Scan Existing Addons**);
  - mode-specific window minimum widths are derived from whichever is wider: toolbar requirements or visible columns;
  - expanding/collapsing Advanced preserves user-added width while applying the larger Advanced minimum;
  - every GitHub row in Advanced custom-draws a visible combobox/dropdown-arrow affordance in the Branch cell;
  - clicking an unselected Branch cell selects that addon and requests the branch dropdown in one interaction;
  - if branch discovery is still running, TocPilot automatically opens the dropdown as soon as smart-HTTP discovery completes rather than requiring a second click;
  - failed branch discovery clears the deferred-open request and can retry from the selector;
  - stale deferred-open requests are cleared when selection changes or Advanced collapses.
- Static audit confirms requested labels/header, equal-width Advanced code, dropdown-arrow drawing, one-click Branch-cell handler, and removal of the old fixed Advanced width constant.
- Windows CI for the newest commits is currently queued/running at this checkpoint; no v0.1.24 tag has been created yet.
- Runtime testing still required for visual arrow rendering, one-click auto-open timing, wide Advanced geometry, and branch switching.

## Exact next step

1. Require Windows x64 Release compile + complete CTest pass.
2. Publish v0.1.24 only through the normal release workflow.
3. Self-update from v0.1.23 to v0.1.24.
4. Verify compact **Remove Addon** and Advanced **Reinstall Addon / Remove Addon** labels.
5. Verify Advanced buttons are all equal width and resize evenly with the window.
6. Verify **Branch** cells show an arrow on every GitHub addon and one click opens the dropdown (immediately if cached/loaded, automatically when discovery finishes otherwise).
7. Scroll/sort/resize with a selection and verify the real selector remains aligned over the Branch cell.
8. Continue subsequent runtime feedback in a fresh chat; this conversation is heavily tool-loaded and the handoff is current.

## Continuation checkpoint — 2026-09-20 v0.1.24 branch-cell UX refinement

- Active branch: `p2-github-branches`.
- Published baseline: `v0.1.23`.
- Current branch head entering this slice: `282d656` — **Record v0.1.23 branch selector release**.
- Requested UI changes:
  - rename **Remove** -> **Remove Addon**;
  - rename **Reinstall** -> **Reinstall Addon** (and use **Install Addon** when the selected package is not installed);
  - rename Advanced list column **Source / Track** -> **Branch**;
  - give Advanced toolbar the same equal-width, minimum-width, auto-fill behaviour as compact instead of fixed per-button widths;
  - show a branch dropdown affordance on every GitHub addon row that can select a branch;
  - make the Branch cell one-click: first click selects the addon and opens its dropdown; if branches are still loading, auto-open when loading completes rather than requiring a second click.
- Keep compact toolbar as **Update All / Add Git / Remove Addon / Advanced**.
- Keep Advanced button order as **Update All / Add Git / Refresh All / Reinstall Addon / Remove Addon / Scan Existing Addons / TocPilot / Advanced**.
- Provider expansion remains frozen.

## Exact next step

1. Rework toolbar sizing helpers so compact and Advanced both distribute equal-width buttons and enforce the widest-label minimum.
2. Rename requested labels/column.
3. Custom-draw a dropdown arrow in every branch-capable Advanced Branch cell.
4. Handle a click on any GitHub Branch cell as select + open, with deferred auto-open after async branch discovery.
5. Build/test on Windows before preparing v0.1.24.
6. Runtime-test branch affordance/alignment and toolbar geometry.

## Release checkpoint — 2026-09-20 v0.1.23 published

- Active branch: `p2-github-branches`.
- Published version: `v0.1.23`.
- Release tag `v0.1.23` points exactly to `395bb2a752077834f31f73c548b880f5c8e12346` — **Request v0.1.23 branch selector release**.
- Release workflow run `35530425508` completed successfully:
  - source/version validation passed;
  - Windows x64 Release configure/build passed;
  - complete CTest suite passed;
  - SHA-256 sidecar generation passed;
  - tag creation passed;
  - release asset publication passed.
- Published `TocPilot.exe`: 896,000 bytes; SHA-256 `d10771039a13f42766f095da1202f7b8126186ab1fbefc86aae794a5ba3ccece`.
- v0.1.23 UI changes:
  - compact toolbar: **Update All / Add Git / Remove / Advanced**;
  - **TocPilot** is Advanced-only;
  - Advanced order: **Update All / Add Git / Refresh All / Reinstall / Remove / Scan Existing Addons / TocPilot / Advanced**;
  - selected GitHub rows in Advanced mode get an inline branch dropdown over **Source / Track**;
  - dropdown branch discovery is asynchronous and generation-guarded;
  - choosing a branch saves tracking + latest fetched SHA only; addon files are unchanged until an explicit update/reinstall action;
  - selector tracks scrolling, sorting, column sizing and window resizing;
  - branch lookup failure can retry by reopening the dropdown;
  - Add Git still uses the existing initial branch-selection dialog.
- Validation history:
  - intermediate `7627fff` build failed from a missing forward declaration;
  - fix `7853ba4` passed Windows build + full CTest;
  - v0.1.23 version builds `911fe54` and `4af5fbc` also passed;
  - final release workflow passed.
- Runtime testing still required.

## Exact next runtime step

1. Self-update TocPilot to `v0.1.23`.
2. Confirm compact has **Update All / Add Git / Remove / Advanced** only.
3. Open Advanced and confirm **Remove / Scan Existing Addons / TocPilot** appear consecutively in that order.
4. Select several GitHub addons and confirm the branch dropdown appears in the selected row's **Source / Track** cell.
5. Scroll, resize and sort while a row is selected; confirm the dropdown remains aligned.
6. Change a test addon from `main` to `dev` (or another known branch); confirm the saved tracking/status changes without modifying addon files.
7. Run **Update All** only if you want to test applying the new branch to disk.
8. Report runtime/UI observations in a fresh chat; this conversation is now tool-heavy and the repo handoff is current.

## Release-prep checkpoint — 2026-09-20 v0.1.23 Advanced branch selector

- Active branch: `p2-github-branches`.
- Published baseline entering this slice: `v0.1.22`.
- New source version: `v0.1.23`.
- Key implementation commits:
  - `7853ba4` — **Initialize Advanced branch selector on toggle**.
  - `7627fff` — **Add inline Advanced branch selector**.
  - `0c17600` — **Checkpoint v0.1.23 branch selector slice**.
  - `911fe54` / `4af5fbc` — v0.1.23 build/version bumps.
- Completed:
  - **TocPilot** is now Advanced-only and is hidden in compact mode;
  - compact toolbar is now **Update All / Add Git / Remove / Advanced**, still equal-width;
  - Advanced toolbar order is **Update All / Add Git / Refresh All / Reinstall / Remove / Scan Existing Addons / TocPilot / Advanced**;
  - therefore **Scan Existing Addons** is directly between **Remove** and **TocPilot** as requested;
  - removed the obsolete hidden **Set Branch** button/command from the main window;
  - Advanced mode now overlays a branch dropdown on the selected GitHub package's **Source / Track** cell;
  - selecting a row asynchronously loads visible branches using the existing GitHub/Git smart-HTTP repository-info path;
  - changing the dropdown immediately updates the tracked branch and saved latest branch SHA in `TocPilot.json`, but does **not** reinstall or modify addon files;
  - branch selection records the fetched SHA as fresh so a following **Update All** can use that known branch head without an unnecessary immediate re-query;
  - stale async branch-load results are generation-guarded when selection changes;
  - the selector follows list scrolling, column resizing, sorting, window resizing, and Advanced toggling;
  - failed branch lookups leave the current branch visible and can retry on the next dropdown open;
  - the existing branch-selection dialog remains for the initial **Add Git** workflow.
- Validation:
  - Windows x64 build run `35530113414` completed successfully from the branch-selector implementation commit;
  - complete CTest suite passed in that run;
  - an earlier intermediate build from `7627fff` failed because the first branch-selector commit lacked one forward declaration; `7853ba4` fixed it and passed.
- Still requires runtime UI testing:
  - compact toolbar after removing TocPilot;
  - exact Advanced button spacing/order;
  - selected-row inline dropdown alignment while scrolling/resizing;
  - branch list loading/retry behavior;
  - changing branch updates tracking/status without installing files;
  - branch dropdown behavior on repositories with many branches or a missing old branch.

## Exact next step

1. Publish v0.1.23 only if the release workflow passes Windows Release build + complete CTest suite.
2. Self-update the runtime install to v0.1.23.
3. In Advanced mode select several addons and verify the dropdown appears in each selected **Source / Track** cell and follows scrolling.
4. Change one package between `main` and a custom branch such as `dev`; verify only tracking/status changes until **Update All** or **Reinstall** is explicitly invoked.
5. Confirm compact has no TocPilot button and Advanced has **Remove / Scan Existing Addons / TocPilot** in that order.
6. Refine from runtime feedback; provider expansion remains frozen.

## Continuation checkpoint — 2026-09-20 v0.1.23 branch-selector/UI refinement

- Active branch: `p2-github-branches`.
- Published baseline: `v0.1.22`.
- Current branch head entering this slice: `20b19da` — **Record v0.1.22 release result**.
- Requested UI changes for the next source build:
  - **TocPilot** must be Advanced-only, not present in compact mode;
  - Advanced toolbar ordering should place **Scan Existing Addons** between **Remove** and **TocPilot**;
  - implement the previously deferred branch dropdown selector so a managed GitHub branch can be changed without opening the separate Set Branch dialog.
- Existing v0.1.22 runtime verification remains pending for single-instance, compact/Advanced geometry, app-update tool window, icon identity, name styling, and launch buttons.
- Deferred unless this slice naturally touches it:
  - clickable repository links in Advanced;
  - human-readable Installed/Latest addon versions;
  - complex multi-root GAM adoption/selection;
  - GitLab/Gitea/OctoWoW provider expansion.

## Exact next step

1. Rework compact/Advanced toolbar composition and minimum width for the new Advanced-only TocPilot placement.
2. Inspect the current branch-enumeration and branch-change paths.
3. Implement an Advanced branch dropdown bound to the selected managed GitHub package, reusing existing branch discovery and `SetPackageBranch` persistence rules.
4. Keep the old Set Branch dialog only as fallback if useful; avoid duplicating branch-setting logic.
5. Run Windows CI/tests before preparing any v0.1.23 release.
6. Runtime-test the new toolbar order and branch-selection interaction.

## Release checkpoint — 2026-09-20 v0.1.22 published

- Active branch: `p2-github-branches`.
- Published version: `v0.1.22`.
- Release tag `v0.1.22` points exactly to `6b5b9353d95154bad916028041c9f51892a03d73` — **Retry v0.1.22 release after Windows build fix**.
- Release workflow run `35528375519` completed successfully:
  - source/version validation passed;
  - Windows x64 Release configure/build passed;
  - complete CTest suite passed;
  - SHA-256 sidecar generation passed;
  - tag creation passed;
  - release asset publication passed.
- Published `TocPilot.exe`: 887,296 bytes; SHA-256 `94f441841e8baeed0da5c8abb261e51e46f6b60a71fb21559cf668846a63e7c2`.
- Published checksum sidecar: `TocPilot.exe.sha256`.
- The first v0.1.22 release attempt (run `35528077055`) failed safely during compilation before tests/tagging because MSVC exposed Win32 `LONG`/`int` template type mismatches in new custom-draw/window-centering code. No bad tag or asset was published.
- Fix commit `d593f8f` — **Fix Win32 layout type conversions** — passed normal Windows build run `35528228350` including the complete test suite before the corrected release was requested.
- Runtime testing is still required for the new v0.1.22 UI/identity slice.

## Exact next runtime step

1. Let `v0.1.21` self-update normally to published `v0.1.22`. This also exercises the updater-helper interaction with the new single-instance guard.
2. Launch TocPilot a second time from the same WoW directory: it should restore/foreground the existing window and exit the second process.
3. Check the smaller compact layout and five equal-width buttons: **Update All / Add Git / Remove / TocPilot / Advanced**.
4. Expand Advanced and verify **Scan Existing Addons / Refresh All / Reinstall** plus the detailed columns.
5. Confirm addon base names are bold while branch suffixes such as `(dev)` remain regular weight.
6. Open **TocPilot** and verify the GitHub/release links plus update check resolving to disabled **Up to date** on v0.1.22.
7. Check the application icon in Explorer/title bar/taskbar.
8. Check the larger/spaced WoW/VanillaFixes launch buttons and conditional VanillaFixes visibility.
9. Keep provider expansion frozen; refine this UI only from runtime feedback.

## Continuation checkpoint — 2026-09-20 v0.1.22 UI identity/update-control implementation

- Active branch: `p2-github-branches`.
- Source version: `v0.1.22`; Windows CI/release has not yet been triggered at this checkpoint.
- Branch head entering release preparation: `470e975` — Remove legacy main-window updater controls.
- Latest implementation commits:
  - `470e975` — Remove legacy main-window updater controls.
  - `80269ee` — Bump TocPilot to v0.1.22.
  - `1af361d` — Build v0.1.22 with application icon resources.
  - `eec3207` — Embed TocPilot application icon.
  - `dc8fc56` — Define TocPilot application icon resource.
  - `8a78342` — Add TocPilot application icon asset.
  - `80876b1` — Fix refresh and update-check sequencing.
  - `5715f90` — Separate app and addon refresh flows.
  - `e71b920` — Refine compact controls and app update UI.
  - `ef5c9e3` — Record revised TocPilot UI direction.
- Completed in v0.1.22 source:
  - normal UI is single-instance per TocPilot/WoW directory using a path-derived named mutex; the updater-helper path bypasses the normal guard;
  - a second launch for the same install restores/foregrounds the existing main window when possible;
  - removed the old top TocPilot/version placeholder panels; version remains in the title bar;
  - compact baseline reduced from 620x520 to 590x480 and minimum width is computed from toolbar-vs-visible-column requirements;
  - compact primary row is now five equal-width controls: **Update All / Add Git / Remove / TocPilot / Advanced**;
  - Advanced labels/actions now expose **Scan Existing Addons / Refresh All / Reinstall** with the renamed controls;
  - **Refresh All** uses the existing safe status-sweep path and does not change addon files;
  - addon base names custom-draw bold while non-main/master branch suffixes such as `(dev)` remain regular weight;
  - WoW/VanillaFixes launch buttons increased to 40px with more separation; VanillaFixes is hidden entirely when `VanillaFixes.exe` is absent;
  - new **TocPilot** tool window shows current version, GitHub repository/release links, and an explicit app-update state/action;
  - TocPilot update states are **Checking...** (disabled), **Up to date** (disabled), **Update Available** (enabled), plus retry states on failure;
  - the dedicated update action reuses the already runtime-proven download/SHA-256/two-process replacement flow;
  - manual TocPilot update checks no longer trigger addon refreshes; the existing startup app-check can still sequence into the automatic addon status sweep;
  - new gnome/courier application icon is embedded as a Windows multi-resolution icon resource (16/32/48/64/128) for EXE/window/taskbar identity.
- Static audit passed:
  - CMake/version.h both report v0.1.22;
  - resource script/header are wired into the TocPilot target;
  - requested labels are present;
  - legacy main-window updater control/id and old Adopt Git dialog title are removed;
  - single-instance, icon-resource and bold-name custom-draw paths are present.
- Runtime/Windows-CI untested:
  - MSVC/resource-compiler build of the new icon resource;
  - single-instance foreground/restore behavior;
  - self-update helper interaction with the single-instance guard;
  - real compact sizing and five-button equal-width layout;
  - bold name + regular branch suffix rendering under selection/focus;
  - dedicated TocPilot window links and all update button states;
  - Refresh All manual behavior;
  - 40px launcher spacing and conditional VanillaFixes visibility;
  - app icon appearance in Explorer/title bar/taskbar.
- Deferred:
  - inline per-row branch dropdowns;
  - clickable per-row repository link in Advanced;
  - human-readable Installed/Latest addon versions;
  - complex multi-root GAM adoption/selection;
  - GitLab/Gitea/OctoWoW provider expansion.

## Exact next step

1. Trigger the `v0.1.22` release workflow only from this exact source state plus the release-request/status commits.
2. Require Windows x64 Release build and the complete CTest suite to pass before the tag/assets are created.
3. Let v0.1.21 self-update to v0.1.22.
4. Runtime-test: second-launch single-instance behavior, compact/Advanced geometry, all renamed controls, Refresh All, dedicated TocPilot update window/links/states, icon identity, bold addon names/regular branch suffixes, and launcher visibility/spacing.
5. Refine only from that runtime feedback; keep provider expansion frozen.

## Continuation checkpoint — 2026-09-20 revised compact controls + app identity

- Active branch: `p2-github-branches`.
- Current published/source version entering this slice: `v0.1.21`.
- Runtime result for v0.1.21 compact/advanced layout:
  - compact and Advanced widening behavior are good;
  - startup scan no longer drags the list viewport;
  - attention-first sorting and persisted sort behavior work;
  - existing Add/Update/Refresh/Reinstall/Remove actions still work;
  - VanillaFixes launcher works.
- Revised compact/advanced control direction:
  - rename **Update** -> **Update All**;
  - rename **Refresh** -> **Refresh All**;
  - rename **Existing Addons** -> **Scan Existing Addons**;
  - rename **Add** -> **Add Git**;
  - add a dedicated **TocPilot** button for application/about/update functions.
- New **TocPilot** window requirement:
  - small dedicated child/tool window, separate from addon-management controls;
  - include links to the TocPilot GitHub repository and releases page;
  - include a self-update status/action area;
  - clicking Check for Updates runs the existing self-update discovery path;
  - no update -> resolve to a disabled/inactive **Up to date** button/state;
  - update found -> resolve to an enabled **Update Available** action that applies the existing proven self-update flow;
  - do not overload addon **Refresh All** or **Update All** with application-update semantics.
- Compact layout refinement requirements:
  - remove the top `TocPilot` and `Version v...` placeholder boxes;
  - compact mode may become smaller;
  - compact primary buttons should have equal width, with a minimum at least large enough for the widest minimum label;
  - distribute extra width evenly as the window grows;
  - compact minimum window width should be bounded by whichever is wider: the equal-width toolbar requirement or the visible compact columns;
  - addon base names should render bold while custom branch suffixes such as `(dev)` remain regular weight.
- Launcher refinement requirements:
  - make bottom executable icons slightly larger;
  - increase spacing to reduce misclicks;
  - only show the VanillaFixes launcher when `VanillaFixes.exe` exists;
  - WoW launcher remains available when `WoW.exe` exists.
- Process-safety requirement:
  - normal TocPilot UI should be single-instance **per WoW/TocPilot directory**, not globally across all installations;
  - a second launch for the same directory should activate/restore the existing TocPilot window and exit;
  - self-update helper mode must bypass the normal instance guard so the two-process updater continues to work.
- App identity requirement:
  - supplied gnome/courier icon direction is approved for TocPilot;
  - embed a proper multi-resolution Windows application icon for EXE/Explorer/window/taskbar usage.
- Deferred in this slice:
  - inline per-row branch dropdowns;
  - clickable per-row repository links inside Advanced;
  - human-readable Installed/Latest addon versions;
  - GitLab/Gitea/OctoWoW provider expansion.

## Exact next step

1. Implement single-instance-per-install startup protection without breaking updater-helper mode.
2. Remove the compact header/version boxes and reflow the window.
3. Rename controls to **Update All**, **Refresh All**, **Scan Existing Addons**, and **Add Git**.
4. Add the dedicated **TocPilot** child/tool window and wire it to the existing self-update discovery/apply flow.
5. Recompute compact toolbar/window minimum sizing with equal-width primary buttons.
6. Enlarge/space launch icons and hide VanillaFixes when its EXE is absent.
7. Embed the supplied application icon as Windows resources.
8. Add bold base-name / regular branch-suffix rendering in the package list.
9. Run CI/tests, publish the next runtime-test build only after exact source/version validation passes, then visually/runtime-test this UI pass.

## Continuation checkpoint — 2026-09-20 compact/advanced UI v0.1.21

- Active branch: `p2-github-branches`.
- Current published/source version: `v0.1.21`; tag `v0.1.21` points exactly to release request commit `c7e1d10e4aeaabc8a1f40afb0d5f023a886d1e6b`.
- `v0.1.20` GitHub transport gate is runtime-passed: user confirmed Set Branch works correctly with the REST quota exhausted, completing the smart-HTTP GitHub branch-list/default-branch validation.
- v0.1.21 release workflow run `35525749093` passed source/version validation, Windows x64 Release build, full CTest suite, SHA-256 generation, tag creation, and asset publication.
- Published `TocPilot.exe`: 846,336 bytes; SHA-256 `bf424a110b3d07c84346df08878eba4d36b207b945a7eac1a0a59b467db4d254`.
- Latest implementation/release commits:
  - `c7e1d10` — Request v0.1.21 compact layout release.
  - `6f43b53` — Set CMake version to 0.1.21.
  - `9d102e5` — Bump version to v0.1.21.
  - `950585b` — Keep compact state errors visible.
  - `1ec26e6` — Set compact layout baseline to 620x520.
  - `19a6bb3` — Make branch chooser neutral for Add flow.
  - `4a6b699` — Avoid stale row mapping after addon removal.
  - `253d7c0` — Use large executable icons for launch buttons.
  - `ca06b32` — Serialize startup app and addon update checks.
  - `ce93523` — Test persisted package sorting.
  - `7e6c8d8` — Prompt for app updates automatically at startup.
  - `4e8a43e` — Align Add dialog with compact install flow.
  - `6d2e5ff` — Make compact Add choose branch and install.
  - `a67284e` — Make Remove delete addon and TocPilot record.
  - `c1289f1` — Align compact update and advanced reinstall labels.
  - `a56f114` — Wire advanced widening and executable icon launchers.
  - `cc268f2` — Lay out compact and widened advanced views.
  - `a04f0f3` — Add compact advanced UI state.
  - `7105e9f` — Stop background refreshes moving addon list.
  - `9c159d4` — Persist sorting and pin attention addons.
  - `e6e7556` — Load and save package sort preferences.
  - `cc99dd9` — Persist package sort settings in app state.
- Completed in v0.1.21 source:
  - compact 620x520 default window with **Update / Add / Remove / Advanced** toolbar above the addon list;
  - **Update** retains the proven batch Update All backend; **Remove** transactionally removes owned addon files and the TocPilot record;
  - Advanced widens the same window 520px to the right and reveals **Existing Addons / Refresh / Reinstall** plus the existing detailed columns, rather than switching to a separate details panel;
  - compact list shows Name + Status; main/master names remain plain while alternate branches display as `Addon (branch)`;
  - addons requiring attention are grouped ahead of normal/current addons regardless of user-selected sort; sorting still applies within each group;
  - sort column/direction persist in `TocPilot.json` and restore on launch, with deterministic round-trip test coverage;
  - background startup/status/Update sweeps no longer select/EnsureVisible each processed addon; deliberate list rebuilds preserve the user's top visible addon where possible;
  - bottom-right WoW.exe and VanillaFixes.exe launchers use the executables' extracted icons; missing VanillaFixes disables its launcher;
  - Add now performs a complete GitHub flow: repository URL -> smart-HTTP branch choice -> save -> staged/validated install;
  - app self-update check is automatic at startup and prompts only when an update is available; addon status sweep starts after the app-update decision so the two startup jobs do not race;
  - old Inspect/Set Branch/Uninstall/manual app-update/text-size/status controls and supporting code largely remain under the hood but are not exposed in this first layout.
- Runtime-untested in v0.1.21:
  - actual compact spacing/feel at 620x520 and Advanced widen/collapse;
  - attention-first ordering with real update states plus persisted sort after restart;
  - list viewport remaining stable during the full startup status sweep;
  - WoW/VanillaFixes icon appearance and launch behavior on the user's install;
  - complete Remove behavior on an installed addon;
  - complete Add -> branch -> install flow in the new compact UI.
- Intentionally deferred until after the first real UI review:
  - inline per-row branch dropdown in Advanced;
  - clickable repository link;
  - human-readable addon/version metadata for Installed/Latest instead of commit SHA display;
  - further spacing/button/column refinements driven by the running UI;
  - provider expansion remains frozen.

## Continuation checkpoint — 2026-09-20 smart-HTTP transport

- Active branch: `p2-github-branches`.
- Transport checkpoint release: `v0.1.20`; GitHub smart-HTTP branch-picker/runtime gate passed.
- Current runtime-validated GitHub update path: `v0.1.19` Refresh / Update All passed repeatedly while the prior REST quota was exhausted.
- Transport checkpoint tag: `v0.1.20` points to release request commit `38a29ac`.
- v0.1.18 exposed a bootstrap blocker: v0.1.17 self-update discovery still depended on GitHub REST and could not discover v0.1.18 after the user's quota was exhausted. v0.1.19 fixes this by resolving the normal `github.com/.../releases/latest` redirect, parsing the final `/releases/tag/vX.Y.Z` URL, and constructing direct asset/checksum URLs. One manual direct-asset replacement is still required to cross from an older REST-dependent build while quota is exhausted.
- Latest commits:
  - `d63e87f` — Rename GitHub test away from API terminology.
  - `38a29ac` — Request v0.1.20 smart HTTP branch lookup release.
  - `54ba294` — Set CMake version to 0.1.20.
  - `4da3351` — Bump version to v0.1.20.
  - `35cc339` — Describe smart HTTP branch loading in UI.
  - `2700565` — Drop obsolete GitHub REST parser tests.
  - `0b9d40c` — Delete GitHub REST branch transport implementation.
  - `077db28` — Remove obsolete GitHub REST branch API surface.
  - `318e856` — Test smart HTTP branch enumeration and default.
  - `d4f5f0f` — Use smart HTTP for GitHub branch picker.
  - `d363104` — Enumerate branches and default via smart HTTP.
  - `9ee2245` — Expose smart HTTP repository ref discovery.
  - `fd71e9e` — Request v0.1.19 API-free self-update release.
  - `3a06113` — Set CMake version to 0.1.19.
  - `4a7d084` — Bump version to v0.1.19.
  - `bdc0304` — Run self-update discovery tests.
  - `c7e900b` — Test API-free self-update discovery.
  - `c4d8163` — Remove REST dependency from self-update discovery.
  - `71d49ed` — Request v0.1.18 smart HTTP transport release.
  - `e967923` — Set CMake version to 0.1.18.
  - `0012898` — Bump version to v0.1.18.
  - `0d256ad` — Run live GitHub smart HTTP probe in CI.
  - `380641e` — Add live GitHub smart HTTP probe.
  - `b697aa5` — Require smart HTTP service header before refs.
  - `020883d` — Test direct GitHub codeload paths.
  - `6fabbf2` — Download exact GitHub archives without REST.
  - `57443b0` — Use smart HTTP for GitHub branch head refresh.
- Completed in source:
  - provider-neutral WinHTTP smart-HTTP ref discovery against `info/refs?service=git-upload-pack`;
  - protocol-v0/v1 pkt-line parsing for `refs/heads/<branch>`, including first-ref capability/NUL handling and 40/64-character object IDs;
  - GitHub `ResolveGitHubBranchHead` now delegates to the smart-HTTP resolver, so routine refresh/inspect/install branch-head resolution no longer pages the GitHub REST branches API;
  - deterministic malformed/truncated/service-header/missing-branch/protocol-v2 parser coverage;
  - live CI probe added against public GitHub smart HTTP;
  - exact-SHA GitHub archive downloads now target `codeload.github.com/<owner>/<repo>/zip/<sha>` directly, removing the remaining REST zipball hop while preserving existing ZIP validation, staging, ownership, rollback, and transaction code.
  - GitHub branch enumeration and default-branch discovery now use the same smart-HTTP advertisement: all `refs/heads/*` are parsed and `symref=HEAD:refs/heads/<default>` selects the default branch;
  - `FetchGitHubRepositoryInfo` / Set Branch now use smart HTTP instead of GitHub repository metadata plus paged REST `/branches` calls;
  - obsolete GitHub REST branch implementation, API path surface, JSON parser tests, and API headers/endpoints were removed; a runtime-source audit found no `api.github.com`, GitHub JSON API headers, `/repos/`, or `/branches` endpoint references;
  - all Git repository discovery/branch operations are now smart HTTP. Direct codeload ZIPs and direct release assets remain ordinary HTTPS because they are file-distribution endpoints, not Git protocol operations, and do not depend on GitHub REST quota.
- Runtime result immediately before this slice: `v0.1.17` self-update succeeded and `pfUI-VendorTweaks` updated successfully, but the startup/update sweep exhausted the user's unauthenticated GitHub REST allowance and produced one rate-limit error. This made the transport replacement urgent.
- v0.1.19 release validation:
  - release workflow passed source/version validation, Windows x64 Release build, all 13 CTest tests, SHA-256 generation, tag creation, and asset publication;
  - deterministic self-update redirect parsing passed;
  - live `github.com/.../releases/latest` redirect resolution passed without the GitHub REST API;
  - tag `v0.1.19` points exactly to `fd71e9e585f8e5186390e5c129bad089b0c3ccee`;
  - published `TocPilot.exe` is 834,560 bytes with SHA-256 `acd935d3f097a0ec585202521808d4212ae17adc9a2ed7ddb10fa39cc0ec9264`.
- Release validation:
  - release workflow run `35521952232` passed source/version validation, Windows x64 Release build, the full 11-test CTest suite, SHA-256 generation, tag creation, and release asset publication;
  - the live GitHub smart-HTTP WinHTTP probe passed in CI;
  - tag `v0.1.18` points exactly to commit `71d49edf1fa016b3b3e3fb9ce0d86ea1c03e90cb`;
  - published `TocPilot.exe` is 834,048 bytes with SHA-256 `523786dbaab66879f96714a6568da189c66decd5f8e76fe4e0ac9189864292e6`.
- Runtime validation passed for v0.1.19:
  - user repeatedly spammed **Refresh** with no errors;
  - user repeatedly ran **Update All** with no errors;
  - this was performed after the prior GitHub REST allowance had been exhausted, validating that routine managed-addon branch refresh no longer depends on the GitHub REST branches API;
  - real managed-addon update flow therefore validated smart-HTTP branch-head discovery plus direct GitHub codeload archive transport under normal application use.
- v0.1.20 release validation:
  - release workflow run `35523160550` passed source/version validation, Windows x64 Release build, the full CTest suite, checksum generation, tag creation, and asset publication;
  - deterministic smart-HTTP branch enumeration/default-branch tests passed;
  - live GitHub smart-HTTP repository lookup passed with branch enumeration and default-branch detection;
  - tag `v0.1.20` points exactly to `38a29ac69eda4caa0ea07905b6da8168442263c5`;
  - published `TocPilot.exe` is 832,000 bytes with SHA-256 `775de755b8ce56f17434aec75a3d7ad4588aa3a5dd17d281e7a8c3ad369b8b04`.

- Product priority changed after the v0.1.19 GitHub transport pass: **do not add GitLab/Gitea/OctoWoW provider support yet**. Finish the GitHub-only product to a smooth, properly laid-out state first. Cross-provider transport is deferred until the GitHub UX/layout is considered complete.
- Deferred after the GitHub runtime gate:
  - validate the same provider-neutral ref discovery against GitLab and OctoWoW/Gitea-style hosts;
  - direct non-GitHub archive URL construction;
  - multi-root GAM adoption/selection and the remaining compact-UI work;
  - private-repository authentication, libgit2/git.exe, and bundled Git remain explicitly out of scope.


## Continuation checkpoint — 2026-09-19

- Active branch: `p2-github-branches`.
- Current runtime-tested application version includes the `v0.1.13` adoption slice. The user successfully adopted existing Git-managed addons, then ran Update All across 21 installed packages: Queued 21 / Processed 21 / Updated 1 / Already current 20 / Failed 0. This validates adoption feeding the normal refresh/update transaction path. Restart persistence and continued GitAddonsManager visibility of retained `.git` metadata remain the final adoption runtime checks.
- v0.1.15 runtime exposed one root-addon update mapping bug in `pfUI-VendorTweaks`. The fix is versioned as v0.1.16 from release trigger `b6ae493`. Tag `v0.1.16` now exists and resolves to the 0.1.16 source, confirming source/version validation, Windows x64 Release build, and the complete CTest suite passed before tag creation. Runtime validation is pending.
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
- Highest priority: runtime-validate v0.1.17 rate-limit handling, then replace routine public branch-head REST polling with a lightweight Git-over-HTTPS ref-discovery path that does not require libgit2, git.exe, or user credentials
- Current adoption runtime result: 21 managed packages completed Update All with 1 updated / 20 current / 0 failed after existing Git installs were adopted.

## Latest commits

- `c61adab` — Report rate-limited status sweeps clearly
- `bb27ab2` — Stop redundant work when GitHub rate limits
- `c0f96e4` — Reuse fresh branch heads during Update All
- `659d9a0` — Build and test refresh freshness cache
- `1a997d0` — Test package refresh freshness cache
- `79c1f89` — Harden refresh freshness includes
- `b61a019` — Implement package refresh freshness cache
- `844a84c` — Add package refresh freshness API
- `934d20c` — Record startup refresh rate-limit runtime bug
- `b6ae493` — Request v0.1.16 root mapping fix release
- `d8ddde8` — Set v0.1.16 application version
- `6986cc2` — Bump TocPilot to v0.1.16
- `676c77e` — Preserve adopted addon root during archive mapping
- `93ee693` — Test owned root preservation for VendorTweaks
- `7ef0a5c` — Preserve owned root for root-level addon updates
- `6deec8c` — Allow root-addon mapping to preserve owned root
- `684e06c` — Record v0.1.15 Update All layout failure
- `2d9d963` — Request v0.1.15 status and diagnostics release
- `f14f97e` — Set v0.1.15 application version
- `f6819a1` — Bump TocPilot to v0.1.15
- `0c8f08a` — Update handoff for v0.1.15 feature head
- `2059d73` — Harden folder scanner test includes
- `f91f51e` — Harden folder scanner includes
- `a3d285e` — Show classified AddOns folder diagnostics
- `5a069ab` — Auto-refresh managed addon update status
- `0f82805` — Declare package row selector before sorting
- `e7bbe0c` — Add stable package column sorting
- `11e5a98` — Build and test AddOns folder scanner
- `235e371` — Test AddOns folder classification
- `896a639` — Classify AddOns folders conservatively
- `3572a82` — Add AddOns folder classification API
- `dd86ca3` — Set next TocPilot runtime slice
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
- v0.1.15 runtime finding: Update All after the automatic status pass reported Queued 21 / Processed 21 / Updated 2 / Already current 18 / Failed 1. The sole failure was `pfUI-VendorTweaks`: install preparation refused the GitHub root-addon archive as ambiguous. Repository `Seraphic8x2244/pfUI-VendorTweaks` has a root-level `pfUI_VendorTweaks.toc`, while the adopted/owned live folder remains `pfUI-VendorTweaks`. The current root-addon safety rule requires the root TOC stem to match the repository name, so the normalized underscore TOC stem trips the guard. Update All correctly continued and no live files were changed for the failed package.
- v0.1.16 runtime finding: after the automatic startup status refresh, an immediate Update All queued/processed 22 packages and failed all 22 during GitHub refresh with `GitHub API access was refused or rate-limited. Try again later.` This demonstrates the new startup sweep plus Update All performs redundant remote-head requests and can exhaust GitHub's unauthenticated API budget. Fix direction: track when each package's `latest_revision` was refreshed in-process and let Update All reuse a sufficiently fresh startup result instead of immediately requerying GitHub; still refresh when metadata is stale or unknown. Do not hide real rate-limit/network errors.
- v0.1.17 release workflow run `35518021515` completed successfully end-to-end: source-version validation, Windows x64 Release build, full CTest suite, checksum generation, tag creation, and release asset publication all passed. Tag `v0.1.17` resolves correctly. The fix uses a 5-minute in-process freshness cache for successful branch-head checks, lets Update All reuse those results, passes the already-known exact SHA into install preparation to avoid a second branch lookup, and stops automatic status/Update All batches early when GitHub reports rate limiting instead of generating one failure per package.
- v0.1.17 runtime gate passed: TocPilot self-updated successfully, `pfUI-VendorTweaks` updated successfully, and the owned-root underscore/name fix is therefore runtime-validated. However, the startup/status sweep still exhausted the user's unauthenticated GitHub REST quota and produced one rate-limit error. Treat the freshness cache as only a partial mitigation; replacing routine public branch polling with Git smart-HTTP ref discovery is now urgent and is the active development priority.
- Architecture decision after the v0.1.16/v0.1.17 rate-limit finding: the 5-minute cache is an immediate mitigation, not the final transport design. TocPilot should stop spending one unauthenticated provider REST request per public addon branch check. The planned replacement is a very small Git-over-HTTPS ref-discovery implementation over WinHTTP, sufficient only to resolve tracked branch SHAs. **Do not add libgit2 or git.exe at this stage.** The design target is to keep TocPilot in roughly the current small standalone-EXE class rather than importing a multi-megabyte Git backend.
- The transport change does **not** remove staging. Exact-SHA archives still go through `Interface\\TocPilot\\staging`, secure ZIP validation/extraction, addon-root mapping, ownership/collision checks, rollback preparation, then the existing transactional live commit.
- v0.1.15 remaining runtime checks: column sorting/action targeting and Add Git folder diagnostics still need user validation. Startup refresh clearly discovered multiple changed packages because Update All immediately attempted three changed installs; a focused confirmation of the startup summary/status display is still useful.
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
- GitHub/GitLab/Gitea release package/assets support beyond branch archives;
- private-repository authentication/credential storage;
- DLL/direct-file installation;
- import/export;
- persisted column order/width and layout locking.

## Important design decisions

1. Put `TocPilot.exe` beside `WoW.exe`.
2. If another WoW install needs management, copy TocPilot there too.
3. No local `.git` repositories for TocPilot-managed packages.
4. Keep TocPilot lightweight: **do not add Qt, libgit2, a bundled Git runtime, or a required `git.exe` dependency** unless the lightweight transport approach is proven insufficient.
5. For routine **public branch-head discovery**, move away from provider REST APIs and use the repository's public Git-over-HTTPS ref-discovery endpoint through the existing WinHTTP stack. TocPilot only needs to learn the exact commit SHA of a tracked branch; it does not need checkout, index, merge, rebase, local object storage, or a working Git repository.
6. Public branch tracking should require **no user credentials** on normal GitHub, GitLab, OctoWoW/Gitea-style, and comparable public HTTPS Git hosts. Private repositories/authentication remain deferred and may require provider-specific credentials later.
7. Provider APIs may still be used where they are actually appropriate, such as TocPilot self-update/release metadata and future provider release-asset browsing. They should not be the default mechanism for polling every public addon branch.
8. For changed addons, resolve an **exact SHA first**, then download that exact revision. Prefer direct public archive-download URLs that do not consume a low REST API quota when the provider offers them.
9. **Keep the staging area and transactional install/rollback model.** The transport redesign changes only how TocPilot learns/fetches a revision. Archives must still be downloaded, securely validated, extracted, addon-root mapped, ownership/collision checked, and transaction-prepared under `Interface\\TocPilot\\staging` before live `Interface\\AddOns` is changed.
10. A managed **package** may install addon folders, multiple addon folders, a release ZIP, a DLL, or another safe relative file.
11. Self-update comes before addon management so later builds can be tested without manual copy-over.
12. Self-update publishes/downloads a direct `TocPilot.exe` asset.
13. The updater waits for the old process to fully terminate before replacing the EXE and uses retries/rollback rather than reproducing GitAddonsManager's Windows file-lock false failure.
14. Local state is portable with the WoW install, initially planned as `TocPilot.json`.
15. P0 release builds also publish a small `TocPilot.exe.sha256` sidecar as a fallback if GitHub does not expose an asset digest.

## Priority roadmap

### P0 — Self-update bootstrap

Complete. A real `v0.1.0 -> v0.1.1` in-app self-update succeeded on Windows beside a real `WoW.exe`: update detected, downloaded, verified, old process closed, executable replaced, and new version restarted successfully.

### P1 — State and basic UI

- `TocPilot.json`;
- package list;
- text-size preference;
- URL/provider parsing.

### P2 — Public Git branch packages

- keep the existing exact-SHA, archive, staging, ownership, transactional install/update/remove model;
- replace routine GitHub REST branch polling with lightweight public Git-over-HTTPS ref discovery using WinHTTP;
- first transport spike must validate branch-head lookup against **GitHub, GitLab, and OctoWoW/Gitea-style hosting** without credentials;
- parse only the minimum Git ref-advertisement data required to resolve the selected branch SHA; do not implement a general Git client;
- prefer direct public archive downloads by exact SHA so normal addon management does not depend on low REST API request quotas;
- retain provider-specific URL construction only where needed for archive/release endpoints;
- no local TocPilot `.git` checkout and no libgit2/git.exe dependency in the first implementation;
- private repository authentication remains out of scope.

### P3 — GitHub releases/direct assets

- browse releases/assets;
- latest stable/prerelease tracking;
- ZIP assets;
- direct DLL/file assets.

### P4 — Cross-provider release support

- public GitLab and Gitea-style branch tracking should already be covered by the provider-neutral Git-over-HTTPS ref path from P2;
- add provider-specific release browsing/assets only where Git transport alone cannot provide the required release metadata.

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
- Treat complex multi-root adoption as a later UI-wrapper/layout pass after the GitHub-only compact layout is stable.
- New UX requirement after v0.1.20 runtime pass: persist package-list sort column and ascending/descending direction in `TocPilot.json`, save on change/close, and restore on next launch so the user's chosen ordering survives restarts.
## Exact next step

1. Let `v0.1.20` self-update normally to published `v0.1.21`.
2. Judge the real compact window rather than refining from ASCII: toolbar spacing, list proportions, Name/Status widths, bottom launch icons, and whether 620x520 is the right baseline.
3. Click **Advanced** repeatedly and confirm the window feels like the same program widening/revealing hidden controls/columns, then collapsing without moving its left edge.
4. During the automatic startup addon scan, manually scroll/select elsewhere and confirm TocPilot no longer drags the list viewport through each scanned addon.
5. Verify addons needing attention remain grouped at the top while column sorting still works inside attention/current groups; close/reopen and confirm sort column/direction persist.
6. Smoke-test **Update**, **Existing Addons**, **Add** (URL -> branch -> install), **Refresh**, **Reinstall**, and **Remove** semantics.
7. Verify the WoW.exe and VanillaFixes.exe icon buttons look correct and launch the intended sibling executables.
8. After visual/runtime feedback, refine the real UI first. Planned Advanced refinements are inline branch dropdowns, clickable repository links, and human-readable Installed/Latest addon versions.
9. Keep GitLab/Gitea/OctoWoW provider expansion frozen until the GitHub-only UI is considered complete.
