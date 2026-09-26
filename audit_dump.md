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

## Network/provider + updater deep pass — completed

### A5 deep-pass confirmation — MEDIUM — self-update payload/checksum reads are not size-bound or metadata-size-verified

The earlier A5 finding is confirmed.

- `GitHubReleaseAsset` already carries GitHub's `size`, but `ReleaseInfo` drops it when `CheckLatestRelease()` copies the exact `TocPilot.exe` asset.
- `DownloadFile()` streams the updater executable until EOF with no byte ceiling and no equality check against release metadata.
- checksum fallback uses `HttpGetString()`, which appends until EOF with no cap.
- the direct-DLL path is the useful precedent: it rejects zero/over-256-MiB metadata, caps streamed bytes, and requires final downloaded size to equal GitHub release metadata; its checksum text is capped at 1 MiB.

SHA-256 still prevents an oversized/mismatched executable from being installed if the expected digest remains trustworthy, but resource exhaustion happens before that final verification.

**Test gap / focused shape**

`tests/update_tests.cpp` exercises release-tag URL parsing plus an optional live discovery path; it has no injectable download reader and therefore no oversize/metadata-size mismatch coverage. Carry asset size into `ReleaseInfo`, add a bounded stream helper with deterministic tests, and cap fallback checksum text.

### A8 deep-pass confirmation — LOW — checksum fallback parsers bind to the first arbitrary 64-hex run

Both `src/update.cpp::ResolveExpectedDigest()` and `src/direct_dll.cpp::ParseChecksumBody()` scan the entire checksum response and accept the first 64 consecutive hexadecimal characters, without line/filename association.

That can bind to the wrong digest in a multi-entry or oddly formatted sidecar. In normal GitHub release usage this is more likely to produce a false verification failure than an integrity bypass, but it is unnecessarily loose parsing for executable/DLL verification.

**Test gap / focused shape**

No tests exercise multi-entry checksum bodies, filename mismatch, comments containing a digest, or digest-only compatibility. Parse a conventional SHA-256 sidecar line and, where a filename is present, require the exact expected asset name; retain an explicitly tested digest-only one-line form if desired.

### A10 — MEDIUM — self-update asset/checksum requests do not enforce the HTTPS boundary on the initial URL

**Evidence**

- Rulebook section 9 requires HTTPS for provider/download traffic.
- GitHub release metadata is itself fetched from fixed `api.github.com` over HTTPS.
- `ParseGitHubReleaseJson()` accepts any non-empty `browser_download_url` string and performs no scheme validation.
- `CheckLatestRelease()` copies those URLs directly into `ReleaseInfo.assetUrl` / `checksumUrl`.
- `src/update.cpp::CrackUrl()` records whether the URL is HTTPS but does not reject non-HTTPS; `OpenRequest()` simply omits `WINHTTP_FLAG_SECURE` when `parts.secure` is false.
- By contrast, the direct-DLL `CrackUrl()` explicitly rejects any initial release-asset URL that is not HTTPS.
- Microsoft documents WinHTTP's default redirect policy as following redirects except HTTPS -> HTTP downgrades, so the uncovered source-level gap is the **initial URL** accepted by the self-update path, not a normal secure-to-insecure redirect. Reference: https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags

**Impact**

Normal GitHub metadata currently supplies HTTPS browser-download URLs, so this is not a reproduced production failure. It is nevertheless a confirmed violation of TocPilot's explicit security boundary and makes updater transport less fail-closed than the direct-DLL path.

The SHA-256 layer materially reduces risk when a trusted digest is present, but the fallback checksum URL uses the same permissive transport helper; transport policy should not depend on the remote metadata always remaining well-formed.

**Test gap / focused shape**

Centralize an HTTPS-only URL check for self-update asset/checksum requests and add unit coverage that rejects `http://` URLs before network I/O. Preserve WinHTTP's default HTTPS->HTTP downgrade refusal.

### A11 — MEDIUM/LOW — Update All does not propagate addon archive rate limits as queue-stop conditions

**Evidence**

