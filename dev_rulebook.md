# TocPilot Development Rulebook

This document is the canonical engineering and development workflow for TocPilot.

Project-specific development state, architecture decisions, invariants, protocols, testing state, deferred scope and exact next work belong in `DEV_PROGRESS.md`.

Permanent public/user documentation may exist where useful, but it is not a competing development source of truth.

---

## 1. Authority and sources of truth

Use these sources for different kinds of truth:

- `dev_rulebook.md` — canonical workflow, engineering and release rules.
- `DEV_PROGRESS.md` — current product-development state, live development contract and fresh-chat recovery source.
- Current code — implemented reality.
- Git history — historical implementation record.
- `CMakeLists.txt`, `src/version.h` and `.github/release-version` — release-version sources that must agree when publishing a release.

Do not maintain multiple competing live handoff documents.

Legacy `STATUS.md`, `DEVELOPMENT.md` and `docs/dev/HANDOFF.md` are superseded once their still-relevant content has been migrated into this rulebook or `DEV_PROGRESS.md`.

Git carries history. `DEV_PROGRESS.md` carries current state and current design intent.

---

## 2. Product development environment

TocPilot is a portable native Windows application.

Current engineering direction:

- C++20.
- Win32 desktop UI and native Common Controls.
- CMake.
- MSVC.
- x64 Windows.
- Static Microsoft C/C++ runtime where practical.
- WinHTTP for HTTPS/API/download work.
- BCrypt or equivalent Windows cryptography APIs for SHA-256.
- Standard Win32 file/process APIs for installation, atomic replacement and updater handoff.
- Small auditable vendored libraries where they solve a concrete need; avoid framework-scale dependencies.

Do not introduce Qt, Git/libgit2, Chromium/WebView, a managed runtime, or another large framework merely for implementation convenience.

Use operating-system facilities where they provide the required capability cleanly.

---

## 3. Repository and branch discipline

`main` is the current product branch and stable source baseline.

Runtime feature work should normally be isolated on a focused branch and validated through pull-request CI before merge.

Rules:

- Keep development slices narrow.
- Do not silently broaden a requested feature or fix.
- Do not mix unrelated refactors into a targeted change.
- Verify the current remote head before editing and again before moving a shared branch.
- Never force-update over concurrent work.
- If the branch moved, inspect and reconcile the new commits first.
- Documentation-only maintenance may be committed on `main` without a product version bump or release when it does not change runtime behaviour or release inputs.

A build artifact from a pull request or ordinary CI run is not a TocPilot product release.

---

## 4. Versioning and release sources

TocPilot uses semantic versions.

For a release, these must agree:

- `project(TocPilot VERSION ...)` in `CMakeLists.txt`;
- version macros in `src/version.h`;
- `.github/release-version`.

The release tag is `vX.Y.Z`.

Do not bump the version merely for internal documentation changes.

A version bump is meaningful because published builds are part of TocPilot's runtime-test path.

---

## 5. Release and self-update delivery rule

TocPilot's self-updater is part of the product and must be exercised during normal runtime validation.

During active runtime development:

- CI artifacts are for compile/test validation.
- A build handed to the user for normal runtime testing should be version-bumped and published through the real GitHub Release path.
- Runtime testing should normally begin from the previously installed TocPilot version and allow TocPilot to update itself to the new release.
- Manual EXE replacement is a fallback when the updater itself is under repair, not the normal test path.

The Release workflow must build and test the exact `main` commit being published.

A valid TocPilot release must:

1. validate that the requested tag matches the source version;
2. configure and build Windows x64 Release;
3. run the complete CTest suite;
4. generate the SHA-256 sidecar;
5. create or verify the release tag against the exact release commit;
6. publish direct `TocPilot.exe` and `TocPilot.exe.sha256` assets.

Do not treat a PR artifact, locally built EXE or untagged workflow artifact as equivalent to this release path.

Documentation-only commits after a release do not require a new release and do not change the published runtime baseline.

---

## 6. Starting or resuming development

Before substantial work:

1. read `dev_rulebook.md`;
2. read `DEV_PROGRESS.md`;
3. resolve the documented branch and relevant source/release commits;
4. verify the actual remote head before editing;
5. identify the latest published release separately from later documentation-only commits;
6. identify what is runtime-confirmed, CI-checked, published-but-untested and deferred;
7. confirm the requested work does not cross a documented scope boundary.

A casual interpretation of a chat request must not silently override the written development contract. If the user intentionally changes the contract, update `DEV_PROGRESS.md` accordingly.

---

## 7. Scope and architecture

Preserve established ownership and transaction boundaries unless deliberately changing them.

Rules:

- Route new operations through the existing authoritative subsystem when one already owns that lifecycle.
- Do not create parallel package state, ownership, update or transaction paths for work already owned elsewhere.
- Keep state mutation ownership explicit.
- Fix underlying causes rather than layering retries, delays or compatibility hacks over an unexplained failure.
- Temporary workarounds require an explicit reason and must remain identified as temporary.
- Structural changes require a concrete need; do not add abstractions or dependencies speculatively.

When a runtime failure exposes an assumption, inspect the actual protocol/API/filesystem behaviour before tuning around it.

---

## 8. User agency and tool design

Design for capable users, not hypothetical misuse.

A tool should restrict the user only where unrestricted behaviour would make the tool incorrect, corrupt state, violate a required invariant, or exceed a genuine external constraint. It should not restrict behaviour merely because a value is unusual, a workflow is uncommon, or the developer believes the user probably should not do it.

Prefer capability over paternalism.

