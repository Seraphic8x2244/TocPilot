# TocPilot Development Progress

## v0.6.0 Gale-style layout implementation — slice 1 (2026-10-23)

**Source commit:** `33c7232` — `src/main.cpp` begins the approved structural migration by separating the Launch WoW control onto an independent top toolbar row, while moving existing package actions to the row beneath. The package-action order begins Update All → Refresh → Add Git in Advanced and icon-based Compact layouts; other existing legacy actions remain temporarily reachable during the transition. Package list top is moved down to account for the second row. The same underlying Win32 controls and command handlers are reused; no package/update/sync logic was intentionally changed.

**Not yet delivered:** The full Packages / Account Sync / TocPilot tab architecture, flat Preferences/Scan/About sections, package summary, removal of legacy Inspect, dark Account Sync interior and refined list styling. These are still required under the approved UI target. This is an incremental layout foundation, not a claim that the new design is complete.

**Validation:** Source committed only. Windows Build/CTest and actual DPI/compact layout checks are **not verified** for this commit; user runtime testing must not be requested until the next updater-installable development prerelease has been built and published. Stable `main` unchanged.

---

## Approved v0.6.0 UI target — Gale-inspired layout (2026-10-23)

**Status:** User approved the inline layout prototype, with the final navigation correction below. This supersedes plans to merely reskin the crowded legacy layout. Gale is a loose layout reference, **not** a pixel-perfect copy. Preserve TocPilot's already approved dark-grey palette and full-row five semantic state colours (Already Good blue, Good Now green, Ignored grey, Needs Attention amber, Error red), including the single inset selection border and hover treatment. Implement with native C++20/Win32, retaining DPI, accessibility, keyboard and high-contrast support and the existing package/update/sync logic.

**Primary structure:**
- Top horizontal area: TocPilot application branding and **Launch WoW**, prominently arranged in the top row; no separate TocPilot-information button here.
- Primary navigation: **Packages | Account Sync | TocPilot**. Rename the proposed `Settings` tab to **TocPilot**; **do not add an Add Git navigation tab**.
- Packages toolbar: exactly **Update All → Refresh → Add Git** in that left-to-right order; Update All visually primary, others neutral. **No search box and no filter dropdown.** Sorting and existing package behaviours must remain available without introducing requested-removed toolbar clutter.
- Packages main content: well-aligned, spacious package list with existing semantic state colourway, and persistent/contextual **package summary** panel for the selected package. Summary includes status, branch, type, repository/source, installed/latest revisions, relevant file/path details where actually available. Package contextual actions include **Reinstall and Remove adjacent**, plus **Open Folder** where supported. Preserve distinct remove-tracking vs delete-files semantics and any confirmation/safety protections. **Remove Inspect as a separate UI action.** Keep compact display option (may conceal summary).
- Account Sync: **its own dedicated tab**, not a miniature panel on the Packages screen. Keep existing source/target accounts, comparison arrows, Refresh/Sync, approvals, conflict/safety behaviour and layout semantics. Do not fabricate statuses or replace actual comparison with decorative checks.
- **TocPilot tab:** use the available full content area to display **Preferences, Scan and About directly as three flat sections**, without opening another panel/dialog merely to reach them. Move Scan here from the package toolbar, and move TocPilot/about/updater information here from the top navigation. Preserve actual operational settings and scan/update behaviour.

**Design execution:** coherent aligned spacing, modern native control appearance (icons, headings, column headers, scrollbars, buttons and dark surfaces), sensible resizable layout, and consistent styling beyond the main view. No new heavyweight framework or hidden functional rewrites. No AI-generated image references; use the user's Gale reference and the accepted in-chat prototype and written corrections. **Do not start implementation of popup replacement automatically.**

**Acceptance/release:** Design direction approved; implementation and Windows tests pending. Before **every** requested runtime-testing milestone, build/publish an actual updater-installable `dev` prerelease with assets/checksum and verify the GitHub release workflow as far as tools allow. Never ask the user to test an unbuilt change. Keep stable `main` untouched.

---


## v0.6.0-dev.2 Windows visual feedback — 2026-10-23

**User runtime confirmation:** Screenshot from running `TocPilot v0.6.0-dev.2` shows the selected AutoPickLockbox row with a single inset outline; user explicitly says "working". This validates the visible double-selection-border fix in that runtime environment, not a complete keyboard/high-contrast matrix or automatic-updater provenance.

**Visual acceptance remains open:** User describes the app as a patchy, dated Windows interface. The dark row fills and main/window surfaces contrast with legacy white Account Sync tiles, white header/scrollbar, rectangular system-style Refresh/Sync controls, and black toolbar icons. Windows 11 supplies rounded outer-window corners, not a complete internal design system. User expects coherent modern native Win32 styling across toolbar, list/header, Account Sync and other windows while preserving accepted workflows and theme semantics. Do not close v0.6.0 after the border fix.

**Next focused work:** continue shared central palette usage, modernise these visible controls with DPI/high-contrast/keyboard support and retain Account Sync semantics. Validate Windows Build/CTest and deliver an updater-installable development prerelease **before** asking the user to test again, per standing requirement. Popup replacement remains a later slice; stable `main` unchanged.

---

## Runtime-test release requirement — 2026-10-23

**User standing instruction:** Whenever Windows runtime testing is requested, first prepare and trigger a real installable development prerelease on `dev` through TocPilot's existing development Release workflow, with a version increment, published EXE and SHA-256 checksum, and updater visibility. A plain Build workflow artifact is not sufficient. Do not ask the user to test source changes that lack an updater-eligible release. This requirement applies to all future development testing milestones. Stable `main` remains untouched unless separately approved.

**Current release preparation:** `v0.6.0-dev.2` packages the single-selection-border fix from `fd95ebd` / `9c16c49`. This note precedes the release trigger; success, exact CI run, assets, and automatic updater installation must be checked independently. No runtime success is inferred in advance.

---

## v0.6.0-A selection-border follow-up — 2026-10-23

**User runtime evidence:** The selected `AuctionQueryThrottle.dll` row in v0.6.0-dev.1 showed two stacked outlines. The item custom-draw handler explicitly called `DrawFocusRect` inside the already outlined row while native ListView focus handling remained available.

**Source fix on `dev`:** `fd95ebd` removed the extra `DrawFocusRect` and masked `CDIS_FOCUS` from the ListView custom-draw item state outside Windows high-contrast mode; `9c16c49` makes the single 2px selection outline slightly brighter when the ListView itself has keyboard focus (`Palette::primaryText`), using `Palette::selection` otherwise. High-contrast native focus path remains enabled; row data, keyboard selection, hover, branch controls and painting geometry were not deliberately changed. **This fixes the known duplicated custom focus rectangle in source, but Windows runtime confirmation is still pending.** No new prerelease published; prior `v0.6.0-dev.1` executable is unchanged.

**Next:** verify Windows Build/CTest on the exact new `dev` commit, then validate actual focused/unfocused selection, mouse hover, keyboard focus and high-contrast modes in the next dev prerelease along with the other unfinished neutral chrome work. Keep `main` untouched, no popup replacement.

---

## v0.6.0-dev.1 real Windows visual observation — 2026-10-23

**User screenshot — running titlebar `TocPilot v0.6.0-dev.1`:** Development release is running on the user's Windows machine. Native ListView shows multiple blue `Up To Date` rows, white row text and cube-like package glyphs (including .dll-named packages), and the grey toolbar/outer surface is visible. This is **partial visual runtime evidence**, not end-to-end feature acceptance or proof of updater transition (how the EXE was installed is not established).

**Visible unfinished visual integration:** Account Sync contents and tiles remain bright white with legacy coloured labels, while the surrounding bottom chrome is grey; the ListView column header, exposed scroll region, and unused right area remain system white; icons in grey toolbar buttons appear black rather than centrally tinted. This mismatches the approved coherent dark native palette. Two accounts (BLACKWAVES, BLACKWAVESTWO) and Refresh/Sync controls remain present; no evidence of broken Sync, and do not infer a successful Sync transaction from the screenshot. The package list is vertically shallow because the existing Account Sync panel reserves height; do not alter that accepted layout contract purely from the screenshot without studying the existing Compact/Advanced/minimum-height behaviour. The `Up To Date` labels are visible and indicate no write; they must remain blue, not green.

**Next focused visual work:** determine native header/list blank-region theming and toolbar icon source/colour without changing icon/word options, Windows high contrast, accessibility, branch-selector overlay, clipping or DPI; migrate Account Sync tile and outer chrome to central neutrals while preserving its existing comparison colours/arrows/ticks and all engine semantics. Test selected/hover/focus and green/amber/error states separately; screenshot does not establish those states. Add regression checks, pass Windows CI, and issue a new development prerelease via normal release-first route for user runtime validation. No popup replacement.

---

## v0.6.0-dev.1 development release preparation — 2026-10-23

**Purpose:** deliver the existing v0.6.0-A theme work through the real development self-updater for Windows acceptance, without promoting stable `main`. User reported green Build CI for the prior development source but did not provide run ID/logs. Source version bumped from `0.5.1` to `0.6.0-dev.1` in CMake and `src/version.h`; development release trigger marker `.github/development-release-version` is the final commit of preparation. **This paragraph does not assert the release has succeeded:** after trigger, verify exact run result, published prerelease `v0.6.0-dev.1`, EXE and SHA256, and updater visibility. The 0.6.0-A runtime UI still needs acceptance; do not remove dialogs or change Account Sync policy.

---

## v0.6.0-A shared-theme implementation checkpoint — 2026-10-23

**Active branch:** `dev`; starting head `a5204a64ac17589ad11da36fbeacd09c49da7ca5`. **Stable `main` / published `v0.5.1` remain untouched.** This is a **partial implementation awaiting Windows build, CI and runtime validation**, not a completed/accepted 0.6.0-A slice, and no new version/release has been published.

**Follow-up source/test correction (2026-10-23):** Moved target-to-Addon/DLL/MPQ kind classification into central `ui_theme.h` and exercised all target spellings in `ui_theme_tests.cpp`. Corrected a critical test-harness weakness: `assert` is compiled out in Windows Release with `NDEBUG`; all theme regression conditions now use explicit nonzero exit failures. Commits `b9d222f`, `add5ef2`, `e26b1b5`, `4393605`, `a5245fb`. **Neither tests nor actual MSVC build have yet been run.**

**Source changes:** `src/ui_theme.h` is the new central token source for the exact agreed neutral and five row status colours; hover is calculated by a single RGB lift function; selection/focus is modelled separately from fill; high contrast detects the OS override. `src/main.cpp` now consumes status colours in the existing ListView, retains the `g_packageViewOrder` / package data model, paints name/link/branch and other cells, adds a full-row 2px inset selection outline plus keyboard focus, tracks hover by a list subclass, draws neutral native GDI cube/chip/archive package glyphs, and draws icon-toolbar plus main/toolbar/Account-Sync-region backgrounds with shared greys. Existing semantic text-status rules now use theme tokens. No popup, installer, sorting, branch-selector, update or Account Sync engine changes were intended.

**Provenance:** `c7831e0` theme model; `1e59e85` ListView colours; `160803b` theme tests; `d05da70` CTest registration; `f8b9e9e` toolbar buttons; `5326115` hover; `3076c5d` package glyphs; `73dcaa6` full-row selection and other cells; `d67f38d` null-character fix; `4771c56` surfaces. No stable-branch edits.

**CI result reported by user (2026-10-23):** User inspected the TocPilot GitHub Actions Build page for `dev` after the latest changes and reported **“all pass”**. This is user-observed green Windows build/CI, not an independently fetched exact run ID or job log: the connector's commit-workflow lookup is PR-filtered and returns no push-triggered runs. The exact run SHA, number of CTest cases, and detailed build logs have not been independently obtained. Do not report the tests as locally executed. **Next gate is Windows UI runtime review**, plus inspection of actual reported job logs if needed; stable `main` remains untouched.

**Validation truth:** A deterministic `ui-theme-visual-policy` CTest target was added for exact colours, precedence (error > persisted ignore > attention > actual-session change > already-good), hover derivation and unchanged selected fill. **Not executed** in this session: Windows CMake/MSVC compile, CTest (historical 26/26 applies to v0.5.1, not this delta), Windows DPI, Compact/Advanced, text/icon toolbar, high-contrast visual, keyboard or Account Sync runtime tests. GitHub connector `fetch_commit_workflow_runs` exposes only PR-triggered runs; its empty result for `dev` cannot establish whether a push-triggered build ran or passed. Combined statuses likewise empty; do not mark CI green without real run evidence. The newly registered test would make the suite 27 tests if baseline tests remain unchanged.

**Open first-slice defects/debt to address before claiming completion:**
- `Ignored` and `Error` row colours exist in the central model, but the current `PackageRecord` exposes no safe persisted ignore flag or per-package error outcome. The present adapter wires actual attention and confirmed session updates only; do **not** manufacture error/ignore flags or label a read-only refresh green. Connect these two states to authoritative operation/persistence events when available, without changing package semantics.
- Some legacy literal RGB/GDI colours outside the touched paths remain (other dialogs, Account Sync tiles, text toolbar controls, headers, non-client title bar, etc.). Central theme adoption for these is migration debt. High-contrast fallback must be visually verified; any incorrect selected-row, default control, hover repaint or branch overlay must be fixed before publication.
- Drawn glyphs use package `target` (`addons`, `dll`/`dlls`, `mpq`/`mpqs`) without mutating records. Verify actual durable target spellings and test package-kind mapping; inspect glyph legibility at scaled DPI.
- Toolbar surface and Account Sync area are painted using existing layout geometry; word-mode/system controls still require consistency and contrast validation. No title-bar theming or popup replacement has been attempted.

**Exact next actions:** (1) Verify the latest `dev` head and the corresponding **push-triggered** GitHub Actions Build run (not just PR-filtered runs). (2) Run Windows x64 Release configure/build and CTest, inspect failures and fix them, especially the new Win32 theme header and full-row custom draw. (3) Add package-kind and state-adapter tests with truth-backed ignore/error inputs only, inspect high-contrast/hover/selected/focus interactions and grey toolbar/Account Sync/word-mode surfaces. (4) Repeat the Compact/Advanced, 100/125/150%-DPI, scaling, link/branch, keyboard, resize/sort, and Account Sync no-regression matrix. (5) Once the foundation passes compile/CI and real Windows visual validation, move to row-first routine status/results. **Do not begin popup replacement or publish to `main` yet.**

---


## v0.6.0 Win32 interface audit and visual/feedback design — 2026-10-23

### Agreed fixed colour and interaction specification — 2026-10-23

**Central palette source-of-truth decision (user confirmed 2026-10-23):** Define **every UI colour in one shared theme module**, e.g. `src/ui_theme.h` with a single palette/type API used by the main package rows, toolbar, Account Sync, Info, Add Git and all future screens. **Never duplicate literal RGB values throughout feature/UI implementations**: editing one base colour must propagate everywhere that semantic colour is used. Base row colours (Already Good/Good Now/Ignored/Needs Attention/Error) are centrally specified by exact agreed RGB/hex values above. **Derive hover shades automatically and consistently from each base status colour** using one central colour transformation rather than maintaining independent per-row hover constants (the previously agreed hover hexes are reference targets for the chosen subtle lift, not independently edited colours). Selected rows retain their base fill with the separate centrally defined 2px inset selection border; hovering while selected uses the calculated hover fill plus the same border. Define all other neutral chrome, text, focus, divider and button state colours centrally as well. Keep semantic mapping and dynamic result truth separate from palette/rendering, and retain high-contrast accessibility overrides. Any local hardcoded colour surviving the first slice must be documented migration debt, not silently treated as fully themed.

**User approved the exact colours and interaction semantics below. Do not reinterpret them as suggestions in implementation.** No coding done yet. This is the visual contract for the first v0.6.0 slice.

**Window neutrals:** outer frame/background `#1F2125`; title bar `#2B2E33`; top toolbar `#3A3D42`; package-list background `#2A2D31`; lower Account Sync background `#3A3D42`; neutral buttons `#4A4E54`, button hover `#5A5F66`, button pressed `#676D75`; primary text `#F1F3F5`, secondary text `#C1C6CD`; subtle dividers `#555A61`. Top grey strip is the toolbar; bottom region is **Account Sync**, not AI-concept giant Sync/Refresh buttons. Keep the original playful Warcraft reference artwork in splash/branding rather than behind working rows.

**Package row states (exact palette):**

| Semantic state | Base fill | Hover fill | Meaning |
|---|---|---|---|
| **Already Good** | `#3F6F94` | `#4D7FA6` | Already current; no action needed |
| **Good Now** | `#4F8A57` | `#5E9B67` | Actually updated/repaired **this session** |
| **Ignored** | `#5A5E64` | `#686D74` | Deliberately ignored/disabled update |
| **Needs Attention** | `#B7842E` | `#C7933C` | Update available or approval/review needed |
| **Error** | `#A14F4F` | `#B56060` | Actual failure/block |

**Selection/focus (user correction):** Base status fill **does not lighten when selected**. `Normal=base`; `Hover=hover fill`; `Selected=base + 2px inset border #C1C6CD`; `Selected+hover=hover fill + the same inset border`. Keyboard focus must remain distinct and accessible (visible focus indicator in addition to selection). The previous brighter `Selected` colour column is **rejected**, remove from code/spec. Preserve row size on selection, no shifts/reflow. Use short statuses and icons as redundant cues, never only colour.

**Distinct neutral package-kind glyphs:** Addon = **cube/package**, DLL = **microchip/component**, MPQ = **archive/crate**. Shape identifies package kind; fill colour identifies state. The shared existing `toolbar_icons.cpp`/Font Awesome mechanisms are potential implementation tools; verify available glyphs and bundled assets rather than assume a particular font glyph exists.

