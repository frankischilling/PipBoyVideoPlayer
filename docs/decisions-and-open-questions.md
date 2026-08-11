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

Consequence: the next run made the checkerboard smaller but left it at the upper-left. The bridge resolved the container at `42,411` through `362,591`, so the container traits changed while its child image stayed at local `x = 0` and `y = 0` in the existing locus. This rejects the assumption that nesting alone gives children the rectangle's visual origin.

### Locus-owned diagnostic viewport

Date: August 9, 2026

Decision: set `locus = 1` on `PBVP_VideoRect`. Keep the video surface at local `x = 0` and `y = 0`, and keep the status strip derived from the same rectangle. The rectangle remains the single owner of the viewport position.

Evidence: the compact run logged `PBVP_VideoRect` at `42,411` through `362,591`, but the user saw only a resize at the original upper-left position. The active Vanilla UI Plus `MM_MainRect` and its positioned child containers set `locus = 1`. The installed B42 Recoil prefab uses the same trait when a nested window supplies the origin for its children.

Rejected alternatives: do not duplicate the screen position on the surface, strip, and text. That would create several independent anchors and make later scaling changes harder to verify. Do not resize the checkerboard again because the latest run proved that size was not the remaining placement fault.

Consequence: the next run moved the checkerboard and status elements together. The user reported that the result looked great and occupied a good lower-left position. This accepts the locus rule for the active VNV Extended UI stack.

### Slightly larger lower-left diagnostic viewport

Date: August 9, 2026

Decision: increase `PBVP_VideoRect` from 320 by 180 to 384 by 216. This is a 20 percent increase in each dimension and retains the 16:9 shape, 12-unit lower-left inset, locus, and tested draw depths.

Evidence: the user accepted the locus-corrected lower-left position and asked for the complete checkerboard to be a little bigger.

Rejected alternatives: do not return to the 560 by 315 rectangle because it covered the upper radio station list. Do not change the anchor or locus in the same candidate because their visual behavior just passed.

Consequence: the next run resolved the panel at `42,375` through `426,591`. The user reported that the larger panel looked good, so 384 by 216 is the accepted Phase 1 size for the active fullscreen 1920x1080 VNV Extended profile. Other resolutions and UI profiles still need separate checks.

### Base UI button clearance candidate

Date: August 10, 2026

Decision: raise `PBVP_VideoRect` by changing its bottom inset from 12 to 64 logical units. Keep the accepted 384 by 216 size, left inset, locus, texture path, and draw depths unchanged.

Evidence: the first 1920x1080 native windowed Base VNV run displayed the checkerboard and otherwise looked and ran correctly. The user reported that the panel sat a little too low and covered the Local Map and World Map buttons. This disproves the assumption that the Extended lower-left position clears controls in the base UI.

Rejected alternatives: do not shrink the accepted viewport, move it horizontally toward the list content, hide the native map buttons, or special-case the Base profile before one shared raised position is tested across both layouts.

Consequence: the next candidate moves the panel up 52 logical units. On the previously measured 1706.67 by 960 canvas, its expected rectangle changes from `42,375` through `426,591` to `42,323` through `426,539`. The candidate needs a Base visual check and an Extended regression check before it becomes the shared Phase 1 position.

Follow-up evidence: the user tested the raised candidate in the same Base VNV windowed configuration and reported that it looked good. The panel no longer blocks the Base map buttons. Later runs with the full Extended stack, isolated Vanilla UI Plus, and Extended without Pip-Boy UI Tweaks also looked good. The raised 384 by 216 panel is now the shared Phase 1 position for all four isolated UI profiles.

### Verified reset hook only

Date: August 9, 2026

Status: superseded on August 10, 2026 by the managed-texture decision below.

Decision: use a pinned MinHook 1.3.4 detour on `NiDX9Renderer::Recreate` only after an exact runtime signature check accepts the original function entry. Refuse rendering for the session if the entry is already redirected or its bytes are unknown.

Evidence: maintained Fallout NV graphics code identifies `NiDX9Renderer::Recreate` as the engine boundary for device recreation. Direct3D requires default-pool resources to be released before `Reset`, so detecting loss after presentation is not sufficient.

Rejected alternatives: hooking the Direct3D device `Reset` vtable entry has the same live-vtable ownership problem as a `Present` hook. Guessing through an occupied entry point cannot provide a safe chain order.

Consequence: the plugin accepts one relocation-free signature recovered from an independent Ghidra audit. It logs the live decrypted entry and installs the hook only after an exact match. Unknown or occupied entries fail closed.

Verification: on August 9, 2026, the patched local Steam executable exposed the exact reviewed signature after runtime decryption. The hook installed with the VNV Extended graphics plugin stack present, and the game exited normally from the main menu.

Follow-up evidence: the current Psycho audit at commit `85c96c1415b636051dff690036b510761de25d7a` proves that the native function returns `0` on failure, `1` after recovering the original presentation parameters, and `2` after applying the requested parameters. The caller treats any nonzero result as usable, but PBVP accepts only the two documented success values. It also requires the renderer to publish a device before clearing its lost state. The next frame validates that device and reacquires the engine-owned texture.

### Opt-in deferred recreation test build

Date: August 9, 2026

Decision: provide a private compile-time test option that writes Fallout's deferred recreation request once, after the checkerboard upload path has validated the shared game and render thread. Verify the exact 23-byte main-loop request gate before writing the byte. Keep the option disabled in normal builds and refuse to package any build directory marked as armed.

Evidence: five Alt+Tab cycles preserved the managed texture but never called `NiDX9Renderer::Recreate`. The current Psycho checkout at commit `22b0030cd48d190a0cd9a0b4a945ebc2585b338e` identifies the full recreation helper at `0x004DC360`. The game main loop calls that helper only when byte `0x011C6FBB` is nonzero, then clears the byte. Its Ghidra cross-reference report finds no other executable reads or writes. The helper requires nonzero requested width and height values at `0x011C70E0` and `0x011C70E4` before calling the reviewed renderer owner. These are distinct from the active render-size globals at `0x0118947C` and `0x01189480`.

Rejected alternatives: do not call the renderer owner directly, call `IDirect3DDevice9::Reset`, patch a device vtable, or enable this request in a normal package. Those paths bypass engine ordering or leave an unsafe test control in the release build.

Consequence: the private candidate can exercise the engine-owned reset order without guessing function arguments. It still requires an in-game run and a clean log with one successful recreation. The normal build cannot schedule the request.

Runtime result and decision update: the August 10, 2026 VNV Extended run wrote the request after a successful checkerboard upload, but the verified `NiDX9Renderer::Recreate` detour never ran. About ten seconds later the MapMenu snapshot became unavailable and the available engine texture received another checkerboard upload. The log does not establish that the request caused the UI transition. The user's report that the checkerboard returned proves surface recovery after the transition, not a Direct3D device recreation.

