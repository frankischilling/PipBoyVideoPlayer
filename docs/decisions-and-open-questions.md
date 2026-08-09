# Decisions and open questions

## Recorded decisions

### Native decoding

Decision: decode MP4 files inside the game process through an xNVSE plugin and FFmpeg.

Reason: the project exists to remove the frame-extraction workflow used by older Pip-Boy video mods. Native decoding also permits variable frame rate timing, seeking, and bounded streaming memory.

Cost: a decoder and graphics hook inside a 32-bit game increase crash and licensing risk. The roadmap isolates those risks into separate gates.

### Documentation before implementation

Decision: the documentation-only period ended on August 9, 2026. Phase 1 implementation may proceed from the recorded VNV inspection and source research.

Reason: the render location, UI bridge, device-reset behavior, and VNV conflicts still decide whether the design is viable. The repository now has enough evidence to build and test that spike before decoder work.

### Data-page entry

Decision: place Videos under the Pip-Boy Data section instead of adding a fourth top-level tab.

Reason: many UI layouts and replacers assume three top-level tabs. A Data entry has a smaller compatibility surface.

### UIO injection

Decision: inject a prefab through UIO and do not overwrite a complete menu XML file.

Reason: UIO exists to compose UI extensions and is already in the VNV baseline.

### Corrected UIO registration record

Date: August 9, 2026

Decision: retain the `MapMenu::MM_MainRect` target for the Phase 1 prefab and add the required `true` condition line. Keep the UIO composition design.

Evidence: the first in-game test showed that UIO found the registration and prefab but did not add them to the map menu. UIO's bundled author instructions define each public entry as a target line followed by a condition line and recommend `true` for an unconditional entry. The working Mojave Radio Captions registration follows that format. The plugin log consequently contained no rectangle or Direct3D activity.

Rejected alternatives: adding another path component does not match UIO's three-part public entry format. Replacing the complete map menu would bypass UIO but violate the compatibility design.

Consequence: a missing UIO add or inject entry is a failed acceptance check, even if the plugin DLL loads normally. The native tile bridge remains a separate acceptance check.

Verification: the next VNV Extended run logged both the add and inject operations for `Player.xml` at `MM_MainRect`. The prefab still did not reach a visible native draw, so this closes only the registration-format defect.

### Official UI trait identifiers

Date: August 9, 2026

Decision: source standard UI trait identifiers from xNVSE's `Tile` definitions. Do not copy their numeric values into the bridge.

Evidence: a diagnostic run found `PBVP_VideoRect` but reported that its height or width trait was unavailable. The local constants were one position early because the game enumeration skips `0xFA5`. This made the bridge query `target` and `height` instead of `height` and `width`.

Consequence: the corrected build still needs a visible draw test. Custom trait names, when introduced, will be resolved through the game trait registry instead of assigned guessed identifiers.

### Logical UI canvas

Date: August 9, 2026

Decision: resolve the logical canvas from the bounded ancestor chain above `MapMenu`. Use the first tile with a positive standard width and height pair.

Evidence: the native bridge found the video rectangle and its dimensions, but `MapMenu` did not expose width and height. The active Vanilla UI Plus globals use the shared `screen` tile as the logical coordinate canvas and apply `resolutionconverter` when deriving physical dimensions.

Rejected alternative: using the Win32 client size would treat logical XML coordinates as physical pixels and produce incorrect placement when UI scaling or aspect ratio changes.

Consequence: the first fullscreen 1920x1080 test resolved a 1706.67x960 logical canvas and produced a visible checkerboard. Other resolutions and UI scales still need independent verification.

### Native screen-space draw

Decision: let XML define the rectangle, then let the native renderer draw the decoded texture into that screen-space region.

Reason: the legacy XML image system has no planned interface for a live FFmpeg texture. Keeping coordinates in XML preserves layout flexibility.

The first fullscreen 1920x1080 VNV Extended test verified a visible draw and coordinate conversion. This decision remains provisional until layer order, state isolation, device recovery, and the remaining compatibility matrix pass.

### xNVSE frame-present boundary

Date: August 9, 2026

Decision: draw from xNVSE 6.4.5's `kMessage_OnFramePresent` notification and ignore notifications marked as loading screens.

Evidence: xNVSE dispatches the notification immediately before the engine presentation call from its main-loop and loading-screen patches. The callback therefore runs at a known render-thread boundary without another patch at `Present` or `EndScene`.

Rejected alternatives: a root `d3d9.dll` proxy would conflict with common wrappers. Rewriting the live device vtable is unsafe when another graphics plugin owns it. An independent `Present` detour would duplicate a boundary that xNVSE already provides.

Consequence: Phase 1 needs no presentation hook. It still needs measured proof that this boundary has the required Pip-Boy draw order under native Direct3D 9 and any claimed DXVK configuration.

### Verified reset hook only

Date: August 9, 2026

Decision: use a pinned MinHook 1.3.4 detour on `NiDX9Renderer::Recreate` only after an exact runtime signature check accepts the original function entry. Refuse rendering for the session if the entry is already redirected or its bytes are unknown.

Evidence: maintained Fallout NV graphics code identifies `NiDX9Renderer::Recreate` as the engine boundary for device recreation. Direct3D requires default-pool resources to be released before `Reset`, so detecting loss after presentation is not sufficient.

Rejected alternatives: hooking the Direct3D device `Reset` vtable entry has the same live-vtable ownership problem as a `Present` hook. Guessing through an occupied entry point cannot provide a safe chain order.

Consequence: the plugin accepts one relocation-free signature recovered from an independent Ghidra audit. It logs the live decrypted entry and installs the hook only after an exact match. Unknown or occupied entries fail closed.

Verification: on August 9, 2026, the patched local Steam executable exposed the exact reviewed signature after runtime decryption. The hook installed with the VNV Extended graphics plugin stack present, and the game exited normally from the main menu.

### Private development licensing

Date: August 9, 2026

Decision: original work remains all rights reserved during private development. The repository and release candidate remain private. No public binary or public source release will be made until the owner selects a project license and the exact FFmpeg build receives a distribution review.

Reason: this permits the requested private implementation and testing without making a license choice on the owner's behalf.

Consequence: dependency notices and build records will be complete in the private candidate, but publication remains an explicit approval gate.

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

1. Does the xNVSE frame-present boundary place the draw at the correct depth relative to every required Pip-Boy layout?
2. What coordinate transforms are required between UI tile space and the backbuffer for each aspect ratio?
3. Does DXVK preserve the selected D3D9 behavior and Reset path?
4. Which Pip-Boy replacers retain the same menu coordinate contract?
5. Can the UI provide a clean Data entry without editing scripts or requiring an ESP?

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