- branch/status refresh errors are passed through `IsProviderRateLimitError()`; startup/status/update-all refresh stops remaining checks when it sees `rate-limited` / `rate limited`.
- direct-DLL install/update failure also passes `result->error` through `IsProviderRateLimitError()` before `CompleteUpdateAllStep()`.
- branch addon installation preparation can fail in `DownloadPackageBranchArchive()`.
- GitLab archive HTTP 429 is explicitly reported as `temporarily rate-limited`.
- GitHub codeload combines 403/429 into `refused or temporarily limited`.
- `WM_TP_PACKAGE_INSTALL_COMPLETE` handles install-preparation failure by calling `CompleteUpdateAllStep(... Failed, message)` **without** a rate-limit flag.

**Impact**

If Update All reaches a provider rate limit during archive download, TocPilot can continue attempting later addon downloads instead of stopping the provider-heavy queue as it already does during status refresh/direct-DLL resolution. This is inconsistent and can turn one rate-limit event into a run of avoidable failures.

GitHub's current combined 403/429 wording is also too ambiguous for the existing string classifier to identify a definite 429.

**Test gap / focused shape**

Keep provider status mapping structured enough to distinguish HTTP 429 from ordinary refusal, and propagate a confirmed rate-limit outcome through branch install-preparation results into `CompleteUpdateAllStep()`. Add queue tests proving a 429 stops further provider work while an unrelated download failure does not.

### A12 — LOW — updater parent-process synchronization failure is silently ignored

**Evidence**

`RunUpdaterMode()` opens the parent PID with `SYNCHRONIZE`; if `OpenProcess()` succeeds it waits indefinitely, but it does not inspect the `WaitForSingleObject()` result. If `OpenProcess()` fails, it immediately proceeds to stage/replace the target executable.

The launching process treats successful creation of the staged updater process as updater success, posts `WM_CLOSE`, and has no return channel for a later updater-mode failure.

**Impact**

Under the normal same-user path the synchronization handle should usually succeed. If it does not, the updater can race the still-running parent and rely only on the 10-second file-operation retry loop. It can then report failure after the old UI has already committed to closing for an apparently successful update.

This is a robustness/handoff gap, not a reproduced normal-runtime defect.

**Test gap / focused shape**

Make parent-wait acquisition/result explicit and fail with a clear updater message rather than silently switching synchronization strategy. A focused helper seam is needed to test OpenProcess/wait failure without depending on timing.

### Network/provider areas verified robust in this pass

- Git smart-HTTP ref discovery uses a caller-validated host, HTTPS port, `WINHTTP_FLAG_SECURE`, explicit timeouts, an 8 MiB body cap, status mapping, pkt-line bounds checks, service-header validation, object-ID validation and malformed/truncated response rejection.
- GitHub release metadata uses fixed `api.github.com` HTTPS, explicit timeouts, an 8 MiB body cap, required JSON members/types, exact asset-name matching and duplicate exact-name rejection.
- GitHub/GitLab branch archives use fixed HTTPS provider hosts, explicit timeouts, 256 MiB streaming caps, response-status checks and ZIP signature validation before extraction.
- direct-DLL release-asset/checksum requests reject non-HTTPS initial URLs; direct DLL bytes are capped at 256 MiB and must match GitHub's exact asset size before SHA-256 acceptance; checksum text is capped at 1 MiB.
- WinHTTP's documented default redirect policy disallows HTTPS -> HTTP downgrade redirects; no code in these paths overrides that policy.
- self-update replacement uses `ReplaceFileW(... REPLACEFILE_WRITE_THROUGH)` with a backup and attempts rollback if the replacement executable cannot be launched.
- post-update backup/staged-file deletion retries and then schedules stubborn files for deletion at reboot; failure to remove the now-empty staging directory is ignored and can leave benign temp-directory residue.

## Async/UI ownership + Win32 resource deep pass — completed

### A3 deep-pass confirmation — MEDIUM/LOW — the Add-Git branch dialog has a stale-result/HWND-reuse race