The deferred request byte is no longer accepted as a reset test by itself. The follow-up private diagnostic now observes whether the byte stays pending, is consumed, changes unexpectedly, or times out after five seconds. It adds no hook and does not call the renderer owner or Direct3D device directly. Phase 1 still requires a log containing the detour's before and after records, a successful texture reacquisition, and an orderly session summary.

Second runtime result and decision update: the clean follow-up run did not write the request because a helper precondition was unavailable. The checkerboard remained visible, the renderer summary recorded one successful upload and no failures, and the process shut down normally. Because the device path had already resolved the renderer singleton from the same address, one or both requested-size values remain the likely failed checks. The next diagnostic will identify each value without changing it. Direct recreation and unguarded writes remain rejected.

Third runtime result and decision update: both requested-size addresses were readable and held zero. The renderer address was readable and nonzero, the Direct3D backbuffer was 1920x1080, the checkerboard stayed visible, and the session ended with a clean summary. The helper therefore returned at its requested-size checks in both earlier runs. A consumed byte with zero requested dimensions cannot exercise recreation.

Decision: the next private test may copy an independently verified active width and height into the two transient requested-size globals before setting the deferred byte. It must require zero original values, writable memory, a matching active render size and Direct3D backbuffer, bounded dimensions, and successful readback. It writes the request only after those checks. After the main loop consumes or cancels the request, the diagnostic restores the original values and verifies the restoration. Shutdown also cancels a still-pending request and restores the values. This remains unavailable in normal builds.

Reason: the maintained Psycho display implementation identifies the target setting objects as `iSize W` and `iSize H`, and the audited helper copies the transient request into those settings before entering the complete engine recreation sequence. Using the already active size requests no display-mode change. It only supplies the values required to traverse the same deferred engine path.

Rejected alternatives: do not leave the transient values changed, accept a mismatch between active and backbuffer dimensions, overwrite an existing nonzero request, call the recreation helper directly, or call `IDirect3DDevice9::Reset`.

Fourth runtime result and decision update: the guarded same-size request reached the verified `NiDX9Renderer::Recreate` detour and then froze before the original engine call returned. The log contains the pre-recreation record and none of the expected return, consumption, restoration, summary, or shutdown records. CrashLogger produced no dump. This disproves the assumption that matching dimensions and reversible field staging are sufficient to invoke the full renderer transition safely.

Decision: retire the private forced-recreation diagnostic and remove its request scheduler. Do not repeat it with other dimensions or broaden it into a direct helper, renderer, or Direct3D reset call. Keep the normal development DLL installed while the production lifecycle boundary is reassessed.

Reason: PBVP owns no persistent Direct3D resource in the current renderer path. It temporarily references an engine-owned `D3DPOOL_MANAGED` texture, updates it, and releases the reference before the callback returns. The user's successful Alt+Tab and reopen checks already show that this surface survives the tested native fullscreen transitions. A synthetic full renderer restart introduces game-wide callback and resource ownership that PBVP cannot safely reconstruct.

Consequence: Phase 1 still needs a safe lifecycle result, but forced recreation is no longer an acceptance route. The next architecture change must either observe a naturally initiated engine recreation or remove the recreation detour after proving that PBVP retains no default-pool resource across callbacks. No DXVK or untested display-mode claim follows from the managed-texture result.

### Managed texture without a reset detour

Date: August 10, 2026

Decision: require the engine-owned video texture to report `D3DPOOL_MANAGED`, release every temporary texture and device reference before the frame callback returns, and remove the `NiDX9Renderer::Recreate` detour. If a UI stack supplies a default-pool or unknown-pool texture, disable video updates for that session.

Evidence: the accepted VNV Extended runs report pool value `1`, which is `D3DPOOL_MANAGED`. The renderer acquires the texture only for validation and upload, releases the device reference returned by `GetDevice`, and releases the texture reference before returning. It stores only non-owning identities so a later frame can detect and validate a replacement device or surface. Five Alt+Tab cycles and ten Pip-Boy reopen cycles kept the checkerboard visible, and the 19-upload run reacquired changing engine surface identities without a recreation callback. The first build without the reset detour also kept the panel visible through Alt+Tab, uploaded in 26.30 microseconds with no failure, and shut down normally.

Reason: Direct3D 9 owns managed-resource eviction and restoration across `Reset`. PBVP has no default-pool allocation, state block, render target, vertex buffer, or retained COM reference to release before that operation. Detouring a game-wide reset function adds a conflict and failure boundary without protecting a PBVP-owned reset-sensitive object.

Rejected alternatives: do not keep the detour only for observation, patch the Direct3D device vtable, call the renderer directly, or accept a default-pool texture without an audited pre-reset owner. A future upload path that introduces a default-pool resource must reopen this decision before the resource is added.

Consequence: Phase 1 uses the xNVSE frame-present notification as its only runtime callback boundary and performs no executable or device-vtable patch. The next implementation must enforce the managed-pool contract and treat a changed device or surface as a fresh validation. Natural display-transition testing remains required, but no synthetic reset is permitted.

### Reversible compatibility cases

Date: August 9, 2026

Decision: configure Phase 1 display cases only in the save-free PBVP MO2 profiles. Use the existing FalloutNV.exe RTSS application profile for the required 30, 60, 90, and 120 FPS caps. Save the original profile INIs and RTSS profile before the first case. Restore exact bytes when the script owns the write. If RTSS requires a manual UI change, require every functional line to match and allow only its `[Info]` timestamp to change.

Evidence: the installed NVTF configuration exposes a maximum timing tolerance, not a general frame limiter. The maintained Fallout NV Performance Guide recommends RTSS and states that VSync is not a limiter. RTSS is already installed with a FalloutNV.exe profile, passive waiting, and front-edge sync. DXVK also supplies a built-in D3D9 cap, but its maintained configuration reference recommends an external limiter when one is available.

Rejected alternatives: do not edit the original VNV profiles, use VSync as a cap, repurpose NVTF's tolerance setting, or change the limiter between native Direct3D 9 and DXVK. Do not automate changes to a profile that contains saves.

Consequence: the test-case script accepts only the documented Phase 1 resolutions, frame caps, display modes, VSync states, and isolated profile names. It refuses cross-profile overlap and restores the original files. Manual in-game checks remain necessary for every recorded result.

Final cleanup result: the user restored `Limit=0`, left `LimitDenominator=1`, and closed RTSS. The current and saved 1,394-byte profiles have identical sections and functional values. Nine bytes differ only in the `[Info]` timestamp written by RTSS when the profile was saved. This disproves the assumption that a normal manual restoration preserves the complete file hash, but it confirms that the test cap was removed without changing another setting.

First capped result: the 1280x720 native windowed case used VSync off and the existing RTSS FalloutNV profile at 30 FPS. Six visible cadence samples measured 30.00 to 30.01 FPS. The user reported that the panel and input looked good. The log confirmed the requested backbuffer, one 25.70-microsecond upload, no failure, both summaries, and a clean shutdown. This accepts the 1280x720, 30 FPS, and VSync-off row. It does not cover another resolution or cap.

