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

### Full-glass playback stage

Date: August 9, 2026

Decision: use the largest safe named rectangle inside the active Pip-Boy glass
for playback. Show the catalog before playback and keep controls in a temporary
overlay instead of reserving a permanent side panel.

Evidence: the Pip-Flicks 3000 author screenshot places the picture across the
usable screen while preserving the physical device frame. The first native draw
also proved that UIO can provide a named rectangle without replacing a complete
menu file.

Rejected alternative: a catalog and video panel shown side by side would reduce
the already small playback area. Copying Pip-Flicks' Apparel item and ESP flow
would conflict with the recorded ESP-less design.

Consequence: the Phase 1 prefab will test full-glass bounds, clipping, and layer
order across the UI matrix. Pip-Flicks is a design and compatibility reference,
not a runtime dependency. Its implementation or assets will not be copied unless
the author permission clearly covers that use.

### Leave render targets untouched

Date: August 9, 2026

Decision: the screen-space draw must not rebind the current render target or depth surface because it never changes either object. Restore only the pipeline state changed by the plugin through a one-use `D3DSBT_ALL` state block.

Evidence: Microsoft documents that `SetRenderTarget` resets the viewport to the full size of the target. Rebinding the unchanged target after `state->Apply()` could overwrite the viewport that the state block had just restored. Microsoft also documents that `CreateStateBlock` captures the selected state immediately.

Rejected alternative: rebinding the original surfaces and then manually restoring the viewport adds calls and still risks missing another coupled state. If a future path changes render targets, it will need a separate reviewed restoration sequence.

Consequence: the Alt+Tab and state-isolation tests will use the corrected path. The first 300-frame timing remains a baseline for the older redundant sequence and will be measured again.

### xNVSE frame-present boundary, superseded

Date: August 9, 2026

Decision: draw from xNVSE 6.4.5's `kMessage_OnFramePresent` notification and ignore notifications marked as loading screens.

Evidence: xNVSE dispatches the notification immediately before the engine presentation call from its main-loop and loading-screen patches. The callback therefore runs at a known render-thread boundary without another patch at `Present` or `EndScene`.

Rejected alternatives: a root `d3d9.dll` proxy would conflict with common wrappers. Rewriting the live device vtable is unsafe when another graphics plugin owns it. An independent `Present` detour would duplicate a boundary that xNVSE already provides.

Consequence: Phase 1 needs no presentation hook. It still needs measured proof that this boundary has the required Pip-Boy draw order under native Direct3D 9 and any claimed DXVK configuration.

### Frame-present draw order rejected

Date: August 9, 2026

Decision: do not use `kMessage_OnFramePresent` as the final video draw location. Keep the verified callback available for diagnostics while Phase 1 tests an engine-owned render point that runs before menu XML is drawn.

Evidence: the test prefab placed a black image and the text `PBVP UI LAYER` inside `PBVP_VideoRect`. UIO processed and injected that updated prefab during the same run. The native bridge resolved the rectangle, and the checkerboard drew successfully. The user saw the checkerboard cover the Pip-Boy but saw neither probe element. This confirms that the native draw runs after the menu UI at the current callback.

Rejected alternative: drawing the playback controls in Direct3D would duplicate the UIO control layer and make input, fonts, scaling, and UI compatibility harder. The project will first test credible engine render locations before considering that design.

Consequence: the visible checkerboard proves device access and coordinate conversion only. It does not prove a usable presentation path. FFmpeg integration remains blocked on a render point where UIO controls can appear above the video.

Candidate under test: replace the normal-frame relative call at `0x00870403` only when its live target is the reviewed `0x00709B40` routine. The replacement draws first and then calls the original routine. This is not a final render-point decision until the black strip and `PBVP UI LAYER` text appear above the checkerboard in game.

### Normal-frame pre-UI candidate rejected

Date: August 9, 2026

Decision: remove the normal-frame call replacement and test an engine-owned `TileImage` surface. Gamebryo will draw the image in XML order; the plugin will update pixels without issuing a separate primitive draw.

Evidence: the runtime log decoded the unmodified call at `0x00870403` to `0x00709B40`, installed the replacement, and recorded successful checkerboard draws. The user saw the checkerboard hovering over the Pip-Boy, saw neither the black strip nor `PBVP UI LAYER`, and confirmed that the ordinary frame and controls remained visible and usable. Draw samples averaged 47.08 to 63.36 microseconds. The failure is therefore the visible order of the candidate, not hook installation or device access.

