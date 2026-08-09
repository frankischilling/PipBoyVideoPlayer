# Pip-Boy Video Player

Pip-Boy Video Player is a native xNVSE plugin for Fallout: New Vegas. The first release is intended to decode local MP4 files and play them inside the Pip-Boy with synchronized audio.

The repository contains the implementation, automated tests, UIO files, build scripts, and technical documentation. It does not contain FFmpeg binaries or playable media.

## Project status

Status: Phase 1 render and UI feasibility

The current diagnostic build targets a Viva New Vegas installation managed by Mod Organizer 2. It loads only under FalloutNV 1.4.0.525 with xNVSE 6.4.5 or newer, registers lifecycle callbacks, installs a UIO prefab, and probes the engine's Direct3D recreation function. It refuses to render until the live function entry matches a reviewed signature. This allows the first in-game run to collect the required evidence without patching unknown code.

The host and Win32 automated tests pass. In-game render order, device recreation, resolution coverage, and DXVK support are not verified yet.

## Build

The repository follows the same local layout and script conventions as RadioCaptions. Downloaded sources stay under the ignored `external` directory, generated projects stay under `build-vs` or `build-host`, staging uses `stage`, and archives use `dist`.

Required tools are CMake, Visual Studio with the x86 C++ workload, PowerShell, and 7-Zip. Run these commands from a PowerShell prompt:

```powershell
.\scripts\fetch-dependencies.ps1
.\scripts\configure.ps1 -Target plugin
.\scripts\build.ps1 -Configuration Debug
.\scripts\test.ps1 -Configuration Debug
```

The dependency script downloads the official xNVSE 6.4.5 and MinHook 1.3.4 source archives and verifies their SHA-256 hashes before extraction. The plugin build uses a Win32 Visual Studio generator. A separate host build can run the portable tests without building the plugin:

```powershell
.\scripts\configure.ps1 -Target host
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
```

## Intended user experience

The user installs the mod through Mod Organizer 2, creates a separate personal media mod, and places MP4 files in its `Data\NVSE\Plugins\PipBoyVideoPlayer\Videos` directory. A Videos page in the Pip-Boy lists those files. Selecting a file starts native decoding without converting the video into DDS frames first.

The planned player supports:

- local MP4 files with H.264 video and AAC audio for the first release;
- play, pause, stop, restart, and short forward or backward seeks;
- mouse, keyboard, and controller input;
- aspect-fit and aspect-fill presentation;
- full-color and Pip-Boy-tinted display modes;
- variable frame rate media;
- audio-led synchronization with late video frame dropping;
- clean shutdown when the menu closes, a save loads, or the game exits.

The mod will not stream from websites, bypass DRM, scan arbitrary folders, or ship copyrighted video packs.

## Runtime pieces

The design has four runtime parts:

1. An x86 xNVSE plugin owns lifecycle, menu state, worker threads, logging, and configuration.
2. A private FFmpeg runtime opens MP4 containers, decodes audio and video, converts pixel formats, and resamples audio.
3. A Direct3D 9 renderer uploads decoded frames on the game render thread and draws them over a UIO-injected Pip-Boy placeholder.
4. An XAudio2 stream plays decoded PCM. Its sample cursor is the master playback clock when audio exists.

The plugin remains ESP-less. xNVSE and UIO are required. JIP LN, JohnnyGuitar, and ShowOff are part of the VNV baseline but are not hard dependencies.

## Documentation map

- [Project scope](docs/project-scope.md)
- [Architecture](docs/architecture.md)
- [Playback design](docs/playback-design.md)
- [UI, input, and files](docs/ui-input-and-files.md)
- [Build, packaging, and licensing](docs/build-packaging-and-licensing.md)
- [Compatibility and test plan](docs/compatibility-and-test-plan.md)
- [Risk register](docs/risk-register.md)
- [Roadmap](docs/roadmap.md)
- [Decisions and open questions](docs/decisions-and-open-questions.md)
- [Research notes](docs/research-notes.md)
- [Contributing](CONTRIBUTING.md)

## Non-negotiable constraints

Fallout: New Vegas is a 32-bit process. Video queues must stay bounded, source dimensions must be checked before allocation, and all Direct3D work must stay on the render thread. The plugin must release default-pool graphics resources before a Direct3D device reset. Decoder failure must stop playback and return control to the Pip-Boy instead of taking down the game.

VNV compatibility is a release gate, not a best-effort claim. Each release candidate must pass against the documented VNV profiles and UI variants before publication.

The project can still fail during the rendering spike. The [risk register](docs/risk-register.md) lists the conditions that would block or redirect the design.

## Naming

"Pip-Boy Video Player" is a working project name. The final public name should be checked for conflicts before the first release.

## License status

No project license has been selected. Until a license file is added, the repository should be treated as all rights reserved. FFmpeg has separate license obligations described in [Build, packaging, and licensing](docs/build-packaging-and-licensing.md).
