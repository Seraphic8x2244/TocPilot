# TocPilot Development Plan

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
- a GitHub or GitLab release asset;
- a ZIP containing one or more addons;
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
   - Select a release asset when multiple assets exist.
   - Support direct files such as `ClassicAPI.dll` as naturally as addon ZIPs.

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
- a general package manager for arbitrary Windows software.

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

- repository/archive ZIP -> one or more addon directories;
- release ZIP -> one or more addon directories;
- direct DLL -> WoW root;
- direct file -> explicitly selected safe relative destination.

Never allow a remote archive to write outside the WoW root.

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
Mode: Latest release
Asset: ClassicAPI.dll
Target: WoW root
Installed: vX.Y.Z
Latest: vX.Y.Z
```

No special DLL subsystem is required. DLLs are simply direct-file release assets with an appropriate install target.

### ZIP release assets

If a selected release asset is a ZIP, TocPilot should inspect its contents and offer or infer an addon installation mapping.

---

## 9. Addon/archive layout detection

Repository archives and release ZIPs vary.

TocPilot should not assume every archive has exactly one folder with exactly one addon.

### Detection strategy

After secure extraction to a staging directory:

1. ignore provider-generated wrapper/root directories;
2. recursively identify directories containing one or more WoW `.toc` files;
3. identify plausible addon roots;
4. group related addon directories when the archive clearly ships multiple addons;
5. present a preview when the structure is ambiguous.

Common cases should require no manual intervention.

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

Automatic update checks should not block normal startup.

### Download

When the user chooses Update:

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

Package updates should use staging and commit semantics rather than directly extracting over the live addon.

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

Development bootstrap:

- `v0.1.0` — self-updater bootstrap;
- `v0.1.1` — self-update validation target.

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

Deliver:

- release browser;
- latest stable tracking;
- prerelease option;
- release-asset selection;
- ZIP release installation;
- direct file/DLL installation to WoW root;
- update/remove ownership tracking.

Exit criterion:

> A package such as a release-provided DLL can be tracked and updated alongside normal addons.

### P4 — GitLab

Deliver feature parity for public GitLab repositories:

- branches;
- branch archive;
- releases/assets where provider API permits;
- update tracking.

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

Mandatory before package work:

- current version == latest -> no update;
- newer version exists -> update shown;
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
- release ZIP;
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