The dedicated `src/branch_dialog.cpp` loader is the one detached UI worker that does not carry request identity.

**Evidence**

- `ShowBranchDialog()` owns `DialogContext` on its stack, creates the popup, then runs a nested message loop until the dialog is destroyed.
- `StartLoad()` launches a detached worker carrying only the dialog `HWND`, host and repository.
- `LoadResult` carries only success/info/error — no generation, repository identity or request token.
- Cancel/close destroys the dialog immediately and `ShowBranchDialog()` can return while the worker is still running.
- A successful `PostMessageW()` transfers the raw result pointer to whatever window currently owns that handle. Microsoft explicitly documents that window handles are recycled and can identify a different window after destruction: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow
- Reopening the branch dialog quickly therefore permits the old lookup to target a newly-created branch dialog if Windows reuses the same `HWND`. The receiving dialog has no token/repository check and will accept/populate the stale repository info.

This is distinct from the main-window inline branch lookup: `BranchLoadResult` there carries a monotonically increasing generation and package ID, and `WM_TP_BRANCHES_READY` discards any result whose generation/package no longer matches.

**Impact**

The race is rare and requires close/reopen plus handle reuse, but stale branches can be shown for the wrong Add-Git repository. If the user then selects one, the wrong branch name/SHA/repository-info can flow into the new package setup; subsequent repository inspection should normally fail because that SHA is not in the new repository, but the dialog should never admit the stale result.

**Test/fix shape**

Give each branch-dialog request a request/generation identity tied to the dialog context/repository and reject any completion that does not match the live request. Prefer making worker lifetime explicit rather than relying on `HWND` validity. Add a deterministic testable result-acceptance helper; UI handle reuse itself does not need to be timing-tested.

### A4 deep-pass confirmation — MEDIUM — latest-stable DLL discovery blocks the dialog/UI thread

`src/add_package_dialog.cpp::LoadLatestStableDllAssets()` calls `FetchLatestStableGitHubRelease()` synchronously.

It is invoked from both:

- `ValidateAndAccept()` when the DLL fallback has not yet loaded matching release metadata; and
- the DLL-only fallback dialog's `WM_CREATE` path, meaning the window procedure itself can remain inside provider I/O before creation finishes.

The function updates the status text and calls `UpdateWindow()`, but the UI thread then remains inside WinHTTP work until it completes or fails. Provider timeouts therefore translate directly into an apparently frozen Add Git/DLL dialog.

**Test/fix shape**

Move release discovery to the same worker/result-message pattern as other provider operations. Disable relevant controls while loading, carry a request identity/repository, allow close/cancel without a stale result being accepted, and add acceptance-state unit tests around repository changes/close-reopen.

### A13 — LOW — the old inline branch COMBOBOX is dead UI infrastructure after the popup-selector redesign

The main window still creates `g_branchSelector` as a hidden `COMBOBOX`, and `PopulateBranchSelectorCurrent()` / `PopulateBranchSelectorLoaded()` continue resetting and filling it.

However:

- `PositionBranchSelector()` now unconditionally hides the combo;
- `IDC_BRANCH_SELECTOR` has no command-selection handling;
- actual branch selection is rendered in the list cell and opened with `CreatePopupMenu()` / `TrackPopupMenu()` using `g_branchSelectorBranches`.

The combo no longer contributes to visible behaviour or selection state. It is leftover infrastructure from the pre-popup selector and increases the number of null/enable/populate paths that future UI work must reason about.

**Fix shape**

After the audit gate, remove the hidden combo, its control ID, update/populate calls that exist solely to drive it, and the associated scroll/reposition messages/subclass plumbing if no longer used for anything else. Preserve the generation/package branch cache and list-cell popup behaviour.

### A14 — LOW — main-window close is not gated for non-install detached work

`WM_CLOSE` blocks only `g_updateAllInProgress` and `g_packageInstallInProgress`. It does not block package refresh/inspection, app-update check, or app-update download/verification.

