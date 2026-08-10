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

Decision: configure Phase 1 display cases only in the save-free PBVP MO2 profiles. Use the existing FalloutNV.exe RTSS application profile for the required 30, 60, 90, and 120 FPS caps. Save the original profile INIs and RTSS profile before the first case, and restore their exact bytes after the session.

Evidence: the installed NVTF configuration exposes a maximum timing tolerance, not a general frame limiter. The maintained Fallout NV Performance Guide recommends RTSS and states that VSync is not a limiter. RTSS is already installed with a FalloutNV.exe profile, passive waiting, and front-edge sync. DXVK also supplies a built-in D3D9 cap, but its maintained configuration reference recommends an external limiter when one is available.

Rejected alternatives: do not edit the original VNV profiles, use VSync as a cap, repurpose NVTF's tolerance setting, or change the limiter between native Direct3D 9 and DXVK. Do not automate changes to a profile that contains saves.

Consequence: the test-case script accepts only the documented Phase 1 resolutions, frame caps, display modes, VSync states, and isolated profile names. It refuses cross-profile overlap and restores the original files. Manual in-game checks remain necessary for every recorded result.

### Native fullscreen focus-cycle baseline

Date: August 10, 2026

Decision: treat the 1920x1080 native Direct3D 9 fullscreen focus-cycle result as a failing VNV baseline. Do not change PBVP to address the NVIDIA driver trace unless a matched PBVP-disabled configuration stops reproducing it. Continue the Phase 1 matrix with native Direct3D 9 windowed mode and test DXVK separately.

Evidence: the first two measured runs with PBVP loaded crashed after four and nine completed focus cycles. Both reports remained inside `nvd3dum`, and the second exactly matched a March crash from before PBVP development. The PBVP-disabled control completed 50 cycles, then crashed one second later with the same six driver frames. PBVP was absent from the module list, and its log file remained byte-for-byte unchanged.

Reason: the control isolates the crash from PBVP, even though the enabled runs reached it sooner. The maintained FNV performance guide documents unstable Alt+Tab behavior in legacy fullscreen mode and recommends windowed mode or DXVK for the affected presentation paths. The active NVTF pool option differs from the guide's explicit failure case, so the precise driver trigger remains unknown.

Rejected alternatives: do not hide the control crash, count the result as a clean 50-cycle pass, patch the NVIDIA driver path, force a Direct3D reset, or weaken PBVP's texture validation. Do not claim native fullscreen Alt+Tab support from this installation.

Consequence: the accepted fullscreen evidence remains limited to ordinary presentation, short Alt+Tab checks, and the earlier uncounted stress run. A supported repeated-focus configuration still requires a clean measured windowed or DXVK result. Native fullscreen can remain a playback configuration only if its focus-loss limitation is explicit and other required lifecycle checks pass.

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
