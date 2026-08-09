# Decisions and open questions

## Recorded decisions

### Native decoding

Decision: decode MP4 files inside the game process through an xNVSE plugin and FFmpeg.

Reason: the project exists to remove the frame-extraction workflow used by older Pip-Boy video mods. Native decoding also permits variable frame rate timing, seeking, and bounded streaming memory.

Cost: a decoder and graphics hook inside a 32-bit game increase crash and licensing risk. The roadmap isolates those risks into separate gates.

### Documentation before implementation

Decision: keep this repository documentation-only until the phase-one work is approved.

Reason: the render location, UI bridge, device-reset behavior, and VNV conflicts decide whether the design is viable. Decoder code would not answer those questions.

### Data-page entry

Decision: place Videos under the Pip-Boy Data section instead of adding a fourth top-level tab.

Reason: many UI layouts and replacers assume three top-level tabs. A Data entry has a smaller compatibility surface.

### UIO injection

Decision: inject a prefab through UIO and do not overwrite a complete menu XML file.

Reason: UIO exists to compose UI extensions and is already in the VNV baseline.

### Native screen-space draw

Decision: let XML define the rectangle, then let the native renderer draw the decoded texture into that screen-space region.

Reason: the legacy XML image system has no planned interface for a live FFmpeg texture. Keeping coordinates in XML preserves layout flexibility.

This decision is provisional until phase one proves render ordering and coordinate conversion.

### Audio as master clock

Decision: use XAudio2's played-sample cursor as the master clock when audio is present. Use QueryPerformanceCounter for silent files.

Reason: synchronizing video to consumed audio prevents a slow game frame rate from causing cumulative drift.

### Bounded software decode

Decision: start with software decoding, a 1080p source cap, and small bounded queues.

Reason: hardware decoding adds device and format paths before the basic renderer is known to be stable. Bounded queues protect the game's limited address space.

### Private FFmpeg directory

Decision: place FFmpeg DLLs in a mod-private directory and load the pinned set through restricted absolute paths.

Reason: shared plugin or game-root DLLs can collide with unrelated mods and allow accidental version substitution.

### No save persistence

Decision: store no media or playback state in game saves or xNVSE co-saves for the first release.

Reason: the player should be safe to add or remove and should not leave missing state in a playthrough.

## Open questions before phase one

1. Which game render function provides the correct order relative to Pip-Boy menu drawing?
2. Can an existing maintained hook framework chain safely at that location, or does the project need a small local hook layer?
3. How will the plugin obtain and verify the Direct3D device without a root proxy DLL?
4. What coordinate transforms are required between UI tile space and the backbuffer for each aspect ratio?
5. Does DXVK preserve the selected D3D9 behavior and Reset path?
6. Which Pip-Boy replacers retain the same menu coordinate contract?
7. Can the UI provide a clean Data entry without editing scripts or requiring an ESP?

## Open questions before phase two

1. Which FFmpeg release and minimal configure set will be pinned?
2. Will the x86 build use MSVC-compatible import libraries or a fully MinGW-built dependency set?
3. Does custom Win32 I/O see all media supplied by MO2's virtual filesystem?
4. Which malformed-media corpus can be used without redistribution problems?
5. Should the decoder scale directly to the current presentation rectangle or to a fixed intermediate size?
6. What exact queue byte limits fit the VNV address-space budget under a large mod list?

## Open questions before phase three

1. Is XAudio2 2.7 the right compatibility target, or should the plugin select 2.8 or 2.9 dynamically on newer Windows?
2. How should player volume relate to the game's master and effects volume?
3. What buffering depth avoids underruns without making pause and seek feel sluggish?
4. How should audio-device removal appear to the user?

## Open questions before release

1. What project license will govern original code and documentation?
2. Which H.264 and AAC binary distribution obligations apply to the chosen release method?
3. Which graphics injectors can be supported honestly?
4. Should PDB files ship in the main archive or a separate symbols archive?
5. What is the final project name, mod-page name, and configuration prefix?
6. What minimum xNVSE and UIO versions follow from the implementation rather than the planning environment?

## Decision process

A closed question should become a dated entry with the evidence, chosen option, rejected alternatives, and consequences. If testing later overturns it, add a new entry that supersedes the old one instead of silently rewriting the history.