Rejected alternative: continue trying frame-wide calls between `Main::Render` and presentation. Two credible late-frame boundaries have now produced the same overlay behavior, and another nearby call would not give the video an owned place within the XML child order.

Consequence: Phase 1 will use the named UIO image as the presentation owner. The next spike must validate the maintained `TileImage` to `NiTexture` to `NiDX9TextureData` chain, prove thread identity, and update only a private texture. The direct checkerboard renderer remains useful as measured evidence but is no longer a release path.

### Deferred private image load

Date: August 9, 2026

Decision: keep the UIO-owned image, but refresh its private filename once through the reviewed `Tile::SetStringValue` function after the live MapMenu is available. If the tile already contains the expected filename, the bridge clears and restores that trait so the engine sees a real value change. Both calls run on the game thread. The render callback remains read-only toward game objects.

Evidence: the first engine-image run showed `PBVP UI LAYER`, the normal Pip-Boy frame, and usable controls. The plugin found `PBVP_VideoSurface`, verified it as a `TileImage`, and confirmed that the game and Direct3D callbacks share one operating-system thread. Its texture reference at offset `0x3C` remained null across repeated Pip-Boy openings. A separate 32-bit `D3DXGetImageInfoFromFileInMemory` check accepted the packaged DDS as a 256x256, one-level `A8R8G8B8` texture, so the file is not rejected by the game version's DirectX image parser.

Rejected alternatives: the plugin will not write an engine reference-counted field, reuse a shared UI texture, or call the less-understood `TileImage::SetTexture` routine. Changing the DDS header without evidence would not address the verified DirectX result.

Consequence: this candidate was tested once and rejected by the follow-up run described below.

### Tile shader source texture path

Date: August 9, 2026

Decision: remove the filename refresh and resolve the image used for drawing through `TileImage::shaderProp` at offset `0x40`, followed by `TileShaderProperty::srcTexture` at offset `0x60`. The bridge must verify the exact `TileShaderProperty` and `NiSourceTexture` vtables before following either object. It may use the direct `TileImage::texture` member at offset `0x3C` only when that member is non-null and has the exact `NiSourceTexture` vtable.

Evidence: the filename-refresh build produced the same visible result as the first engine-image build. The user saw `PBVP UI LAYER` and usable Pip-Boy controls, but no private surface or checkerboard. The log recorded a null `TileImage + 0x3C` member and a stable non-null `+0x40` member before and after the refresh. Psycho's maintained decompilation of the `TileImage` node-building routine passes `+0x40` into the engine's render-object setup. JIP LN NVSE identifies that member as `TileShaderProperty`, gives its vtable as `0x010B9D28`, and places the visible source texture at `TileShaderProperty + 0x60`.

Rejected alternatives: `+0x40` is not a texture and will not be cast as one. The plugin will not write either engine field or call a texture replacement routine. Keeping the filename refresh would add game-object mutation without addressing the field that Gamebryo uses for drawing.

Consequence: the next build will follow the shader property's source texture only after both object types match. Its diagnostic record will include the direct texture, shader property, shader source texture, and their vtables. Any mismatch leaves the Pip-Boy usable and disables the upload.

### MapMenu video sibling depth

Date: August 9, 2026

Decision: assign depth 10 to `PBVP_Root` for the current Vanilla UI Plus feasibility candidate. Keep the surface, probe background, and probe text ordered within that root. Do not raise the whole root above the existing headline or tab controls.

Evidence: the shader-source build verified the full engine and Direct3D texture chain and uploaded the checkerboard repeatedly. The checkerboard was visible for about one second during the Items-to-Data transition, then normal MapMenu content covered it. The active MapMenu uses depths through 8 for page content, 15 for headline cards, and 22 for the tab line. The PBVP root previously had no explicit sibling depth.

Pip-Flicks 3000 provides a useful behavioral comparison but not a drop-in placement. It replaces Pip-Boy background frames while the Items screen is active and stops video when the player switches to Stats or Data. PBVP keeps its ESP-less UIO design and uses the reference only to confirm that an engine-owned Pip-Boy texture can carry video beneath native controls.

Rejected alternatives: a frame-wide overlay already failed the layer-order test. Setting the PBVP root above depth 15 could obscure existing navigation. Copying the Pip-Flicks ESP, scripts, frames, audio, or assets is unnecessary and would change PBVP's architecture and distribution obligations.

