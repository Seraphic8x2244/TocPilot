# P6A Audit Dump

> Temporary audit scratchpad only. `DEV_PROGRESS.md` remains the sole authoritative live development document.
>
> Purpose: preserve verified evidence from the interrupted P6A robustness audit so the audit can continue in smaller bounded passes without losing work. This is not a final findings report and contains no runtime/source change.

## Audit baseline

- Repository: `Seraphic8x2244/TocPilot`.
- Audited branch/head: `main` at `72c3d60bd9b0a8539545baf3c6ec634e4b56cdb1`.
- Published/runtime-confirmed product baseline remains `v0.3.7` at `ec410df48787fa88a97d49489c4f99ea6893811a`.
- Current `main` Build run `36203512485` (#562), job `108294999580`: configure/build/test/upload all succeeded; CTest reported 17/17 passing.
- The audit was read-first. No runtime code was changed.

## Confirmed findings retained so far

### A1 — HIGH — interrupted addon transactions have no restart recovery path

**Evidence**

- `src/install.cpp` builds a package transaction under `Interface/TocPilot/transactions/<TransactionKey>`.
- `PrepareAddonInstallTransaction()` refuses to continue when that transaction directory already exists, reporting: “An unfinished TocPilot install transaction already exists for this package.”
- Commit/rollback/finalize state is held in the in-memory `AddonInstallTransaction` object.
- Repository search found no startup transaction-recovery routine that interprets an interrupted on-disk transaction and reconciles live roots/backups/state after a process crash or power loss.
- Existing install tests cover injected failures while the process remains alive and can call rollback, including partial new-root commit rollback. They do not exercise restart/crash recovery.

**Affected invariant/runtime behaviour**

A process crash after old live roots have been moved to backup and/or new roots have been committed, but before state save/finalize completes, can leave:

- live addon files differing from durable `TocPilot.json`;
- rollback material stranded under `Interface/TocPilot/transactions`;
- later operations for that package blocked by the unfinished transaction directory.

This is the strongest confirmed robustness issue found so far because it crosses the filesystem/state atomicity boundary.

**Proposed focused fix/test slice**

Add a durable transaction journal/phase sufficient to decide recovery safely, then perform startup recovery before normal package scanning/status work. Add restart-style tests that construct interrupted phases on disk and verify deterministic recovery before ordinary package operations are allowed.

---

### A2 — HIGH — loaded state is not globally revalidated for duplicate package IDs / ambiguous durable ownership

**Evidence**

- `src/state.cpp::ParsePackages()` parses each package object and directly `push_back`s it after only per-record required-field checks.
- `AppendPackage()` rejects duplicate IDs when TocPilot itself creates a new record, but that validation is not applied to a state file being loaded.
- The product contract requires no two durable package records to ambiguously own the same addon root.
- `FindPackageOwningAddonRoot()` returns the first matching package, so a malformed/hand-edited/legacy state containing duplicate ownership is already ambiguous once accepted.

**Affected invariant/runtime behaviour**

A corrupted, manually edited, or historically malformed `TocPilot.json` can be accepted with duplicate package identity or overlapping ownership even though normal mutation APIs would not create that state. Later replacement/removal/selection logic then operates on a state that violates TocPilot's ownership model.

**Proposed focused fix/test slice**

After parsing but before accepting state:

- reject package IDs that collide case-insensitively;
- reject conflicting addon-root ownership;
- reject conflicting direct-DLL destination ownership;
- validate durable package mode/target combinations enough to prevent ambiguous ownership records from entering runtime state.

Add state-load tests for each conflict and verify the existing file is left untouched with a clear state error.

---

### A3 — MEDIUM — branch-dialog async result can become stale if a destroyed HWND is reused

**Evidence**

- `src/branch_dialog.cpp::StartLoad()` starts a detached worker and captures only the raw dialog `HWND`, host and repository.
- The result object contains the fetched repository info/error but no generation/request token.
- Completion is delivered by `PostMessageW(hwnd, WM_TP_BRANCHES_READY, ...)`.
- The dialog can be destroyed while the request is in flight. A failed post frees the result, but there is no identity check if Windows reuses the old handle value for a newly created branch dialog before the old request posts.
- By contrast, the main-window branch selector already carries `g_branchSelectorGeneration` and rejects stale results.

**Affected invariant/runtime behaviour**

In the rare HWND-reuse race, an old repository's branch list could be consumed by a newer branch dialog.

**Proposed focused fix/test slice**

Give branch-dialog loads a monotonically increasing request/generation token stored in both dialog context and result; reject mismatches before applying data. Prefer factoring the accept/reject decision into a small testable helper.

---

### A4 — MEDIUM — latest-stable DLL discovery in Add Git runs network I/O synchronously on the UI thread

**Evidence**

- `src/add_package_dialog.cpp::LoadLatestStableDllAssets()` directly calls `FetchLatestStableGitHubRelease()`.
- It is invoked from dialog/UI handling and uses `UpdateWindow()`, but the provider fetch itself remains synchronous.
- WinHTTP provider calls use multi-second connect/send/receive timeouts.

**Affected invariant/runtime behaviour**

Slow or unavailable GitHub responses can make the Add Git dialog appear hung/unresponsive until the synchronous network call returns.

**Proposed focused fix/test slice**

Move DLL-release discovery to the same generation-safe worker/message model used for branch loading. Disable only the relevant controls while loading and discard stale completion after close/reopen.

---

### A5 — MEDIUM — self-update executable and checksum downloads lack explicit response-size bounds

**Evidence**

- `src/update.cpp::DownloadFile()` streams the update executable to disk until EOF but has no maximum byte limit and does not verify against the GitHub asset size.
- `src/update.cpp::HttpGetString()` accumulates checksum response text in memory without a cap.
- `GitHubReleaseAsset` already contains `size`, but `ReleaseInfo` used by self-update does not currently carry that size into `DownloadVerifyAndLaunchUpdater()`.
- Comparable paths already use bounds: Git ref advertisement and GitHub release JSON are limited to 8 MiB; direct-DLL checksum text is limited to 1 MiB; branch archives/direct DLLs are capped at 256 MiB.

**Affected invariant/runtime behaviour**

A malformed/misrouted response can consume unbounded disk space for the staged updater or unbounded memory for the checksum text before SHA-256 rejection occurs.

**Proposed focused fix/test slice**

Carry the exact release asset size through `ReleaseInfo`, impose a sane executable ceiling, require downloaded size to match metadata, and cap checksum text (1 MiB is already the direct-DLL precedent). Add tests for oversize and size mismatch helpers.

---

### A6 — MEDIUM — ZIP extraction can allocate a single very large member entirely in RAM

**Evidence**

- `src/archive.cpp::ExtractZipSecure()` caps downloaded ZIP size at 256 MiB and total uncompressed size at 1 GiB.
- Each file is extracted by allocating `std::vector<unsigned char>(plan.uncompressedBytes)` and then passing the full buffer to miniz.
- There is no smaller per-entry allocation cap and no catch around allocation failure.

**Affected invariant/runtime behaviour**

A legal archive under the current total limits can contain one very large member and force hundreds of MiB up to roughly 1 GiB of contiguous allocation, risking `std::bad_alloc` / process termination rather than a controlled TocPilot error.

**Proposed focused fix/test slice**

Prefer streaming extraction directly to the staged file if practical. If not, add a conservative per-entry cap before allocation and return a clear validation error. Add a fixture that is rejected by per-entry policy without attempting the allocation.

---

### A7 — LOW — provider/repository staging-directory names can collide after sanitization

**Evidence**

- `src/archive.cpp::ProviderPackageStagingDirectory()` forms one directory name from:
  `SanitizeStagingComponent(provider) + "-" + SanitizeStagingComponent(repository)`.
- Sanitization maps separators/unsupported characters to `-` and collapses repeated dashes.
- Distinct repository identities can therefore produce the same sanitized leaf.

**Affected invariant/runtime behaviour**

Normal package operations are serialized, so this is not currently a concurrent-write bug. However, reset/cleanup of one repository can target the same scratch directory name as another repository, especially around stale crash remnants, and diagnostics become ambiguous.

**Proposed focused fix/test slice**

Keep the readable prefix but append a stable hash of canonical provider + repository identity. Add a collision test using two distinct identities that sanitize to the same prefix.

---

### A8 — LOW — checksum fallback parsing accepts the first 64 hexadecimal characters anywhere in the file

**Evidence**

- `src/direct_dll.cpp::ParseChecksumBody()` scans for any 64-character hexadecimal run.
- `src/update.cpp::ResolveExpectedDigest()` uses equivalent first-match scanning for the self-update sidecar.

**Affected invariant/runtime behaviour**

A multi-entry or oddly formatted checksum file can bind to the wrong digest. This is more likely a false verification failure than a practical integrity bypass because the checksum asset itself is part of the trusted release metadata path.

**Proposed focused fix/test slice**

Parse a conventional SHA-256 sidecar line and, where a filename is present, require the expected exact asset name. Keep support for a digest-only one-line file if desired.

---

### A9 — LOW / CLEANUP — current successful CI still emits compiler/linker warnings

Current main Build job emitted:

- one `C4100` unused-parameter warning in `src/main.cpp`;
- multiple `C4457` local `message` variables shadowing the window-proc `message` parameter;
- `LNK4098` for `TocPilotArchiveTests`: defaultlib `LIBCMT` conflicts with other libs.

These are not runtime defects by themselves. The linker warning is the only one worth treating as a build-configuration robustness item rather than cosmetic cleanup.

**Proposed focused fix/test slice**

After higher-priority behavioural findings, remove the source warnings and determine why the archive-test target mixes CRT defaults. Do not mask `LNK4098` with `/NODEFAULTLIB` until the library/runtime mismatch is understood.

## Contract-driven risks observed but not classified as defects

### Direct DLL failed update can remove the previously installed DLL

The direct-DLL path intentionally writes only to the exact final filename with `CREATE_ALWAYS`, creates no staged/temp/renamed/backup DLL, and deletes the target if download/hash verification fails.

That means an update attempt that truncates an existing DLL and then fails can leave no DLL at the destination. The durable installed state is correctly not advanced, but the old file is not recoverable by TocPilot.

This matches the currently documented direct-write/no-backup product policy, so the audit must not silently “fix” it by introducing a backup/staging DLL. Revisit only as an explicit product-policy decision.

### Direct DLL state-save failure can leave new verified bytes with old durable state

After a successful exact-path DLL write and SHA-256 verification, a later `TocPilot.json` save failure leaves the verified new DLL present while durable installed state remains unchanged. Current UI explicitly reports this and no backup exists by design.

Again, this is a known consequence of the direct-write policy, not a newly classified implementation defect.

## Robust areas explicitly verified during the interrupted pass

These should not be re-audited from zero unless a later finding intersects them:

- State writes use a `.tmp` file, `FlushFileBuffers`, then `ReplaceFileW(..., REPLACEFILE_WRITE_THROUGH)` or write-through move for first creation.
- State read size is capped at 8 MiB.
- Git smart-HTTP branch advertisement is capped at 8 MiB.
- GitHub release JSON is capped at 8 MiB.
- GitHub/GitLab branch archive downloads are capped at 256 MiB.
- ZIP inspection rejects absolute paths, backslash-separated names, traversal/dot segments, Windows-unsafe characters, trailing dot/space and reserved device names.
- ZIP total uncompressed size is capped at 1 GiB.
- Direct DLL assets are capped at 256 MiB and final downloaded size must equal GitHub release metadata before hash acceptance.
- Direct DLL asset/destination is constrained to one exact WoW-root DLL filename.
- Direct-DLL checksum text is capped at 1 MiB.
- Main-window branch-selector async work has a generation guard.
- Branch refresh/install results carry package identity/ref information and are checked before applying durable changes.
- Addon install tests already cover in-process transactional rollback, including an injected partial-new-root-commit failure.
- Self-update verifies SHA-256 before replacement and attempts executable rollback if the new installed executable cannot be launched.
- v0.3.7 runtime-confirmed branch/list fixes remain outside the current audit concern unless a robustness finding directly intersects them.

## Audit still outstanding

The cancelled all-in-one pass had not yet completed these areas:

1. **Transaction/state recovery deep pass**
   - exact crash points through backup/live commit/state save/finalize;
   - what minimum journal data is needed for deterministic restart recovery;
   - state-load validation matrix beyond identity/ownership.

2. **Network/provider + updater deep pass**
   - HTTPS/redirect invariants end-to-end;
   - rate-limit/error classification consistency;
   - malformed/partial response handling;
   - self-update handoff/cleanup edge cases.

3. **Async/UI ownership + Win32 resource pass**
   - every detached worker/result message pair;
   - close/reopen/reentrancy paths;
   - handle/GDI lifetime and silent API failures;
   - stale legacy branch-selector control/code.

4. **Tests/build-debt pass**
   - map each confirmed finding to existing tests;
   - identify missing failure-injection seams;
   - investigate `LNK4098`;
   - final severity/priority ordering.

## Recommended audit execution method from here

Do not attempt the remaining P6A audit in one giant connector/tool call. Continue in four bounded passes matching the outstanding groups above. After each pass:

- append only newly verified evidence here;
- promote concise active findings/priorities into `DEV_PROGRESS.md`;
- avoid runtime code changes until the findings list is stable enough to choose the first focused fix slice.

The first implementation slice should not be chosen solely from this scratchpad until the transaction/state deep pass has confirmed the exact recovery design and priority.
