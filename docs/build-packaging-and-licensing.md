# Build, packaging, and licensing

## Planned toolchain

The plugin is a 32-bit Windows DLL. The initial toolchain should use:

- Visual Studio 2022 with the current v143 x86 compiler;
- CMake presets for repeatable developer and release builds;
- the Windows SDK and Direct3D 9 headers and import library;
- XAudio2 2.7 from the legacy DirectX SDK if the audio spike selects it;
- xNVSE plugin headers pinned to a reviewed commit;
- an x86 FFmpeg build pinned by release tag and configure options;
- a small hook library only after the rendering spike compares the available choices.

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

FFmpeg should be built with only the libraries, demuxers, parsers, decoders, and conversion features required by the support contract. The planning baseline includes MP4/MOV demuxing, H.264 parsing and decoding, AAC decoding, core protocol support for local files or custom I/O, software scaling, and audio resampling.

Network protocols, encoders, capture devices, filters, command-line programs, and unrelated external codec libraries should be disabled. A smaller feature set reduces package size and the number of untested parsers in the game process.

Dynamic FFmpeg libraries are preferred because they keep FFmpeg separate from the plugin and make LGPL replacement practical. They belong in a private `PipBoyVideoPlayer\bin` directory. The plugin should construct absolute dependency paths and use restricted Windows DLL search APIs. FFmpeg DLLs must not be copied into the game root or the shared `Data\NVSE\Plugins` directory.

The final FFmpeg configure line, configuration report, source revision, patches, and license texts belong in the source repository and release notices once implementation starts.

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

The linker writes only the PDB filename into the DLL. The symbols archive renames the stripped output to that expected filename. Package checks inspect binary contents for repository, build, temporary, and user-profile paths. Checking archive entry names alone is insufficient.

The private Phase 1 recreation test uses a separate build directory and development-mod install script. CMake marks that directory as armed. The package script checks the marker and refuses to create either archive from it. Running the normal configure command explicitly disables the test and removes the marker from the normal build directory.

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