**Begin with a non-destructive proof of visual foundation on `dev`, not a wholesale interaction rewrite.** First PR/slice: shared central colour/style tokens, row state/kind mapping and accessible hover/selection painting in the **existing** main package list, plus top grey toolbar surface/neutral chrome. Preserve current ListView data, sorting, branches, update action state, scrollbar/resizing, compact/advanced layout, icon-vs-word settings, DPI/text scale and all existing Account Sync behavior. Treat animated working/download states as later integration after understanding existing event outputs. Add deterministic tests for state precedence (error > attention > good-now > already-good, with ignored according to existing persisted ignore policy), package kind, and selection/hover visual decision model without tying state to UI paints. **Do not treat successful refresh as an update**; green must mean an actual confirmed change. Avoid visual-only dummy code that cannot be shipped. Subsequent slice replaces popups with genuine row-based details/approvals while preserving fail-safes. CI and Windows runtime feedback gate before continuing.

**Updated user design decision — 2026-10-23: ROW-FIRST.** The user wants the **entire interaction model to be visual, colour- and row-based**, not a series of toast/banner notifications, cards or lengthy dialogue boxes. This supersedes the initial audit's transient banner/toast proposal. In the package table, each package's own row shows progress/result/action; in Account Sync, keep the existing colour/arrow/tick semantics and render details/approvals in expandable rows or a focused inline inspector; in Add Git, Info/update, and scans, show compact action/result rows in-place. Represent `Idle` neutrally, `Working` with subtle animated blue/progress, `Needs review` amber, `Applied successfully` green **only after verified changes**, and `Failed/blocked` red; successful checks with no write must not masquerade as completed writes. **Colour cannot be the only signal:** include icon, succinct status text, keyboard focus and screen-reader status. Only current important items expand; technical detail/provenance/copy affordances remain available without polluting every row. No sounds for normal status/errors. A destructive or trusted operation becomes a clearly marked **inline approval row** with distinct `Confirm/Cancel` choices and a safe Cancel default; do not autoapprove, time out, or weaken security/backup constraints. Show blocking failures persistently in-row; if the application cannot safely proceed (especially before the UI is available), retain an explicit silent fallback dialog or fatal screen. Keep a modest in-app Activity/History view for resolved messages. Treat design details as proposed until the user approves mockups and runtime tests.

**Status: SOURCE AUDIT COMPLETE; DESIGN/IMPLEMENTATION NOT STARTED.** User direction: TocPilot should get a **coherent clean native-Win32 skin**, with the **verbose Windows popup message boxes and alert sounds eliminated as a primary v0.6.0 usability requirement**, alongside the previously agreed application-wide wording/visual overhaul. User explicitly prioritises reducing interruption and visual datedness, without sacrificing important confirmation or safety semantics. Treat this audit as an implementation plan for user review, **not** runtime-validated UI behaviour or approval to change underlying installer/Account Sync logic. Stable `v0.5.1` on `main` is protected, and `dev` currently contains only post-stable documentation changes.

### Source audit: current interaction surfaces

- Main application: `src/main.cpp` (~12,967 lines), live Compact/Advanced toolbar and ListView; 11 toolbar actions; configurable icon/word modes; GDI `DrawFrameControl(DFC_BUTTON)` icon-button framing; current Windows ListView report with custom-draw colours/links/branch; old text-toolbar code present within `#if 0` and **not live**. Existing content-derived Compact minima, two-column transactional resize, viewport preservation, DPI and toolbar modes are tested/accepted constraints—retain all.
- Notifications/interruptions: source-counted **69 direct `MessageBoxW` call sites in `src/main.cpp`**, **2** in `add_package_dialog.cpp`, **3** in `branch_dialog.cpp`, **1** in `repository_selection_dialog.cpp`, **1** in `account_sync_tiles.h`, plus **1** fallback `MessageBoxW` in `ui_dialog.cpp`: **77 source occurrences across these audited files** (not an execution frequency count). In addition, **8 main.cpp call sites** invoke `tp::ShowExpandableDialog` in `src/ui_dialog.cpp`, which normally calls `TaskDialogIndirect` with an information/warning/error icon and falls back to `MessageBoxW`. Most native message boxes use `MB_ICONERROR`, `MB_ICONWARNING`, `MB_ICONINFORMATION` or `MB_ICONQUESTION`. No direct `PlaySound` or `MessageBeep` calls found in these inspected UI sources. The audible interruptions are consistent with system sounds generated by icon-bearing native dialogs; confirm exact OS behaviour in real Windows tests.
- Concrete unnecessary blocking examples: `RunAccountSyncFromUi` shows a detailed OK popup after manual successful Sync; managed DLL install, addon package install/removal, scan/adoption completion and row-detail inspection also produce acknowledgement dialogs. Preserve tile outcome semantics (green/tick status only for actual writes), detailed provenance and one-click inspection, but put ordinary feedback inline, in a quiet banner or Activity panel instead of a mandatory OK popup.
- High-stakes decisions currently in popup code: pfUI Settings / macros / keybindings replacement and backup acknowledgement; `Uninstall` versus `Remove` tracking+file differences; ownership-transfer replacement and unmanaged addon collision refusal; DLL publisher trust with exact asset/destination/no-backup/antivirus caveats; MPQ removal and backup/rollback states; opt-in development channel; abort WoW launch when Sync-before-Launch fails. **Do not remove or silently autoapprove any decision, trust boundary, conflict block, backup/write fail-safe or explicit launch block.** Render these as silent, focused themed confirmation sheets with safe default Cancel, concise top-level consequence, expandable exact paths/details and clear action labels.
- Additional windows/screens: Info/update channel panel (`src/main.cpp`) has `Check for Updates` → `Update Available` two-step button semantics and a `Receive development builds` checkbox. This caused real release-version confusion; distinguish **checking** from **installing**, show friendly stable/preview labels while retaining exact `vX.Y.Z[-dev.N]` tags for technical details and comparison. `add_package_dialog.cpp` (Add Git URL/Scan), `branch_dialog.cpp` (branch chooser), `repository_selection_dialog.cpp` (branch and multiple content/asset selection), `library_dialog.cpp` (library picker; runtime unused by this user), Account Sync config+accounts+GDI tiles, and special graphical splash `splash.cpp` need a coherent shared theme—not disconnected restyles.
- Styling opportunities: `ApplyUiFont` already uses DPI-scaled Segoe UI with configurable text scale. `toolbar_icons.cpp` already uses bundled Font Awesome glyph assets and disabled variants; `src/account_sync_tiles.h` already owner-paints semantic tiles; `splash.cpp` uses GDI+ artwork. These are reusable foundations, but default common-control frames, group boxes, fixed dialog geometry and colour/spacing decisions are fragmented. The project is C++20/Win32/Windows-x64 portable; **no Qt, WebView, managed runtime or CSS wrapper merely for a skin**.

### v0.6.0 design contract / implementation order

1. **Define one shared native design system before changing screens:** theme-aware surfaces/text/dividers, single restrained accent plus semantically distinct success/warning/error colours, consistent 4/8 px spacing rhythm, button sizes/states, Segoe UI type hierarchy, accessible focus/disabled/hover styles, icons and DPI-aware component measurements. Prefer modern neutral low-chrome appearance; support light/dark/system settings if feasible without degrading high contrast. No arbitrary paint overlay on top of mismatched system controls; build styled native controls/custom draw as necessary. Accessibility: keyboard operation, visible focus, Escape/Enter, tab ordering, contrast, text scale, screen-reader-friendly control names, native high contrast fallback.
2. **Central row/feedback/confirmation API ahead of mass replacements:** Model `Idle`, `Working`, `CompletedWrite`, `NoChange`, `ReviewRequired`, `RecoverableError`, `BlockedCritical`, `ApprovalRequired` as UI states with distinct visual semantics. Render in the **affected row** wherever possible. Non-package operations (Git scan, app update, launch) use compact, persistent activity rows in their respective panels. Success requires verified results, not mere selection, comparison colour, or forecast; cache, settings, rollback and old/new status rules are unchanged. Show a terse label, progress/state mark, an action when applicable, and expand-on-demand details/copy. Maintain an optional accessible Activity/History listing for past outcomes. **Do not use transient toast/banner messages as the primary feedback model** and don't invoke Windows warning beeps.
3. **Replace interruption families systematically through row states:** ordinary successes/statuses first (installed, updated, removal, adoption, Sync, details), then recoverable errors, then destructive approval/trust/ownership decisions. Approvals must remain explicit and gated; use an expanded action row or integrated quiet confirmation region with labelled Confirm and Cancel (default safe), full details and keyboard focus. A failed Sync-before-Launch must still **prevent WoW launch** and display a persistent failure row. Serious startup failures before UI initialises may require a minimal quiet blocking fallback. UI event handlers must continue to await actual user consent, not simulate acceptance or route around security. Add tests for state transitions, modal-vs-inline safety and action result mapping. Remove routine `MessageBoxW` and icon-bearing `TaskDialogIndirect` sounds without deleting diagnostics.
4. **Polish the main shell:** matching soft/flat native toolbar controls in Compact and Advanced; coherent icons/text labels/tooltip affordances, hierarchy/spacing, application-level status/activity entry point and subdued status badges. Keep launch control location and meaning, user-saved icon/word choice, package semantic colours, selected-row keyboard behaviours, exact compact width/column/scroll/resize contracts and DPI handling.
5. **Restyle workflows consistently:** Info/version-update flow, Add Git URL → branch/contents selection, per-package actions, Account Sync settings and tiles, errors/confirmations; leave GDI+ splash recognisable, only adjust branding/theme where needed. Account Sync arrows/colours/grouping/Refresh and Sync button workflows already runtime accepted and should not be redesigned functionally.
6. **Wording rules:** each row needs a compact readable action/state (ideally a short phrase), meaningful colour and icon, plus at most one next-step action; technical paragraphs/path lists/stack traces live under expandable Details. Examples (illustrative): `3 addons updated` [Details], `Sync couldn't finish — WoW wasn't launched` [Details] [Retry], `Replace pfUI settings on 1 account?` [Cancel] [Replace settings], `Stable 0.6.0 available — Install`, `Preview build 5` (exact SemVer identifier remains inspectable). Use genuine operation counts/status; never claim a write succeeded from an in-memory projection alone.
7. **Validation gates:** focused source-level unit tests for notice classification, primary/safe-default actions, keyboard/Escape/cancel and long-details handling; manual Windows visual matrix across Compact/Advanced, icon/word mode, text scaling, 100%/125%/150% DPI, dark/light/high contrast, screens with no/many packages, multiple account widths, window resize/scroll persistence, updates/failures, ownership collision, approval declined, rollback errors, and silent/no-beep outcome. Runtime-test through a development prerelease and real updater before stable; never publish a mockup as implemented.

**Suggested delivery slices:** 0.6.0-A shared theme and row-state feedback foundation; B migrate routine statuses/results into rows; C replace blocking confirmations/fatal paths with safety checks; D main toolbar/list/window design; E Info/Add Git/Account Sync/other dialog consistency and complete copy sweep; F Windows UI acceptance and final release. Prefer coherent meaningful slices over repeated tiny patches/releases, especially to avoid regressions in the accepted sync engine. **Next user/design decision:** choose the visual direction (system-matching clean light/dark versus a specific dark/light palette), then implement shared foundation plus representative notification/control proof before migrating all call sites.

---

## Stable v0.5.1 user-machine executable verified — 2026-10-21

**PASS (identity only):** User's Info window displays `TocPilot v0.5.1`, with `Receive development builds` unchecked and the update UI reporting "Installed v0.5.1 is the latest release." The user then ran `(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash` on the running TocPilot executable and supplied **`EC510C5C996ADF32B078390D5CB648731A4A895727FCAB1A03240A88F478BFBE`**, an **exact match** for the published stable `v0.5.1` executable digest. The user is genuinely running the official stable binary, not a prerelease mislabeled as stable, and GitHub's stable release metadata and update status are consistent.

**Auto-upgrade mechanism NOT established:** User had initially reported no observed restart or upgrade from `v0.5.1-dev.5`, then suggested they may have already swapped executables. This remains possible but unconfirmed; the matching stable binary alone cannot prove automatic installation or a failure. Treat the user-machine **dev-to-stable updater transition as unverified**, not failed, and do not claim the two-stage check/install flow was exercised. The actual `v0.5.1-dev.5` source has SemVer rules correctly ranking stable `v0.5.1` above matching dev.5; its Info update UI checks on first click and installs on the subsequent `Update Available` click. Automated tests cover version policy but not this exact user's update sequence. **No new code fix indicated.** Future UX can clarify the distinction between Check for Updates and Update Available; v0.6.0 visual/wording pass remains the agreed scope.

---

## Stable v0.5.1 published — 2026-10-20

**Status: PUBLISHED STABLE / EXACT-HEAD RELEASE GATE PASSED.** User explicitly approved v0.5.1 promotion. Release-preparation PR #48 merged to `dev` as `f40d8dd6b275e44b6a5c7418d8369f7d3bf2d226`; exact candidate head `7b0b40208ac6d4bb6af786d83c8d234f69294e6e` passed Windows x64 Release build + **26/26 CTest** in Build run `39454815844`. Main's two exclusive preexisting v0.5.0 documentation commits were reconciled into `dev` with two-parent history-preserving merge `c6650e0e2f4075901a14df326ebf80a672cebea7` without changing source trees; the current dev document already contained v0.5.0's exact tag/release/checksum history. Promotion PR #49 passed its exact head Windows Release + **26/26 CTest** (Build run `39455757928`), and merged with history preserved at **`main` release/source commit `b17584edccae3ec2d2c8debd2b7016dc79fd0cd3`**. Main-push Build run `39456668978` passed **26/26 CTest**.

**Stable Release workflow:** Run `39456668993` completed successfully on the exact `main` release commit, including stable version validation, Windows x64 Release compilation, **26/26 CTest**, SHA-256 sidecar generation, tag creation and direct published assets. Tag `v0.5.1` resolves to `b17584edccae3ec2d2c8debd2b7016dc79fd0cd3`. GitHub release `https://github.com/Seraphic8x2244/TocPilot/releases/tag/v0.5.1` is non-draft and non-prerelease; GitHub `/releases/latest` resolves to `v0.5.1` at that same commit. Assets: `TocPilot.exe` — **3,319,808 bytes**, SHA-256 **`ec510c5c996adf32b078390d5cb648731a4a895727fcab1a03240a88f478bfbe`**; `TocPilot.exe.sha256` — 78 bytes, published. App header, CMake and stable release marker all identify `0.5.1` / `v0.5.1`.

**Accepted Windows runtime evidence carried into stable:** `v0.5.1-dev.5` passed the user's two-account pfUI Settings/Cache Sync, Refresh and clear status/outcome presentation; Sync-before-Launch with a pending Cache merge, and again with nothing to sync; reinstalling managed `AuctionQueryThrottle.dll` appended its absent loader entry to `dlls.txt`. Automated tests cover 3+/content-group/mixed-cache projections, stale-plan and backup safety; no claim of live 3+ Windows acceptance. Real MPQ lifecycle and library/multi-content repository installation have **not** been runtime-tested because the user has no applicable Solocraft assets; they are consciously deferred and remain unadvertised. Other unexercised live error/console cases remain named debt. Whole-app native Win32 styling, controls and wording cleanup are explicitly assigned to **v0.6.0**, not required for v0.5.1.

**Remaining post-publication acceptance:** The stable Release workflow and GitHub public release metadata are verified. **User-machine stable executable verified on 2026-10-21 by exact SHA-256 match to the published `v0.5.1` EXE.** The user-machine automatic development-to-stable updater transition remains **unverified** because the user might have manually replaced the executable; no updater defect has been established. Do not infer the auto-upgrade worked from the hash alone. Next substantial feature development is `v0.6.0` UI/UX modernisation by separate agreement; no work on that feature has started.

---

## Stable v0.5.1 release preparation — 2026-10-20

**User approval:** On 2026-10-20, the user explicitly authorised preparing and promoting TocPilot `v0.5.1` to `main` subject to passing final release checks. Latest runtime-accepted functional candidate `v0.5.1-dev.5` is exact tag/source `a2ac05662c955d84305be8172b56a22578d82fc1`; the development release passed its independent Windows x64 Release/26 CTest, published EXE/SHA-256 metadata checks and development-channel self-update from `v0.5.0`. Subsequent `dev` commits through `283268cd8117ab6fc8c46ac9a7e3b6d565162c41` document live acceptance and deferrals and do **not** modify runtime code. Windows runtime PASS: two-account Account Sync pfUI Settings/Cache Sync+Refresh; Sync-before-Launch with a pending Cache merge and again with nothing pending; automatic addition of `AuctionQueryThrottle.dll` to `dlls.txt` on managed reinstall. Automated Account Sync covers 3+/matching content groups, pfUI merge contributors, missing files and stale/backup-failure boundaries; these are **not** claims of 3+ live Windows runtime verification.

**Explicit non-blocking validation debt:** No actual MPQ assets or library/multi-content repositories are used in the user's Solocraft setup, so MPQ lifecycle/mixed installs, library/multi-content flows remain untested in Windows runtime, despite automated tests. Do not invent fixtures or advertise them as validated. Remaining Ctrl-launch/induced sync-error live cases are not proven but covered partially by automated tests. Whole-app visual design and Phase 7 wording modernisation are deferred to **v0.6.0**. Do not undertake cosmetic refactoring during stable release.

**Stable release candidate:** `release-v0.5.1` branch created from verified `dev` HEAD `283268cd8117ab6fc8c46ac9a7e3b6d565162c41`. Release preparation changes **only** `src/version.h` (`0.5.1`/`v0.5.1` without dev suffix), `.github/release-version` (`v0.5.1`) and this development record; CMake already specifies `0.5.1`. Do not change implementation. Next: open PR to `dev`; require successful Windows x64 Release build and all CTest at the exact PR head, then merge into `dev`. Prepare separate promotion PR from `dev` into `main`, reconcile `main`'s older docs-only stable history if needed without losing content, rerun exact-head CI, merge with history preserved, and require the real stable Release workflow to publish `v0.5.1` from the exact `main` commit with EXE, SHA256, tag, metadata and stable-channel latest checks. Do not claim stable published before observing these.

