# Pip-Boy Video Player

Pip-Boy Video Player is a native xNVSE plugin for Fallout: New Vegas. The first release is intended to decode local MP4 files and play them inside the Pip-Boy with synchronized audio.

The repository contains the implementation, automated tests, UIO files, build scripts, and technical documentation. It does not contain FFmpeg binaries or playable media.

## Project status

Status: Phase 2 media core

The current diagnostic build targets a Viva New Vegas installation managed by Mod Organizer 2. It loads only under FalloutNV 1.4.0.525 with xNVSE 6.4.5 or newer, registers lifecycle callbacks, and installs a UIO prefab. Once the Pip-Boy is open, it validates the live UIO image, engine texture objects, managed Direct3D texture, device, and callback thread before uploading a generated checkerboard. Any unknown object type, texture profile, or thread arrangement disables the update instead of guessing. The plugin does not patch game functions or a Direct3D device vtable.

The host and Win32 automated tests pass. Two overlay draw points were rejected because they rendered above the Pip-Boy UI. The accepted path updates an engine-owned managed texture and leaves drawing to the game. It uses no executable hook or Direct3D device vtable patch. The raised 384x216 panel passed all four isolated UI profiles at 1920x1080. Keyboard and mouse input, ten Pip-Boy reopen cycles, and 50 measured focus-loss and return cycles passed in the tested native windowed configuration.

A synthetic recreation test froze inside the game's native reset sequence, so the test, reset hook, and MinHook dependency were removed. A PBVP-disabled control later reproduced the native fullscreen NVIDIA driver crash, so repeated fullscreen Alt+Tab is not supported. The isolated test-profile save guard passes Base and full Extended exit checks without changing normal profiles. Native windowed rows passed at 1280x720 and 30 FPS, 1280x960 and 60 FPS, 2560x1440 and 90 FPS, and 3440x1440 and 120 FPS. The two larger windows were clipped by the 1920x1080 monitor, so those results cover the visible panel and logged backbuffer rather than the full window.

Phase 1 supports native Direct3D 9 windowed mode. PBVP owns no default-pool resource and retains no Direct3D reference between callbacks. A changed device or engine surface is validated before use, but the tested windowed focus path did not produce an observable device recreation. DXVK and a safe root-management tool are not installed in the target VNV instance, so this phase makes no DXVK claim and does not add a root proxy. Controller and input-method switching remain Phase 5 work.

Phase 2 has pinned FFmpeg 8.1.2 and a minimal Win32 runtime. The reproducible build enables only the MOV demuxer, H.264 and AAC decoders and parsers, software scaling, and audio resampling. Two clean builds produced the same five DLL hashes. The runtime audit rejects non-i386 images, changed hashes, unexpected imports, extra DLLs, and private build paths. The plugin loads that set from its private directory with restricted absolute paths and rejects wrong versions or configurations. Custom Windows AVIO supports bounded local reads, seeks, Unicode names, and cancellation. Checked media layouts and generation-aware queues enforce dimension, item-count, and byte limits before decoded data reaches the game.

The decoder worker opens MP4 and MOV containers, requires H.264 video, accepts optional AAC audio, converts video to BGRA, applies right-angle display rotation, and resamples audio to bounded interleaved 16-bit PCM. It keeps variable frame rate timestamps from FFmpeg's best-effort timestamp field. Forward and backward seeks clear queued output by generation before the worker flushes its codecs. Synthetic tests cover valid decoding, silent variable frame rate video, rotation, cancellation, damaged input, unsupported codecs, and source limits. Live MO2 virtual-filesystem access and the 1080p in-process memory measurement are still in progress.

## Build

The repository follows the same local layout and script conventions as RadioCaptions. Downloaded sources stay under the ignored `external` directory, generated projects stay under `build-vs` or `build-host`, staging uses `stage`, and archives use `dist`.

Required tools are CMake, Visual Studio with the x86 C++ workload, PowerShell, MSYS2, and LLVM 22.1.0. The FFmpeg build uses the MSYS2 i686 GCC 15.2.0 toolchain, NASM 3.01, GNU Make 4.4.1, and pkgconf 2.5.1. Release packaging uses the pinned LLVM version for `llvm-pdbutil` and `llvm-readobj`. Run these commands from a PowerShell prompt:

```powershell
.\scripts\fetch-dependencies.ps1
.\scripts\build-ffmpeg.ps1 -Clean
.\scripts\configure.ps1 -Target plugin
.\scripts\build.ps1 -Configuration Debug
.\scripts\test.ps1 -Configuration Debug
```

The dependency script downloads the official xNVSE 6.4.5 and FFmpeg 8.1.2 source archives and verifies both SHA-256 hashes before extraction. The FFmpeg script builds and audits the five private i386 runtime DLLs. The plugin build then uses a Win32 Visual Studio generator. A separate host build can run the portable tests without building the plugin:

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
3. A Direct3D 9 renderer uploads decoded frames into an engine-owned UIO image on the game render thread.
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
