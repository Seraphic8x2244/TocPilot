# TocPilot Development Plan

## 2026-09-23 release status: v0.3.1 published

`v0.3.1` is published from merge commit `7222280319bc4ff2f46eac58dc3686ead78e8895`. Release workflow run `35903180687` rebuilt that exact `main` commit, passed all 16 CTest tests, generated the SHA-256 sidecar, created/verified the `v0.3.1` tag, and published direct `TocPilot.exe` plus `TocPilot.exe.sha256`. The application self-updater can therefore consume this build through its normal latest-stable GitHub Release path.

The release includes the Add Git shallow repository-library flow and the managed addon-root collision replacement gate. Same-root collisions with an existing single-root TocPilot package are resolved by an explicit overwrite/cancel prompt; no duplicate durable package record is created. Replacement is staged while the old package remains authoritative, and ownership/state swaps only after the live transaction commits. State-save failure rolls the filesystem back. Unmanaged live addon roots and partial replacement of multi-root packages remain refused.

Runtime validation still required: automatic `v0.3.0 -> v0.3.1` startup self-update, overwrite/cancel against a real same-root addon collision, and the remaining `Cabro/Atlas` repository-library cases.

## 2026-09-23 implementation status: v0.3.1 managed-root replacement

The Add Git collision gate is implemented and Windows x64 Release CI-green on source head `71181a629bb9782553a0f296e0d38f31b7137d6d` (Build run `35902647161`, 16/16 tests). Before Add Git persists a package, it now checks whether the prospective `Interface\\AddOns\\<root>` is already owned by another TocPilot package. A single-root managed collision presents an overwrite/cancel decision. The overwrite path stages the new source while the old package remains authoritative, uses the old owned-file set as prior transaction ownership, commits the live replacement, and only then swaps the package record in-place. If state save fails, the live transaction rolls back and the old package record remains authoritative. Unmanaged live addon folders are still refused, and multi-root managed packages are not partially overwritten.

Repository-library selections without collisions keep the existing independent-child queue behavior. A library batch containing a managed-root collision is stopped before any new records are saved; the colliding child must be selected by itself to use the overwrite path. This keeps package ownership non-overlapping without broadening the collection model in this patch.

The source and release marker are now `v0.3.1`. The feature branch's earlier artifact-only handoff was not a product release. After merge, the existing Release workflow must rebuild/test the merged commit and publish direct `TocPilot.exe` plus checksum as GitHub Release `v0.3.1`, which is the artifact the startup self-updater consumes.

## 2026-09-23 implementation checkpoint: addon-root collision replacement

The repository-library implementation is runtime-gated on one ownership correction before release. A newly added package must not be persisted alongside an existing managed package when both resolve to the same live addon install root. Add Git should detect that collision before committing new package state or beginning installation, then offer an explicit overwrite/replace choice or cancel. Replacement must leave exactly one owner for the install root, remove or transfer the superseded package record coherently, and keep state/filesystem rollback semantics correct if installation fails.

The last fully green code baseline remains `2a43e2f9ec7eae25d56488ed3cba589bdf42971e`; branch commits after it through the checkpoint are documentation-only. After implementing and testing this gate, the next `0.3.x` build must go through the existing GitHub Release workflow rather than being handed off only as a pull-request artifact, because TocPilot self-update discovers direct `TocPilot.exe` assets from published GitHub releases.

## 1. Project summary

**TocPilot** is a lightweight, portable World of Warcraft addon and release manager.

The program is designed specifically around the way Vanilla WoW installations are actually used rather than around general-purpose Git repository management.

The intended deployment model is deliberately simple:

```text
World of Warcraft/
├─ WoW.exe
├─ TocPilot.exe
└─ Interface/
   └─ AddOns/
```

The user copies `TocPilot.exe` into a WoW installation. TocPilot detects `WoW.exe` beside itself and therefore knows both the WoW root and `Interface\AddOns` without requiring directory setup.

If a second WoW installation needs management, copy TocPilot into that installation as well. TocPilot will not implement multi-installation profiles in the initial product.

TocPilot manages **packages**, not local Git repositories. A package can be:

- a GitHub or GitLab repository branch;
- a supported GitHub or GitLab release asset;
- a source-revision archive used internally for addon installation;
- a standalone file such as a DLL installed into the WoW root;
- potentially other simple downloadable artifacts later.

This removes the need for hidden `.git` directories and avoids local Git branch/upstream corruption while retaining the branch-following workflow that motivated the project.

---

## 2. Product goals

### Primary goals