Second capped result: the first 1280x960 visual run passed placement and input, but focus or menu pauses lowered its cadence average to 41.97 FPS. The strict 60 FPS check rejected that timing sample. A retry kept the Data page focused and untouched. Its eight samples measured 59.98 to 60.04 FPS with a 60.00 FPS average. The retry confirmed the 1280x960 backbuffer, one 26.30-microsecond upload, no failure, an empty save directory, and a clean shutdown. This accepts the 4:3 resolution, 60 FPS cap, and VSync-on row without relabeling the disturbed run as a pass.

Third capped result: the 2560x1440 native windowed case used VSync off and RTSS at 90 FPS. The user reported that the visible panel looked good on the 1920x1080 monitor. Eight cadence samples measured 89.99 to 90.04 FPS with a 90.01 FPS average. The log confirmed the requested backbuffer, one 25.70-microsecond upload, no failure, an empty save directory, and a clean shutdown. This accepts the 2560x1440 and 90 FPS rows. The larger window was clipped by the physical display, so this result is limited to the visible panel and logged backbuffer.

Fourth capped result: the 3440x1440 native windowed case used VSync on and RTSS at 120 FPS. The user reported that the visible panel looked good. Eight cadence samples measured 119.89 to 120.19 FPS with a 120.01 FPS average. The log confirmed the requested ultrawide backbuffer and logical canvas, one 24.90-microsecond upload, no failure, an empty save directory, and a clean shutdown. This accepts the 3440x1440 and 120 FPS rows. As with 2560x1440, the physical display clipped part of the larger window.

### Native fullscreen focus-cycle baseline

Date: August 10, 2026

Decision: treat the 1920x1080 native Direct3D 9 fullscreen focus-cycle result as a failing VNV baseline. Do not change PBVP to address the NVIDIA driver trace unless a matched PBVP-disabled configuration stops reproducing it. Continue the Phase 1 matrix with native Direct3D 9 windowed mode and test DXVK separately.

Evidence: the first two measured runs with PBVP loaded crashed after four and nine completed focus cycles. Both reports remained inside `nvd3dum`, and the second exactly matched a March crash from before PBVP development. The PBVP-disabled control completed 50 cycles, then crashed one second later with the same six driver frames. PBVP was absent from the module list, and its log file remained byte-for-byte unchanged.

Reason: the control isolates the crash from PBVP, even though the enabled runs reached it sooner. The maintained FNV performance guide documents unstable Alt+Tab behavior in legacy fullscreen mode and recommends windowed mode or DXVK for the affected presentation paths. The active NVTF pool option differs from the guide's explicit failure case, so the precise driver trigger remains unknown.

Rejected alternatives: do not hide the control crash, count the result as a clean 50-cycle pass, patch the NVIDIA driver path, force a Direct3D reset, or weaken PBVP's texture validation. Do not claim native fullscreen Alt+Tab support from this installation.

Consequence: the accepted fullscreen evidence remains limited to ordinary presentation, short Alt+Tab checks, and the earlier uncounted stress run. A supported repeated-focus configuration still requires a clean measured windowed or DXVK result. Native fullscreen can remain a playback configuration only if its focus-loss limitation is explicit and other required lifecycle checks pass.

### Native windowed focus-cycle acceptance

Date: August 10, 2026

Decision: accept native Direct3D 9 windowed mode at 1920x1080 with VSync on for the Phase 1 repeated-focus requirement. Keep native fullscreen and DXVK as separate compatibility cases.

Evidence: the PBVP-enabled windowed run completed all 50 focus cycles with no incomplete loss. FalloutNV remained running at the target, then exited normally. The plugin recorded three successful managed-texture uploads from 25.00 to 51.20 microseconds, no upload failure, complete renderer and cadence summaries, and a clean shutdown. The strict log check passed, and no new crash report appeared.

Reason: this is the first exact 50-cycle run that completed without the native fullscreen driver failure. It uses the same hook-free managed-texture renderer as the failing fullscreen baseline, so no renderer change is needed to explain or accept the windowed result.

Rejected alternatives: do not treat this as evidence for fullscreen or DXVK, combine it with the disabled fullscreen control, or claim that it proves a Direct3D device reset occurred.

Consequence: Phase 1 has a supported native windowed focus-loss configuration. The remaining matrix still needs other resolutions, UI profiles, input cases, frame caps, and a separate DXVK result.

### Phase 1 native graphics support boundary

Date: August 10, 2026

Decision: complete the Phase 1 graphics gate with native Direct3D 9 windowed mode as the supported path. Do not claim DXVK or repeated native fullscreen focus changes. Move controller and input-method validation to Phase 5, where the player controls are implemented.

Evidence: all four isolated UI profiles passed at 1920x1080. The native windowed matrix passed 1280x720 at 30 FPS, 1280x960 at 60 FPS, 2560x1440 at 90 FPS, and 3440x1440 at 120 FPS across both VSync states. The exact 50-cycle windowed focus run passed and shut down cleanly. Managed-texture uploads measured 22.70 to 51.20 microseconds. Host tests passed 6 of 6, and Win32 Release tests passed 8 of 8.

The target VNV instance contains no DXVK installation or root-management tool. The windowed focus path did not expose a device recreation, and the game has no verified safe in-process display toggle. PBVP owns no Direct3D resource, retains no COM reference between callbacks, rejects non-managed textures, and validates a changed device or surface before use.

Rejected alternatives: do not install a root `d3d9.dll` proxy, claim untested DXVK behavior, count the native fullscreen driver crash as a pass, retry the retired synthetic recreation path, or keep controller testing in a phase that has no player controls.

Consequence: Phase 2 may start after the Phase 1 documentation, artifacts, GitHub issue, and pull request are finalized. DXVK can be added only after a separate isolated installation passes its own matrix. Phase 5 retains controller and input-method acceptance.

### Isolated profile save guard

Date: August 10, 2026

Decision: enable a separate local Stewie Tweaks multi-INI override in every PBVP Phase 1 profile. Disable improved autosaves, save-on-exit, and the autosave timer there. Do not edit the shared VNV INI, disable MO2 local saves, or include the guard in a PBVP package.

Evidence: the first windowed test created a new `.fos` file and `.nvse` co-save at shutdown because the source Extended INI enables save-on-exit. The isolated profile had no save directory before the run, so both files were test output. They were moved together to an ignored quarantine and remain recoverable. The installed Stewie Tweaks 9.80 readme and active INI document and enable multi-INI overrides.

Reason: a profile-only override stops the side effect at its source while preserving the rest of the Extended configuration. Keeping MO2 local saves enabled ensures that an unexpected save still stays inside the isolated profile instead of reaching a normal save directory.

Rejected alternatives: do not edit the shared Extended INI, copy and replace its complete settings file, turn off local saves, delete generated files without a recoverable copy, or package test-only save behavior with the plugin.

Consequence: profile creation verifies the override file, the active Stewie Tweaks multi-INI setting, and one enabled guard entry per test profile. The display-case helper refuses to run without the guard. A later in-game exit must confirm that no new test save is produced.

