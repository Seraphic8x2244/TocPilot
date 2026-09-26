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

## Transaction/state recovery deep pass — completed

### A1 deep-pass confirmation — HIGH — interrupted addon transactions are not restart-recoverable

The earlier A1 finding is confirmed and its failure boundary is now mapped.

**Exact process-crash windows**

- **During prepare, before any live rename:** `prepared/` and `backup/` can remain under the per-package transaction directory. Live addons and `TocPilot.json` are still old, but the next operation is blocked because `PrepareAddonInstallTransaction()` rejects any existing transaction directory.
- **After a live root is renamed into `backup/`, including the instant before that root is pushed into the in-memory `backedUpRoots` vector:** durable state is still old, but one or more old live addon roots may be absent. The only authoritative list of completed backup moves is in memory and is lost on restart.
- **After all backups but before any prepared root is moved live:** durable state is still old while old live roots can all be absent.
- **After one or more prepared roots are renamed live, including the instant before `installedRoots` is updated in memory:** live files can be partly new while durable state is still old. A moved prepared directory no longer exists under `prepared/`, so the transaction directory alone does not describe the intended complete new-root set.
- **After filesystem commit but before `SaveState()`:** live files are new and backups are old, while durable package state is still old. Correct recovery is rollback to the old live installation.
- **During the atomic state write:** `TocPilot.json` is designed to be either the old or new complete file because it is written through a flushed `.tmp` then replaced with write-through semantics. Recovery therefore has to inspect which state actually became durable.
- **After `SaveState()` succeeds but before `FinalizeAddonInstallTransaction()`:** new package state is authoritative and the new live files must be kept; only backup/transaction cleanup is required. Blind rollback here would corrupt an already committed installation.
- **During finalize cleanup:** state and live files are new-authoritative, but a partially removed transaction directory can remain and block the next operation.

The destructor rollback in `PackageInstallResult` only helps while the original process is alive. The explicit uninstall/remove paths have the same live-files -> state-save -> finalize boundary and therefore the same restart-recovery problem.

**Why the current directory layout is insufficient**

`AddonInstallTransaction::active`, `backedUpRoots`, and `installedRoots` are memory-only. The transaction directory stores prepared and backup trees but no durable transaction phase, package identity, complete intended root inventory, or relationship to the old/new state file. Filesystem inspection alone cannot reliably distinguish “rollback the filesystem to the old state” from “state already committed; keep the new filesystem and only finalize cleanup.”

**Minimum durable recovery information**

A focused recovery design can remain small. Before the first live rename, write one atomic/flushed journal containing at least:

1. a journal schema/version and transaction/package identity; for managed replacement, both the replaced and replacement identity where relevant;
2. the affected addon-root inventory, including which roots existed before commit and which roots are intended to exist after commit;
3. an unambiguous old-vs-new state discriminator. The simplest audited design is durable fingerprints of the exact pre-transaction state and intended post-transaction state, or an equivalent transaction token that is atomically persisted with the committed state;
4. enough path/root data to restore old roots from `backup/` and remove newly created roots without reconstructing intent from missing `prepared/` entries.

With that information, startup recovery can be deterministic:

- current durable state matches **pre-state** -> restore backups/remove newly installed roots, then remove the transaction;
- current durable state matches **post-state** -> keep the live install and remove transaction/backup residue;
- state matches neither -> preserve the transaction evidence, refuse automatic mutation for the affected package, and surface a recovery error rather than guessing.

Per-root “done” records are not obviously required for process-crash recovery if same-volume directory renames are atomic and recovery uses the durable intent inventory plus actual backup/live presence. They may still be useful for diagnostics. This should not be assumed for hard power-loss durability without verifying the filesystem guarantees.

**Power-loss durability gap**

`TocPilot.json` explicitly flushes and uses write-through replacement. Addon directory moves use `std::filesystem::rename` with retries and no explicit metadata flush/write-through step. The source audit therefore establishes deterministic *process-crash* recovery requirements, but exact ordering guarantees across sudden power loss are not currently proven by tests or a documented Windows/NTFS durability contract. Treat that as a verification gap when implementing recovery; do not claim full power-loss atomicity from the current rename calls alone.

**Existing coverage / missing seams**

- `tests/install_tests.cpp` exercises normal commit/finalize, explicit rollback, state-save-style rollback, and injected failure after root backup / after new-root commit.
- Those injections remain in-process and occur only after the corresponding in-memory vectors are updated. They do not simulate process death between a successful rename and bookkeeping, restart with a leftover transaction directory, crash after atomic state commit, or partial finalize cleanup.
- No startup transaction scanner/recovery entry point exists to test today.

### A2 deep-pass confirmation — HIGH — state load validates JSON shape but not the full durable package invariants

`LoadOrCreateState()` / `ParsePackages()` reject malformed JSON structure, unsupported schema, missing required strings, invalid tracking-field types, and invalid settings types. Settings also normalize bounded column widths/order, and unknown top-level/package fields are intentionally preserved.

The loaded package array is then accepted directly into `state.packages`. It does **not** re-run the semantic invariants enforced by normal mutation paths.

**Confirmed missing load-time checks**

- case-insensitive duplicate package IDs;
- conflicting addon-root ownership across durable package records;
- package ID consistency with provider/repository/source-path identity;
- supported provider/mode/target combinations;
- required branch `ref` for a persisted branch package;
- required latest-stable release policy/asset/wow-root target/target path for a persisted direct-release package;
- installed-state coherence such as non-empty installed revision requiring owned files;
- empty or structurally unsafe ownership paths;
- direct-release ownership being exactly the configured target file.

Some revision-string format validation is also absent at load time; normal provider paths validate/produce revisions before mutating state, whereas a hand-edited/corrupt state file can bypass that provenance.

**Why this matters**

Mutation APIs such as `AppendPackage()`, `ReplacePackageRecord()`, `SetPackageLatestRevision()`, and `SetPackageInstalledState()` prevent several of these invalid states from being created normally. Loading a corrupt or externally edited file can nevertheless instantiate them, after which ownership lookup returns the first matching package and later install/remove planning may encounter ambiguity only when an operation is attempted.

**Test gap**

`tests/state_tests.cpp` covers duplicate-ID rejection through `AppendPackage()`, replacement identity rules, normal ownership lookup, round-trip persistence, legacy column migration, and settings normalization. It does not load semantic-corruption fixtures for duplicate IDs, overlapping ownership, invalid provider/mode/target combinations, incoherent installed state, unsafe ownership paths, or mismatched direct-DLL ownership.

**Focused fix/test shape**

Add one centralized durable-state semantic validator used after parsing and before accepting `state.packages`. Keep forward-compatible unknown JSON fields intact; validate only invariants TocPilot must understand to operate safely. Add table-driven bad-state fixtures so the loader fails closed with a specific error before the UI treats the state as ready.

## Audit still outstanding

The cancelled all-in-one pass had not yet completed these areas:

1. **Network/provider + updater deep pass**
   - HTTPS/redirect invariants end-to-end;
   - rate-limit/error classification consistency;
   - malformed/partial response handling;
   - self-update handoff/cleanup edge cases.

2. **Async/UI ownership + Win32 resource pass**
   - every detached worker/result message pair;
   - close/reopen/reentrancy paths;
   - handle/GDI lifetime and silent API failures;
   - stale legacy branch-selector control/code.

3. **Tests/build-debt pass**
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