1. **Portable**
   - No installer required.
   - Copy one EXE into the WoW directory.
   - No Qt, .NET, Java, Python, Git, or other separately installed runtime should be required.
   - Configuration/state stays local to that WoW installation.

2. **Small**
   - Native Windows application.
   - Target a low-single-digit-megabyte release, preferably smaller.
   - Size is a design goal, not a reason to compromise reliability.

3. **Self-updating**
   - This is the highest-priority feature.
   - The first usable TocPilot build must be able to update itself before addon management grows complicated.
   - Update replacement must explicitly handle Windows executable locking and must not repeat GitAddonsManager's false-failure race.

4. **Branch-aware**
   - A user can select a branch such as `master`, `development`, `vanilla`, or another remote branch.
   - TocPilot follows that branch's remote commit.
   - Switching branches must not require or create a local Git repository.

5. **Release-aware**
   - Browse repository releases.
   - Follow the latest stable release.
   - Select a supported release asset when multiple assets exist.
   - Support exact standalone files such as `ClassicAPI.dll`.
   - Do not download or unpack user-uploaded release ZIP/archive assets to locate executable files.

6. **Safe package ownership**
   - TocPilot records which files it installed.
   - Remove/update operations should affect only files owned by that managed package.
   - The removal confirmation should be compact by default with an expandable file list.

7. **Simple UI**
   - Clear, dense desktop UI designed for this single task.
   - Readable text-size control.
   - Consistent widths and predictable layout.
   - No dynamic branch-dropdown widths based on longest hidden branch names.

### Secondary goals

- Import/export managed package definitions.
- Detect local modifications before destructive updates.
- Rollback/recovery for failed package installs.
- Optional prerelease tracking.
- Better package discovery and automatic install-layout detection.
- Optional authenticated API access if rate limits or private repositories become relevant.

---

## 3. Explicit non-goals

The initial TocPilot design will **not** attempt to be:

- a general Git client;
- a Git working-tree manager;
- a multi-game addon manager;
- a CurseForge/Wago client;
- a multi-WoW-directory profile manager;
- a source-code editor;
- a general package manager for arbitrary Windows software;
- a downloader/unpacker for user-uploaded release ZIP, 7z, RAR, installer, or bundle assets used to locate DLL/executable payloads. Direct DLL management requires an exact standalone DLL release asset.

It also does not need to preserve Git history locally. TocPilot only needs enough remote metadata to answer:

- what package is this?
- which branch/release does the user follow?
- what revision is currently installed?
- what revision is currently available?
- which local files belong to the package?

---

## 4. Technology direction

### Language and platform

Preferred implementation:

- **C++20**
- **Win32 desktop UI**
- **CMake**
- **MSVC**
- **x64 Windows** initially
- static Microsoft C/C++ runtime where practical

The Vanilla WoW client being 32-bit does not require the manager itself to be 32-bit.

### Windows APIs

Use operating-system functionality instead of bundling large frameworks:

- **WinHTTP** for HTTPS/API/download operations;
- **BCrypt** or another Windows cryptography API for SHA-256;
- standard Win32 file/process APIs for atomic replacement and package installation;
- Common Controls / native Win32 controls for the UI;
- Shell APIs only where they materially improve usability.

### Small vendored libraries

Prefer tiny, auditable dependencies rather than frameworks.

Likely candidates:

- **miniz** for ZIP extraction;
- **yyjson** for JSON parsing/writing.

These are implementation preferences rather than immutable requirements. The first implementation should confirm that they keep the binary simple and portable.

### Dependencies to avoid

Do not bundle:

- Qt;
- libgit2;
- Git;
- Chromium/WebView unless a future feature absolutely requires it;
- a managed runtime solely for UI convenience.

---

## 5. Local layout and portability

TocPilot runs from the WoW root.

### Startup validation

On startup:

1. resolve the executable's own directory;
2. verify that `WoW.exe` exists in that directory;
3. verify or create `Interface\AddOns`;
4. if `WoW.exe` is missing, show a concise error explaining that `TocPilot.exe` should be placed beside `WoW.exe`.

No directory chooser is needed for the normal workflow.

### Local state

Initial preference:

```text
World of Warcraft/
├─ WoW.exe
├─ TocPilot.exe
├─ TocPilot.json
└─ Interface/
   └─ AddOns/
```

`TocPilot.json` contains package definitions and installed-state metadata.

Temporary downloads/extraction should use the Windows temporary directory where practical so the WoW folder is not cluttered with caches.

A separate data directory should only be introduced if state becomes too large or structurally awkward for one JSON file.

---

## 6. Package model

TocPilot's central abstraction is a **package**.

A package is independent of how its files are obtained.

