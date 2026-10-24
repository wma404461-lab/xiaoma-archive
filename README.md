# Xiaoma Archive

A native Windows 10/11 x64 archive utility with background context-menu operations and an optional management window. It creates ZIP, 7z and TAR archives and uses a separate 7-Zip engine for extraction, listing, validation, passwords and split volumes.

## Build

Install Visual Studio C++ Build Tools, Windows SDK, Python 3 and Pillow. Obtain official Windows x64 `7z.exe`, `7z.dll` and their license from 7-Zip and put them in `vendor/`, or specify an engine directory. The existing integration uses 7-Zip 26.04; other versions require verification. Third-party binaries are not included.

```powershell
python -m pip install Pillow
.\build.ps1 -OutputDirectory .\dist
.\build.ps1 -OutputDirectory .\dist -EngineDirectory 'C:\path\to\7zip'
```

Keep the adjacent `engine/` directory with the built application. Build does not register shell menus. Run `Install-ContextMenu.ps1` in the output directory to register menus for the current user, and `Uninstall-ContextMenu.ps1` to remove them. Launching the executable normally opens the management window.

## Source and validation

- `src/core.cpp`: engine invocation, archive operations, path validation and temporary results.
- `src/quick.cpp`: background context-menu operations.
- `src/main.cpp`: native Windows UI.
- `src/tests.cpp`: core regressions. The full original suite also requires generated archive fixtures that are not included in this source edition.

The publication copy supports an explicit engine directory and fixes removal of password-extraction menus in the uninstall script. It does not modify the installed application or shell registration. See [User guide](USER_GUIDE.txt) for behavior and limitations.

## Licensing

Project-authored code is MIT licensed. The independent 7-Zip engine retains its LGPL, BSD and unRAR terms; see `vendor/7-Zip-LICENSE.txt`. It is not covered by this project's MIT license. Upstream and corresponding source: [7-Zip](https://www.7-zip.org/) and [ip7z/7zip](https://github.com/ip7z/7zip).

The existing generated icon is exported by `tools/make_icon_v4.py`. No WinRAR proprietary source, icons or resources are included. Source publication is not a claim that every Windows version or archive format has been tested.