Consequence: the next run kept the same transient flash. Parent depth alone is rejected as a complete placement rule.

### Explicit drawable depths

Date: August 9, 2026

Decision: retain depth 10 on `PBVP_Root`, then assign the actual video surface, black probe, and text probe depths 10, 11, and 12. All PBVP drawables remain below native headline cards at depth 15 and the tab line at depth 22. The PBVP containers remain non-targetable so this test does not consume input.

Evidence: the parent-only depth candidate still disappeared after the Data transition. The surface and label appeared together during each brief transition, with the label correctly above the checkerboard. The runtime log showed a valid texture and a successful upload on every MapMenu return. The active Vanilla UI Plus XML gives drawable children explicit depths and explicitly derives a shadow child's depth from its parent image instead of relying on a container ancestor.

Rejected alternatives: repeated texture uploads would not change the draw order and would add unnecessary per-frame work. A native overlay remains rejected because it covered the Pip-Boy. Raising PBVP above depth 15 could hide headline or tab controls.

Consequence: the next run kept the checkerboard, black probe, and text probe visible together, and the user reported that the result looked good. This accepts the depth rule for the active VNV Extended stack. Native input, other UI profiles, resolutions, display modes, and DXVK remain separate acceptance checks.

### Lower-left status overlay

Date: August 9, 2026

Decision: anchor the temporary black status strip 12 units from the left and bottom edges of `PBVP_VideoRect`. Position its text 10 units from the strip's left edge and 3 units from its top edge. Keep both positions derived from named sibling or parent traits instead of duplicating absolute screen coordinates.

Evidence: the first successful steady presentation placed the diagnostic strip at the upper-left of the video area. Keyboard and mouse input remained usable, but the user reported that this placement got in the way of the radio station list and preferred the lower-left area.

Rejected alternatives: the status overlay will not become a permanent panel beside the picture. The final player shows it only on the Videos page and fades it during playback. Ordinary Radio, Map, Quests, and Notes pages must not retain any PBVP playback layer.

Consequence: the next Phase 1 package moved only the diagnostic strip. The user reported that the checkerboard did not move, which rejects this as the requested placement change. The relative status-strip layout remains useful inside the player, but the complete video rectangle must move with it.

### Lower-left diagnostic video rectangle

Date: August 9, 2026

Decision: anchor `PBVP_VideoRect` 12 units from the left and bottom edges of `PBVP_Root`. Preserve the current 560 by 315 logical size and the tested drawable depths. All child surfaces and overlays move with the rectangle.

Evidence: after the status-only candidate, the user clarified that the checkerboard was expected to move. The current video rectangle still used fixed coordinates `x = 80` and `y = 65`, so moving its children could not satisfy that request.

Rejected alternatives: do not assign new absolute screen coordinates for the active 1920x1080 profile. Derive the vertical position from the parent height and the rectangle height so the candidate responds to UI scaling. Do not resize the video until the new location is seen in game.

Consequence: the next run resolved the rectangle 68 units farther left and 168 units lower, but the user still described the checkerboard as top-left. The unchanged 560 by 315 size prevents this candidate from reading as a small lower-left panel. Do not guess a smaller playback size without resolving that design choice because it conflicts with the documented full-glass stage.

### Compact lower-left diagnostic viewport

Date: August 9, 2026

Decision: use a 320 by 180 `PBVP_VideoRect` for the next Phase 1 placement test. Retain the 12-unit lower-left anchor and the tested drawable depths. Keep the 256 by 256 private DDS because it is only a generated upload target, not the final decoded-frame size.

Evidence: the user clarified that the complete checkerboard must move away from the radio station list at the upper-left. The 560 by 315 candidate was already bottom-anchored in the live trait geometry, but its height still placed its top edge at logical `y = 276`. Reducing the height to 180 moves that edge down by another 135 units while preserving the verified bottom edge.

Rejected alternatives: another coordinate-only change would ignore the measured reason the large surface still reaches the upper-left. Do not change the final aspect-fit or aspect-fill contract based on this diagnostic texture. Those modes still apply inside the selected playback viewport.

Consequence: the next run must show the full checkerboard as a smaller lower-left panel, leave the upper radio station list visible, and preserve keyboard and mouse input.

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

1. Which verified engine-owned render point places the video below UIO controls without interfering with the world or other menus?
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