Follow-up evidence: the first Base test created no save data, but it did not verify the guard. Mod Organizer had remained open while the local guard mod was added. When the user switched to the Base profile, MO2 wrote that new mod as disabled. External profile edits made while MO2 is running can therefore be lost or replaced by its in-memory state.

Updated consequence: both profile setup and display-case mutation now refuse to run while the selected instance's Mod Organizer process is active. Close MO2, enable or repair the guard, reopen MO2, and verify its checked state before the next in-game exit.

Verification: after the repair, the Base profile retained one enabled guard entry and no disabled duplicate. A normal in-game exit left its local `saves` directory empty. That result verifies only the Base preset, which already disables improved autosaves in its main INI.

Extended result: the full Extended regression retained one enabled guard entry and all three guard values at zero, but normal exit still created a named `.fos` save and its `.nvse` co-save. Both files were new output in the isolated profile. They were moved together to the ignored quarantine and remain recoverable. No normal profile or existing save was touched.

Updated decision: keep the profile-only multi-INI design, but map each override to the section used by Stewie Tweaks. `bImprovedAutoSave` belongs under `[Tweaks]`. `bSaveOnExitGame` and `iAutoSaveTimer` belong under `[Save Manager]`. The current one-section file is not accepted for another Extended run.

Reason: Stewie Tweaks resolves settings by section and key. The installed 9.80 readme documents multi-INI replacement, and the active Extended INI places the two Save Manager options in their own section. The earlier guard put every key under `[Tweaks]`, so it did not replace those two Extended values.

Consequence: profile verification must require both sections and reject a save-related key in the wrong section. The corrected guard needs a fresh full Extended exit with an empty local save directory before isolated profile safety is accepted.

Runtime verification: the corrected two-section guard remained enabled exactly once in the full Extended profile. After a normal test-world session and menu exit, the isolated save directory was empty. DiaMove recorded the exit-game message without a following save-game message. The strict PBVP log check also passed with two successful uploads, no upload failure, and a clean shutdown. This accepts the guard for the remaining isolated Phase 1 sessions. Release-candidate save, uninstall, and reinstallation tests remain separate Phase 6 requirements.

### Separate public and private symbols

Date: August 9, 2026

Decision: keep the full PDB as a private local build artifact. Generate a second stripped PDB for the symbols archive, omit every PDB from the runtime archive, and make the DLL refer only to `PipBoyVideoPlayer.pdb` without a directory. Scan the DLL and distributed symbols for build-local and user-profile paths before accepting a package.

Evidence: a binary-content scan found the full local build path to `PipBoyVideoPlayer.pdb` in the normal DLL. The full PDB also contains repository, build, and Windows user-profile temporary paths. This disproves the earlier clean-package result, which checked archive entries and ordinary text but did not inspect binary contents. Microsoft documents `/PDBALTPATH` for writing a path-independent PDB reference into the image and `/PDBSTRIPPED` for creating a second PDB with public symbols and stack-walking records.

A current linker test showed that `/PDBSTRIPPED` retains 493 absolute `Module` and `ObjFile` fields. An LLVM 22.1.0 YAML round trip removed those paths and preserved all 4,114 public name, flag, and address tuples, but a deeper check found that it dropped the FPO and section-contribution streams. That result rejects PDB reconstruction as a release method. A Visual Studio generator test also showed that passing `%_PDB%` through CMake produced the literal name `%%%PipBoyVideoPlayer.pdb%%%`. The linker option therefore uses the explicit stable filename instead of the percent placeholder.

A second test replaced the absolute names inside the DBI logical stream with equal-length path-neutral names and wrote the stream back to the same MSF blocks. It also cleared six unreferenced blocks and the unused tails of referenced blocks, where stale paths remained outside the live stream lengths. The output kept the original byte size, GUID, age, stripped status, public-symbol dump, FPO dump, and section-contribution dump. It contained no drive-qualified path and loaded through LLVM's DIA-compatible reader.

Rejected alternatives: do not ship the full PDB in either archive, publish the raw stripped PDB, rebuild the PDB through YAML, resize its DBI records, leave an absolute CodeView path in the DLL, or call an archive clean after checking filenames alone. Do not discard symbols entirely because crash diagnosis still needs matching public symbols and frame records.

Consequence: the earlier Phase 1 archives are not clean-package candidates. Packaging must apply the equal-length DBI cleanup, use the pinned LLVM tool to compare identity and diagnostic streams with the linker output, rename the result to the DLL's expected PDB filename, and reject absolute drive paths and known local path prefixes in packaged binaries.

### Private development licensing

Date: August 9, 2026

Decision: original work remains all rights reserved during private development. The repository and release candidate remain private. No public binary or public source release will be made until the owner selects a project license and the exact FFmpeg build receives a distribution review.

Reason: this permits the requested private implementation and testing without making a license choice on the owner's behalf.

Consequence: dependency notices and build records will be complete in the private candidate, but publication remains an explicit approval gate.

### Audio as master clock

Decision: use XAudio2's played-sample cursor as the master clock when audio is present. Use QueryPerformanceCounter for silent files.

Reason: synchronizing video to consumed audio prevents a slow game frame rate from causing cumulative drift.

### Bounded integrated playback and presentation

Date: August 10, 2026

Decision: keep FFmpeg contexts on one decoder worker, XAudio2 and playback state on the game-thread owner, and Direct3D access on the frame-present thread. Move only one owned BGRA frame through a bounded renderer mailbox. Select the newest frame eligible for the audio clock and drop older eligible frames. Use one frame of presentation lead without advancing the media clock.

Evidence: the automated scheduler completed a simulated 30-minute 30 FPS stream at a 10 FPS game cadence without cumulative drift. Native tests cover pause, resume, forward and backward seeks, stop during buffering, stale generation rejection, silent-video timing, hidden presentation, foreign-thread refusal, and orderly shutdown. The first live integrated run decoded 20 frames, presented 18, dropped two at startup, played all 96,967 audio samples with zero underruns, and uploaded every submitted frame. The maximum measured upload was 59.40 microseconds.

The first 720p long-test attempt exposed an accounting defect rather than an ownership change. The controller subtracted a frame's BGRA size after moving it to the presentation slot, so the moved-from vector contributed zero and the bounded staging counter only increased. The corrected code records the size before moving the frame. A complete 1080p regression and a five-second run against the 30-minute fixture passed with the failure site clear.

Rejected alternatives: do not use game frames as the media clock, retain a full decoded video queue on the renderer, call Direct3D while holding the mailbox lock, let a stale seek generation reach the texture, or retain engine Direct3D references between callbacks.

Consequence: low game frame rates reduce video smoothness through bounded frame dropping instead of slowing audio or growing queues. The live 30-minute drift target and the full low-FPS in-game matrix remain release tests.

### Measure the long-file allocation failure before changing layout

Date: August 10, 2026