- Distinguish **invalid** from merely **unwise, unusual or inconvenient**.
- If an operation is valid, allow it even when the result may be extreme, inefficient, visually awkward or easy to misuse.
- Do not silently replace user intent with developer judgement.
- Prefer clear feedback, warnings and documentation over prevention.
- Do not add arbitrary caps, clamps, cooldowns, retry limits, disabled states or forced workflows for convenience or presumed safety.
- Every imposed constraint should have a concrete technical justification.
- Scope necessary constraints as narrowly as possible to the invariant or external limitation that requires them.
- Preserve advanced and unexpected uses when the underlying system can support them correctly.
- Treat robustness as a way to support a wider range of valid behaviour, not as a reason to narrow what the user is allowed to do.
- Keep behaviour predictable: accept the user's choice faithfully, expose its consequences clearly, and avoid hidden normalization or correction.

When an unusual input exposes weakness in the implementation, first ask whether the implementation can be made robust enough to support it. Do not default to forbidding the input.

**Enforce correctness, not preference. Trust the user with every capability the system can reliably provide.**

### Genuine constraints

Constraints are appropriate where required to preserve correctness, state/data integrity, security boundaries, transactional guarantees, or real API/protocol/operating-system requirements.

When a constraint is necessary:

- enforce the narrowest constraint that solves the real problem;
- make the reason clear where useful;
- do not extend it to adjacent valid behaviour for convenience;
- remove it later if the underlying technical limitation is eliminated.

Retries, waits, locks and throttles are observable product behaviour. Do not introduce or tune them by guesswork; base them on concrete runtime evidence or a real external requirement.

---

## 9. Correctness and security boundaries

TocPilot manages executable files and live addon installations, so correctness boundaries are mandatory.

At minimum:

- use HTTPS for provider/download traffic;
- verify TocPilot self-update payloads with SHA-256;
- reject archive path traversal, absolute paths and writes outside approved roots;
- constrain managed destinations to the WoW installation;
- do not construct shell commands from untrusted remote filenames;
- keep configuration/state writes atomic;
- keep package installation rollback-minded;
- do not advance installed state before the filesystem operation it describes has committed;
- do not allow ambiguous overlapping package ownership;
- do not alter antivirus/security-software configuration automatically;
- do not log future authentication secrets.

These are integrity requirements, not reasons to restrict unrelated valid user behaviour.

---

## 10. Testing and provenance

TocPilot's release-first runtime workflow means validation states are not a simple linear "tested before released" chain.

Keep these facts distinct for every development slice:

- **Implemented** — source change exists.
- **CI checked** — Release build/static/automated tests passed for an exact commit.
- **Published** — an exact `main` commit was rebuilt/tested/tagged and released.
- **Runtime tested** — the user exercised the relevant behaviour in the target WoW/Windows environment.
- **Accepted baseline** — the observed runtime behaviour is considered known-good for future work.

A published release can still have runtime validation debt.

Rules:

- CI does not count as runtime testing.
- A PR artifact does not count as a release.
- A release does not automatically become runtime-confirmed.
- Bind runtime observations to the exact source/release version tested.
- When the user tests an automatic update, record both the starting installed version and target release.
- A successful adjacent path does not prove an untested path.
- Preserve previously runtime-confirmed behaviour as inherited baseline, while marking new deltas separately.
- Never rewrite validation history to imply testing that did not occur.

---

## 11. `DEV_PROGRESS.md`

`DEV_PROGRESS.md` is the sole live TocPilot development document.

It must contain enough current product context for a fresh chat to make the next correct decision without reading historical checkpoints.

At minimum track:

- active branch;
- current source version;
- latest relevant branch/docs head;
- latest published release and exact release commit;
- current goal and scope boundary;
- active architecture/ownership/invariants;
- current package/provider/state/update/release design decisions that still constrain future work;
- recent relevant commits;
- runtime-confirmed work;
- implemented/published but runtime-untested work;
- CI/automated checks and exact commits;
- current issues;
- last runtime test;
- next runtime test;
- planned work;
- deferred/out-of-scope work;
- exact next step.

Do not turn it into a chronological release diary.

Remove or compress old implementation narratives once the resulting behaviour/invariant is established and the history no longer affects a future decision.

---

## 12. Long-chat and handoff discipline

Before a development chat becomes too tool-heavy or context-heavy to remain reliable:

1. stop at a coherent checkpoint;
2. update `DEV_PROGRESS.md`;
3. record branch, version, relevant commits and release baseline;
4. distinguish runtime-confirmed, CI-checked, published-but-untested and deferred work;
5. record the exact next step and any explicit "do not start yet" boundary;
6. commit the documentation checkpoint;
7. recommend a fresh chat and provide a concise resume prompt.

The fresh chat must verify the remote head before new work.

---

## 13. Commit discipline

Prefer coherent commits that identify meaningful states.

Runtime feature slices should keep source, tests and the corresponding state update coherent enough that their provenance can be reconstructed from Git.

Documentation commits may follow runtime/release commits without changing the source version.

Before publishing a release, verify:

- source-version files agree;
- the intended runtime delta is present;
- CI is green;
- the release is built from the intended merged `main` commit;
- the direct EXE and SHA-256 assets are published;
- validation debt is recorded honestly in `DEV_PROGRESS.md`.

---

## 14. Development contract changes

This rulebook is the TocPilot workflow contract.

Do not edit it casually during ordinary feature work.

Project/product architecture changes belong in `DEV_PROGRESS.md`. Change this file only when the engineering or release standard itself is intentionally changing.
