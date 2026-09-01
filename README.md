# Nvidia Byp SMOKEY

Windows utility that launches the NVIDIA bridge payload and exposes a small modern control panel for stream-proofing the overlay window.

## Features

- Calls `NvCore::Launch()` once before overlay HWND initialization (background inject into `nvcontainer.exe`)
- **Streamproof** checkbox toggles `SetWindowDisplayAffinity` on the overlay window:
  - checked → `WDA_EXCLUDEFROMCAPTURE`
  - unchecked → `WDA_NONE`
- **Unload** tears down the injected payload then closes the app
- Warning label: *Don't Press Because Freeze Clips*
- Auto-update from GitHub Releases on startup
- **Administrator required** — UAC prompt on launch (elevated token kept for inject/unload)

## Requirements

- Windows 10/11 x64
- NVIDIA drivers / `nvcontainer.exe` with `capcore64.dll` (for the bridge payload)
- Administrator (UAC elevation enforced by manifest)

## Build

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build/Release/NvidiaBypSMOKEY.exe`

## Auto-update

On launch, the app checks `https://api.github.com/repos/5kzwh55j5v-maker/Nvidia-Byp-SMOKEY/releases/latest`.

If a newer tagged release exists with a `.exe` asset, it downloads the update and restarts.

Publish updates by tagging a release:

```bat
git tag v1.0.1
git push origin v1.0.1
```

GitHub Actions builds and attaches `NvidiaBypSMOKEY.exe` to the release.

## Version

Current version: **1.0.0** (`include/version.h`)