Decision: retain the 1920 by 1080 source limit and current queue sizes until a measured failure identifies the allocation boundary. Do not claim that the file is malformed, lower the supported source resolution, fragment the MP4, or change decoder output dimensions based on the generic allocation result.

Evidence: the second live attempt used the corrected staging-accounting build and failed before the first presentation. The decoder reported `allocation_failed`, and XAudio2 was still uninitialized. The standalone x86 decoder completed the identical file after the accounting fix. The old record cannot distinguish process address-space fragmentation from allocation inside long-file indexing or the first decoded frame.

Rejected alternatives: do not reduce source limits, queue sizes, decoder threads, or test duration as speculative fixes. Do not treat a C++ allocation exception as a codec or container rejection.

Consequence: one short diagnostic run must record the new allocation site and address-space snapshot. If the failure is the full-resolution BGRA buffer and contiguous space is constrained, revise the decoder to scale into a bounded presentation intermediate while keeping the 720p and 1080p input checks. If container opening fails, investigate MP4 indexing without changing the frame pipeline.

### Bound decoded output before queue insertion

Date: August 10, 2026

Decision: keep validating source dimensions against the 1920 by 1080 input cap, but scale decoded BGRA output to fit within a 512 by 512 intermediate before rotation and queue insertion. Do not upscale smaller sources. Preserve original source and display dimensions in media metadata. This entry supersedes the previous instruction to retain source-sized decoded frames.

Reason: the engine-owned presentation texture is 256 by 256. The full-resolution BGRA queue consumes memory that the renderer discards when it prepares that texture. A 16:9 source becomes 512 by 288, or 589,824 bytes per queued frame. Three frames use 1,769,472 bytes instead of 24,883,200 bytes for three 1080p frames. Rotation keeps the same pixel count and swaps the scaled dimensions.

Evidence: the allocation diagnostic identified `video_pixel_buffer` on the third long-playback attempt. The 1280 by 720 fixture opened and reached buffering, but its first 3,686,400-byte BGRA vector allocation failed. Process private memory was 1,590,300,672 bytes, the largest free region was 1,536,557,056 bytes, and total free virtual memory was 1,609,023,488 bytes. The source-sized C++ frame allocation is unreliable in the loaded VNV process despite ample contiguous address space and successful standalone decoding.

Rejected alternatives: do not lower the accepted source resolution, shorten the test file, fragment the MP4, or place an unbounded allocation in another thread. Do not decode directly to 256 by 256 because the current renderer still needs room for later aspect-mode and tint processing.

Consequence: update the decoder, high-resolution memory test, performance measurements, and documentation. The 30-minute in-game test must restart from the beginning after a short live run proves that the first frame and audio stream both start.

Implementation evidence: the x86 decoder now produces 512 by 288 frames for a 1080p source. Three queued frames occupy 1,769,472 bytes. The complete 1080p regression passed, and the real long fixture stayed in playback for a five-second native run with no error or failure site. The in-game startup check remains open.

### Allocate video payloads outside the game heap

Date: August 10, 2026

Decision: allocate video pixel vectors with a Windows `VirtualAlloc` allocator and release them with `VirtualFree`. Keep the existing vector interface, move-only frame handoff, frame counts, and byte limits. Audio sample vectors remain on their current allocator because the live audio path did not fail.

Reason: video payloads are large, short-lived blocks that cross the decoder, game-thread scheduler, and render mailbox. A virtual-memory allocator gives each payload its own committed region and does not depend on the game process's C++ heap state. The allocator still throws a contained allocation error if Windows refuses the request.

Evidence: the first bounded-output live run played for about four seconds, then a 589,824-byte video pixel allocation failed. Process private memory was 1,542,602,752 bytes, the largest free region was 1,547,436,032 bytes, and total free virtual memory was 1,606,086,656 bytes. Reducing the payload by 84 percent did not make repeated C++ heap allocation reliable.

Rejected alternatives: do not reduce the source or intermediate resolution again, increase queue limits, hide the error, or move allocation to the render thread. Do not use FFmpeg's allocator because queued frames can outlive decoder contexts and must not depend on FFmpeg library lifetime.

Consequence: add a typed allocator with checked byte arithmetic, use it only for `DecodedVideoFrame` pixel storage, test repeated allocation and move ownership, and repeat the short live startup test before restarting the 30-minute run.

Implementation evidence: the allocator stress test completed 512 allocation, move, and release cycles for 589,824-byte payloads and retained less than 8 MiB after the loop. The 1080p queue and complete decode passed, and the real long fixture completed a five-second native startup run with no error or failure site. The live build then displayed continuous video for about 56 seconds with 1,659 successful uploads and no playback or allocation error. This passes the short live startup check. The complete 30-minute run remains open.

### Apply the synchronization tolerance on both sides of the media end

Date: August 10, 2026

Decision: the strict long-playback checker accepts the final audio clock within 50 milliseconds on either side of the exact 1,800-second duration. It also retains the separate requirement that the absolute audio-to-video difference is at most 50 milliseconds.

Reason: the acceptance target measures absolute synchronization error. Requiring the audio clock to reach or exceed the video end adds a one-sided condition that the specification does not require.

Evidence: the first complete live run ended with a 1,799,971,875-microsecond audio clock and a 1,800,000,000-microsecond video end. Their 28,125-microsecond difference satisfies the target. The run decoded all 54,000 frames, submitted 86,400,000 audio samples, and reported zero underruns. The old checker rejected only the audio clock's lower bound.

Rejected alternatives: do not round the runtime clock, change the playback metrics, treat a valid early offset as zero, or widen the 50-millisecond target.

Consequence: align the checker's lower and upper duration bounds with the 50-millisecond target, add boundary regression cases, and rerun it against the untouched live log before accepting the result.

Implementation evidence: the checker now accepts the measured 28.125-millisecond early offset. Regression cases reject clocks one microsecond outside both 50-millisecond duration boundaries and reject a 50,001-microsecond synchronization error. The untouched live log passed every synchronization, memory, upload, privacy, and teardown check.

### Drain bounded video while rebuilding the audio buffer

Date: August 10, 2026

Decision: use the existing bounded video-discard path whenever playback is buffering, including recovery after an audio underrun. Keep the oldest staged frame until the audio buffer is ready, then let audio-led selection discard stale frames against the current sample clock.

Reason: the decoder worker reads interleaved video and audio from one media timeline. A full video staging path can block that worker before it produces the 200 milliseconds of audio required to leave buffering. Initial buffering already avoids this dependency by discarding excess video. Rebuffering must do the same even though the XAudio2 voice has started.

Evidence: the first live 10 FPS run reached playing, returned to buffering after 499 milliseconds, and remained there until normal shutdown. The render callback stayed between 10.00 and 10.06 FPS, but only four video frames reached the texture. The controller's discard predicate excludes the rebuffer case when `audio_started_` is true.

Rejected alternatives: do not grow either queue, decode on the render thread, submit XAudio2 buffers from its callback, move game objects to a worker, lower the supported media frame rate, or replace the consumed-sample clock with game-frame timing.

