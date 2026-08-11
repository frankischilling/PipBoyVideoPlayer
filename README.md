# Pip-Boy Video Player

Pip-Boy Video Player is a native xNVSE plugin for Fallout: New Vegas. The first release is intended to decode local MP4 files and play them inside the Pip-Boy with synchronized audio.

The repository contains the implementation, automated tests, UIO files, build scripts, and technical documentation. It does not contain FFmpeg binaries or playable media.

## Project status

Status: Phase 5 catalog and controls in live validation

The current build targets a Viva New Vegas installation managed by Mod Organizer 2. It loads only under FalloutNV 1.4.0.525 with xNVSE 6.4.5 or newer, registers lifecycle callbacks, and installs a UIO prefab. Once the Pip-Boy is open, it validates the live UIO image, engine texture objects, managed Direct3D texture, device, and callback thread before uploading decoded frames. Any unknown object type, texture profile, or thread arrangement disables the update instead of guessing. The plugin does not patch game functions or a Direct3D device vtable.

The Host Release suite passes 21 of 21 tests. The Win32 Release suite passes 30 of 30 tests. Two overlay draw points were rejected because they rendered above the Pip-Boy UI. The accepted path updates an engine-owned managed texture and leaves drawing to the game. It uses no executable hook or Direct3D device vtable patch. The raised 384 by 216 panel passed all four isolated UI profiles at 1920 by 1080 during Phase 1. Keyboard and mouse input, ten Pip-Boy reopen cycles, and 50 measured focus-loss and return cycles passed in the tested native windowed configuration.

A synthetic recreation test froze inside the game's native reset sequence, so the test, reset hook, and MinHook dependency were removed. A PBVP-disabled control later reproduced the native fullscreen NVIDIA driver crash, so repeated fullscreen Alt+Tab is not supported. The isolated test-profile save guard passes Base and full Extended exit checks without changing normal profiles. Native windowed rows passed at 1280x720 and 30 FPS, 1280x960 and 60 FPS, 2560x1440 and 90 FPS, and 3440x1440 and 120 FPS. The two larger windows were clipped by the 1920x1080 monitor, so those results cover the visible panel and logged backbuffer rather than the full window.

Phase 1 supports native Direct3D 9 windowed mode. PBVP owns no default-pool resource and retains no Direct3D reference between callbacks. A changed device or engine surface is validated before use, but the tested windowed focus path did not produce an observable device recreation. DXVK and a safe root-management tool are not installed in the target VNV instance, so the project makes no DXVK claim and does not add a root proxy.

Phase 2 has pinned FFmpeg 8.1.2 and a minimal Win32 runtime. The reproducible build enables only the MOV demuxer, H.264 and AAC decoders and parsers, software scaling, and audio resampling. Two clean builds produced the same five DLL hashes. The runtime audit rejects non-i386 images, changed hashes, unexpected imports, extra DLLs, and private build paths. The plugin loads that set from its private directory with restricted absolute paths and rejects wrong versions or configurations. Custom Windows AVIO supports bounded local reads, seeks, Unicode names, and cancellation. Checked media layouts and generation-aware queues enforce dimension, item-count, and byte limits before decoded data reaches the game.

The decoder worker opens MP4 and MOV containers, requires H.264 video, accepts optional AAC audio, converts video to BGRA, applies right-angle display rotation, and resamples audio to bounded interleaved 16-bit PCM. It keeps variable frame rate timestamps from FFmpeg's best-effort timestamp field. Forward and backward seeks clear queued output by generation before the worker flushes its codecs. Synthetic tests cover valid decoding, silent variable frame rate video, rotation, cancellation, damaged and encrypted input, unsupported codecs, audio layouts, and source limits. A live MO2 run opened a 1080p fixture that existed only in a separate media mod, decoded all expected video and audio, and measured a 62,976,000 byte private-memory increase after a stable no-decode control.

