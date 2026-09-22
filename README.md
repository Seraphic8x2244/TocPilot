![TocPilot](resources/TocPilot_logo_cutout.png)

SuperLightWeight, portable World of Warcraft addon and release manager.

## How to Install
- Download TocPilot.exe and copy once per WoW installation, in the base game folder where wow.exe exists
- On first run, you _can_ click "Advanced" -> "Scan Existing Addons" to search for any git metadata and automatically import addons you may have installed with... _other_ addon managers.

## How to Use
- Run TocPilot.exe
- Press the buttons, paste in git links

# What's different?
- Native Windows application with no large bundled UI/runtime framework
- No hidden `.git` repositories in addon folders
- GitHub/GitLab branch tracking is not broken *cough* 
- Staging are in Interface folder for checksums on downloads
- Portable ,local configuration in your Interface folder
- Automatically self updates

## In Active Development
- Release/asset tracking (repos that have multiple addons bundled)
- Direct DLL/file management (nampower, superwow, classicAPI etc)

## License

MIT.