Consequence: change only the buffering discard predicate, add an integrated forced-underrun recovery regression, and repeat the five-minute 10 FPS live case. Preserve all existing queue limits and thread ownership.

### System XAudio2 2.9 runtime

Date: August 10, 2026

Decision: use the Windows 10 and Windows 11 system XAudio2 2.9 runtime through the x86 Windows SDK import library. Create a separate PBVP engine and voices on the default output device. Do not package an XAudio2 DLL.

Evidence: the target system has Windows SDK 10.0.26100.0 with the x86 `xaudio2.lib`, and its 32-bit system directory provides `XAudio2_9.dll`. The current SDK desktop entry point restricts loading to the system directory. Microsoft's version documentation makes XAudio2 2.9 the current system component for the project's existing Windows 10 and Windows 11 support range. Its voice state exposes `SamplesPlayed`, and the default-device path supports ordinary device switching through the Windows audio stack.

Rejected alternatives: do not add the legacy DirectX SDK solely for XAudio2 2.7, ship an XAudio2 runtime beside the plugin, load an arbitrary DLL path, or alter Fallout's audio engine objects. Dynamic selection between 2.7, 2.8, and 2.9 would add untested behavior outside the supported operating systems.

Consequence: the runtime archive gains no audio DLL. The build and package checks must use the x86 system import and reject a bundled `XAudio2*.dll`. Phase 3 must still test engine creation, default-device failure, pause, seek, end of stream, and sample-origin accounting on the target machine.

### XAudio2 callback and buffer lifetime

Date: August 10, 2026

Decision: preallocate a fixed PCM buffer pool. Voice callbacks may only update atomics that identify completed buffers, stream end, or an audio error. They may not allocate, log, block, acquire a PBVP mutex, decode, or touch game objects.

Evidence: Microsoft requires submitted audio memory to remain valid until `OnBufferEnd` or voice destruction. `Stop` is asynchronous and preserves queued audio. `FlushSourceBuffers` removes pending buffers but can return their callbacks out of order. `DestroyVoice` waits for audio processing to become idle and guarantees that callbacks and buffer reads have ended before it returns.

Rejected alternatives: do not allocate or free from a callback, perform decoder work there, reuse a flushed slot before its completion is observed, hold a player lock while draining XAudio2, or release a callback target before destroying its source voice.

Consequence: each submitted slot remains stable until its own completion. Pause uses `Stop` without a flush. Seek and stop halt and flush the source voice, then drain completions outside PBVP locks. Shutdown destroys the source voice before releasing the callback target and pool. The audio owner must create a new sample origin after a flushed seek because the source voice counter no longer describes the old media position.

### COM ownership for XAudio2

Date: August 10, 2026

Decision: initialize COM on the audio owner's thread before creating XAudio2. If `CoInitializeEx` returns `S_OK` or `S_FALSE`, balance it with `CoUninitialize` on that same thread after all XAudio2 objects are gone. If it reports an existing apartment model, use that initialized apartment without changing it or calling `CoUninitialize` for someone else's ownership.

Evidence: the first x86 stream test created the XAudio2 engine but failed every default mastering-voice attempt while the Windows audio services and endpoints were available. Microsoft requires COM initialization before XAudio2 setup. After the backend added tracked COM ownership, every initialization case and device-recovery case opened its mastering voice, and the complete stream test passed.

Rejected alternatives: do not assume Fallout or a test process initialized COM, change an existing thread apartment, balance another component's COM initialization, or uninitialize COM before XAudio2 releases its engine and voices.

Consequence: initialization and shutdown must occur on the audio owner. A COM setup failure becomes a structured engine-creation error. Phase 4 must keep this ownership when it connects the player state machine.

### Bounded XAudio2 stream and initial prebuffer

Date: August 10, 2026

Decision: use 16 fixed 16 KiB PCM slots and a 200 ms startup prebuffer for the current reference system. Accept only mono or stereo signed 16-bit PCM from 8 to 192 kHz at the XAudio2 boundary. The media decoder continues to supply stereo 48 kHz PCM during normal playback.

Evidence: the native x86 test passed 100, 200, and 300 ms prebuffers without an underrun. The decoder-to-audio test passed 44.1 kHz stereo, 48 kHz mono, and 48 kHz 5.1 sources after conversion. The live MO2 diagnostic used the 200 ms default, played all 96,967 output samples, reached the expected 2,020,125 microsecond clock, and reported zero underruns. Its pool remained fixed at 262,144 bytes.

Rejected alternatives: do not allocate PCM inside callbacks, grow the queue when the game is slow, begin before the chosen threshold unless the entire short stream is already queued, or expose multichannel PCM to XAudio2 when the media worker already performs the tested stereo downmix.

Consequence: Phase 4 may tune the threshold only if integrated measurements show a reason. Any larger configurable pool must remain within the 2 MiB hard cap and must pass the 32-bit memory budget again.

### Bounded software decode

Date: August 10, 2026

Decision: start with software decoding, a 1080p source cap, and small bounded queues.

Reason: hardware decoding adds device and format paths before the basic renderer is known to be stable. Bounded queues protect the game's limited address space.

Evidence: the standalone x86 process filled all three 1920x1080 BGRA queue slots and measured a 65,880,064 byte private-memory increase. The video queue held exactly 24,883,200 bytes, the audio queue held 28,672 bytes, and the total stayed below the 128 MiB target. The first in-game measurement decoded the same 30 frames and 48,128 audio samples through MO2, but process private memory rose by 167,104,512 bytes from a baseline taken at `DeferredInit`. Normal game-thread initialization began 53 milliseconds after that baseline, so the process-wide result could not be assigned to the decoder alone.

The corrected diagnostic waited five seconds, measured a one-second no-decode control, and then reset its baseline immediately before starting the worker. The control delta was zero. The 1080p decode then added 62,976,000 private bytes, produced all 30 frames and 48,128 samples, and joined before FFmpeg unloaded. Its video and audio queues peaked at 16,588,800 and 24,576 bytes because the game thread drained them while decoding.

Consequence: the measured source, queue, and process totals fit the current 128 MiB budget. Keep the 1080p cap and current defaults for Phase 3. Reopen this decision if longer playback or integrated audio adds enough memory to cross the budget.

### Private FFmpeg directory

Decision: place FFmpeg DLLs in a mod-private directory and load the pinned set through restricted absolute paths.

Reason: shared plugin or game-root DLLs can collide with unrelated mods and allow accidental version substitution.

### Minimal FFmpeg 8.1.2 runtime

Date: August 10, 2026

Decision: use FFmpeg 8.1.2 built as five shared i686 libraries with MSYS2 GCC 15.2.0. Enable only MOV demuxing, H.264 and AAC parsing and decoding, software scaling, audio resampling, and Windows threads. Link the required winpthreads clock functions statically into `avutil`.