---

## Agreed v0.5.1 closeout / v0.6.0 roadmap — 2026-10-20

**Repository-library / multi-content Windows runtime explicitly deferred (2026-10-20):** User confirms they do not use addon libraries or multi-content GitHub repositories in their current setup, so there is no genuine installation fixture. Do not create artificial repositories or alter the working Solocraft install solely to test them. Preserve this as **runtime-untested / non-blocking** feature-validation debt, distinct from source/automated tests (`repository-candidate-discovery`, `repository-selection-policy`, `repository-mixed-install-policy`) that passed the `v0.5.1-dev.5` 26/26 Windows CTest gate. Together with the separately deferred MPQ paths, this does **not** imply runtime acceptance for these specialized workflows and should not be marketed as verified.

**v0.5.1 closeout decision point:** Real Windows runtime passed for two-account pfUI Sync/Refresh, Sync-before-Launch with both pending Cache and no pending changes, and direct-DLL auto-registration after a user-initiated reinstall. Automated Windows 26/26 CTest, development prerelease publication/checksums and development-channel self-update passed for `v0.5.1-dev.5`. Unexercised 3+ account Windows cases, profile-approval-at-launch, induced write-failure and console-launch flows remain separate optional coverage; protect real settings, do not claim them tested, and prioritize regression evidence over risky manual scenarios. No new defect is currently reported from accepted user-tested paths. **Next ask the user whether to begin stable release preparation**; only after explicit approval may TocPilot be promoted to `main` through the usual PR and exact-head CI gates. Reconcile stable version macros/CMake/release marker, verify complete Windows tests at the final candidate, publish exact `v0.5.1` stable assets with SHA-256 and check latest stable update metadata. Preserve deferred `v0.6.0` visual and wording overhaul.

**MPQ real-world runtime validation explicitly deferred (2026-10-20):** The user's current Solocraft installation has no TocPilot-managed MPQs and no actual MPQ asset they want to manage. **Do not install an artificial MPQ solely for validation**, do not infer MPQ install/update/remove/collision paths passed in Windows runtime, and do not hold the unrelated `v0.5.1` Account Sync/DLL functional closeout for these unadvertised paths. The `mpq-package-backend` and `repository-mixed-install-policy` tests passed in the `v0.5.1-dev.5` 26/26 Windows CTest gate; this is automated evidence only. Preserve MPQ lifecycle and mixed-repository runtime validation as explicitly deferred debt until a suitable real-world installation or a separately agreed test fixture exists. **Next:** check whether there is a genuine repository-library/multi-content repo to exercise, otherwise defer that independently, then review the remaining release-critical gates for `v0.5.1` stable promotion (requires explicit approval).

**Direct DLL loader registration Windows runtime PASS (2026-10-20; v0.5.1-dev.5):** The user deliberately removed the `AuctionQueryThrottle.dll` entry from the WoW-root `dlls.txt`, then used TocPilot to reinstall the already-managed DLL. TocPilot **automatically added `AuctionQueryThrottle.dll` back to `dlls.txt`**, confirming the missing-entry registration path in a real Windows installation. This is direct user runtime evidence for the non-destructive loader-entry feature introduced in `v0.5.0-dev.3` and inherited by `v0.5.1-dev.5`. The user has not separately confirmed byte-for-byte preservation of unrelated lines, no-duplicate behaviour or UTF-16 refusal at runtime; all are covered by the green `direct-dll-policy` automated tests. Do not repeat the missing-entry reinstall scenario without cause. **Next:** assess user-feasible MPQ and repository-library/multi-content runtime paths; if no suitable real fixture is available, explicitly defer these unadvertised features instead of risking a WoW installation or blocking the otherwise accepted Account Sync and DLL closeout.

**Sync-before-Launch Windows runtime PASSED — both scenarios (2026-10-20; v0.5.1-dev.5):** The user has long had `Sync before Launch` enabled. In the first live test, pfUI Cache needed synchronising across the user's two accounts; clicking the TocPilot WoW launch button synchronised the pending Cache change and launched WoW normally immediately after, without manual Sync. In the second test the user explicitly clicked Refresh to clear the green success ticks, then launched WoW while there was no pending operation. No new completion icons appeared, no apparent launch hesitation occurred, and WoW launched normally. **Both active-merge-and-launch and no-op-and-launch workflows are accepted from direct Windows observation.** No evidence yet for Settings approval while launching, intentional write failure/abort during launch, optional Ctrl-launch console behavior, or 3+ live account configurations. Do **not** repeat these two accepted paths as outstanding. Next focus on remaining distinct gates and decide with user which are practical; do not make unsafe test modifications to real WoW configuration.

**Decision:** Ship `v0.5.1` as a functional, reliability-focused update. The user has runtime-accepted the compact Account Sync interface as usable even though the application's overall styling is dated. **Do not start a piecemeal Account Sync visual redesign or hold this release for cosmetic polish.** Focus on validating existing functionality and fixing only proven regressions. Retain native Win32, the existing authoritative Account Sync engine, freshness rechecks, confirmations, account selection, pfUI Settings-vs-Cache semantics, backup-before-write and outcome status rules.

**Verified development release evidence:** `v0.5.1-dev.5` is published as a non-draft prerelease at source/tag commit `a2ac05662c955d84305be8172b56a22578d82fc1` (2026-10-15). Development Release workflow run `38662846493` completed successfully, including source-version check, Windows x64 Release build and tests, checksum asset publication, published-channel metadata verification and **real development-channel self-update**. Concurrent Build workflow `38662846527` passed. Published assets: `TocPilot.exe` (3,320,320 bytes, SHA-256 `3c4e437082f6624c776183237bfa0aa9cc252159b96892d78f7646fd0cf37015`) and `TocPilot.exe.sha256`; last stable remains `v0.5.0` at `1173a91918db134939b65c0a37891abce35dfb37`. The user's 2026-10-19 two-account pfUI sync, post-sync green/tick outcomes and Refresh runtime successes are documented below. A documentation-only subsequent Build run `39421597011` also passed; this does not itself constitute a new released runtime version.

**v0.5.1 remaining verification before requesting stable approval:** Review which safety/multi-account cases are already exercised by automated Account Sync tests (including 3+/identical source groups, independent pfUI cache merge contributions, stale-source/changed-target rejection and backup/write failure), and add/fix tests only for demonstrated gaps. Check feasible Windows runtime paths together: Sync-before-Launch and representative multi-account scenarios where real accounts are available, plus any previously deferred direct DLL `dlls.txt`, MPQ lifecycle/mixed-repository and repository-library/multi-content paths the user can reasonably exercise. Preserve the explicit not-yet-validated status of unadvertised MPQ/library paths; do not silently call them tested or impose artificial live-account creation. Triage any failure before patching. Recheck exact final CI/release version, published assets and updater gate before proposing promotion; **never modify `main` or publish stable without the user's explicit go-ahead**.

**v0.6.0 is the agreed coherent Win32 UI/UX modernisation milestone:** Consolidate a visual design system (toolbar icon consistency, icon sizing, typography, spacing, borders/panels, button/control states, selection and semantic status colours); revisit the large-toolbar and floating Account Sync presentation across the *whole app* instead of isolated patches; review labels, commands, workflow discoverability, dialog/error language and the historical Phase 7A/7B wording backlog. Prefer lightweight native Win32 styling; do not replace the UI stack with a heavyweight framework without a separate case and approval. Maintain useful Account Sync green/blue/amber semantics and arrows. All visual/wording overhaul work is **deferred to v0.6.0** unless a wording issue makes v0.5.1 objectively incorrect or unsafe.

**Next joint action:** Audit existing tests against the v0.5.1 closeout list, then guide the user through one small, safe Windows test at a time, marking each result here. When the remaining critical gates are resolved or explicitly deferred, request approval to promote `v0.5.1` to `main` and publish stable. Do not automatically begin v0.6.0 implementation.

---

## Current checkpoint — 2026-10-20

**Account Sync v0.5.1-dev.5 — 2026-10-19 Windows runtime gate (EXERCISED PATHS PASSED):** The user exercised the actual Windows app after updating pfUI and reported success. On the tested account pair, a changed pfUI source was detected; per-account Settings and Cache states, intended directional arrows and necessary confirmation were displayed as expected; the safeguarded Sync action completed, and the post-sync view showed all relevant rows green with completion ticks and no further planned changes. Refresh re-inspected read-only, cleared the transient success ticks, and retained green satisfied statuses. The compact 150-logical-px tiles no longer carry the redundant `Match N` / `Merge N input(s)` labels; the permanent left-column Refresh and Sync controls were available and used. These findings supersede the 2026-10-15 notes below claiming this UI has no release or runtime testing. This is user-observed **success for the exercised Windows paths**, not proof of every multi-account or induced-error edge case.

**Repository/release checkpoint:** Verified remote `dev` HEAD before this documentation edit: `a2ac05662c955d84305be8172b56a22578d82fc1` (commit subject: Trigger `v0.5.1-dev.5` development release for compact Account Sync UI). `.github/development-release-version` contains `v0.5.1-dev.5`; the user ran the resulting version. `main` remains the stable `v0.5.0` baseline and has not been authorised for Account Sync promotion. No sync-engine redesign or change to pfUI profile/cache semantics is implied by the UI runtime acceptance. Do not infer full workflow/CI, checksum verification or updater E2E purely from user runtime observations; verify the relevant workflow records before asserting those gates separately.

**Remaining validation/development debt:** The two-account successful sync/Refresh flow is accepted for continued development. Separately verify 3+ accounts and matching-source groups, mixed-source pfUI Cache contributions, any unexercised partial-write/backup-failure, stale-plan rejection under concurrent file changes, Sync-before-Launch, and other induced-error paths before claiming comprehensive runtime coverage. Keep freshness revalidation, confirmation, backup-before-overwrite, existing account selection/config persistence and original sync engine unchanged. No `main` promotion without explicit approval. Next chat should start from this checkpoint rather than reimplementing or re-testing already accepted successful paths without a reason.

---

> Live TocPilot development context for a fresh chat. Keep this current and concise. Git carries chronology; this file carries the current product contract and next work.

**Account Sync compact tile refinement (2026-10-15; SOURCE IMPLEMENTED, WINDOWS CI PENDING):** Following the user’s first Windows runtime review of `v0.5.1-dev.4`, simplify the account tile presentation: remove redundant `Match N` and `Merge N input(s)` status labels (and other unnecessary repeated action text) from the tile faces. Retain green/blue/amber/black semantic row text colours, actual planned directional arrows, confirmation `!` markers, and completion/failure markers, with full source/contributor, timestamps, and operation explanations available on row click. Narrow the fixed-width tiles accordingly while preserving aligned baselines, horizontal scrolling, and correct multi-account operation routing. Add a permanent **left-side vertical action column** containing **Refresh** (read-only re-inspection; no writes) and **Sync** (invoke the existing safeguarded Sync Now path with its confirmations, freshness checks and backup-before-overwrite). Do not add further buttons without a demonstrated need. Keep account selection, Sync-before-Launch and persistence unchanged. Source implementation commits `e9d66923db513eae485a15497bc8fe46df1257c6`, `3b5b431da3a59ab4f368182eaa7713dea58fd9bf`, and `14632e53f87535d034786f574176410421e48c09` remove redundant tile operation/match counters, reduce tile width from 212 to 150 logical px and gap from 56 to 44 px, and add main-panel Refresh/Sync buttons wired to existing read-only inspection and safeguarded sync paths. No sync-engine changes. Await Windows Build/CTest and user visual validation; no new release has been published for this UI refinement. Recorded from verified `dev` HEAD `dcceb08f01c4aa55860bd368cec08fb95ef97bba`.

**Slice 3 release candidate (2026-10-15):** Windows Build #730 (`38654739081`) passed on exact source commit `0138b5dcc6160fb8f233851adc6c15a265c525dc`, validating the semantic Settings and Cache details and regression assertions. A `v0.5.1-dev.4` development prerelease is being prepared on `dev`; CI pass on the source candidate is not yet proof the release workflow, published checksum or real self-update passed. Remaining runtime validation includes visual arrow/dialog comparison and user-observed multi-account partial failure cases. No changes to `main`. Verify Development Release workflow results, published EXE/SHA metadata and updater E2E before calling Slice 3 published.**

**Slice 3 substantive pfUI detail implementation (2026-10-15; NOT YET VALIDATED):** Starting remote `dev` HEAD `18dcb0ed80a82ae018959ed57f922fc5bffd6656` verified unchanged. Commits `6fa06aeb4888c14af9f7b3d074283b677099d64b`, `e85e3ccb291c03b98d4b4e2fbf4e3b82f0ca8211`, `7f490bdea6fcbd88931ac293268382334e6084c9` add per-account read-only semantic detail entries from the existing parsed pfUI documents: top-level Settings/profile keys and each of the 3 supported Cache merge sections, with actual peer-specific differing entry names in the tile details rather than generic comparison statements. Commit `33ed355c04bb4941561a7ba169a350208fdfbeae` tests semantic entries, section names, divergent fixtures and missing files. No changes to write engine. Windows Build run `38654687257` queued for `33ed355` at last check; no compile/test claim yet. Source signatures remain display-only, not an authorisation to write. Outstanding final confirmation/arrow parity and partial-write coverage; green CI on final candidate, dev version/release, checksum and updater E2E. `main` unchanged.**

**Slice 3 further continuation (2026-10-15; INCOMPLETE):** Verified `dev` remained at `03b70630455eb19d43cd744b38e75118ef94a03d`. Commits `7fae010283d8d86a143b5353e73f7a88de302999` and `3681c4294d79b0166a6aadcacc591f010bbd28b0` clarify pfUI read-only details (semantic Settings comparison, supported Cache contributions/automatic existing-file merge and approval-dependent missing creation) and assert merge-vs-copy/confirmation projections in tests. These are limited clarification/parity regressions, NOT the outstanding concrete key-level semantic diff feature. Build run `38654205164` for `7fae010` remained in progress at last observation; newer `3681c42` not CI checked. Earlier green builds `38652269208` and `38652317573` still apply only to old commits. No release/version bump/updater verification. Next: implement actual per-account semantic differences and remaining partial-failure/staleness regressions, get green full Windows CI on final candidate, then publish/verify a development release. `main` untouched.**

**Slice 3 follow-up (2026-10-15; INCOMPLETE):** Verified `dev` HEAD `d2e83e970e272151863b5f3672fdb87d3b2024bd` exactly matched expected (no divergent commits). Windows Build runs `38652269208` (commit `9449a54130389ead0986864717961892490264b0`) and `38652317573` (commit `d2e83e970e272151863b5f3672fdb87d3b2024bd`) both completed successfully. Commit `d8b8e1678e44e1b0ca83a0d13d5742d5412452bc` adds assertions that backup failure yields a failed destination outcome rather than success, and stale-source rejection never yields a success outcome. These latest assertions have NOT been CI-verified. Existing pfUI Settings/Cache detail comparisons remain generic rather than showing concrete semantic differences, and operation/dialog parity plus comprehensive partial-write regressions are outstanding. No dev version bump, release, executable/hash verification, or updater E2E; no `main` modifications. Next: complete detail/parity/regressions; validate this newer commit with full Windows Release/CTest; only then version and publish real Development Release.**

**Slice 3 continuation (2026-10-15; INCOMPLETE):** Verified starting `dev` HEAD exactly `66522a885e1c24df46d2bf90dc81ce6d5988abe4`. Run `38651017252`: Windows x64 Release compiled; 25/26 CTest passed; the only failure, `self-update-live-latest`, encountered a refused/rate-limited GitHub API call and is not demonstrated to be an Account Sync defect. Commits `1643a80f8a2ee0d3829bca074d203810ef9ac410` and `9449a54130389ead0986864717961892490264b0` clarify pfUI confirmation: it approves Settings/profile replacement, while existing-file Cache merges are independent and missing-file creation requires approval. The sync engine is unchanged. CI runs `38652250403` and `38652269208` were in progress/queued when checked; not yet validated. Remaining: concrete per-account pfUI semantic differences, strict operation/dialog parity, remaining failure/staleness/backup regressions, green Windows CI, version bump and genuine Development Release plus assets/metadata/updater E2E. No release or `main` change.

## Account Sync UI — fixed-width account tiles (Slices 1–2 implemented; Windows runtime pending)

Replace the current Account Sync comparison/status table with horizontally arranged, fixed-width **account tiles**. Each selected account gets one tile; the layout remains consistent for two, three, or more accounts. Retain the current window width and use a **horizontal scrollbar** when the selected tiles do not fit. Avoid switching to a different UI model for 3+ accounts.

Tile hierarchy (aligned across all tiles):

```text
[Account name]
Keybinds
Macros
pfUI                 <- neutral, inactive section heading
  Settings           <- actionable pfUI profile/settings comparison
  Cache              <- actionable supported cache merge comparison
```

**Visual semantics**

- File/status **text colour**, on the existing white background: **green = good / the applicable sync rule is satisfied**, **blue = newer**, **amber = older**, **black = default/neutral/unclassified**. Green is *not* merely a timestamp match; it means the relevant macro, keybinding, pfUI Settings or supported Cache sync criteria are already satisfied. Contents, not just timestamps, determine equality for macros/keybindings.
- The **pfUI** parent is black/neutral, not clickable and carries no operation icon. Only **Settings** and **Cache** display state and sync operation indicators.
- **Arrows, ticks, and warning exclamation marks are reserved for actual operations/outcomes**, never used as labels for accounts or files. An arrow between aligned tiles denotes the direction of an actual proposed copy/merge; a tick indicates an operation completed successfully; an exclamation denotes a needed confirmation/attention. No arrow for an unchanged/satisfied row. Do not confuse older/newer *state* with the direction of an operation.
- Timestamps remain visible via an item-details selection and in confirmation, clearly labelled per account to seconds; sorting/decisions retain native full-precision timestamps.
- Do not repeat redundant `No changes` action text next to a healthy status.