Conceptual record:

```json
{
  "id": "stable-internal-id",
  "name": "ExampleAddon",
  "provider": "github",
  "repository": "owner/repository",
  "mode": "branch",
  "ref": "master",
  "asset": null,
  "target": "addons",
  "installed_revision": "commit-or-release",
  "installed_files": []
}
```

This is illustrative; the final schema can differ.

### Source/provider

Initial providers:

- GitHub
- GitLab

Provider parsing should accept common repository URLs and normalize them into a host/repository identity.

### Package modes

#### Branch mode

Tracks the HEAD commit of a selected remote branch.

Stored state includes:

- repository;
- branch name;
- installed commit SHA;
- install mapping;
- installed file list.

No `.git` directory is created.

#### Release mode

Tracks repository releases.

Possible tracking policies:

- latest stable release;
- latest release including prereleases;
- pinned release;
- selected asset pattern/name.

Stored state includes:

- release tag/version;
- selected asset;
- install mapping;
- installed file list.

### Asset/install types

At minimum:

- provider-generated branch/source ZIP -> one or more addon directories;
- exact standalone DLL release asset -> WoW root;
- direct file -> explicitly selected safe relative destination.

User-uploaded release ZIP/archive assets are not an install type. TocPilot does not download or unpack them to locate executable payloads.

Never allow a remote archive to write outside the WoW root.

### Repository inspection and source classification

**Implementation status — 2026-09-23:** implemented on `feature/repository-libraries`. Add Git now stages the selected branch first, runs a dedicated root + one-level shallow classifier, persists explicit root/child source selection, presents the repository-library selector for multiple immediate child addons, and only reaches GitHub latest-stable standalone-DLL fallback after a no-addon result. The older recursive archive detector remains for post-selection install validation and is no longer the library classifier. Windows x64 Release CI passed on code baseline `2a43e2f9` with 16/16 CTest tests passing; real-repository runtime testing is the remaining gate before merge/release.


The Add Git flow should evolve from manual source-mode selection toward a small deterministic inspection pipeline.

For the first implementation, addon discovery is intentionally shallow. After removing the provider-generated archive wrapper, TocPilot inspects only:

- `.toc` files directly in the repository root; and
- `.toc` files directly inside each immediate child directory.

Do **not** recursively search arbitrary repository depth when deciding whether a repository is an addon or repository library. Deeper recursion can find embedded libraries, test fixtures, archived copies, modules, or other non-installable material and would make automatic classification unpredictable. Once an addon root has been selected, TocPilot still installs/copies that selected root's full subtree normally.

The Add Git decision tree is:

1. resolve the selected branch/revision;
2. inspect repository root plus one directory level for addon `.toc` files;
3. if a root-level `.toc` exists, classify the repository as a **root addon**;
4. if no root-level `.toc` exists and exactly one immediate child contains a `.toc`, classify it as a **single nested addon**;
5. if no root-level `.toc` exists and multiple immediate children contain `.toc` files, classify it as a **repository library** and let the user select one, several, or all detected addon roots;
6. if no addon `.toc` is found, then and only then check the latest stable release for a supported exact standalone `.dll` asset;
7. if a supported standalone DLL is found, offer the existing direct-DLL package path;
8. otherwise return a concise **No addon or supported DLL found** result.

If root-level addon files and immediate-child addon roots coexist, treat the layout as mixed/ambiguous and present it explicitly rather than silently guessing which roots belong together.

The DLL fallback must preserve the existing security boundary: exact standalone `.dll` release assets only. TocPilot must not inspect ZIP/7z/RAR/installer/bundle release assets for executable payloads.

The DLL fallback remains GitHub-only for now because GitLab release support is still out of scope. A GitLab repository with no detected addon roots should stop at a concise no-addon result rather than adding GitLab release handling as part of this rework.

This shallow classifier is an Add Git/discovery rule. The existing recursive archive detector may remain internally for legacy install validation where needed, but it must not drive repository-library classification.

#### Concrete repository-library example: Cabro/Atlas

`https://github.com/Cabro/Atlas` is a representative repository library. Its `master` branch has three sibling top-level addon roots:

- `Atlas/Atlas.toc`
- `AtlasLoot/AtlasLoot.toc`
- `AtlasQuest/AtlasQuest.toc`

TocPilot should classify this shape as a repository library rather than collapsing the entire repository into a single opaque addon package. The repository/branch is the shared source container, while each detected top-level addon root is a selectable install unit. The UI should be able to offer one, several, or all detected addons from that repository.