Evidence: the official archive and detached signature passed the pinned hash and release-key checks. The first five-library build required `libwinpthread-1.dll`. Static linkage removed that runtime dependency. Two clean builds with fixed source time and neutral file-prefix mapping produced the same hashes. The automated audit verified every PE machine type, import, hash, and private-path check. Host tests passed 7 of 7, and Win32 Release tests passed 10 of 10.

Rejected alternatives: do not use a general FFmpeg binary bundle, enable unrelated codecs, ship command-line tools, depend on a shared MinGW runtime DLL, link FFmpeg statically into the plugin, or retain absolute source paths in diagnostic strings.

Consequence: the plugin must load this exact private DLL set through restricted absolute paths and verify the expected library majors before decoding. Any FFmpeg or toolchain update requires new source verification, two clean builds, updated hashes, an import review, an upstream warning review, and a license review.

### Custom overlapped Windows AVIO

Date: August 10, 2026

Decision: open media through a custom `AVIOContext` backed by a Unicode Win32 file handle. Accept only one direct-child filename under a drive-qualified media root. Reject directories, reparse points, empty files, network roots, and files beyond the configured limit. Use overlapped reads with a cancellation event and keep all seek positions in signed 64-bit range.

Evidence: the Win32 test passed reads, seeking, size queries, Unicode names, cancellation, and every implemented refusal. The bridge allocates one bounded FFmpeg buffer and frees the current context buffer before releasing the context, as required by FFmpeg's AVIO contract.

Rejected alternatives: do not use FFmpeg's default file protocol, convert paths to the game's narrow character set, accept arbitrary absolute media paths, allow recursive paths in the first release, map an entire media file into 32-bit address space, or leave shutdown waiting on an uncancellable synchronous read.

Consequence: the decode worker owns the AVIO context and closes it before FFmpeg unloads. The game thread may request cancellation, but it must join the worker before releasing callback targets. The isolated live test proved that the same `CreateFileW` path sees a media file supplied only by MO2.

### Bounded media payloads and seek generations

Date: August 10, 2026

Decision: check every video and audio payload calculation before allocation. Reject sources above 1920x1080. Bound each queue by both item count and total payload bytes. Every queued item carries a seek generation, and advancing the generation clears queued data, wakes waiters, and rejects work from an older generation.

Evidence: portable tests cover checked addition, multiplication, alignment, and 64-bit size conversion. They also cover BGRA and PCM layout limits, count and byte capacity, stale generation rejection, queue drain after close, and blocked producer wakeup after a generation change or shutdown. The completed Phase 2 suite passes all 9 host tests and all 15 Win32 Release tests. The x86 run uses the same 32-bit `size_t` as the game process.

Rejected alternatives: do not rely on frame count alone, allow allocation arithmetic to wrap, keep pre-seek buffers until the consumer notices them, or let shutdown wait for queue capacity.

Consequence: the decoder may block only through the queue API and may hold no other PBVP lock while waiting. Queue capacities remain a profiling decision, but they must fit the release memory budget and cannot be changed by untrusted media metadata.

### Single-owner decoder worker

Date: August 10, 2026

Decision: one worker owns the custom AVIO context, demuxer, codec contexts, packet, frames, scaler, and resampler. It accepts H.264 video with optional AAC audio from MP4 or MOV. Output consists of owned BGRA frames and interleaved signed 16-bit PCM chunks tagged with the active seek generation. Right-angle display matrices are applied during BGRA conversion. Other rotation angles are rejected.

Evidence: synthetic x86 tests decode H.264 with AAC at 44.1 and 48 kHz, resample mono, stereo, and 5.1 layouts to stereo 48 kHz, preserve both 100 ms and 200 ms frame intervals in a variable frame rate file, and turn a display matrix into a 90x160 output frame. Forward and backward seeks return only the requested generation. Cancellation wakes a worker blocked by a full queue. Unsupported codecs, encrypted samples, random and empty input, source limits, missing files, and a half-truncated MP4 produce structured failures. All 15 Win32 Release tests pass.

The truncated fixture exposed a demuxer edge case. FFmpeg dropped the final partial packet and returned ordinary end of file. PBVP now compares the last decoded video timestamp with the declared track end, using a 250 ms tolerance, and reports damaged media when too much of the track is missing.

Rejected alternatives: do not share FFmpeg contexts across threads, expose FFmpeg-owned frame buffers to the renderer, retain output from an old seek generation, or treat every demuxer end-of-file result as proof of a complete track.

Consequence: the owner must join this worker before unloading FFmpeg. The live diagnostic follows that order, and packaging rejects any DLL that retains the diagnostic marker.

### Playback status uses the pinned xNVSE tile setter

Date: August 10, 2026

Decision: update the injected `PBVP_LayerProbe` string from the game thread with the `Tile::SetStringValue` member documented by the pinned xNVSE 6.4.5 source. The bridge first resolves the named tile and verifies its owned string trait. It caches the tile identity, state, and error so it does not repeat the same write every frame.

Reason: Phase 4 needs visible opening, buffering, paused, and failure states. Replacing menu XML is out of scope, and workers cannot touch game objects. The existing injected status strip is already at the reviewed depth and position. Fixed messages also keep local paths and media metadata out of the UI.

### Low-FPS recovery remains open

Date: August 10, 2026

Decision: keep Phase 4 open after the second 10 FPS run. Add controller update count, submitted audio, played samples, queued XAudio2 buffers, and decoder queue depth to the next failure record before changing queue sizes or callback ownership.

Reason: rebuffering returned to playback three times, but the next underrun reached the configured limit and produced `audio_stream_failed`. The visible callback rate was 9.97 FPS. State changes alone cannot identify whether the empty audio path came from controller service cadence, the interleaved decoder, or queue capacity.

Rejected alternatives: do not hide the failure by raising the underrun limit. Do not enlarge queues or move playback between xNVSE callbacks without the missing counters.

Consequence: the 10 FPS row remains failed. The next in-game run is a short diagnostic, not an acceptance retest.

### Pause XAudio2 during underrun recovery

Date: August 10, 2026

Decision: pause the started source voice when a detected underrun enters rebuffering. Continue bounded decode and submission while the voice is paused, then resume only after the existing 200 millisecond prebuffer is ready. If the user requests pause during recovery, keep the voice paused when buffering completes. Keep all queue limits and callback ownership unchanged.

Reason: `BeginRebuffer` changes the playback state but does not stop the source voice. Newly submitted samples are consumed while the controller is trying to rebuild the prebuffer.

Evidence: the short diagnostic recorded 32 controller updates, an audio clock of 2,816,000 microseconds, 86 decoded video frames, 140,288 submitted samples, and 135,168 played samples before the fourth underrun. XAudio2 still had five buffers queued after the final refill. The decoder retained two video items and two audio items, so neither decoder queue was full. The preserved log has SHA-256 `7E8721DF3B317093411CBEDAA780FA3C9ACAE4E1CFD5A9E07C0BABBBC409B2D5`.

Rejected alternatives: do not raise the underrun limit, grow either queue, move playback between xNVSE callbacks, or replace the consumed-sample clock.