Phase 3 uses the Windows 10 and Windows 11 system XAudio2 2.9 runtime. The audio stream owns a fixed 256 KiB PCM pool, keeps callbacks limited to atomic updates, and uses the consumed sample count as the media clock. A checked QPC clock handles silent video. Automated tests cover pause, resume, mute, volume, stop and flush, forward and backward clock origins, end of stream, bounded queue pressure, default-device reconstruction, 44.1 and 48 kHz mono and stereo voices, 5.1 downmix through the decoder, and 25 complete audio lifetimes. The 100, 200, and 300 ms prebuffer cases completed with zero underruns.

The live Phase 3 MO2 diagnostic played a generated two-second AAC fixture through FalloutNV. The user heard the tone. XAudio2 consumed 96,967 output samples, reached the expected 2,020,125 microsecond clock, and reported zero underruns. The decoder joined before FFmpeg unload, and the source voice and callback targets were released before process shutdown. The release packager rejects this private diagnostic and any bundled XAudio2 DLL.

Phase 4 connects decoding, XAudio2, audio-led frame selection, the Pip-Boy texture upload, status text, and menu lifecycle handling. It keeps bounded decoder queues and one presentation frame, drops late video without changing the media clock, uses QPC for silent media, and stops playback when the Pip-Boy closes or the game changes state. Automated tests cover pause, resume, forward and backward seeks, stop during buffering, stale generations, silent playback, foreign-thread refusal, bounded 1080p memory, and orderly shutdown.

The accepted live 30-minute run decoded 54,000 frames, uploaded 53,993, and recorded zero underruns. It finished with 28.125 milliseconds of audio-to-video error and 41,811,968 bytes of additional private memory. Upload time was 18.30 microseconds minimum, 24.64 microseconds average, and 150.40 microseconds maximum.

The accepted live five-minute 10 FPS run used the same 30 FPS fixture. It decoded 9,001 frames, presented 2,998, dropped 6,002, and reached a 299,980,000 microsecond audio clock with zero underruns. Its maximum controller update gap was 110 milliseconds. Renderer shutdown accounted for all 3,343 submissions as 3,342 uploads and one cleared pending frame, with no replacement or upload failure.

Phase 5 adds a bounded direct-child MP4 catalog with Unicode filenames, natural sorting, eight visible rows, lazy title metadata, and scoped mouse, keyboard, and XInput controller controls. It implements aspect fit, aspect fill, Pip-Boy tint, full color, volume and resource settings, idle-only configuration reload, and privacy-safe normal logs. The accepted live catalog run displayed ten separate entries and played selected files. Mouse activation and every shipped keyboard action worked, including both seek directions, pause, stop, and return to the Data page.

These results satisfy the Phase 4 synchronization, frame-rate independence, seek and buffering-stop automation, and memory exit criteria. Phase 5 portable checks are complete, but live controller prompt switching, the four-profile UI matrix, and the Fit and Fill reload comparison remain open. DXVK, repetition tests, and the two-hour soak remain Phase 6 work.

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

The player currently implements:

- local MP4 files with H.264 video and AAC audio for the first release;
- play, pause, stop, and short forward or backward seeks;
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
- [Phase 5 live test guide](docs/phase5-live-test-guide.md)
- [Risk register](docs/risk-register.md)
- [Roadmap](docs/roadmap.md)
- [Decisions and open questions](docs/decisions-and-open-questions.md)
- [Research notes](docs/research-notes.md)
- [Contributing](CONTRIBUTING.md)

## Non-negotiable constraints

Fallout: New Vegas is a 32-bit process. Video queues must stay bounded, source dimensions must be checked before allocation, and all Direct3D work must stay on the render thread. The plugin must release default-pool graphics resources before a Direct3D device reset. Decoder failure must stop playback and return control to the Pip-Boy instead of taking down the game.

VNV compatibility is a release gate, not a best-effort claim. Each release candidate must pass against the documented VNV profiles and UI variants before publication.

The remaining compatibility and stability work can still expose a release blocker. The [risk register](docs/risk-register.md) lists the conditions that would block or redirect the design.

## Naming

"Pip-Boy Video Player" is a working project name. The final public name should be checked for conflicts before the first release.

## License status

No project license has been selected. Until a license file is added, the repository should be treated as all rights reserved. FFmpeg has separate license obligations described in [Build, packaging, and licensing](docs/build-packaging-and-licensing.md).
