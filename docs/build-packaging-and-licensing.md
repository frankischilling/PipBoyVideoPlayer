# Build, packaging, and licensing

## Toolchain

The plugin is a 32-bit Windows DLL. The checked build uses:

- Visual Studio 2026 with the v145 x86 compiler;
- CMake presets for repeatable developer and release builds;
- LLVM 22.1.0 `llvm-pdbutil` for validating public release symbols;
- the Windows SDK and Direct3D 9 headers and import library;
- XAudio2 2.9 from the Windows SDK and the matching Windows system runtime;
- xNVSE plugin headers pinned to a reviewed commit;
- FFmpeg 8.1.2 built for i686 with MSYS2 GCC 15.2.0;
- NASM 3.01, GNU Make 4.4.1, and pkgconf 2.5.1 for the FFmpeg build.

The exact C++ language level is an implementation decision. It must be supported by the selected compiler and every dependency. Release builds need symbols, deterministic version metadata, and an optimization profile that preserves useful crash stacks.

## Dependency policy

Every native dependency needs:

- a pinned source revision or release;
- its upstream URL and license;
- the exact x86 build options;
- hashes for downloaded source archives;
- a written update procedure;
- a list of files placed in the release archive;
- a vulnerability and regression review before updates.

Do not download DLLs from general DLL mirror sites. Release dependencies must come from upstream source or a documented trusted build process.

## FFmpeg build profile

FFmpeg 8.1.2 is pinned by source archive, signature, toolchain, configure arguments, and runtime hashes in `dependencies/ffmpeg-8.1.2.json`. The source archive SHA-256 is `464BEB5E7BF0C311E68B45AE2F04E9CC2AF88851ABB4082231742A74D97B524C`. Its detached signature was verified with release key fingerprint `FCF986EA15E6E293A5644F10B4322F04D67658D8`.

The build enables shared `avcodec`, `avformat`, `avutil`, `swscale`, and `swresample` libraries. It enables the MOV demuxer, H.264 and AAC decoders and parsers, Windows threads, software scaling, and audio resampling. Network support, protocols, encoders, muxers, filters, capture devices, command-line programs, hardware acceleration, and automatic external dependency detection are disabled.

The exact configure arguments are stored in the manifest and applied by `scripts/build-ffmpeg.ps1`. The build sets a fixed source epoch and maps compile-time file names to the neutral `pbvp` prefix. It statically links the two required winpthreads clock functions so no MinGW runtime DLL is needed. FFmpeg remains in replaceable shared libraries. The winpthreads archive and license hashes are also pinned.

The runtime contains only `avcodec-62.dll`, `avformat-62.dll`, `avutil-60.dll`, `swresample-6.dll`, and `swscale-9.dll`. Every file is an i386 PE image. The manifest records each SHA-256 hash and exact import set. Two clean builds on August 10, 2026 produced the same hashes. The automated audit rejects a changed hash, wrong architecture, changed import, extra DLL, or private local path.

These DLLs belong in the private `PipBoyVideoPlayer\bin` directory. The plugin must construct absolute dependency paths and use restricted Windows DLL search APIs. FFmpeg DLLs must not be copied into the game root or the shared `Data\NVSE\Plugins` directory.

PBVP links the x86 Windows SDK `xaudio2.lib` import library and uses the system `XAudio2_9.dll` supplied by Windows 10 and Windows 11. The release does not include an XAudio2 DLL or the legacy DirectX SDK runtime. The package audit must reject any bundled `XAudio2*.dll` file.

## Proposed release layout

```text
Data\
  Config\
    PipBoyVideoPlayer.ini
  Menus\
    Prefabs\
      PipBoyVideoPlayer\
        Player.xml
  NVSE\
    Plugins\
      PipBoyVideoPlayer.dll
      PipBoyVideoPlayer\
        bin\
          FFmpeg runtime DLLs
        Videos\
          Place videos here.txt
  UIO\
    Public\
      PipBoyVideoPlayer.txt
LICENSES\
  Project license
  FFmpeg license and notices
README.txt
```

The exact XML filenames may change after the UIO spike. The matching stripped PDB belongs in the separate symbols archive because New Vegas crash loggers can produce better stacks when symbols are available. The full PDB remains private and is never packaged.

## Package separation

Development artifacts should produce two archives:

1. A runtime archive containing the DLL, private FFmpeg runtime, config, UI, notices, and install instructions.
2. A symbols archive containing the matching stripped PDB and source-reference information.

Personal media is never part of either archive.

The linker writes the explicit filename `PipBoyVideoPlayer.pdb` into the DLL. The raw stripped PDB still contains the absolute names of contributing object files and libraries. Before packaging, a format-aware cleanup reads the DBI logical stream, replaces each absolute module or object path with an equal-length path-neutral name, and writes the same-size stream back to its existing MSF blocks. It also clears unreferenced blocks and unused block tails that may retain stale linker data. It does not resize records or rebuild the PDB. The symbols archive renames the cleaned output to the filename expected by the DLL.

The package check requires the cleaned PDB to retain the original GUID, age, stripped status, complete public symbol set, FPO data, and section contributions. It also scans every logical PDB stream, the raw PDB bytes, and the DLL for absolute drive paths and known repository, build, temporary, and user-profile markers. Checking archive entry names alone is insufficient.

The retired Phase 1 forced-recreation test is not part of CMake, the development installer, or the package script. There is no armed build mode. Lifecycle testing must use the normal plugin and a transition initiated by the game.

## Versioning

Use semantic versioning after the first public release. Before then, use `0.x` milestones. Runtime logs and DLL metadata must report the same version.

The package manifest should also record:

- supported FalloutNV executable hashes or runtime identifiers;
- minimum and tested xNVSE versions;
- minimum and tested UIO versions;
- FFmpeg library major versions;
- bridge version shared by the DLL and XML;
- build commit.

An incompatible XML bridge, FFmpeg major version, or runtime layout change requires a clear upgrade note. The plugin should refuse a mismatched private DLL set instead of loading whatever matching filename appears first.

## Licensing work before implementation

The project license remains open. It must be chosen before accepting source contributions. The choice must cover the plugin, build files, original UI XML, and documentation without claiming ownership of Fallout assets or third-party libraries.

FFmpeg is mainly LGPL, with optional GPL components and other license combinations depending on configuration. The project must review the actual build, not rely on a generic statement about FFmpeg. A release that uses LGPL FFmpeg DLLs should provide the required notices, the exact corresponding source or a compliant offer and link, build configuration, local modifications, and a way for users to replace the libraries where the license requires it.

Codec patent rules vary by country and distribution method. Before a public binary release, the maintainer should review H.264 and AAC distribution obligations for the intended hosting and jurisdiction. This document is a release task, not legal advice.

The repository and release must not contain Bethesda game assets, Bink components, commercial video samples, or code copied from another mod without permission.

## Continuous integration plan

Once code exists, CI should:

- configure and build x86 debug and release targets;
- run unit tests for timestamp math, checked allocation math, catalog normalization, and state transitions;
- verify dependency versions and approved hashes;
- inspect the DLL architecture and imported libraries;
- assemble the release tree from a clean checkout;
- reject unexpected files, absolute paths, and personal media extensions;
- publish checksums and a dependency inventory with release candidates.

CI configuration is intentionally absent during the documentation-only phase.