Consequence: add an integrated recovery regression that verifies the source voice is paused during rebuffering and resumed after the fixed threshold. Repeat the five-minute 10 FPS live case after the native suites pass.

Implementation evidence: the native controller regression forces rebuffering after the XAudio2 voice has started. It verifies the voice pauses in `buffering`, resumes when automatic recovery returns to `playing`, and stays paused when the user requests pause during recovery. All 15 host tests, all 24 normal x86 tests, and all 24 armed x86 tests pass with the existing prebuffer and queue limits.

### Free due video before the audio feed

Date: August 10, 2026

Decision: for `playing` and `paused`, perform audio-led video selection before draining new video and feeding audio. Initial buffering and underrun recovery still build the fixed audio prebuffer before their first selection. Keep the consumed-sample clock, queue limits, and thread ownership unchanged.

Reason: the playing update currently feeds audio before it removes clock-eligible video. At 10 FPS, a full video queue can hold the interleaved decoder until selection runs. Audio produced after that point cannot be submitted until the next game-loop update.

Evidence: the paused recovery build returned to `playing` in one 100 millisecond update, but it continued to underrun every 400 to 501 milliseconds and failed on the fourth event. The failure record contains 24 controller updates, a 1,749,333 microsecond audio clock, 54 decoded frames, 89,088 submitted samples, 83,968 played samples, five queued XAudio2 buffers, and two items in each decoder queue. Visible cadence stayed between 10.00 and 10.07 FPS. The preserved log has SHA-256 `A6095AFDA5BB6D1DEF7FFC7C80F4D8579A42467F66DA529FF8642E3E9DADADD9`.

Rejected alternatives: do not increase the audio prebuffer, grow a queue, raise the underrun limit, or change callback ownership before testing the existing service-order dependency.

Consequence: preserve paused recovery, reorder the playing and paused update path, rerun the native suites, and stop the next live run at the first underrun.

Implementation evidence: `PlaybackController::Update` now runs audio-led selection before `DrainVideo` and `FeedAudio` when the state is `playing` or `paused`. Selection still runs after `StartBufferedPlayback` when the controller enters playback from initial buffering or underrun recovery. All 15 host tests, all 24 normal x86 tests, and all 24 armed x86 tests pass. The live 10 FPS check remains open.

Live result: the candidate still failed at a steady 10 FPS without Alt+Tab. The first underrun occurred after 2.6 seconds, and the fourth produced `audio_stream_failed` after 4.1 seconds. The failure record contains 44 updates, 113 decoded frames, 183,296 submitted samples, 178,176 played samples, five queued XAudio2 buffers, and two items in each recorded decoder queue. The preserved log has SHA-256 `4546A674D91B3CFD7F1E2F12F64D2CA861A90BD59DCC5F4FBDA5F949A919F813`. This rejects service order as a complete correction.

### Reclaim completed audio slots before readiness

Date: August 10, 2026

Decision: `XAudioStream::ReadSnapshot` must reclaim completed slots before it calculates `ready_to_start`. Add a native ordering regression, then run the real long fixture at a 100 millisecond controller cadence before changing the initial prebuffer.

Reason: the current snapshot calls `ReadyToStart` before `ReclaimCompleted`. Finished samples can therefore satisfy the 200 millisecond threshold even though the same snapshot reports only the live buffers that remain after reclamation.

Rejected alternatives: do not raise the prebuffer or queue limits until readiness uses the current live slot count and the controlled 100 millisecond test measures the first underrun separately.

Consequence: recovery must never resume from stale sample accounting. The cause of the first live underrun remains open.

Implementation evidence: the readiness regression drains a 200 millisecond non-final stream, pauses after every submitted buffer completes, and requires zero live buffers, zero queued bytes, and `ready_to_start` false. The corrected order passes.

### Allow six bounded video items for packet bursts

Date: August 10, 2026

Decision: raise the default decoder video item limit from three to six. Keep the 32 MiB video byte cap, 200 millisecond audio prebuffer, 16-slot XAudio2 pool, consumed-sample clock, and current thread ownership.

Reason: the native 100 millisecond cadence test reproduced one underrun with three video items. A 300 millisecond audio prebuffer did not correct the supply rate. Six video items passed with both audio thresholds and then completed 30 seconds with zero underruns. The extra item capacity absorbs short MP4 packet bursts so the interleaved FFmpeg worker can reach audio packets between controller updates.

Evidence: the 30-second run decoded 897 frames, delivered 273, dropped 623 late frames, submitted 1,440,768 samples, reached a 29,830,000 microsecond audio clock, retained four XAudio2 buffers, and recorded zero underruns over 276 updates.

Rejected alternatives: do not increase audio latency, grow the XAudio2 pool, remove the video byte cap, or move audio ownership to another thread while the bounded item change satisfies the controlled cadence test.

Consequence: update the decoder memory regression for six scaled frames, run all native suites, and repeat the live five-minute 10 FPS gate. The controlled result is not a live pass.

Implementation evidence: the production configuration and memory regression use six video items under the unchanged 32 MiB cap. The XAudio2 readiness regression, all 15 host tests, all 24 normal x86 tests, and all 24 armed x86 tests pass. The final 30-second real-fixture cadence run stayed in `playing` with zero underruns, reached a 29,800,000 microsecond audio clock, and retained five XAudio2 buffers over 275 updates. The live gate remains open.

### No save persistence

Decision: store no media or playback state in game saves or xNVSE co-saves for the first release.

Reason: the player should be safe to add or remove and should not leave missing state in a playthrough.

## Open questions before phase one

1. Which verified engine-owned render point places the video below UIO controls without interfering with the world or other menus?
2. What coordinate transforms are required between UI tile space and the backbuffer for each aspect ratio?
3. Does DXVK preserve the selected D3D9 behavior and Reset path?
4. Which Pip-Boy replacers retain the same menu coordinate contract?
5. Can the UI provide a clean Data entry without editing scripts or requiring an ESP?

## Remaining open questions before phase two

1. Does custom Win32 I/O see all media supplied by MO2's virtual filesystem?
2. Which malformed-media corpus can be used without redistribution problems?
3. Should the decoder scale directly to the current presentation rectangle or to a fixed intermediate size?
4. What exact queue byte limits fit the VNV address-space budget under a large mod list?

## Open questions before phase three

1. How should player volume relate to the game's master and effects volume?
2. How should audio-device removal appear to the user?

## Open questions before release

1. What project license will govern original code and documentation?
2. Which H.264 and AAC binary distribution obligations apply to the chosen release method?
3. Which graphics injectors can be supported honestly?
4. Should PDB files ship in the main archive or a separate symbols archive?
5. What is the final project name, mod-page name, and configuration prefix?
6. What minimum xNVSE and UIO versions follow from the implementation rather than the planning environment?

## Decision process

A closed question should become a dated entry with the evidence, chosen option, rejected alternatives, and consequences. If testing later overturns it, add a new entry that supersedes the old one instead of silently rewriting the history.