This does not expose the live-addon transaction corruption boundary: addon install preparation sets `g_packageInstallInProgress` before its worker begins, so close is blocked through prepare/commit/state/finalize. Main async result ownership is also safe when `PostMessageW()` fails because the worker retains its `unique_ptr`.

Remaining effects are cleanup/UX oriented:

- closing during inspection can terminate the process with package staging still present; later staging reset paths are designed to clear it;
- closing during self-update before updater launch can leave temporary update staging; later cleanup already tolerates/schedules stubborn files;
- closing during a read-only refresh/check simply abandons the result.

Do not inflate this into a state-integrity defect. If shutdown behaviour is tightened later, distinguish mutating critical sections from cancelable/read-only work rather than blocking all closes indiscriminately.

### Win32/GDI ownership review — no high/medium leak found

Verified ownership pairs include:

- main `GetDC()` / `ReleaseDC()`;
- dynamically-created UI fonts are replaced/deleted and deleted on `WM_DESTROY`; failed bold-font creation safely falls back to the normal font in custom drawing;
- executable icons returned by `PrivateExtractIconsW()` are retained and destroyed with `DestroyIcon()`;
- branch popup menus are destroyed after `TrackPopupMenu()`;
- splash `GetDC` / `ReleaseDC`, compatible DC / `DeleteDC`, DIB selection restoration / `DeleteObject`, GDI+ bitmap smart pointers and GDI+ startup/shutdown are paired across the inspected success/failure paths;
- `CreateStreamOnHGlobal(..., TRUE,...)` transfers the backing `HGLOBAL` to the stream, which is released after GDI+ copies the image.

**Low-priority silent Win32 failure debt**

- many child `CreateWindowExW()` calls are not checked. Failure of a critical main control such as the package list would currently produce a partially-functional window rather than fail `WM_CREATE` cleanly;
- `SetWindowSubclass()` return is ignored. Its current scroll/reposition purpose is largely obsolete because the legacy branch combo is always hidden;
- splash `SetTimer()` return is ignored; failure primarily loses animated progress dots because phase changes themselves call `RenderSplash()`;
- nested dialog `GetMessageW()` loops treat `-1` the same as quit/exit from the loop and do not surface the Win32 error.

These are robustness debt, not evidence of a current normal-path resource leak.

### Main detached-worker ownership/identity matrix

- inline branch lookup: generation + package ID guard — good;
- package refresh: index/package/tracking fields are checked before state/UI acceptance — good;
- package inspection: index/package/ref checked before acceptance — good;
- branch addon install/replacement: package/replacement identity and tracking checked before live commit; result destructor rolls a prepared transaction back if ownership never reaches the UI — good for in-process lifetime;
- direct-DLL install: package ID/mode/asset/target checked before state advance — good;
- self-update check/update: single-operation flags prevent duplicate starts; result pointer ownership follows the same `unique_ptr` transfer pattern — good;
- dedicated Add-Git branch dialog: missing request identity — A3 above.

## Tests/build-debt + final-priority pass — completed

### Coverage map against confirmed findings