A repository library is therefore distinct from a normal multi-root package whose addon folders are inseparable parts of one logical package. For the first implementation, multiple immediate child directories with direct `.toc` files are the structural signal for a library. TocPilot should not assume those siblings are dependencies of one another merely because they share a repository. Dependency metadata in their `.toc` files can be shown or used for warnings later, but collection membership and addon dependency are separate concepts.

---

## 7. Branch support

Branch management is a core feature rather than an advanced afterthought.

### Required behaviour

For a repository:

1. query available branches from the provider;
2. display them in a consistent-width selector;
3. record the selected branch;
4. query its current remote commit SHA;
5. compare that SHA with the installed SHA;
6. download a fresh archive only when required;
7. safely replace files owned by that package;
8. record the new SHA.

### Branch switching

Switching from branch A to branch B should:

1. resolve branch B's current SHA;
2. stage branch B's archive;
3. determine the install file set;
4. remove obsolete files owned by branch A;
5. install branch B;
6. commit local state only after installation succeeds.

The old installation must not be irreversibly removed before the new package has been downloaded and validated.

### Why no local Git

TocPilot does not need:

- refs;
- upstream configuration;
- merge bases;
- local commits;
- fetch state;
- working-tree branch checkout.

The remote SHA plus the managed-file manifest supplies the state TocPilot actually needs.

---

## 8. Releases and DLL management

Release support should be first-class.

### Release browser

For a repository, TocPilot should be able to display:

- release tag/name;
- publish date;
- stable/prerelease status;
- available assets;
- asset filename and size.

The user can choose an asset and choose whether to:

- follow future latest stable releases;
- follow prereleases;
- remain pinned to the selected release.

### DLL example

A package such as a standalone API DLL can conceptually be:

```text
Source: GitHub repository
Mode: Latest stable release
Asset: ClassicAPI.dll
Target: WoW root
Installed: vX.Y.Z
Latest: vX.Y.Z
```

DLLs remain normal TocPilot packages for discovery/status/UI purposes, but their install path is deliberately different from addon/archive packages.

For the first direct-DLL milestone:

- GitHub only;
- latest stable release only;
- the user selects one exact `.dll` asset name;
- the configured destination is the exact approved filename in the WoW root;
- future releases must contain that same asset name; TocPilot must not guess when an asset is renamed or ambiguous;
- the first time a DLL package is managed, show an explicit trust warning identifying the repository, asset, and destination;
- explain that DLLs contain executable code, security software may block/quarantine them, and TocPilot will not alter antivirus settings.

Direct DLL installation is an intentional exception to archive staging. TocPilot should download the selected release asset directly to its configured final DLL path, without creating a staged/temp/renamed DLL or a `.bak` DLL. This is specifically to preserve user-controlled exact-path antivirus configuration where present, not to evade antivirus scanning.

TocPilot must never add exclusions, disable security software, or automatically terminate WoW. The DLL must be writable before update. After download, verify the completed file against the expected release digest/checksum when available and only then advance installed state. If the write is blocked, the file disappears, or verification fails, leave package state unchanged and report that the file may be in use or security software may have blocked it.

### First direct-DLL milestone implementation status — 2026-09-22

The first GitHub direct-DLL slice is implemented on `main` through `3da9d6259a2022c8efa87b8c99aa7cf8e0ae8fd7` and is the `v0.1.36` release candidate. Build run `35772084683` passed the Windows x64 Release build, complete CTest suite, and executable artifact upload.

Implemented behaviour follows the rules above: resolve latest stable first; present filename-safe `.dll` assets and require explicit exact selection; show the first-manage trust warning; persist release policy/asset/target path in schema 1; require the exact asset on future releases; write directly to the exact WoW-root DLL filename without a staged/temp/renamed/backup DLL; never change antivirus settings; verify size and SHA-256 before recording installed state; surface missing/renamed/unverifiable assets as Needs attention; and include installed DLL packages in startup status scanning and Update New.

For checksum verification TocPilot uses a valid GitHub release-asset `digest` when present, otherwise it requires the exact `<asset>.sha256` release asset. The DLL install is refused if TocPilot cannot obtain an expected SHA-256.

The direct-DLL path is runtime-proven on published `v0.1.37` with ClassicAPI and Nampower. Direct-DLL removal behavior remains as currently implemented; TocPilot does not add extra second-guessing around an explicit user action solely because the package is a DLL.


### Release archive assets

User-uploaded release ZIP, 7z, RAR, installer, and bundle assets are intentionally unsupported for executable/DLL management. TocPilot will not download, inspect, unpack, or offer an override for these assets to locate executable files.

Provider-generated branch/source archives are a separate package path and remain supported for constrained addon installation into `Interface\\AddOns`.

