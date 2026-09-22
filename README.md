![TocPilot](resources/TocPilot_logo_cutout.png)

Lightweight, portable World of Warcraft addon and release manager.

TocPilot is designed to live directly beside `WoW.exe`. It follows GitHub/GitLab branches and releases without creating local Git repositories, and is intended to manage both normal addons and release assets such as DLLs.

## How to Install
- Download TocPilot.exe and copy once per WoW installation, in the base game folder where wow.exe exists
- On first run, you can click "Advanced" -> "Scan Existing Addons" to search for any git metadata and automatically import

## How to Use
- Run TocPilot.exe
- Press the buttons, paste in git links

# What's different?
- native Windows application with no large bundled UI/runtime framework;
- no hidden `.git` repositories in addon folders;
- GitHub/GitLab branch tracking is not broken *cough* 
- precise installed-file ownership;
- portable local configuration;
- self updates

## In Active Development
- release/asset tracking;
- direct DLL/file management;

## License

MIT.