**Multiple accounts and transfer planning**

- Show all selected accounts as peers; do not restrict Account Sync to pairs. If one account has new macros while the other two already match, show only necessary changes. If a middle account has newer pfUI Settings, it can supply both neighbours. If two accounts already share the same newest keybinding content, they are one logically matching source group: update the older/differing account **once**, not by cascading unnecessary copies across both matching sources.
- Compute and display the actual operation plan, respecting existing newest-source selection/confirmation rules for macros/keybindings and the established **pfUI_profiles source selection + supported cache merge/conflict-resolution** semantics. Splitting pfUI into two visible subrows is strictly presentational; don't replace cache merging with whole-file newest-wins copying.
- Preview and confirmation must re-inspect and revalidate relevant files immediately before approval/commit; any changed source, target, direction or planned changes requires fresh confirmation, with backup-before-overwrite retained.
- Keep account selection, Sync Now, Sync-before-Launch and existing configuration persistence; redesign only the presentation/interaction and the minimum required plan-to-UI projection.

**Status / next work:** **Slices 1 and 2 are implemented and Windows CI-checked on `dev`; new Win32 tiles remain runtime-untested. Slice 3 is next** in a fresh ordinary Chat conversation, not Work mode. Do not begin Slices 3–4 automatically. Every slice must read `dev_rulebook.md` and `DEV_PROGRESS.md`, verify remote `dev` HEAD, and preserve all Account Sync safeguards; do not modify `main` without explicit approval.

### Account Sync tile redesign — multi-chat slice plan

**Slice 1 — UI comparison/operation projection only (Chat 1; complete)**

- Expose a read-only, per-account/per-row presentation model from the existing authoritative Account Sync analysis; **no parallel synchronisation engine** or changed file-write logic.
- Rows: Keybinds, Macros, pfUI Settings, pfUI Cache; pfUI itself is a neutral heading. Include per-account rule-satisfied/newer/older/default status and explicit intended transfer or merge actions.
- Model multi-account cases (2, 3, and more), including matching-source groups, single necessary target writes, missing files, and pfUI's independently merged cache surfaces.
- Add focused automated tests confirming the model matches actual planned operations, timestamp/content-based decisions, profile-vs-cache semantics and no-op cases.
- Do not replace the existing table or publish a prerelease in this slice.
- Finish with static checks/CI where accessible, coherent `dev` commit, `DEV_PROGRESS.md` update stating exact results and outstanding debt, and a **copy-ready handoff prompt** for Chat 2.
- **Result (2026-10-14):** PR #45 squash-merged to `dev` at `45ffb40e9818b3d8e1166449546309118e45d7df`. `src/account_sync.h` adds four typed tile rows, per-account full-precision observed timestamps/content groups, states and explicit conditional operations. `src/account_sync.cpp` projects exact-byte macro/keybinding decisions directly from `AnalyzeItem`, and semantic `pfUI_profiles`/supported cache decisions from `BuildPfUiSyncPlan` (including multi-source cache inputs, existing-file automatic cache merges and missing-file creation conditional on profile confirmation). Content-identical and equal-timestamp divergent cases remain non-copying; single differing targets receive only one copy operation per item. `tests/account_sync_tests.cpp` adds deterministic 2-, 3- and 5-account coverage, projection/preview parity, no-write inspection, pfUI automatic merge under declined Settings confirmation and no-op/missing cases. Sync execution functions, backup/file writes, confirmation and Win32 UI are unchanged; no version bump or release. Source candidate `798f28c9b8d1dbf252dd5151d281bb1f13b06da4` passed Windows x64 Release build and **25/25 CTest** (including `account-sync-safety`) in Build run `38545342978`, job `115789883436`. An earlier CI compile failure caused by a new `CopyFile` enum identifier colliding with Win32 `CopyFileW` was corrected to `CopyWholeFile` before the green candidate. PR #45 merge is source-equivalent to the green candidate, with no additional runtime changes. **CI checked, not runtime tested; no tiles yet.** The UI and user-led Windows validation remain Slices 2–4.

**Slice 2 — Native account tile UI only (Chat 2; complete)**

- Replace the existing Account Sync summary/table presentation with fixed-width horizontal account tiles in the agreed window, using horizontal scrolling rather than forcing window resizing. Keep tile widths and row baselines aligned at 2, 3 and larger account counts.
- Neutral, inactive parent `pfUI`; indented and individually styled Settings/Cache subrows; Keybinds/ Macros as top-level rows.
- Use green/blue/amber/black **text** statuses; arrows between tiles, with ticks/exclamations only for actual operations/outcomes; no redundant “No changes” text. Maintain usability on the existing white background.
- Consume Slice 1's model. Preserve account selection, Sync Now, Sync-before-Launch, persistence, existing preview and confirmation safety.
- Layout/static/Windows CI checks with explicit note that visual Windows runtime checks remain pending. No prerelease until Slice 3.
- Finish with a coherent `dev` commit, updated `DEV_PROGRESS.md` with exact verification results, and a **copy-ready handoff prompt** for Chat 3.
- **Result (2026-10-14):** PR #46 squash-merged to `dev` at implementation commit `21d12c4b237dd4e68a7d30ca6619eb208cd56d19`. The exact source-equivalent green CI candidate is `24dc3de0ffd6a60a94e1d1af75c39bc17aefdd1f`: Build workflow run `38564992147`, Windows x64 job `115855280580`, **Release build and all 26/26 CTest passed** (including new `account-sync-tile-layout`); artifact upload passed but was **not released**. `src/account_sync_tiles.h` provides native white-background Win32 owner-painted account tiles, fixed DPI-scaled width/row baselines, horizontal scrollbar and wheel movement, source-labelled target-gutter arrows for real proposed operations and `!` only where approval is needed, with no artificial intermediate transfers. It consumes `InspectAccountSyncTiles` only; file/data rule states are green (satisfied), blue (newer), amber (older), black (neutral); the pfUI parent remains neutral/inactive, while indented Settings and Cache are separate. Matching groups show `Match N`; copy operations identify the exact selected source and other equivalent source candidates; supported Cache operations remain labelled merges rather than newest-copy. `src/account_sync_tile_layout.h` and `tests/account_sync_tile_layout_tests.cpp` cover 2/3/5/12 peers, DPI, width/scroll alignment, direct target markers, overflow and minimum-height layout. `src/main.cpp` replaces only the prior three-row ListView and its read-only preview: account selection, Sync Now, Sync-before-Launch, state persistence, `RunAccountSync`, confirmations, freshness revalidation, backup-before-write and file writes remain under the original engine. Minimum package-list height becomes 120 logical pixels **only when the tile panel is displayed**, avoiding clipping at the existing minimum window size without widening it. The first validation run `38564160584` exposed MSVC-only `LONG`/int and Win32 `SCROLLINFO` spelling compilation errors; both were fixed before the final green candidate. The intermediate `38564860311` candidate was superseded. **No version bump, release, `main` changes, visual runtime check, item-details interaction or outcome tick-state implementation in Slice 2.** Slice 3 owns per-row details, confirmation/operation reconciliation, richer outcome presentation and development prerelease publishing.
- **Next-chat prompt template:** “Continue `Seraphic8x2244/TocPilot` on `dev` in ordinary Chat mode (not Work mode). Read `dev_rulebook.md` and `DEV_PROGRESS.md`, verify remote HEAD and Slices 1–2. Implement Account Sync tile redesign Slice 3 only: selectable per-row details with account-labelled second-resolution local timestamps, confirmation dialogue consistent with the transfer arrows, refresh after sync, stale-plan revalidation and backup-before-overwrite regression coverage. Do not change actual pfUI cache/profile semantics or replace the sync engine. Run full Windows Release build and CTest; fix proven defects; bump development-prerelease version correctly, publish through the real development-release workflow, verify executable/checksum, release metadata and updater E2E. Commit/update `DEV_PROGRESS.md`; give a copy-ready Chat 4 runtime handoff. Do not touch `main`.”

**Slice 3 implementation checkpoint (2026-10-14; INCOMPLETE):** Started directly on shared `dev` from verified HEAD `4d39dbd892a3bcafb7eaa81e78576c094f2b4c90`. Source commits `9af87878e30867b5aee122046e8ade11e0f71b35`, `33ec005d4fd10052965a436d67b3917e23d17dd4`, `1f2a25f28f3728330d039e70951535876757075c`, `7c16270d4c086c7a9a502996ea1e475e570e424f`, `77f27d3b5493b35f262841e966fd79ca102d04f3`, `55a40d7047395f3e5a023e7629bd45f91e8bca1f` introduce clickable account tile rows with read-only inspection details (account, full local date/time to seconds, per-peer comparison, selected source or merge contributors, target/operation, content group, file presence, confirmation need) and engine-derived post-sync item outcomes with a fresh tile inspection. The original file-write engine and confirmation callback are not changed. **Not yet done:** full detail differences, confirmation/visual-plan parity review, stale-plan/backup regression tests, actual Windows Release CI/CTest, version bump, development prerelease publication, artifact/hash/metadata verification and updater E2E. Browser-based GitHub connector does not expose workflow dispatch; container cannot access GitHub network. No CI claim or runtime-testing invitation. Do not treat this partial checkpoint as Slice 3 completion. 

**Slice 3 continuation checkpoint (2026-10-14; INCOMPLETE / no prerelease):** Starting `dev` HEAD `dd3e998c4988cdf87891e0e53d3e2a197c47bc92` matched the expected SHA exactly. Subsequent shared-`dev` commits `a957dd86ada9305af6c40d8c5da982d4dc393c9f`, `640f9e1da56c5caea58f12d5481d2047340d3c20`, `465369fb8c82cc37ad4224cccacef8ae450472e5` replace live timestamp rereads in tile selection details with inspection-snapshot formatting, guard account lookups, and distinguish exact-byte macro/keybinding comparison from pfUI settings/cache comparison labels; `e37e4167245ac7cfb4231b484b6058ee69e9d272` adds a macro/keybinding regression that edits source bytes during approval while restoring the exact native modified time, asserting the write is blocked and no backup is created. This is **source implemented only**: MSVC Windows compilation, complete CTest, full per-account outcome reporting, strict arrow/dialog parity, real detailed pfUI semantic differences, release version bump, development-release assets/checksum/metadata and updater E2E are **NOT verified or completed**. The GitHub connector in this session has GitHub source write/read tools but no workflow-dispatch or create-release tools; development-release workflow is push-triggered by `.github/development-release-version`, but triggering publication before implementation/test completion is inappropriate. Do not begin Chat 4 runtime tests or treat this checkpoint as a published build. Continue Slice 3 only; preserve `main` and transaction safeguards.

**Slice 3 incremental source checkpoint (2026-10-14; NOT CI-verified / NOT published):** From verified initial `dev` HEAD `4afb931e58627057f0e1d1d0202ea0216c217efa`, committed account+row write-outcome plumbing in `src/account_sync.h`, `src/account_sync.cpp`, `src/main.cpp`, `src/account_sync_tiles.h` and regression assertions in `tests/account_sync_tests.cpp`. The engine records successful/failed per-destination macro/keybinding copies, successful/failed independently marked pfUI Settings and supported Cache writes, and declined confirmation targets. The UI refreshes inspection separately and overlays an outcome tick/attention marker only when the engine recorded an actual attempt; no-op is not success. Tests added for two successful macro destinations and zero outcome events for content-identical files. **This is unbuilt and unverified:** no Windows MSVC compilation or full CTest, no strict confirmation-versus-arrow parity validation, no comprehensive per-row difference reporting, no version bump, no release, no self-update E2E or user runtime validation. The existing Build workflow triggers on main pushes and PRs, not direct dev pushes; the Development Release workflow triggers by version-file change but should not be used until completion/build validation. Preserve main; next finish parity/differences and regression cases, validate Windows CI, only then prepare/publish a genuine dev release and verify assets/metadata/updater before Chat 4.

**Slice 3 interim continuation (2026-10-14; STILL INCOMPLETE):** Verified exact starting `dev` HEAD `12f4a19308c2a875adf8a0556cc9ae6fd02a2de8`, read rulebook and this progress file. Commit `c99ed43fa4ef3ac6380cb32425da3d610fe8f1e3` makes declined confirmation outcomes visible as attention markers on refreshed tiles and includes recorded per-row write/decline/failure information in selection details, explicitly separating write history from current filesystem comparison. Commit `90d568a9f247f6e33f54ee3c9c8f21fb428b694a` adds exact-account declined Settings and Keybindings outcome assertions. Draft PR #47 (`dev` -> `main`, **validation-only, do not merge**) was opened to request Windows x64 Release / complete CTest, but CI result has **not** been observed. No MSVC or CTest pass may yet be claimed. Still outstanding: meaningful per-peer pfUI semantic differences rather than generic text; full confirmation/arrow parity review, multi-account partial-write failures and backup-before-overwrite regressions, green Windows CI, version bump, genuine development release and executable/checksum/update E2E verification. **Do not publish or promote yet**; main unchanged.

**Slice 3 — Interaction, safety regression and prerelease (Chat 3)**

- Selected item shows details with actual source/destination accounts, timestamps to seconds, differences and planned writes/merges (never infer source merely from left/right position).
- Synchronisation confirmation reflects the visual operation plan; refresh account tiles after outcomes; stale changes require new confirmation, never reversed silently.
- Preserve existing content comparison, native-precision ordering, pfUI profile/cache merge rules, confirmations and backup-before-overwrite; extend tests where needed.
- Run Windows x64 Release and complete CTest, correct defects, bump the `-dev.N` candidate/version consistently, **publish a real prerelease via the GitHub Development Release workflow** and verify `TocPilot.exe`, `TocPilot.exe.sha256`, metadata and self-update validation. Do not present a CI artifact as a release.
- End with exact release tag/commit, CI results, outstanding runtime debt in `DEV_PROGRESS.md`, plus a **copy-ready handoff prompt** for Chat 4.
- **Next-chat prompt template:** “Continue `Seraphic8x2244/TocPilot` Account Sync tile redesign in ordinary Chat mode (not Work mode). Read `dev_rulebook.md` and `DEV_PROGRESS.md`, verify `dev` HEAD and the published prerelease/CI evidence. Guide runtime tests one at a time on the installed TocPilot development release using `blackwaves` and `blackwavestwo`, optionally a third account: all-matching no-op, macro/keybinding older/newer cases, missing files, multi-target arrows, pfUI Settings vs Cache, confirmations and backups, changed files while confirmation is open, and horizontal scrolling/alignment. Distinguish passed, failed and untested; only fix demonstrated defects in focused commits, publish updated prereleases for retesting as needed, update `DEV_PROGRESS.md`, and do not promote to `main` without explicit approval.”

**Chat 4 — user-led Windows runtime validation (separate chat)**

- Begin from the **published** prerelease, not an ordinary CI artifact; verify actual installed version and dev-channel updater behaviour.
- Test with real accounts and an optional third account, one short step at a time. Record precise pass/fail/untested outcomes, especially 3+ account arrows, cache semantics, coloured text, confirmation safety and horizontal scroll positioning.
- Patch only demonstrated defects; publish subsequent prereleases where necessary before asking for retests. No silent promotion to `main`.
- Finish with a clear accepted/remaining-debt checkpoint in `DEV_PROGRESS.md` and a fresh-chat resume prompt if further work remains.

**Scope guard:** This is three implementation chats **plus one runtime-testing chat**, not four implementation slices. A chat is complete only after a committed `DEV_PROGRESS.md` checkpoint and a copy-ready next-chat prompt; handoffs must stay in Chat mode and must not invoke Work mode.


## Current

- 2026-10-14 Account Sync tile redesign **Slice 2 implemented / Windows CI-checked / Windows runtime-untested**: `dev` implementation `21d12c4b237dd4e68a7d30ca6619eb208cd56d19` via PR #46, CI source `24dc3de0ffd6a60a94e1d1af75c39bc17aefdd1f`, run `38564992147` Windows Release and **26/26 CTest green**, including native tile layout regressions. No release or version bump; next bounded task is Slice 3 in a separate ordinary Chat.
- 2026-10-14 Account Sync tile redesign **Slice 1 is implemented and CI-checked on `dev`**, merge `45ffb40e9818b3d8e1166449546309118e45d7df` via PR #45. Exact code/test candidate `798f28c9b8d1dbf252dd5151d281bb1f13b06da4` passed Windows x64 Release and **25/25 CTest** in run `38545342978`, job `115789883436`. Read-only comparison model and focused tests only; no UI, sync engine changes, release/version bump or Windows runtime confirmation. **Next: Slice 2 native account tiles in a fresh Chat-mode conversation.**
- Latest published development prerelease remains `v0.5.1-dev.3`, published 2026-10-11 at source `eea5bc6e993e54b0b0120d2377a27dd6a4d61a94`. Latest stable is unchanged (`v0.5.0` at `1173a91918db134939b65c0a37891abce35dfb37`).

- 2026-10-11 `v0.5.1-dev.2` CI run `38151452564`: Release build and 25/25 CTest passed; prerelease assets/checksum and metadata verification passed, but final self-update E2E test failed because bootstrap used HEAD^, which already contained the same version tag. This was a workflow test-fixture error, not evidence of an updater failure. Fix at `81b00ce8bfd9320a67bf4a72bd359248dcd15a7b`: bootstrap from published stable `v0.5.0` instead. Prepare `v0.5.1-dev.3` as a new prerelease candidate so the corrected workflow can validate the real stable -> development update transition. No new Account Sync runtime observations yet.