---

## 9. Addon/archive layout detection

Provider-generated repository/source archives vary.

TocPilot should not assume every source archive has exactly one folder with exactly one addon.

### Detection strategy

After secure extraction to a staging directory, separate **repository classification** from **install validation**.

For Add Git classification:

1. ignore the provider-generated wrapper/root directory;
2. inspect the repository root for `.toc` files;
3. inspect only immediate child directories for `.toc` files directly inside them;
4. classify root addon / single nested addon / repository library from that shallow shape;
5. do not recursively discover deeper addon candidates for automatic library classification;
6. if no addon is found, allow the GitHub-only latest-stable standalone-DLL fallback described above.

For installation, TocPilot may still recurse inside the selected addon root to copy and validate its contents. Existing recursive candidate logic may also remain as a compatibility/validation tool for already-managed packages, but it should not silently broaden the Add Git classification result.

Common repository shapes should require no manual intervention.

### Multiple addon folders

A single managed package may own several addon directories, for example:

```text
Interface/AddOns/Example
Interface/AddOns/Example_Config
Interface/AddOns/Example_Options
```

They remain one package/update source.

### Path security

Archive extraction must reject:

- absolute paths;
- drive-qualified paths;
- `..` traversal;
- paths that resolve outside the staging/install root.

This is mandatory.

---

## 10. Installed-file ownership

Each package should record the files it installed.

This enables:

- precise removal;
- removal of obsolete files during upgrades;
- compact "what will be removed?" previews;
- future local-modification detection;
- avoiding deletion of unrelated files in shared locations.

### Removal UI

Default confirmation:

```text
Remove ExampleAddon?

This will remove 3 addon folders / 142 files.

[Show files]   [Cancel] [Remove]
```

The file list is collapsed by default.

### Future local-edit protection

A later version can store hashes for installed files.

Before overwrite/remove:

- unchanged files can be safely replaced;
- locally modified files can trigger a warning or backup.

This is useful but is not required for the first addon-install milestone.

---

## 11. Self-update — Priority 0

Self-updating is the **first implementation milestone**.

It must be working before TocPilot grows enough features that manual copy-over testing becomes annoying.

### Release format

GitHub releases for TocPilot should provide a direct:

```text
TocPilot.exe
```

asset.

The updater should not require a ZIP merely to replace one executable.

### Update discovery

At startup or on manual request:

1. query the latest GitHub release for `Seraphic8x2244/TocPilot`;
2. read the release tag;
3. compare it with the compiled current semantic version;
4. find the `TocPilot.exe` asset;
5. obtain the asset SHA-256 digest from GitHub release metadata where available;
6. report update availability.

The startup check runs under the startup splash. If it finds a newer stable TocPilot release, TocPilot automatically downloads and verifies that exact release, launches the existing updater/replacement handoff, exits the old process, and relaunches the updated version **before addon/package status scanning continues**. A manual update check outside startup remains non-automatic: it reports availability and waits for the user to choose Update.

If an automatic startup update fails before the updater handoff succeeds, the current executable remains runnable, addon/package status scanning may continue, and the splash must finish in an explicit update-failed state rather than implying TocPilot is current. The normal TocPilot update window remains the manual retry path.

### Download

When an update is applied automatically at startup or the user chooses Update manually:

1. download the new `TocPilot.exe` to a temporary path;
2. calculate SHA-256;
3. compare with the expected digest;
4. refuse replacement on a digest mismatch;
5. preserve the currently running executable until the new one is validated.

### Windows-safe replacement

Do **not** attempt to overwrite the currently running executable.

Recommended process:

1. current TocPilot downloads and verifies the new EXE into the Windows temporary directory;
2. current TocPilot starts the downloaded new EXE in a special updater mode, passing:
   - current process ID;
   - real TocPilot path;
   - working directory;
3. current TocPilot exits;
4. updater-mode process waits for the old process handle to signal termination;
5. only after confirmed termination, copy the downloaded binary to a replacement path beside the target;
6. use bounded retries for antivirus/filesystem transient locks;
7. atomically replace `TocPilot.exe`, retaining a temporary backup until success is confirmed;
8. launch the newly installed `TocPilot.exe`;
9. clean up update staging/backup files on successful startup.

This specifically avoids the race observed in GitAddonsManager, where the replacement process began moving files before the old process had completely released them.

### Failure behaviour

A failed update must leave a runnable old TocPilot.

Never:

- delete the old executable first;
- report success before replacement is complete;
- treat an intermediate retry as a permanent failure;
- leave the user with only a partially downloaded EXE.