| Finding | Existing coverage | Missing seam / deterministic test needed |
| --- | --- | --- |
| **A1 HIGH — restart transaction recovery** | `install_tests` covers normal commit/finalize, explicit rollback, state-save-style rollback and injected in-process failures after backup/new-root commit | no startup scanner/recovery API; no process-death fixture; no rename-success-before-bookkeeping case; no pre-state/post-state journal decision test; no partial-finalize restart case |
| **A2 HIGH — semantic validation on state load** | `state_tests` covers round-trip, schema/settings migration/normalization and mutation-time duplicate/ownership/identity rules | table-driven persisted-state fixtures for duplicate IDs, overlapping ownership, invalid mode/provider/target/ref combinations, installed-state incoherence, unsafe ownership and direct-DLL mismatch |
| **A3 MEDIUM/LOW — branch-dialog stale result** | main-window branch selector has generation/package runtime guard | no branch-dialog acceptance helper; no token/repository mismatch unit test |
| **A4 MEDIUM — synchronous DLL release lookup** | GitHub release parsing has unit coverage | no dialog async state machine/request-identity seam; no close/repository-change stale completion tests |
| **A5 MEDIUM — self-update size bounds** | `update_tests` covers latest-tag parsing and optional live discovery; GitHub release tests preserve asset metadata including size | no bounded updater stream helper; no oversize, metadata-size mismatch or checksum-text-cap tests |
| **A6 MEDIUM — large single ZIP member allocation** | `archive_tests` covers path safety, extraction and addon-root layout detection | no per-entry policy seam/fixture that rejects a large member before allocating its full uncompressed size |
| **A7 LOW — staging-name collision** | normal staging/layout behaviour exercised incidentally | no direct test proving distinct canonical repositories cannot map to the same staging directory |
| **A8 LOW — loose checksum parsing** | direct-DLL policy and self-update discovery tests do not exercise checksum body selection | expose/factor a checksum parser; test exact filename, digest-only compatibility, multi-entry sidecars, wrong filename and comment/noise |
| **A10 MEDIUM — self-update accepts initial HTTP URL** | live self-update test expects current GitHub HTTPS URLs; direct-DLL implementation separately rejects non-HTTPS | self-update URL-policy helper with `http://` rejection before I/O |
| **A11 MEDIUM/LOW — archive 429 not stopping Update All** | `update_all_tests` covers candidate queue/order/accounting only | structured provider result or classifier seam plus orchestration test proving confirmed 429 stops remaining provider work and ordinary failure does not |
| **A12 LOW — updater parent-wait failure ignored** | no updater-process synchronization tests | injectable/process helper for OpenProcess/Wait result; deterministic failure handling test |
| **A13 LOW — hidden legacy branch combo** | no UI structural tests | no new behavioural test is required if removed mechanically; existing branch metadata/popup behaviour must remain covered by static review/runtime smoke |
| **A14 LOW — close during non-install work** | package install close gate is runtime code only | optional shutdown-policy helper tests if this is tightened; do not make it a prerequisite for higher-priority work |

Existing parser/policy tests are comparatively strong around smart-HTTP pkt-lines, GitHub release JSON, exact release-asset selection, direct-DLL package policy, archive path traversal/root mapping, update queue accounting and mutation-time state invariants. The audit should preserve those focused seams rather than replacing them with end-to-end-only coverage.

### A9 deep-pass resolution — LOW/CLEANUP — `LNK4098` is a confirmed CRT mismatch in the archive test target

The warning has a direct CMake cause.

- `cmake_minimum_required(VERSION 3.24)` puts CMP0091 in the modern runtime-library mode.
- `TocPilotMiniz` explicitly sets `MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"`, i.e. `/MT` or `/MTd`.
- the shipping `TocPilot` target explicitly uses the same static runtime and is consistent with Miniz.
- `TocPilotArchiveTests` links `TocPilotMiniz` but does **not** set `MSVC_RUNTIME_LIBRARY`.
- CMake's documented default when the property is unset is `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`, i.e. `/MD` or `/MDd`.
- Microsoft documents `LNK4098` as the expected warning when incompatible/default CRT libraries are mixed.

References:
- https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html
- https://learn.microsoft.com/en-us/cpp/error-messages/tool-errors/linker-tools-warning-lnk4098

So `TocPilotArchiveTests` is `/MD` while its linked Miniz library is `/MT`. This should be fixed by compiling that test target with the same runtime as the static library (or by applying one intentional project test-runtime policy), **not** by hiding the warning with `/NODEFAULTLIB`.

The recorded `C4100` and `C4457` warnings remain source-cleanup only.

### A15 — LOW/MEDIUM BUILD RELIABILITY — live network smoke tests are part of the ordinary blocking CTest suite

CMake registers both:

- `git-smart-http-live-github` -> `TocPilotGitRefsTests --live-github`
- `self-update-live-latest` -> `TocPilotUpdateTests --live-latest`

as ordinary tests.

Both `.github/workflows/build.yml` and `.github/workflows/release.yml` run plain:

`ctest --test-dir build -C Release --output-on-failure`

with no exclusion/label filter. Therefore every PR/push build and every release test gate depends on live GitHub/network availability and current repository/release state.

These tests are valuable provider smoke tests, but they are not deterministic unit tests. Provider outage, network interruption or rate limiting can fail an otherwise-correct source build/release.

**Focused build/test fix**

Label live tests (for example `live` / `network`) and keep the normal blocking suite offline/deterministic. Run live smoke explicitly in a separate CI step/workflow or at a controlled cadence; the release workflow may still choose to require the live smoke explicitly if that remains the desired release contract.

### Final P6A priority ordering

**P0 — durability/invariant correctness**

1. **A1 — interrupted addon transaction restart recovery (HIGH).**
   This is the only finding that can leave live managed addon files and durable package state materially divergent after an unexpected process/power interruption. It needs a durable journal/recovery decision before more surface robustness work.
2. **A2 — semantic validation of loaded durable state (HIGH).**
   Fail closed before operating on duplicated/conflicting ownership or structurally impossible package records.

**P1 — bounded-input/security/availability robustness**

3. **A6 — per-entry ZIP extraction memory bound (MEDIUM).**
   Prevent remote archive shape from provoking an uncontrolled very large contiguous allocation/process termination.
4. **A5 + A10 — self-update bounded download + HTTPS-only initial asset/checksum URLs (MEDIUM).**
   These touch the same updater transport boundary and can be implemented/tested as one focused transport-hardening slice without changing update UX.
5. **A4 — asynchronous latest-stable DLL discovery (MEDIUM).**
   Remove provider-timeout UI hangs, using request identity from the outset.
6. **A3 — generation-safe Add-Git branch dialog completion (MEDIUM/LOW).**
   Small targeted race fix; likely pairs naturally with A4's dialog request-identity helper only if doing so does not enlarge the slice.
7. **A11 — structured archive rate-limit propagation into Update All (MEDIUM/LOW).**

**P2 — cleanup / test infrastructure**

8. **A15 — separate live network smoke from deterministic default CTest (LOW/MEDIUM build reliability).**
9. **A12 — explicit updater parent wait failure handling (LOW).**
10. **A8 — strict checksum-sidecar parsing (LOW).**
11. **A7 — collision-proof staging directory identity (LOW).**
12. **A13 — remove the dead hidden branch combo/subclass/reposition plumbing (LOW).**
13. **A14 — optional shutdown cleanup policy for non-mutating workers (LOW).**
14. **A9 — align `TocPilotArchiveTests` CRT with Miniz and clean compiler warnings (LOW/CLEANUP).**

The direct-DLL no-backup/write-final policy risks documented earlier remain product-contract choices rather than audit defects and are not inserted into this implementation ranking.

### First robustness fix slice selected after the completed audit

Start with **A1 only: durable addon transaction restart recovery**.

Keep the implementation slice bounded to:

1. define a versioned transaction journal/manifest and pure recovery-decision model;
2. write/flush the journal before the first live addon rename, carrying transaction/package identity, complete affected-root intent and pre/post durable-state discriminators;
3. add startup recovery scanning before normal package operations become available;
4. if durable state matches pre-state, restore old roots/remove new roots and clean the transaction;
5. if durable state matches post-state, preserve new live roots and finish transaction cleanup;
6. if neither matches, fail closed for that transaction and preserve evidence rather than guessing;
7. add deterministic tests for each crash window identified in the transaction deep pass, including rename-before-in-memory-bookkeeping and state-save-before-finalize boundaries.

Do **not** combine A2, UI cleanup, updater hardening or unrelated warning cleanup into this first runtime slice. Power-loss filesystem metadata guarantees remain a separately stated verification limit; the first implementation target is deterministic restart recovery from observable durable state, not an unsupported claim of full hardware-loss atomicity.

## P6A bounded audit complete

All four bounded audit passes are complete. This file remains detailed scratch/history only; `DEV_PROGRESS.md` is authoritative for the live finding list, priority and exact next step.