- 2026-10-11 CI failure triage: `v0.5.1-dev.1` Development Release run `38133618890` compiled successfully, but 24/25 CTest passed; `account-sync-safety` expected a backup after its test callback deleted the source during confirmation. The new intended freshness invariant correctly rejects the changed source **before backup**, so the stale test assertion was corrected at `47d7b8472b4657b9646d3744c8b012d8407c2779`. `v0.5.1-dev.1` was never published. Next candidate `v0.5.1-dev.2` is to be validated and published only if the Windows workflow passes; runtime still pending.

- 2026-10-11: Prepared Account Sync reliability test prerelease `v0.5.1-dev.1` (source version `0.5.1`); development release workflow triggered by a change to `.github/development-release-version` must build Windows x64 Release, pass all CTest tests, publish checksum-verified assets and exercise development-channel self-update. Until workflow success and release assets are independently confirmed, this is **not yet published or CI-accepted**. Starting baseline `v0.5.0` stable or a previous dev-enabled build; user must have **Receive development builds** enabled. `main` remains unchanged.

- 2026-10-10 Account Sync reliability follow-up implemented on `dev` through `11400fc8a23daa78eea7048a2a1f0b2e1f380a04` (**source implemented, Windows CI and runtime NOT YET VERIFIED**). Macros/keybindings inspect and compare exact file bytes; identical copies are `Already synced`, never queued for confirmation, backup, or overwrite. Different content retains the existing native-precision newest-source rule and older-target confirmation policy. Preview and confirmation now show per-account local modified times including seconds, without rounding comparison timestamps. Before confirmation and after approval, pending plans are checked against fresh contents and timestamps; snapshots are rechecked before each destination write, accounting for TocPilot's own earlier writes in multi-target jobs. pfUI's profile comparison/cache merge is unchanged; its underlying file snapshots are also guarded from modification between inspection, approval, and writes. Changed snapshots abort rather than silently changing the approved direction. Targeted regression tests added for identical macros/bindings and destination modifications during a dialog. No release/version bump or main changes in this slice. **Next:** run Windows Release build and CTest, fix any demonstrated defects, validate runtime with blackwaves/blackwavestwo, then select a new development prerelease version for normal self-update testing. In particular test multi-target sync, pfUI cache updates, and source/destination changes with the confirmation dialog open.

- Active branch: `dev`.
- Source/application version on `dev`: `v0.5.1-dev.3` (base `0.5.1`; unchanged in Account Sync Slice 1).
- Latest published stable release: `v0.5.0` at exact main/release commit `1173a91918db134939b65c0a37891abce35dfb37`.
- Latest published development prerelease before stable promotion: `v0.5.0-dev.3` at `e471aad66a64ddeb3f1458f2800edc3542b5521e`. GitHub stable `/releases/latest` now resolves to `v0.5.0`.
- Stable release/source commit and tag target: `1173a91918db134939b65c0a37891abce35dfb37` (`v0.5.0`).
- Current stable product baseline is `v0.5.0` at `1173a91918db134939b65c0a37891abce35dfb37`, released with explicit non-blocking validation debt for wording, MPQ runtime paths, repository-library/multi-content runtime paths, and direct-DLL `dlls.txt` runtime confirmation. Account Sync is accepted for the runtime paths exercised on 2026-09-30, with explicit non-blocking validation debt for Sync-before-Launch/Ctrl-click and induced backup/write-failure. The previously accepted v0.3.22 toolbar/DPI behaviour remains inherited; slight softness at Windows 125% scaling remains accepted as normal fractional-DPI rendering.
- Last known-good Account-Sync-free code baseline is published/runtime-accepted `v0.3.22` at `8678334c0a0015eba14aecf58f7c416c045f6581`. The final pre-Account-Sync repository commit is documentation-only `6fcf117cf4f639612827acf88b10c6933f4b7802`, so its executable source is equivalent to that accepted v0.3.22 baseline.
- Account Sync recovery checkpoint `2e80b1378a7a01bb311140828e4e54069b1576d6` passed Build workflow run `36440593954`, Windows x64 job `108989739617`, with **19/19 CTest tests**, including `account-sync-safety`. That recovery line is now superseded by published stable `v0.4.0`; keep the checkpoint only as provenance.
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
- Package-list viewport reset slice: **implemented / CI-checked / published in `v0.4.1-dev.1` / runtime-accepted for forward development on 2026-10-01**. Implementation commit: `eace26841fdd41073413605bfb4f5f3c578b3da0`; prerelease commit: `325523b05c34c59d6e15fe56cd152d9cfafb5fdf`. Runtime passed the exercised paths: single-addon branch/Inspect/Reinstall actions do not move the viewport; Refresh All resets at start/finish; Update New resets correctly; selection/sorting/Compact/Advanced presentation does not reset the viewport. Multi-package Add Git remains explicit non-blocking runtime debt because no suitable multi-addon repo was available. The package list rebuilds rows in place instead of clearing the control, `RefreshPackageStateUi()` no longer restores a semantic top row or calls `ListView_EnsureVisible`, and retained logical selection is restored without scrolling.
- Viewport CI: draft validation PR #34 targeted the exact implementation commit and was closed without merge after Build run `36775185023`, Windows x64 job `110091188659`, passed the Release build and **20/20 CTest tests**, including `package-list-viewport-policy`.
- Startup scan auto-enter slice: **implemented / CI-checked on `dev` / not yet released / runtime-untested** at implementation commit `b539b593900ccc046baffeffdcdf19e9a57a8ffb`. Normal startup completion after addon update scanning now reveals the main window and destroys the splash immediately; the old normal `AwaitingContinue` / “Click to continue!” state was removed. The separate `AppUpdateFailed` acknowledgement remains intentionally interactive so a failed self-update is still surfaced before entering the app. Draft validation PR #35 was closed unmerged after Build run `36893192930`, Windows x64 job `110473610222`, passed the Release build and **20/20 CTest tests**.
- `v0.5.0` runtime matrix is **in progress**. Accepted so far: startup auto-enter, existing addon update and package-list viewport behaviour from the dev.1 gate; on `v0.5.0-dev.2`, root-level `Seraphic8x2244/Performante` Add Git passed through default-branch discovery -> Contents -> `dev` branch switch -> root install, and the user explicitly preferred the streamlined flow; an ordinary single-addon repository on its default branch also passed the direct fast path with Contents correctly skipped. Still pending for suitable real fixtures: multi-content branch switching, addon-library install, DLL update, MPQ-only lifecycle, addon+MPQ, addon+DLL+MPQ, MPQ collision handling and independent branch/release movement. A direct-DLL follow-up found on 2026-10-09 is now published in `v0.5.0-dev.3`: successful managed DLL installs ensure the exact DLL filename is present in root `dlls.txt` without rewriting existing bytes, preserve existing comments/spacing/order, avoid case-insensitive duplicates, and refuse UTF-16 input rather than corrupting it. That new `dlls.txt` behaviour is runtime-pending.
- Current scope boundary for the Account Sync tile series: **Slices 1–2 implemented and CI checked; Slice 3 interaction/confirmation/regression + actual development prerelease next**, in a separate ordinary Chat conversation. Do not start Slice 4/runtime testing until Slice 3's published prerelease is verified. Do not modify `main`; previous accepted Account Sync, package viewport and toolbar/DPI paths remain inherited, and unrelated v0.5.1 work is deferred.
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
- GitHub latest-stable exact standalone MPQ release assets;
- TocPilot's own GitHub Release self-update.

Branch packages store/compare the selected remote commit SHA and create no local Git repository.

Exact release-asset packages track the release policy, exact asset and persistent destination needed to update deterministically.

Private repositories/tokens and self-hosted GitLab remain deferred.

### Repository Classification

Add Git discovery is intentionally shallow and deterministic.

After removing the provider archive wrapper, branch-addon discovery inspects only:

- direct `.toc` files in repository root;
- direct `.toc` files in each immediate child directory.

Branch classification remains:

1. root `.toc` -> root addon;
2. no root `.toc` + one immediate child addon -> single nested addon;
3. no root `.toc` + multiple immediate child addons -> repository library with selectable children;
4. root addon plus immediate-child addon roots -> mixed/ambiguous; do not guess between overlapping addon roots.

For GitHub, latest-stable release discovery runs independently of branch-addon discovery and contributes every valid exact standalone `.dll` and `.mpq` asset to the same candidate result. A successful category does not suppress another category, and one category failing does not erase valid candidates from the other. GitLab remains branch-addon-only; GitLab release support is not implied.

Do not recursively search arbitrary repository depth to classify libraries. Once a specific addon root is selected, its subtree can be copied/validated normally.

Repository-library siblings and release assets are independent selectable install units unless explicit dependency work is added later.

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

### Development channel / branch model — Slice 1 branch transition complete

TocPilot now has enough external/stable use that experimental builds must be isolated from normal users.

Branch contract after the transition is complete:

- `main` = runtime-accepted / release-ready code;
- `dev` = active development and runtime-test candidates;
- ordinary feature/fix work lands on `dev`, passes CI, is runtime-tested there, then is merged/promoted to `main` for a stable release;
- do not force-rewrite existing public history merely to establish this model.

One-time transition:

- Account Sync recovery already landed on `main` before this policy was introduced. Do **not** reset or rewrite `main` to hide it.
- `dev` was created from `main` at `5e5fa28e3047d14ec2f7fb73b7de1246fbe995a9`; continue new development there.
- `v0.3.22` at `8678334c0a0015eba14aecf58f7c416c045f6581` remains the published/runtime-accepted stable baseline until the next accepted stable release.
- Once the current dev work is runtime-accepted and promoted, enforce `main` = accepted/release-ready going forward.

Update-channel UI:

- Add an opt-in checkbox in **Info -> Updates** named **Receive development builds**.
- Default is **off**, including for existing installations.
- The setting changes only TocPilot's own update channel; it must not weaken addon/state/transaction validation or enable unrelated developer/debug behavior.
- Persist it locally in `TocPilot.json` as an additive backward-compatible setting.
- First enable should show a concise warning/confirmation that development builds may contain unfinished changes.
- A development build must identify itself clearly in the displayed version, e.g. `v0.3.23-dev.1`.

Release/channel policy:

- Stable releases keep normal tags such as `v0.3.23`.
- Development builds use GitHub **prereleases** with ordered tags such as `v0.3.23-dev.1`, `v0.3.23-dev.2`, etc.
- Stable TocPilot must continue using the stable-only release path. Current code already calls GitHub's `/releases/latest` through `FetchLatestStableGitHubRelease()` and explicitly rejects `draft` or `prerelease` metadata, so stable users must never receive a dev build unless they deliberately opt in.
- With **Receive development builds** enabled, TocPilot should consider both stable releases and matching TocPilot prereleases, selecting the newest valid version according to proper semantic/prerelease ordering.
- Turning the option back off returns the installation to the stable channel. If the running build is a prerelease, the updater must be able to move to the appropriate current stable release even where the stable tag has the same base major/minor/patch.
- Development releases must retain the same executable-size, HTTPS, SHA-256 and rollback protections as stable self-update.

Version-ordering requirement:

- The current `ParseSemVer` / `IsNewer` logic stops at `-` / `+` and compares only major/minor/patch. It therefore cannot distinguish `v0.3.23-dev.1` from `v0.3.23-dev.2`, and would also treat `v0.3.23-dev.N` and stable `v0.3.23` as the same base version.
- Do **not** bolt the dev channel onto that comparison unchanged. Add deterministic prerelease-aware ordering and tests for dev-to-dev advancement, stable-over-dev promotion at the same base version, stable users ignoring prereleases, and channel-off behavior from a currently running dev build.

Binary/state contract:

- Use the normal `TocPilot.exe` for both channels; do not create a permanently separate `TocPilot-dev.exe` product path. The goal is to test the same executable/update mechanics that stable users eventually receive.
- Continue sharing the real `TocPilot.json`; do not silently fork normal dev testing into a separate state file.
- A dev build must not write state that the current stable build cannot safely reopen. Add/retain compatibility coverage for stable/pre-feature state -> dev and dev-written additive state -> stable-safe reopen/unknown-field preservation where applicable.
- Because `v0.3.22` does not yet contain the channel selector, the first channel-capable tester build may require one manual/Actions-artifact bootstrap install. After that bootstrap, normal dev testing should use TocPilot's real prerelease self-update path.

Promotion flow:

```text
dev
 |
 +-- CI
 +-- v0.3.23-dev.1 (GitHub prerelease)
 +-- runtime test
 +-- fixes / v0.3.23-dev.2 ...
 |
 +-- accepted
      |
      v
    main
      |
      +-- v0.3.23 stable
```

Other stable users remain on the stable-only channel throughout unless they explicitly enable development builds themselves.

#### Four-slice implementation plan

Because long/tool-heavy chats have repeatedly become unstable, implement the development-channel work as four deliberately bounded slices. **Do not start the next slice in the same chat unless the user explicitly asks to continue.** Each completed slice should end with its own commit, relevant CI/check result, a `DEV_PROGRESS.md` checkpoint, and an exact next step for a fresh chat.

**Slice 1 — branch transition only — COMPLETE**
- Create `dev` from the current `main` head.
- Verify both `main` and `dev` refs.
- Update this document on `dev` so `dev` is recorded as the active development branch and the transition is complete.
- No source, UI, updater or workflow changes in this slice.

**Slice 2 — setting + Info UI — COMPLETE**
- Add additive persisted `receive_development_builds = false` state.
- Preserve backward compatibility with existing/pre-feature `TocPilot.json`.
- Add **Info -> Updates -> Receive development builds**.
- Default off; show the agreed first-enable warning/confirmation.
- Do not change updater release-selection behaviour yet.
- Add focused persistence/old-state compatibility tests.
- Implemented on `dev` as an additive schema-1 setting. Pre-feature state defaults off; enabled state round-trips; invalid values are rejected; existing unknown-field preservation remains intact. The Info window now has an **Updates** group with **Receive development builds** and the first-enable warning/confirmation. Updater release discovery/selection is unchanged in this slice.

**Slice 3 — updater channel logic — COMPLETE**
- Preserve the existing stable-only release path unchanged for users who have not opted in.
- Add prerelease discovery for opted-in users.
- Implement deterministic prerelease-aware ordering, including `v0.3.23-dev.1 < v0.3.23-dev.2 < v0.3.23`.
- Support turning development builds off while currently running a prerelease and returning to the appropriate stable release.
- Keep existing HTTPS, exact-size, SHA-256 and rollback protections.
- Add deterministic channel/version tests before moving on.
- Implemented with non-opted users still using `FetchLatestStableGitHubRelease()` / GitHub `/releases/latest`. Opted-in users enumerate published releases, accept stable releases plus matching `-dev.N` prereleases, and select by prerelease-aware semantic ordering. Stable outranks a same-base prerelease. Turning development builds off while running a prerelease offers the current stable release even when returning to stable requires moving to a lower base version. Existing HTTPS, exact-size, SHA-256 and updater rollback/handoff protections are unchanged. Focused tests cover release-list parsing, stable filtering, draft/non-dev prerelease filtering, dev-to-dev ordering, stable-over-dev ordering and channel-off behavior.

**Slice 4 — development release workflow — COMPLETE**
- Add/adjust the release workflow needed to publish GitHub prereleases for dev builds.
- Produce the first channel-capable prerelease, expected form `v0.3.23-dev.1`.
- A one-time manual/Actions-artifact bootstrap install is acceptable because stable `v0.3.22` cannot yet opt into prereleases.
- After bootstrap, validate TocPilot updating itself through the real development channel.
- Only after this infrastructure is accepted should the Account Sync runtime matrix resume.
- Implemented as a separate dev-only `.github/workflows/development-release.yml`; the existing stable `.github/workflows/release.yml` is unchanged. `.github/development-release-version` drives dev-branch prerelease publication and source/tag validation requires `vX.Y.Z-dev.N`.
- First prerelease `v0.3.23-dev.1` was published from `518a9a1f1407b4a62ca4b09c11522f8859425b61` by Development Release run `36588659393`, Windows job `109475655788`. Release build and **19/19 CTest tests** passed; required EXE/checksum assets were published as a non-draft prerelease; the workflow verified stable `/releases/latest` still returned `v0.3.22`.
- The same workflow then built parent `cce53f8f2b2fd4d97289f497744a4c7949510adf` as the one-time channel-capable bootstrap, seeded `receive_development_builds=true` in a disposable WoW-root fixture, launched TocPilot normally, and verified the installed `TocPilot.exe` was replaced byte-for-byte with the published `v0.3.23-dev.1` asset while preserving the opted-in state. This is the accepted real development-channel self-update gate for Slice 4.

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
- Update New follows the general bulk/whole-list viewport-reset contract below.

Advanced column widths/order are persisted. The dedicated Lock Columns UI/behaviour is removed in v0.3.6. Starting with v0.3.11, persistence enforces the two-column invariant: `Name` and `Status` are always the first two columns and may only swap with each other; Advanced-only columns stay to their right. `Name`/`Status` widths are one shared pair of values across Compact and Advanced, so resizing in either mode updates what the other mode inherits. Advanced-only widths/order remain Advanced-specific. The legacy JSON `package_columns_locked` field is retained for state compatibility but no longer controls the UI.

Branch-selector target behaviour after the v0.3.6 runtime follow-up:

- a branch package whose remote repository advertises more than one branch should always show the Branch-cell arrow once that repository's branch metadata is known;
- a repository advertising exactly one branch should never show an actionable arrow or dropdown;
- branch affordance must be based on per-repository/package metadata rather than only the currently selected row;
- changing a branch may re-sort an addon to the update-available group, but the selection/branch interaction must remain anchored to that package identity after the list rebuild;
- remote branch lists are transient remote metadata and should not be duplicated into durable package state merely for UI rendering;
- Add Git already fetches the full Git smart-HTTP branch advertisement in the branch chooser; reuse that result to seed runtime metadata rather than immediately fetching it again;
- normal branch-head refresh also fetches the full advertisement internally, so retain/reuse that information to discover newly added/removed remote branches during ordinary startup/Refresh All checks without an extra branch-list request.

### Package-list viewport reset — locked design

Goal: make list movement predictable and intentionally simple. The previous attempt to preserve the visible top package / selected-package viewport across repopulate and re-sort is removed rather than repaired.

Authoritative behaviour:

- **Single-addon action:** do **not** move the package-list viewport at start or finish. This remains true even if the implementation internally routes the action through queue machinery; effective work scope of one package means no viewport reset.
- **Bulk / multi-addon action:** scroll the package list to the top when the operation **starts**, then scroll it to the top again when the operation **finishes**.
- **Whole-list operations** such as Refresh All / Update All follow the same start + finish reset rule, whether or not the implementation uses the same queue object as package updates.
- The finish reset occurs after the final list rebuild/re-sort so the user sees the completed result from the top.
- The start reset occurs before bulk/whole-list work begins so progress begins from a consistent visible position.
- Remove the old preservation/restoration behaviour for prior scroll position, previous top-row package identity, and selection-driven viewport restoration during bulk/whole-list rebuilds.
- Do not layer the new reset rule on top of the old preservation path; there must be one authoritative viewport behaviour.
- Existing logical package selection may remain where needed by surrounding UI behaviour, but bulk/whole-list completion must **not** scroll that selection back into view after the explicit top reset.
- Purely presentational controls that do not start package work or rebuild/re-sort the package list are outside this rule and must not reset the viewport.

Implementation direction:

- identify the narrow shared boundaries for operation start and operation completion rather than scattering scroll-to-top calls through individual package handlers;
- make scope explicit enough to distinguish one-package work from multi-package/whole-list work even when both share queue infrastructure;
- strip the old `RefreshPackageStateUi()` top/selection viewport anchoring logic that causes the arbitrary mid-list jump;
- preserve single-addon locality: targeted install/update/reinstall/remove work should leave the user's current list position untouched;
- add focused automated coverage for: single-package no movement, multi-package start reset, multi-package finish reset, whole-list start reset, whole-list finish reset, and no selection-driven re-scroll after a bulk finish reset.
### Account Sync — agreed design / implementation recovery

Goal: retire the user's current `wtf_sync.bat` by moving its account-data synchronization and launch convenience into TocPilot without adding a menu-heavy launch flow.

UI / workflow contract:

- Rename the existing **TocPilot** toolbar action to **Info** when this feature lands; clean its popup into the application/version/update/interface-preferences window.
- Add a separate **Account Sync** action in Advanced only, with a new dedicated sync/transfer icon in Icons mode and `Account Sync` text in Words mode.
- Account Sync popup is configuration only: discovered `WTF\Account\` accounts with checkboxes on the left; **Macros**, **Keybindings**, **pfUI**, and **Sync before Launch** on the right; retain a manual **Sync Now** action. Do not put runtime status/history in this popup.
- When Account Sync is enabled, show a compact main-window panel below addon management with exactly three rows (**Macros**, **Keybindings**, **pfUI**) and columns for item, status, and action/confirmation. This is the runtime view; do not turn it into a scrolling debug console.
- Normal daily flow is one-click: when **Sync before Launch** is enabled, Launch performs Account Sync automatically and then starts WoW. Only genuinely confirmation-required overwrites should interrupt.
- Click **Launch** normally for the existing launch path; **Ctrl-click Launch** adds `-console`. Account Sync runs before either launch form.
- User-declined optional overwrite skips that item and Launch may continue. Backup/copy failure is a real error and blocks Launch.

Files / source selection:

- Macros: `macros-cache.txt`.
- Keybindings: `bindings-cache.wtf`.
- pfUI: `SavedVariables\pfUI.lua`.
- Selected checked accounts form the sync group; there is no fixed global source/destination.
- For each enabled item independently, inspect selected accounts and choose the newest existing copy by `LastWriteTimeUtc`.
- Equal modified timestamps are left untouched.
- If no selected account has the source item, skip/warn; never synthesize data.
- Macros/keybindings: an older or missing target requires confirmation before copying.

pfUI is a **targeted sync/merge surface**, not a whole-file copy. Runtime testing proved the previous comparison-only `Settings / Cache / Junk` model was insufficient because its write path still replaced the entire destination `pfUI.lua`. The targeted implementation is merged on `dev` at `13a801696e2be585ab46d7941f96cc07ba832a18`.

Locked pfUI policy:

- `pfUI_profiles` is the only user-configuration section TocPilot should synchronize directly.
  - Compare profiles semantically so Lua table/write ordering alone is not a difference.
  - A real `pfUI_profiles` difference requires confirmation.
  - On acceptance, source `pfUI_profiles` replaces target `pfUI_profiles`; do not replace the entire `pfUI.lua`.
- `pfUI_addon_profiles` is account/character-local and must be left untouched. Addon enablement can legitimately be character-specific; Account Sync must not propagate one account's addon-profile choices into another.
- `pfUI_throttle` is left untouched/local.
- `pfUI_cache` is handled explicitly by subtable rather than copied wholesale:
  - `libhealth` — additive merge; preserve/accumulate learned mob-health data across selected accounts.
  - `gold` — additive merge across realm/character keys so tracked character gold can be known across accounts.
  - `prediction` — additive merge; preserve/accumulate learned per-character heal/spell prediction data.
  - `chathistory` — leave untouched/local.
  - `abuttons` — leave untouched/local for now; current samples contain empty `add`/`del` tables and there is no locked reason to merge them.
  - unknown/new cache subtables are left untouched by default until explicitly classified.
- Unknown/new top-level pfUI sections outside the explicitly supported surfaces above are left untouched by default. This is deliberately safer than guessing future module semantics.
- Additive selected-cache merge semantics are deterministic and group-wide:
  - union supported cache keys across all selected existing pfUI files;
  - when the same exact key path has different values, the file with the newer `LastWriteTimeUtc` wins;
  - equal timestamps use selected-account order as the tie-breaker, with the earlier selected account winning;
  - nested tables merge recursively; a table/scalar structural conflict is resolved by the same precedence at that conflicting node.
- Parser/merge failure must fail safe: do not modify the destination, report the error, and block Launch when Sync before Launch depends on that operation.
- Existing destination backup ordering remains mandatory before any successful targeted pfUI write.

Backup safety is mandatory:

- Existing destination -> successful backup -> only then overwrite.
- Missing destination needs no backup.
- New backup root is **`WTF\tocpilot\`** (requested as `..\WTF\tocpilot\` relative to account directories), not the BAT's previous backup location.
- Preserve timestamped backup-run directories and account-relative structure beneath each run; retain collision suffix handling such as `-02`, `-03`.
- Backup failure means do not overwrite and block Launch.
- Copy failure after a successful backup leaves the backup intact and blocks Launch.

### Account Sync implementation audit / recovery checkpoint — 2026-09-28

Repository state:

- Account Sync implementation commit: `232e2fb5f8ff843f10e164658efa728926988da6` (`feat: add safe account sync`).
- Ctrl-launch follow-up: `fbd8cef2ceb2626ad3e4e7f296f9b8de2b9e0fd6` (`fix: preserve ctrl launch intent through sync`), current `main` head at this checkpoint.
- Last known-good published/runtime-accepted code baseline: `8678334c0a0015eba14aecf58f7c416c045f6581` (`v0.3.22`).
- Final pre-Account-Sync repository commit: `6fcf117cf4f639612827acf88b10c6933f4b7802` (`docs: hand off account sync design`). It changes documentation only, so the code beneath it is the accepted v0.3.22 baseline.
- Both Account Sync commits reached GitHub Actions, but current main fails **Build Release** before tests. Run `36426680219`, Windows x64 job `108942244145`, is the authoritative failed-CI reference.

Confirmed compile blockers:

1. `src/account_sync.cpp` around lines 390/392 uses default-delimiter C++ raw-string regex literals whose contents contain the terminating `)"` sequence. MSVC therefore terminates the string early and reports the first real errors as C2017/C2065/C2001, followed by extensive parser cascades. Fix with a custom raw-string delimiter or correctly escaped ordinary strings without changing the BAT-equivalent regex semantics.
2. `ShowAccountSyncWindow()` in `src/main.cpp` around lines 8378-8390 calls `std::max(0, <RECT LONG arithmetic>)`, mixing `int` and Win32 `LONG`. Match the already-correct `ShowTocPilotWindow()` pattern by casting the RECT arithmetic to `int` before `std::max`.

Audit result beyond compilation:

- The sync engine follows the locked/source BAT semantics: per-item newest-source selection by timestamp, equal timestamps untouched, older/missing macros/keybindings requiring confirmation, pfUI semantic comparison, cache-only pfUI auto-sync, parser/comparer failure falling back to confirmation, and unknown pfUI sections treated as settings.
- Backup ordering is correct: existing destination -> successful backup under `WTF\tocpilot\<run>\<account>\...` -> overwrite. Missing destinations need no backup. Backup failure prevents overwrite; copy failure after backup leaves the backup intact; fatal backup/copy failures block Launch.
- Backup run naming preserves `dd-MM-yyyy-HHmm` plus `-02`, `-03`, etc. collision handling.
- UI/integration pieces are present: Info rename, Advanced-only Account Sync action/config popup, three-row runtime panel, Sync Now, Sync before Launch, confirmation-decline allowing Launch to continue, fatal sync blocking Launch, and Ctrl state captured before sync prompts so Ctrl-click Launch can still add `-console`.
- State loading remains backward-compatible with pre-Account-Sync `TocPilot.json`; missing new Account Sync fields default safely rather than invalidating old state.
- A suspected main-window-height problem was rechecked and is **not** a finding: the existing 400 design-px minimum is sufficient for the current panel layout.

Recovery result:

- Compile blocker fixes:
  - `83f5e83bda71061d2f68c4ead442d5340b91443d` — repairs the malformed pfUI raw regex literals using a custom delimiter without changing regex semantics.
  - `8099915046e319b373f220e3b63fbeb5da7a3594` — fixes the Account Sync window `LONG`/`int` coordinate arithmetic by matching the existing Info-window pattern.
- The first recovered Release build then exposed one real runtime-path defect in `account-sync-safety`: a missing destination could surface as `std::errc::no_such_file_or_directory` and abort analysis before confirmation. `089e1b44769aec6f21aa4132e0b8d414597f6cbe` fixes that narrowly by treating an actually missing sync file as a valid missing target while preserving all other filesystem errors.
- Build workflow run `36438966237` for `089e1b4...` passed Release build and all **19/19 existing CTest tests**.
- `2e80b1378a7a01bb311140828e4e54069b1576d6` adds the focused recovery coverage and a small testable pre-launch seam without changing the locked launch semantics. Added/verified cases are:
  - copy failure after successful backup -> backup remains + fatal result;
  - backup run-folder collision -> `-02`;
  - no-source item;
  - mixed 5-account pfUI case combining equivalent, cache-only, settings-confirmation, and equal-timestamp targets;
  - parser-fallback execution through confirmation/copy, not only preview;
  - explicit pre-Account-Sync state-file loading with safe defaults;
  - Ctrl-click console intent captured before Sync-before-Launch confirmation flow, plus fatal sync blocking Launch.
- Final recovery CI: Build workflow run `36440593954`, Windows x64 job `108989739617`, passed Release build and **19/19 CTest tests**; `account-sync-safety` passed.
- Untagged dev/test artifact from that exact run: GitHub artifact `TocPilot-windows-x64` / ID `10977293329`. Extracted `TocPilot.exe`: **3,076,608 bytes**, SHA-256 `930dc5d90c9e285d88e4a28187818d88d99b09c1b1bec24163d5abd55db6e68a`. This is for runtime validation only; do not publish it as a normal release.
- The accepted rollback/reference baseline remains `8678334c0a0015eba14aecf58f7c416c045f6581` (`v0.3.22`) / pre-feature documentation checkpoint `6fcf117cf4f639612827acf88b10c6933f4b7802`.

### Account Sync runtime checkpoint / pfUI redesign — 2026-09-30

Runtime results through `v0.3.23-dev.1`:

- Account discovery works for the two real accounts used in testing; selected-account persistence passed. With only one account selected, the main sync preview has no comparison work, which is expected.
- Macros confirmation decline passed: declining left the target `macros-cache.txt` unchanged.
- Macros accepted-sync passed: the older target was backed up under `WTF\\tocpilot\\<run>\\<account>\\macros-cache.txt` before overwrite, then the target matched the source.
- Missing-destination macros passed: deleting the target macro file then accepting the correctly directed sync recreated it without creating a fake backup for a file that did not exist.
- Keybindings runtime validation has not yet been completed.
- pfUI preview correctly reached a confirmation dialog, but inspection of the two real `pfUI.lua` files showed the apparent meaningful difference was only `pfUI_addon_profiles["Current"]` numeric ordering/index movement (for example `pfQuest_Group` occupying a different list position), not a user UI-profile settings difference.
- That finding exposed the larger defect: current `Junk` handling only excludes a subtree from semantic comparison. The actual pfUI sync path still performs a whole-file copy, so ignored data such as chat history would still be overwritten whenever any other pfUI difference triggered a sync.
- The two real sample files show `pfUI_cache` top-level subtables `abuttons`, `libhealth`, `gold`, `chathistory`, and `prediction`. This evidence informed the targeted policy above.
- The old whole-file implementation must not be runtime-tested further. The targeted redesign is now implemented, CI-checked, and published in `v0.3.23-dev.2`; runtime testing should resume only against that targeted build.

### Targeted pfUI implementation result — 2026-09-30

- PR #30 (`Target pfUI Account Sync`) merged to `dev` as `13a801696e2be585ab46d7941f96cc07ba832a18`.
- PR head `6e27c6d390c9c874e866a8fd5b78b4d6de70c0dc` passed Build workflow run `36712586795`, Windows x64 job `109877671575`: Release build passed and **19/19 CTest tests passed**, including `account-sync-safety`.
- The writer now replaces only confirmed `pfUI_profiles`; it never performs a whole-file pfUI copy.
- `libhealth`, `gold`, and `prediction` are merged additively across the full selected account group. Target-only learned data can therefore flow back into the newest file as well as into other selected accounts.
- `pfUI_addon_profiles`, `pfUI_cache.chathistory`, `pfUI_cache.abuttons`, `pfUI_throttle`, unknown cache subtables, and unknown top-level sections remain target-local.
- Missing pfUI destinations, after confirmation, are created only from the confirmed source profiles plus the supported merged cache surfaces; source-local/unknown sections are not synthesized into the new target.
- Existing files are backed up before any targeted write. Targeted pfUI output is staged to a same-directory temporary file, flushed, then atomically replaced with write-through semantics.
- All selected existing pfUI files are parsed and the merge plan is built before the first write. Parser/merge failure is fatal to the sync run and leaves every selected pfUI file untouched; there is no whole-file confirmation fallback.
- Deterministic coverage now includes addon-profile ordering/local-section preservation, accepted and declined profile sync, group-wide additive accumulation, newest-wins and equal-time conflict semantics, missing-target creation, backup preservation, and parser-failure no-write behavior.
- **No pfUI runtime validation was resumed in this slice. No Refresh All work was started.**

Development-channel bootstrap defect found during this runtime session:

- Manually installing `v0.3.23-dev.1` over a stable-era state with no `receive_development_builds` key immediately downgraded back to `v0.3.22` on startup.
- Cause: absent setting defaults to `false`, and startup update selection runs before the user can open Info -> Updates and opt in; a running prerelease therefore sees the stable channel and returns to stable.
- Temporary test bootstrap succeeded after setting `receive_development_builds=true` in `TocPilot.json`.
- The behavior is a real legacy bootstrap edge case, but on 2026-09-30 the user explicitly made it non-blocking for this prerelease: after the Account Sync stable path, normal installations will have an explicit stored `receive_development_builds=false`; reaching this ambiguity later requires manually sideloading a prerelease from a pre-migration state. Preserve default-off/explicit-opt-in behavior and keep this edge documented rather than blocking `v0.3.23-dev.2` runtime validation.

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

### v0.5.0-dev.1 runtime findings — Phase 6

- **Root-addon Add Git identity defect:** runtime Add Git of `Seraphic8x2244/Performante` discovers the root addon but fails when saving the selected package with `Package 'github:Seraphic8x2244/Performante:addon:' has an id that does not match its provider/repository/source identity.` The discovered root candidate carries an empty `repositoryRelativePath`, while Phase 5 package creation recognises only literal `.` as the root case and otherwise calls the nested-addon package constructor. Correct fix: normalise the root representation at the package-creation boundary (empty and `.` must mean the same repository-root addon), keep semantic validation strict, and add a regression using the actual empty root path produced by discovery.
- **Add Git flow is too modal:** current flow is URL → standalone branch dialog → scan → Contents. Desired normal flow is **URL → Scan default branch → Contents/Add**. The repository default branch should be resolved and scanned automatically. When multiple selectable components exist, Contents owns a visible branch dropdown showing the active addon branch; changing it rescans branch-based addon candidates and refreshes the Contents view. Latest-stable DLL/MPQ release candidates remain release-based and independent of branch choice.
- **Single-content fast path:** when discovery yields exactly one valid selectable component, skip the generic Contents selection screen and proceed directly to its appropriate confirmation/install path. Do not bypass meaningful safety/ownership/trust prompts: DLL trust confirmation, ownership replacement, unmanaged-root refusal, MPQ destination conflict, or equivalent constraints still appear when applicable.
- **Branch freshness invariant for the redesigned flow:** branch names may be cached for UI, but a branch selected from Contents must resolve its current remote HEAD before being treated as fresh or used for package/update decisions. Do not reproduce the separately documented stale branch-SHA behaviour in the new Add Git path.

