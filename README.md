# TocPilot

Lightweight, portable World of Warcraft addon and release manager.

TocPilot is designed to live directly beside `WoW.exe`. It follows GitHub/GitLab branches and releases without creating local Git repositories, and is intended to manage both normal addons and release assets such as DLLs.

## Core direction

- one TocPilot EXE per WoW installation;
- native Windows application with no large bundled UI/runtime framework;
- no hidden `.git` repositories in addon folders;
- GitHub/GitLab branch tracking;
- release/asset tracking;
- direct DLL/file management;
- precise installed-file ownership;
- portable local configuration;
- self-updating as the first implementation priority.

## Development

The complete architecture, roadmap, self-update design, package model, testing plan, and milestone order are documented in [DEVELOPMENT.md](DEVELOPMENT.md).

Current handoff/status is in [STATUS.md](STATUS.md).

## License

MIT.