The updater helper can display a native error dialog if the main UI has already exited.

### Version bootstrap test

The first development sequence should intentionally test self-update before addon logic:

#### v0.1.0

- launches;
- validates WoW-directory placement;
- displays current version;
- checks GitHub for updates;
- can download/apply a new TocPilot executable.

#### v0.1.1

- minimal change, e.g. displayed build text;
- published as the next GitHub release;
- v0.1.0 must upgrade to v0.1.1 entirely through TocPilot.

Only after this test passes should package-management development become the main focus.

---

## 12. Network/provider architecture

Provider-specific remote logic should sit behind a small common interface.

Conceptual capabilities:

- normalize repository URL;
- fetch repository metadata;
- list branches;
- resolve branch -> commit SHA;
- download branch archive;
- list releases;
- list release assets;
- resolve/download selected release asset.

### GitHub

Use GitHub's HTTPS API for metadata.

Unauthenticated public API access is sufficient for the MVP, but requests should be economical.

Use:

- conditional requests/ETags later if useful;
- a clear User-Agent;
- reasonable timeouts;
- explicit handling for rate-limit responses.

### GitLab

Implement the same conceptual operations against GitLab.

Initial focus can be public `gitlab.com` repositories. Generic/self-hosted GitLab can be considered later.

### Authentication

Private repositories/tokens are deferred.

If later added:

- secrets must not be written casually into logs;
- Windows Credential Manager is preferable to plaintext tokens in `TocPilot.json`.

---

## 13. UI direction

Use native Windows controls and keep the design intentionally compact.

### Main window

Initial conceptual layout:

```text
TocPilot                                     v0.x.x
World of Warcraft: D:\Games\WoW

[Update All] [Refresh] [Add Package] [Import/Export]

Name          Source / Track       Installed       Latest       Status
-----------------------------------------------------------------------
pfUI          master               a1b2c3d         a1b2c3d      Current
SomeAddon     vanilla              123abcd         456def0      Update
ClassicAPI    Latest release       v1.4.0          v1.5.0       Update
```

Exact UI should evolve through use rather than being over-designed before the package engine works.

### Text sizing

Provide a persisted UI text-size preference.

Controls should size to content rather than assuming a fixed height that clips larger text.

### Columns

Use real native list/table columns rather than imitating columns with unrelated row layouts.

Desired eventual behaviour:

- consistent widths;
- reorderable columns;
- lock/unlock layout;
- persisted order/width;
- sensible defaults.

### Add-package flow

The simplest useful flow:

1. paste GitHub/GitLab repository URL;
2. TocPilot identifies provider/repository;
3. show:
   - Branches
   - Releases
4. choose tracking mode;
5. if release: choose asset/tracking policy;
6. preview detected install destination;
7. Install.

### Status clarity

Avoid modal spam.

Rows should clearly distinguish:

- Current
- Update available
- Checking
- Downloading
- Installing
- Error
- Locally modified (future)

---

## 14. Configuration and state schema

The local state file should be versioned.

Top-level conceptual structure:

```json
{
  "schema": 1,
  "settings": {
    "text_scale": 1.0,
    "check_app_updates": true
  },
  "packages": []
}
```

Requirements:

- writes should be atomic;
- preserve a backup during schema migrations;
- unknown/new fields should not cause destructive reset;
- never mark a package update installed before filesystem commit succeeds.

### Atomic state write

Write:

```text
TocPilot.json.tmp
```

flush/close it, then atomically replace the previous `TocPilot.json`.

Keep a short-lived backup during migrations where appropriate.

---

## 15. Install transaction model

Addon/archive package updates should use staging and commit semantics rather than directly extracting over the live addon.

Direct DLL packages are the deliberate exception: they write the verified release payload to the exact approved final DLL path, with no staged/temp/renamed DLL and no automatic antivirus configuration. This trades some crash-atomicity for compatibility with exact-path security-software exclusions and keeps the behaviour simple and explicit.

### Proposed transaction

1. resolve desired revision;
2. download;
3. validate HTTP result and expected content;
4. securely extract to temp staging if archive;
5. determine managed files/install mapping;
6. check for path collisions or unsafe paths;
7. prepare backup/rollback information for files being replaced;
8. install new files;
9. remove obsolete files previously owned by this package;
10. update local state;
11. remove staging/backup after success.

If installation fails before step 10, local package state must continue to describe the old installation.

---

## 16. Concurrency

Keep the first implementation simple.

Requirements:

- UI network/download work must not freeze the window;
- do not update the same package concurrently;
- Update All may eventually use limited parallel downloads, but sequential install commits are acceptable;
- application self-update must not run while package installation is mid-commit.