Published v0.3.11 added the focused self-update transport hardening and constrained/shared package-column layout on top of the runtime-accepted v0.3.10 baseline. On 2026-09-27 its installed self-update/startup-state path and requested column-layout matrix passed: Compact/Advanced primary-column constraints, Advanced-only boundary/reordering, bidirectional Name/Status width sharing, and persisted layout all passed. The adjacent Compact main-window-width round-trip regression found during that gate is fixed and published in v0.3.12. v0.3.12 is CI/release-verified and awaits only the focused runtime gate below; v0.3.10 remains the latest fully runtime-accepted release until that check passes. A1 remains accepted for forward development after fresh install, reinstall, Update New and Remove Addon passed in its earlier gate; managed same-root replacement remains explicit deferred runtime debt. Remove Addon was not repeated for the v0.3.10 gate and is non-blocking.

P6A is complete. A1 transaction restart recovery is implemented, CI-checked and published in v0.3.8 at `17ce349273c6f2d76572c7107f3c6f7b139cf8d8`, and accepted for forward development with the same-root replacement runtime check deferred. It now writes a versioned, flushed pre-mutation journal; arms it with package/transaction identity, affected-root intent and durable pre/post state markers before live renames; recovers unfinished transactions before normal package mutation; and preserves evidence rather than guessing when durable state is ambiguous.

A2 durable state semantic validation is implemented, CI-checked, merged at `dcdd5058f50c83c427310feba083437db6368dcd` and published as part of v0.3.10. State load now fails closed before runtime use when durable packages violate understood identity, mode/target, ownership, installed-state or direct-DLL invariants; the original JSON is preserved for manual repair. The user accepted A2 for forward development without a separate A2-only runtime gate.

A3 ZIP single-member allocation hardening is implemented, CI-checked, merged at `8ff352e58890399f95bf633ccfe58a32580f2be1` and published as part of v0.3.10. ZIP members are streamed to staged files rather than buffered wholly in RAM; the existing archive/per-entry/total limits remain in force, and deterministic oversized-entry metadata coverage rejects policy violations before extraction staging.

Remaining audited implementation priority after the v0.3.11 runtime gate starts with:

- **branch-switch freshness defect:** the inline branch selector may reuse branch metadata cached earlier in the session. `ApplyBranchChoice()` stores that cached `branch.sha` into `latestRevision` and records the package as freshly refreshed, so switching to a branch can temporarily point TocPilot at an older commit until an explicit Refresh re-resolves the branch head. Confirmed with WanderingGaia: switching to `dev` exposed `0.2.7-dev` until Refresh fetched the current `0.2.8-dev` head. Correct contract: branch-name metadata may be cached for UI, but after selecting a branch TocPilot must resolve that branch's current remote HEAD SHA before treating it as fresh or using it for update/install decisions;
- the dedicated Add-Git branch dialog has no per-request generation/repository token. Closing and reopening it while its detached lookup is still running leaves a rare stale-result/HWND-reuse race; the main inline branch selector already has a generation + package-ID guard;
- Add Git latest-stable DLL discovery performs provider network I/O synchronously on the dialog thread, including the DLL-fallback dialog creation path, so provider timeout/failure can freeze that UI;
- Update All does not treat branch-addon archive HTTP rate limiting as a queue-stop condition, unlike status refresh and direct-DLL update paths;
- provider/repository staging-directory sanitization can collide for distinct identities.

Lower-priority/test/build findings and contract-driven direct-DLL risks are retained in `audit_dump.md`.

UI observation from 2026-09-27: after **Refresh All**, the addon package list could jump to an arbitrary mid-list position because `RefreshPackageStateUi()` preserved the identity of the previous top/selected package across repopulate/re-sort; when ordering changed, the viewport followed that package. **Superseded design decision on 2026-09-30:** do not repair or retain that preservation behaviour. Remove it and use the explicit single-addon versus bulk/whole-list reset contract instead. The async/UI pass otherwise found no high/medium GDI/icon ownership leak. It did confirm low-priority cleanup/debt: the old hidden branch COMBOBOX is now dead infrastructure after the list-cell popup redesign; main close can abandon non-install async work/staging; several rare Win32 control/subclass/timer/GetMessage failures are not surfaced.

Final severity/order:

1. **HIGH — transaction restart recovery:** **IMPLEMENTED / CI-CHECKED / PUBLISHED / ACCEPTED**, with managed same-root replacement runtime validation deferred.
2. **HIGH — durable state semantic validation:** **IMPLEMENTED / CI-CHECKED / MERGED / ACCEPTED FOR FORWARD DEVELOPMENT** at `dcdd5058f50c83c427310feba083437db6368dcd`; no standalone A2-only release gate required.
3. **MEDIUM — ZIP member allocation bound:** **IMPLEMENTED / CI-CHECKED / PUBLISHED / RUNTIME-ACCEPTED in v0.3.10** — per-member extraction streams to the staged file rather than allocating the full uncompressed member in RAM; the existing 256 MiB per-entry policy remains enforced during archive inspection.
4. **SELF-UPDATE TRANSPORT + COLUMN LAYOUT:** **IMPLEMENTED / CI-CHECKED / MERGED / PUBLISHED in v0.3.11; REQUESTED RUNTIME MATRIX PASSED** — installed update/startup-state and the constrained/shared column behavior passed on 2026-09-27.
5. **COMPACT MAIN-WINDOW WIDTH FOLLOW-UP:** v0.3.12 implemented capture/restore; v0.3.13 fixed the startup-default-vs-real-minimum edge. **v0.3.13 PUBLISHED / CI-CHECKED / RUNTIME-ACCEPTED**.
6. **CURRENT NARROW UI FOLLOW-UP — icon-toolbar pass:** replace the wide text toolbar presentation with compact icon buttons while preserving existing command behavior; details are locked under P6B below.
7. **CURRENT NARROW UI FOLLOW-UP — package-list viewport reset:** remove the old viewport-preservation path and implement the locked single-addon versus bulk/whole-list start/finish reset contract.
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

Account Sync runtime validation is **partially complete and ready to resume on published prerelease `v0.3.23-dev.2`**. Account selection persistence and the Macros decline/accepted-backup-copy/missing-destination paths passed against real WTF data. The targeted pfUI redesign is merged and CI-checked at `13a801696e2be585ab46d7941f96cc07ba832a18`. First validate the installed `v0.3.23-dev.1 -> v0.3.23-dev.2` self-update, then resume pfUI, keybindings, Sync-before-Launch, Ctrl-launch and induced-failure runtime checks against `dev.2`. **Refresh All viewport jump** remains queued after Account Sync.

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

- **Clear WDB folder** — future Advanced feature;
- **DXVK advanced logging checkbox** — future Advanced option; the BAT's current `DXVK_LOG_LEVEL=debug` behaviour should become optional rather than forced. `WoW_d3d9.log` archival can be reconsidered with this work and is not part of the locked Account Sync scope;
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

