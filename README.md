# Pip-Boy Video Player

Pip-Boy Video Player is a planned native xNVSE plugin for Fallout: New Vegas. It will decode local MP4 files during play and display them inside the Pip-Boy with synchronized audio.

This repository contains planning and technical documentation only. It does not contain a plugin, build system, UI files, bundled FFmpeg binaries, or playable media.

## Project status

Status: pre-implementation design

The design targets a Viva New Vegas installation managed by Mod Organizer 2. The first technical milestone is a Direct3D 9 rendering experiment. Development should continue only if that experiment can draw a disposable test texture in the Pip-Boy, survive device resets, and coexist with the VNV graphics stack.

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

## Planned runtime pieces

The design has four runtime parts:

1. An x86 xNVSE plugin owns lifecycle, menu state, worker threads, logging, and configuration.
2. A private FFmpeg runtime opens MP4 containers, decodes audio and video, converts pixel formats, and resamples audio.
3. A Direct3D 9 renderer uploads decoded frames on the game render thread and draws them over a UIO-injected Pip-Boy placeholder.
4. An XAudio2 stream plays decoded PCM. Its sample cursor is the master playback clock when audio exists.

The plugin is planned as ESP-less. xNVSE and UIO are required. JIP LN, JohnnyGuitar, and ShowOff are part of the VNV baseline but should not become hard dependencies unless an implementation spike proves that one is necessary.

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

The project can still fail during the rendering spike. That is an expected planning outcome, not a reason to hide the uncertainty. The [risk register](docs/risk-register.md) lists the conditions that should stop or redirect implementation.

## Naming

"Pip-Boy Video Player" is a working project name. The final public name should be checked for conflicts before the first release.

## License status

No project license has been selected. Until a license file is added, the repository should be treated as all rights reserved. FFmpeg has separate license obligations described in [Build, packaging, and licensing](docs/build-packaging-and-licensing.md).