Correctness is more important than maximum download concurrency.

---

## 17. Logging and diagnostics

Normal use should not produce noisy logs.

On errors, diagnostics should include enough information to debug:

- operation;
- provider URL;
- HTTP status;
- package/revision;
- filesystem operation/path;
- Windows error code where applicable.

A simple `TocPilot.log` can be created/rotated when needed.

Never log authentication secrets if token support is added later.

---

## 18. Security/reliability requirements

Minimum requirements:

- HTTPS for providers/downloads;
- SHA-256 verification for TocPilot self-updates;
- archive path traversal protection;
- package install destinations constrained to WoW root;
- no shell-command construction from untrusted repository filenames;
- bounded file-operation retries;
- transactional/rollback-minded updates;
- atomic configuration writes.

Code signing is desirable later but not required for the initial prototype.

Unsigned builds may trigger Windows reputation/SmartScreen warnings; this is separate from the update integrity checks.

---

## 19. Build and release

### GitHub Actions

Use Windows GitHub Actions to produce the release build.

Initial workflow should:

1. configure CMake;
2. build Release x64;
3. run available unit tests;
4. verify the executable starts/version resource where automation permits;
5. upload `TocPilot.exe` directly as a build artifact.

Release workflow should attach:

```text
TocPilot.exe
```

to the GitHub release.

Later optional assets:

- checksums;
- symbols/debug archive;
- source/dependency notices.

### Versioning

Use semantic versions.

Milestone interpretation:

- `0.1.x` — bootstrap, self-update, and basic package-management foundations;
- `0.2.x` — artwork/UI phase. This work was historically completed while releases still carried `0.1.x` numbers, so no published `0.2.x` series is required retroactively;
- `0.3.x` — direct DLL management, GitLab support, and repository-collection work.

`v0.3.0` is the first release adopting this milestone numbering explicitly. Repository collections are part of the `0.3.x` scope but are not pulled into the first GitLab branch implementation unless separately designed and agreed.

Remain in `0.x` while core install/state formats can still change substantially.

A future `v1.0.0` should mean that the package model, state schema, update mechanism, and normal GitHub/GitLab workflows are considered stable.

---

## 20. Development milestones

### P0 — Bootstrap and self-update

Highest priority.

Deliver:

- native window/application skeleton;
- locate own executable directory;
- verify `WoW.exe`;
- compiled/displayed version;
- GitHub latest-release check;
- direct `TocPilot.exe` download;
- SHA-256 verification;
- safe two-process self-replacement;
- restart into new version;
- GitHub Actions x64 build/release pipeline.

Exit criterion:

> A manually installed `v0.1.0` successfully updates itself to `v0.1.1` without manual file copying and without false failure warnings.

### P1 — Local state and basic UI

Deliver:

- `TocPilot.json`;
- atomic state save/load;
- main package list;
- text-size preference;
- refresh/update status model;
- provider URL parsing.

### P2 — GitHub branch packages

Deliver:

- paste GitHub repository;
- list branches;
- select/follow branch;
- resolve SHA;
- download branch archive;
- secure ZIP extraction;
- detect addon folder(s);
- install to `Interface\AddOns`;
- record installed files/revision;
- update/remove.

Exit criterion:

> The branch workflows currently used in GitAddonsManager can be performed without any local `.git` directory.

### P3 — GitHub releases and direct assets

First milestone — direct DLL management:

- generalise the existing TocPilot self-update release parser into reusable GitHub release/asset metadata;
- latest stable release tracking;
- exact release-asset selection;
- persist release policy, exact asset name, target path, installed/latest release identifiers, and owned file;
- first-manage DLL trust warning;
- direct download to the exact approved WoW-root DLL path with no staged/temp/renamed DLL;
- verify the completed file against GitHub's asset digest/checksum when available before updating installed state;
- integrate DLL status with startup scanning and **Update New**;
- fail clearly when the DLL is in use or security software blocks/removes it; never change antivirus settings automatically.

Later P3 expansion:

- prerelease tracking;
- broader direct-file assets and destinations;
- richer release browser/details.

Intentional P3 non-goal:

- user-uploaded release ZIP/archive installation for DLL/executable payloads. TocPilot will not download, inspect, extract, or provide an override for these archives. This restriction does not affect provider-generated source/branch archives used by the constrained addon installation path.

Exit criterion:

> A package such as a release-provided DLL can be explicitly trusted once, tracked, checked, and updated alongside normal addons without TocPilot creating secondary DLL files or altering antivirus configuration.

### P4 — GitLab