- Current published and fully runtime-accepted stable release is `v0.3.22` at `8678334c0a0015eba14aecf58f7c416c045f6581`.
- Development-channel Slice 4 implementation commit `518a9a1f1407b4a62ca4b09c11522f8859425b61` introduced the separate dev prerelease workflow and source version `v0.3.23-dev.1`; the stable release workflow was not modified.
- Development Release run `36588659393`, Windows x64 job `109475655788`, passed source-version validation, Release build, **19/19 CTest tests**, checksum generation, tag creation, prerelease publication, stable-latest isolation, and the real parent-build -> prerelease self-update gate.
- Published `v0.3.23-dev.1` `TocPilot.exe`: 3,095,040 bytes, SHA-256 `39c2bc49b189b1c1021f1023ef9c9df98bdb93a3c5dbde3dc90bc498d613e50b`.
- Published `v0.3.23-dev.1` `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `bb0388b4bb943df3a5f4e2b5bbfb5d3fd384b54cbc31f6726af525f6a1035970`.
- GitHub `/releases/latest` remained stable-only at `v0.3.22` after publishing `v0.3.23-dev.1`.
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

## Handoff — 2026-09-30

- Targeted pfUI Account Sync is implemented / merged / CI-checked at `13a801696e2be585ab46d7941f96cc07ba832a18`.
- PR #30 head `6e27c6d390c9c874e866a8fd5b78b4d6de70c0dc`; Build run `36712586795`, Windows x64 job `109877671575`: Release build + **19/19 CTest passed**, including `account-sync-safety`.
- PR #31 (`Release v0.3.23-dev.2`) head `0c36b1a655fc480df447990117c35ff1cf973981` passed Build workflow run `36722764428`, Windows x64 job `109911820308`: Release build + **19/19 CTest passed**; it squash-merged to `dev` as `5b1575ac72e8db1edc2aac6962bec0a528e732e1`.
- Development Release workflow run `36723197591`, Windows x64 job `109913301965`, passed from exact merged `dev` `5b1575ac72e8db1edc2aac6962bec0a528e732e1`: Release build + **19/19 CTest passed**, prerelease assets published, stable latest remained `v0.3.22`, and the automated development-channel self-update gate passed.
- Real installed `v0.3.23-dev.1 -> v0.3.23-dev.2` self-update passed on 2026-09-30 with development opt-in preserved.
- Runtime Account Sync accepted paths: account discovery/selection persistence; Macros confirmation decline, accepted copy + backup, and missing-target recreation; targeted pfUI cache-only merge; protected/local pfUI preservation; real `pfUI_profiles` difference prompt + accepted profile copy while supported cache processing continued in the same run.
- Keybindings are accepted by shared-path reasoning: they use the same whole-file backup/copy machinery already exercised by Macros; no separate manufactured runtime difference is required.
- Release-gate decision changed intentionally on 2026-09-30: Sync-before-Launch/Ctrl-click and induced backup/write-failure remain untested runtime debt but are **non-blocking** for `v0.4.0`. The legacy missing-`receive_development_builds` sideload/bootstrap ambiguity also remains documented non-blocking.
- PR #32 (`Release v0.4.0`) head `d69b96ed7f3f3ec89842c19be651c6ccc60f4ed0` passed Build workflow run `36766021743`, Windows x64 job `110060274520`: Release build + **19/19 CTest passed**, including `account-sync-safety`; it squash-merged to `dev` as stable-candidate commit `83d6e52f691ac5bc8f9f0694fb5fb96fa80ccfcc`.
- PR #33 (`Promote v0.4.0 to main`) head `808a6334bd32e1871f2d7d0cc91d12a11384bcf3` passed Build workflow run `36766584386`, Windows x64 job `110062167158`: Release build + **19/19 CTest passed**, including `account-sync-safety`; it merge-committed to `main` as `eee95cf319351adbef1a3b549f5fe9531f33567f`, preserving the validated `dev` lineage.
- Main-push Build workflow run `36767072045`, Windows x64 job `110063803742`, passed Release build + **19/19 CTest**, including `account-sync-safety`, from exact main commit `eee95cf319351adbef1a3b549f5fe9531f33567f`.
- Stable Release workflow run `36767072167`, Windows x64 Release job `110063804704`, passed source-version validation, Release build, **19/19 CTest**, SHA-256 sidecar generation, tag creation/verification and asset publication from exact main commit `eee95cf319351adbef1a3b549f5fe9531f33567f`.
- Published `v0.4.0` is non-draft/non-prerelease and GitHub `/releases/latest` resolves to it. `TocPilot.exe`: 3,145,728 bytes, SHA-256 `43dcd24c99373d79f4ad949467856389660c35e653d8b1401d1b873274d04119`. `TocPilot.exe.sha256`: 78 bytes; asset SHA-256 `45000dd1a049b91f8a38f0eac0d24620a69c6fa467a3aec19fb574c3400e109f`.
- `v0.3.22` at `8678334c0a0015eba14aecf58f7c416c045f6581` remains the last Account-Sync-free rollback baseline; current stable is `v0.4.0` at `eee95cf319351adbef1a3b549f5fe9531f33567f`.
- Package-list viewport reset implementation commit `eace26841fdd41073413605bfb4f5f3c578b3da0` is CI-checked by Build run `36775185023`, Windows x64 job `110091188659`: Release build + **20/20 CTest passed**, including `package-list-viewport-policy`.
- Development prerelease `v0.4.1-dev.1` is published from exact commit `325523b05c34c59d6e15fe56cd152d9cfafb5fdf`. Development Release run `36888266631`, Windows x64 job `110457047207`, passed source-version validation, Release build, **20/20 CTest**, tag/assets publication, stable-channel isolation, and real development-channel self-update from parent `7cfa6ba984dbe909622941ae72c8157990cc05d7`. `TocPilot.exe`: 3,145,216 bytes, SHA-256 `996655e59732c7afca9934423d605e9565b8fc26912a048e06419de9e4511387`. Runtime viewport validation passed on 2026-10-01 for single-addon/branch actions, Refresh All, Update New and presentational actions; multi-package Add Git remains untested/non-blocking due to no available fixture. Keep **Clear WDB folder** and **DXVK advanced logging checkbox** deferred.
- Startup scan auto-enter implementation `b539b593900ccc046baffeffdcdf19e9a57a8ffb` removes the normal click-to-continue splash gate while retaining the explicit self-update-failure acknowledgement. Draft PR #35 CI: Build run `36893192930`, Windows x64 job `110473610222`, Release build + **20/20 CTest passed**. This change is not in published `v0.4.1-dev.1`; include it in the next development prerelease.
- v0.5.0 Phase 1 exact release-asset backend: **implemented / CI-checked / merged on `dev`; backend-only, runtime prerelease not required**. PR #36 tested exact head `f040de9f7a44c92c5b8ccc8e6ec8af068ede2d18`; Build run `36896441332`, Windows x64 job `110484495595`, passed the Release build and **21/21 CTest tests**, including `direct-dll-policy` and new `exact-release-asset-policy`. It squash-merged to `dev` as `2809f6296e461fe7f2ebd2bd4cb853dca80a5961`. New `exact_release_asset` code owns kind-aware (`Dll` / `Mpq`) latest-stable exact-asset source validation and metadata/checksum resolution, including the existing exact `<asset>.sha256` fallback. Existing direct-DLL package JSON/state validation, exact WoW-root destination, direct-write/no-staging behaviour, size/SHA-256 checks, trust/security policy and runtime semantics remain unchanged; `DirectDllRelease` now aliases the shared resolved-release model. No MPQ filesystem installation, MPQ target allocation, Add Git redesign, mixed queue or wording work was included.
- v0.5.0 Phase 2 MPQ package/install backend: **implemented / CI-checked / merged on `dev`; backend-only, runtime prerelease not required**. PR #37 tested exact head `b57d7126d1e996f01a6b1eac48f760d3ca682b78`; Build run `36900202483`, Windows x64 job `110497168058`, passed the Release build and **22/22 CTest tests**, including new `mpq-package-backend`. It squash-merged to `dev` as `2723509f292655b5aae7c62e563965e0b387f2b3`. MPQ packages now use the shared exact latest-stable release-asset model, durable `Data/patch-A.mpq` through `Data/patch-Z.mpq` reservation, deterministic lowest-free-letter allocation from both live Data names and persisted TocPilot destinations, persisted target reuse by package identity, collision refusal, transactional install/update/remove with rollback material retained across the state-save boundary, and exact size/SHA-256 integrity verification. Existing direct-DLL direct-write/security behaviour remains separate and unchanged. No unified discovery, Add Git UI, mixed queue or wording work was included.
- v0.5.0 Phase 3 unified repository candidate discovery: **implemented / CI-checked / merged on `dev`; backend-only, runtime prerelease not required**. PR #38 tested exact head `46d21be1289e5f7528e6662ca856f437cad742ba`; Build run `36906019134`, Windows x64 job `110516642569`, passed the Release build and complete **23/23 CTest suite**, including new `repository-candidate-discovery`. It squash-merged to `dev` as `132782f66e069774ca2b86ee04f728699e7755f8`. The new repository-discovery backend returns one result that independently records selected-branch shallow addon/library discovery and latest-stable GitHub release discovery, then aggregates every valid addon, exact standalone DLL and exact standalone MPQ candidate without fallback suppression. Branch and release failures remain independent; mixed root+child addon layouts remain ambiguous/non-selectable without hiding valid release assets; GitLab remains branch-addon-only. Existing Add Git presentation/install orchestration is intentionally unchanged in this phase, so MPQ/mixed selection is not exposed yet. No selection-UI redesign, mixed install queues or wording work was included.
- v0.5.0 Phase 4 Add Git selection UI: **implemented / CI-checked / merged on `dev`; visible selection UI, end-to-end mixed runtime gate deferred to Phase 5 prerelease**. PR #39 tested exact head `55d8c0efd509531d10b88c0db96888721b43353d`; Build run `36910036992`, Windows x64 job `110530123352`, passed the Release build and complete **24/24 CTest suite**, including new `repository-selection-policy`. It squash-merged to `dev` as `940acbb0a50d343cdee03e8919e4e5ed764c2b45`. Add Git is now discovery-first: repository URL → Scan/branch selection → one checkbox surface containing every discovered addon/library/DLL/MPQ candidate, with branch vs latest-stable source context and deterministic proposed `Data/patch-X.mpq` destinations. DLLs are never preselected and retain explicit publisher-trust confirmation. Existing addon-only multi-install orchestration and the existing single-DLL install path remain unchanged; MPQ or mixed-type selections deliberately stop before package-state mutation because package creation/install orchestration remains Phase 5 scope. No broad wording pass was included.
- v0.5.0 Phase 5 mixed selection creation/install orchestration: **implemented / CI-checked / merged / published in `v0.5.0-dev.1`; end-to-end user runtime matrix pending**. PR #40 tested exact head `7093f97d35708a952883f201c6665447f0bc75fb`; Build run `36913804601`, Windows x64 job `110542709644`, passed the Release build and complete **25/25 CTest suite**, including `repository-mixed-install-policy`. It squash-merged to `dev` as `1b671ddd3fb5d1df9cf0ed40adcb3aa60a6ae1e9`. Development Release run `36914667830`, job `110545587877`, passed source-version validation, an independent Release build and **25/25 CTest suite**, prerelease publication/channel isolation, and real development-channel self-update from parent `70af56463ecbb16b2ca34d9ab3020ffa3bad0a2a` to `v0.5.0-dev.1`. The prerelease contains `TocPilot.exe` (3,237,376 bytes, SHA-256 `6abb811e4040602a8e66d0694db63c6e40ed21b3cf2cde054d8934bc79643346`) plus its sidecar; stable `/releases/latest` remains `v0.4.0`. Mixed Add Git now creates independent addon/DLL/MPQ package records, persists all selected records before deterministic package-ID queue execution, routes each package through its established backend, stops a mixed queue at the failing package without advancing its installed state or corrupting already committed/later package ownership, preserves MPQ rollback across state-save boundaries, includes MPQs in Refresh/Update New/Install/Remove routing, and protects exact DLL/MPQ destination reservations. Direct-DLL removal remains exact-path/no-backup in keeping with the existing DLL security model. No Phase 6 fixes or wording-pass work was included.

## v0.5.0 — Unified repository discovery / MPQ support

Target stable release: **v0.5.0**.

Product contract for this release:

- **Add Git becomes discovery-first rather than fallback-first.** A repository is inspected for every supported installable option and the user chooses what TocPilot should manage.
- Branch/archive discovery and release-asset discovery are independent. Finding an addon must not suppress valid release DLL/MPQ choices, and failing one discovery category must not prevent the others from being offered.
- Supported branch candidates remain:
  - one addon rooted at repository root when a direct root `.toc` exists;
  - an addon library when there is no root addon and immediate child folders contain direct `.toc` files.
- Preserve the existing no-guess rule for overlapping/ambiguous addon roots unless deliberately revisited. The 0.5.0 change is about aggregating independent candidate classes, not making unsafe addon-root guesses.
- Supported latest-stable GitHub release candidates become:
  - exact standalone `.dll` assets;
  - exact standalone `.mpq` assets.
- One repository may therefore offer any combination of addon candidate(s), DLL asset(s), and MPQ asset(s). The selection UI should show all valid options together.
- Each selected component remains an **independent TocPilot package record**, even when several came from the same repository. Branch addons track branch revisions; DLL/MPQ release assets track latest-stable release state independently.
- MPQs install under WoW `Data\`.
- TocPilot assigns each managed MPQ a non-conflicting patch-letter destination, treating both TocPilot-managed and unmanaged existing MPQ slots as occupied. The chosen destination is persisted in the package record and reused for all future updates; updates must never silently move an MPQ to another letter.
- DLLs retain their existing exact-asset/latest-stable trust and security policy. MPQs use the same exact release-asset discovery/tracking model but may have their own safe filesystem write policy; do not weaken the special DLL direct-write/security behaviour merely to share code.
- The existing startup auto-enter change at `b539b593900ccc046baffeffdcdf19e9a57a8ffb` is part of the 0.5.0 development line.
- The dedicated user-facing wording pass remains part of the 0.5.0 release gate, but is kept separate from functional payload implementation.

### 0.5.0 chat-sized implementation phases

Each phase is intentionally bounded so a fresh chat can read `dev_rulebook.md` + `DEV_PROGRESS.md`, verify `dev`, complete one slice, CI-check it, update this file, and hand off.

#### Phase 1 — Generalise exact release-asset backend

Goal: create the backend abstraction needed for more than DLLs **without changing Add Git UI or current DLL runtime behaviour**.

Scope:

- identify/generalise the existing latest-stable exact-DLL release metadata/validation/resolution pieces into an exact release-asset model;
- represent asset kind explicitly enough to distinguish DLL vs MPQ;
- preserve existing DLL package/state compatibility and exact destination behaviour;
- add focused tests proving existing DLL validation/resolution behaviour is unchanged;
- no MPQ installation yet;
- no Add Git UI redesign;
- no wording pass.

Gate: Release build + full tests. No runtime prerelease required unless the refactor unexpectedly touches visible DLL behaviour.

Status: **complete on `dev`** at merged source commit `2809f6296e461fe7f2ebd2bd4cb853dca80a5961`; PR #36 Release CI passed **21/21 CTest**. No visible DLL behaviour was intentionally changed, so no Phase 1 runtime prerelease is required.

#### Phase 2 — MPQ package/install backend

Goal: support an independently managed exact latest-stable MPQ package end-to-end below the Add Git UI.

Scope:

- accept/select exact standalone `.mpq` release assets;
- add MPQ package validation/state representation using the shared release-asset model;
- implement deterministic WoW `Data\` destination assignment;
- scan existing Data MPQ names plus persisted TocPilot destinations and choose an unused patch letter;
- persist the assigned target path and never reassign it during normal updates;
- refuse collisions rather than overwrite an unmanaged or differently owned slot;
- implement MPQ install/update/remove and integrity verification;
- add deterministic tests for letter allocation, persistence, collisions, update-in-place, removal, malformed package records and release-asset mismatch;
- no Add Git UI redesign yet.

Gate: Release build + full tests. Backend-only; runtime prerelease optional.

Status: **complete on `dev`** at merged source commit `2723509f292655b5aae7c62e563965e0b387f2b3`; PR #37 final Release CI passed **22/22 CTest** on exact tested head `b57d7126d1e996f01a6b1eac48f760d3ca682b78`. The first CI attempt exposed that standalone MPQ validation still accepted a mixed `source_path`; that validator hole was corrected before the final green gate. No visible runtime/UI path was wired to MPQs in this phase, so no Phase 2 runtime prerelease is required.

#### Phase 3 — Unified repository candidate discovery

Goal: replace sequential fallback classification with one backend discovery result.

Scope:

- introduce a candidate/result model that can contain multiple independent choices from one repository;
- inspect the selected/default branch using the existing shallow addon rules;
- independently inspect latest stable GitHub release for selectable DLL and MPQ assets;
- aggregate valid addon/library/DLL/MPQ candidates instead of stopping after the first successful class;
- one failed/empty class must not hide successful classes;
- preserve GitLab's current capabilities: branch addon discovery only unless GitLab release support is separately added later;
- focused tests for addon-only, library-only, DLL-only, MPQ-only, addon+DLL, addon+MPQ, addon+DLL+MPQ, and empty/ambiguous cases;
- no major dialog redesign yet.

Gate: Release build + full tests.

Status: **complete on `dev`** at merged source commit `132782f66e069774ca2b86ee04f728699e7755f8`; PR #38 final Release CI passed **23/23 CTest** on exact tested head `46d21be1289e5f7528e6662ca856f437cad742ba`. The backend now produces one aggregated result with independent branch and GitHub latest-stable release discovery; ambiguous addon roots remain non-selectable while valid release DLL/MPQ candidates survive. Existing Add Git UI behaviour remains intentionally unchanged until Phase 4, so no Phase 3 runtime prerelease is required.

#### Phase 4 — Add Git selection UI

Goal: make Add Git present the complete discovery result and let the user choose.

Target flow:

`Paste repository URL → Scan → choose available components → Add Selected`

Scope:

- remove the user-facing “normal mode” / DLL fallback concept;
- show all discovered candidates in one selection surface with clear type/source/destination context;
- addon/library candidates identify the branch source;
- DLL/MPQ candidates identify latest-stable release assets;
- MPQ candidate shows its proposed Data patch-letter destination before management begins;
- support selecting one or several candidates;
- do not silently preselect dangerous executable DLL content unless the existing trust policy explicitly permits that UX;
- keep trust confirmation for selected DLLs;
- no broad wording pass outside this redesigned flow.

Gate: Release build + full tests.

Status: **complete on `dev`** at merged source commit `940acbb0a50d343cdee03e8919e4e5ed764c2b45`; PR #39 final Release CI passed **24/24 CTest** on exact tested head `55d8c0efd509531d10b88c0db96888721b43353d`. The old normal-mode/DLL-fallback presentation is gone. One unified candidate surface now exposes addon/library/DLL/MPQ choices together, identifies branch versus latest-stable sources, previews deterministic MPQ Data patch-letter destinations, permits multi-selection, and retains explicit DLL trust confirmation without preselection. Phase 4 preserves the existing addon-only queue and single-DLL execution path; MPQ/mixed package creation and install sequencing are intentionally still deferred to Phase 5.

#### Phase 5 — Mixed selection creation/install orchestration

Goal: one Add Git operation can actually manage several selected component types safely.

Scope:

- create independent package records for each selected candidate;
- generalise the current multi-addon Add Git queue so a mixed selection can install addon(s), DLL(s), and MPQ(s) in a deterministic sequence;
- ensure one failure does not corrupt ownership/state for components already committed or not yet started;
- preserve package ownership invariants;
- ensure Refresh, Update New, Reinstall/Install and Remove route each package type to its correct backend;
- verify list refresh/selection/viewport behaviour after mixed queues;
- add integration tests around mixed repository selections and partial failure.

Gate: Release build + full tests, then publish **v0.5.0-dev.1** for the first meaningful end-to-end runtime gate.

Runtime gate should include:
- startup auto-enter;
- existing addon-only Add Git;
- addon library Add Git when a fixture is available;
- existing DLL Add Git/update regression;
- MPQ-only add/install/update/remove;
- addon + MPQ from one repository;
- addon + DLL + MPQ from one repository when the upcoming real repository is available;
- letter collision handling with an existing unmanaged MPQ;
- independent update states when branch and release move separately.

Status: **complete on `dev` and published as `v0.5.0-dev.1`; runtime matrix started and Phase 6 defects exposed**. PR #40 tested exact head `7093f97d35708a952883f201c6665447f0bc75fb` in Build run `36913804601`, Windows x64 job `110542709644`: Release build + **25/25 CTest passed**, including the new mixed-selection/failure coverage. It squash-merged as `1b671ddd3fb5d1df9cf0ed40adcb3aa60a6ae1e9`. Development Release run `36914667830`, Windows x64 job `110545587877`, independently passed **25/25 CTest**, published the prerelease assets, verified stable-channel isolation, and completed the real development-channel self-update from parent `70af56463ecbb16b2ca34d9ab3020ffa3bad0a2a`. Runtime observations on `v0.5.0-dev.1`: startup auto-enter passed; existing addon update passed; viewport behaviour passed; addon-library/DLL-update/MPQ/mixed/collision/independent-movement cases remain untested because no suitable fixture or update was available. Root-level addon Add Git failed on Performante with a package-ID/source-identity mismatch, so the functional gate is not accepted yet. Phase 5 implementation itself remains the published baseline; corrective work belongs to Phase 6.

#### Phase 6 — Runtime fixes / dev prerelease iteration

Goal: close the defects and unnecessary friction exposed by the `v0.5.0-dev.1` runtime gate without broadening into unrelated feature work.

Current bounded scope:

1. **Fix repository-root addon package identity.**
   - Treat discovery's empty repository-relative path and literal `.` as the same root-addon identity.
   - Root addons keep the repository package ID (`provider:owner/repo`); nested/library addons keep the `:addon:<source>` identity.
   - Do not weaken durable package semantic validation; it correctly caught the malformed record.
   - Add deterministic regression coverage using the actual empty root path emitted by shallow discovery, plus `.` compatibility if still accepted internally.

2. **Streamline Add Git branch/content flow.**
   - Normal path: `Paste repository URL → Scan → Contents/Add`.
   - Resolve and scan the provider's default branch automatically; remove the mandatory standalone branch-choice step before the user has seen repository contents.
   - If discovery yields **2+ selectable components**, show Contents with an active-branch dropdown on that surface. Changing branch resolves that branch's current remote HEAD, rescans only branch-derived addon contents, and refreshes the candidate list; latest-stable DLL/MPQ candidates remain independent release results.
   - If discovery yields **exactly 1 valid selectable component**, skip the generic Contents selection screen and proceed directly to the appropriate confirmation/install path.
   - The single-content fast path must not skip required trust, ownership, collision or replacement prompts.
   - Preserve access to non-default branches without making branch choice a blocking modal in the common case.
   - Do not reuse a cached branch SHA as fresh state when switching branches; current remote HEAD must be resolved before the selection is used.

3. **Runtime/release gate.**
   - Release build + full CTest.
   - Publish `v0.5.0-dev.2` through the development prerelease path.
   - Re-test root-level addon Add Git and the streamlined default/switch-branch flows first, then continue the remaining Phase 5 runtime matrix as suitable real fixtures become available.

Out of scope:
- broad wording pass;
- unrelated audit backlog/refactors;
- inventing synthetic runtime acceptance for DLL/MPQ/mixed cases that still lack real fixtures.

Status: **implemented / CI-checked / published; runtime validation partially passed**. Phase 6 PR #41 final head `0dd22f087e38da4a1be96c554d2318d4af95f8c1` passed Build run `37378410558`, Windows x64 job `111993625004`: Release build + **25/25 CTest passed**. It squash-merged to `dev` as `8d4b6db9303637a745c1f3ce40f7ae3583755954`; Development Release run `37379258344`, Windows x64 job `111996628151`, independently passed **25/25 CTest** and published `v0.5.0-dev.2`. Runtime then passed the Performante root-addon/default-to-`dev` branch flow and the ordinary single-addon default-branch fast path; the user explicitly preferred the streamlined flow. Direct-DLL follow-up PR #42 final head `58f3eff6caf166c14551619fa01c4747c60734ba` passed Build run `37958234647`, Windows x64 job `113914124151`, with Release build + **25/25 CTest passed**, including the expanded `direct-dll-policy` regression. It squash-merged as `e471aad66a64ddeb3f1458f2800edc3542b5521e`. Development Release run `37958978146`, Windows x64 job `113916651576`, independently passed Release build + **25/25 CTest**, published prerelease `v0.5.0-dev.3` at that exact merge commit, verified prerelease-channel metadata, and passed the real development-channel self-update from parent `fe2ab7f167b3b2b69b1c1e7032f0dcaddd37f1eb` to `v0.5.0-dev.3`. The new regression covers missing-file creation, byte-preserving append, case-insensitive duplicate avoidance with surrounding whitespace, and refusal to modify UTF-16 `dlls.txt`. Runtime validation of the new `dlls.txt` behavior remains pending.

#### Deferred to v0.5.1 — wording and extended runtime validation

The user intentionally removed the wording pass, MPQ runtime validation and repository-library runtime validation from the v0.5.0 stable gate on 2026-10-10 so already-demonstrated stable-user bug fixes are not held back.

Historical v0.5.0 deferral list (the **2026-10-20 roadmap above supersedes its wording schedule**):

- Phase 7A/7B wording cleanup across main workflows, dialogs, errors and advanced surfaces **now assigned to v0.6.0**;
- real MPQ lifecycle/collision/mixed-repository runtime validation;
- real repository-library and multi-content branch-switch runtime validation;
- runtime validation of the v0.5.0-dev.3 non-destructive `dlls.txt` registration behavior if not exercised before the next cycle;
- any fixes exposed by those tests.

MPQ and repository-library support remain unadvertised/in-development for this release; absence of those runtime tests does not imply acceptance.

#### Phase 8 — v0.5.0 stable release gate

Status: **complete / published stable on 2026-10-10**.

Release preparation PR #43 changed only stable version/release metadata and the development contract. Its exact candidate head `23f99b18531a8c694129786c7f6b9d7117056717` passed the Windows x64 Release build and **25/25 CTest** on rerun in Build run `38054424890`, job `114220848164`. The first attempt's sole failure was the external `self-update-live-latest` request being refused/rate-limited; the unchanged rerun passed that test.

PR #43 squash-merged to `dev` as `e2b433c4eb27f9aa53585387335802b7f2838860`. Promotion PR #44 then passed its exact-head Release CI and merged `dev` to `main` with history preserved as `1173a91918db134939b65c0a37891abce35dfb37`.

Stable Release workflow run `38055408047`, Windows x64 job `114222725305`, rebuilt and tested that exact `main` commit, passed **25/25 CTest**, generated the SHA-256 sidecar, created tag `v0.5.0`, and published the stable release assets. GitHub `/releases/latest` resolves to non-draft/non-prerelease `v0.5.0` at that exact commit.

Published stable assets:

- `TocPilot.exe` — 3,250,176 bytes — SHA-256 `6ea99ebdb7cac4cb29b62ed43d23bc0c219245f60d24615841e3d23bcc65a86e`;
- `TocPilot.exe.sha256` — 78 bytes.

Accepted runtime evidence carried into v0.5.0:

- startup auto-enter;
- existing addon update;
- package-list viewport behavior;
- Performante root-level addon install with default-branch discovery and live branch switch;
- ordinary single-addon default-branch fast path;
- user confirmed the streamlined Add Git flow is preferable.

Known validation debt intentionally deferred to v0.5.1:

- wording pass;
- MPQ runtime lifecycle/collision/mixed-repository paths;
- repository-library/multi-content runtime paths;
- direct-DLL `dlls.txt` runtime confirmation (automated coverage is green).

## Exact Next Step

**v0.5.1 is published stable.** Run a final optional user-machine stable-version/update/launch smoke test when convenient and record its result honestly. Do not repeat already accepted Account Sync/DLL tests without a demonstrated need. MPQ and library/multi-content Windows runtime tests remain explicitly deferred. The next development cycle is **v0.6.0**, a coordinated native Win32 visual and wording refresh, only when the user asks to begin. Source release commit/tag is `b17584edccae3ec2d2c8debd2b7016dc79fd0cd3`; subsequent documentation-only commits do not change it.