Implement public `gitlab.com` support incrementally.

First slice:

- normalize public GitLab repository URLs;
- list/select branches;
- resolve branch HEAD commit;
- download the selected branch archive;
- reuse the existing secure addon archive inspection/install transaction;
- integrate startup/Refresh/update tracking without changing GitHub behavior.

First-slice implementation status — 2026-09-23:

- public `gitlab.com` repository URL normalization supports nested groups;
- the branch picker and inline branch selector use the provider-generic Git smart-HTTP branch list/HEAD resolver;
- GitLab branch archives download from the public repository archive API at the exact resolved commit SHA, with LFS blob expansion disabled;
- GitLab archives reuse the existing secure ZIP inspection, addon-root mapping, ownership, staging, rollback, and install transaction;
- package state mutators, startup/Refresh, install, **Update New**, and **Update All** now accept GitLab branch packages alongside GitHub branch packages;
- implementation is complete through `ce6e708848b6ac776732a6753cbe60cedf43ec39`;
- runtime validation in a real WoW installation remains pending.

Later parity, only after the branch slice is runtime-proven:

- supported releases/direct assets where provider API and TocPilot's existing trust model permit;
- no user-uploaded release ZIP/archive executable handling.

### P5 — UX and safety refinement

Deliver:

- compact removal dialog + expandable file list;
- column persistence/reordering/locking;
- import/export;
- package-edit flow;
- better ambiguous archive mapping;
- optional local-modification detection and backups;
- richer diagnostics.

---

## 21. Testing strategy

### Self-update tests

Runtime checkpoint: automatic startup replacement from installed `v0.1.37` to published `v0.3.0` passed user runtime testing on 2026-09-23.

Mandatory before package work:

- current version == latest -> no update;
- newer version exists during startup -> update is automatically downloaded/verified/applied and the updated TocPilot relaunches before addon scanning;
- newer version exists during a manual check -> update is shown but not applied until the user chooses Update;
- startup automatic-update failure -> current install stays runnable, failure is explicit on the splash, and the manual retry path remains available;
- download interruption -> current install untouched;
- digest mismatch -> replacement refused;
- old process deliberately held open -> updater waits/retries;
- replace failure -> old executable remains runnable;
- successful replacement -> new version launches;
- stale temp/backup files -> cleaned safely;
- paths containing spaces;
- non-system drive such as `D:\Games\WoW`.

### Package tests

Maintain test fixtures for:

- one-addon repository;
- repository with multiple addon folders;
- branch with long branch name;
- branch switch that removes obsolete files;
- provider-generated source ZIP;
- direct DLL asset;
- nested archive;
- malicious `../` archive path;
- failed download;
- partial extraction;
- file collision;
- missing `.toc`;
- GitHub API rate-limit response;
- GitLab equivalent cases.

### Manual Vanilla WoW validation

Test with a real WoW 1.12 installation before considering major milestones complete.

---

## 22. Design decisions already made

These are intentional unless later testing proves them wrong.

1. **One TocPilot instance per WoW directory.**
   - No multi-directory manager/profile system.

2. **No local Git repositories.**
   - Branch/release metadata comes from provider APIs.

3. **Packages, not repos, are the abstraction.**
   - This lets addons, multi-addon archives, releases, and DLLs use the same engine.

4. **Self-update before addon management.**
   - Development speed depends on making test builds painless.

5. **Direct EXE self-update asset.**
   - No need to unpack a ZIP merely to update TocPilot itself.

6. **Native Windows UI/runtime.**
   - Avoid the ~70 MB framework deployment observed in the Qt-based predecessor.

7. **Portability over centralized configuration.**
   - Each WoW folder owns its TocPilot state.

---

## 23. Deferred ideas

Useful, but not early priorities:

- code signing;
- private repository authentication;
- self-hosted GitLab;
- GitHub token configuration;
- automatic backups of locally modified addon files;
- delta downloads;
- rollback UI/history;
- repository/search discovery;
- changelog viewer;
- package dependency relationships;
- headless/CLI mode;
- scheduled background updating;
- Windows notifications.

---

## 24. First implementation task

The next development chat should **not** begin by implementing addon management.

Start with P0:

1. scaffold native C++/CMake project;
2. produce a small `TocPilot.exe`;
3. detect whether it is beside `WoW.exe`;
4. display `v0.1.0`;
5. add GitHub release query for `Seraphic8x2244/TocPilot`;
6. build the direct-EXE self-update state machine;
7. create GitHub Actions build output;
8. publish/test `v0.1.0 -> v0.1.1`.

Only after that end-to-end update succeeds should the package model implementation start.
