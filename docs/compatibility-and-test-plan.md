# Compatibility and test plan

## Reference VNV installation

The local planning baseline was inspected on August 9, 2026. The active VNV Extended profile contains:

| Component | Installed version |
| --- | ---: |
| xNVSE | 6.4.5 |
| JIP LN NVSE | 57.30 |
| JohnnyGuitar NVSE | 5.20 |
| ShowOff xNVSE | 1.82 |
| NVTF | 10.61 |
| UIO | 2.30 |
| Vanilla UI Plus | 9.48 |
| Clean Vanilla HUD | 1.01 |
| Pip-Boy UI Tweaks | 5.2.1 |

These versions are a reproducible local snapshot, not permanent minimum requirements. Before each release, compare the current VNV guide and Wabbajack profile, then record the versions used for the release candidate.

## Phase 1 execution results

### Main-menu hook verification

Date: August 9, 2026

Profile: Viva New Vegas Extended, native Direct3D 9

The Release diagnostic build loaded through MO2 and xNVSE 6.4.5. The decrypted `NiDX9Renderer::Recreate` entry matched the reviewed 16-byte signature exactly. MinHook installed the reset detour while Fallout Shader Loader 1.32, Depth Resolve 1.30, HD Pip-Boy, UIO 2.30, and the remaining Extended plugin stack were loaded. The game reached the main menu and exited normally. The plugin received the xNVSE exit message and logged its shutdown request.

This run verifies plugin loading, the runtime gate, the local reset signature, hook availability, and ordinary process exit. It does not verify Pip-Boy rendering, UI coordinates, render state restoration, device recreation, Alt+Tab, or DXVK.

### Visible native D3D9 draw

Date: August 9, 2026

Profile: Viva New Vegas Extended, native Direct3D 9, fullscreen 1920x1080

UI stack: UIO 2.30, Vanilla UI Plus 9.48, Clean Vanilla HUD 1.01, and Pip-Boy UI Tweaks 5.2.1

GPU: NVIDIA GeForce RTX 3060 through `nvldumd.dll`

The corrected UIO registration injected `Player.xml` at `MM_MainRect`. The native bridge found `PBVP_VideoRect`, resolved its logical bounds as 110,108 through 670,423, and found a 1706.67x960 logical canvas above `MapMenu`. The user confirmed that the generated checkerboard was visible on the Pip-Boy Data tab.

The generated 256x256 texture upload took 149.10 microseconds. A 300-frame sample of state capture, checkerboard drawing, and state restoration averaged 91.72 microseconds and reached a maximum of 647.70 microseconds.

This run verifies one visible presentation path and its first cost sample. It does not yet verify clipping, layer order against controls, state isolation, device recreation, other resolutions, other display modes, the base profile, or DXVK.

### Native D3D9 Alt+Tab persistence

Date: August 9, 2026

Profile: Viva New Vegas Extended, native Direct3D 9, fullscreen 1920x1080

After the viewport restoration correction, the user completed five Alt+Tab
cycles while checking the Pip-Boy. The checkerboard remained visible and looked
correct after every switch. The run produced no device-recreation entries in
the plugin log, so it verifies visual persistence across those five switches,
not release and recreation of default-pool resources.

An explicit display-mode or resolution transition is still required to exercise
the verified `NiDX9Renderer::Recreate` detour. The full 50-cycle acceptance test
also remains open.

### Frame-present layer order

Date: August 9, 2026

Profile: Viva New Vegas Extended, native Direct3D 9, fullscreen 1920x1080

The test prefab placed a black strip and the text `PBVP UI LAYER` inside the checkerboard rectangle. UIO processed and injected the updated prefab. The plugin resolved the rectangle and logged successful checkerboard draws. The user saw the checkerboard cover the Pip-Boy but saw neither the strip nor the text.

This run rejects `kMessage_OnFramePresent` as the final playback draw location because it runs after the visible menu UI. It does not reject the native Direct3D texture path or the UIO rectangle. Phase 1 must test a verified engine-owned render point before the menu UI is drawn.

### Pre-UI layer-order candidate

Date: August 9, 2026

Result: rejected

The candidate build found the expected relative call at `0x00870403`, decoded its target as `0x00709B40`, and installed both verified hooks. The user saw the checkerboard hovering above the Pip-Boy. The black strip and `PBVP UI LAYER` text did not appear, although the ordinary frame and controls remained visible and usable.

The plugin recorded successful draws and ten 300-frame timing samples. Average cost ranged from 47.08 to 63.36 microseconds, with sample maxima from 97.70 to 221.40 microseconds. This result rejects the call as a usable XML underlay. The next test moves the checkerboard into an engine-owned `PBVP_VideoSurface` image.

### Engine-owned UIO surface candidate

First run: filename declared, texture reference unavailable

The candidate package adds a private 256x256 BGRA DDS as `PBVP_VideoSurface`. Gamebryo owns and draws this image in the prefab layer order. The plugin follows the reviewed `TileImage` texture chain, takes a temporary Direct3D reference, verifies the device and surface description, updates the pixels, and releases the reference before returning. It no longer draws a primitive or patches the rejected normal-frame call.

The first test has four distinct outcomes:

- A green checkerboard means the native texture update succeeded.
- A dark purple surface means UIO loaded the private DDS, but the native update did not succeed. The plugin log should identify the failed check.
- The black strip and `PBVP UI LAYER` text should remain visible above either surface.
- The normal Pip-Boy frame and controls should remain visible and usable.

This run must also confirm that the plugin reports matching game and Direct3D callback thread IDs. It is only a layer-order and texture-chain test. It does not complete the reset, resolution, base-profile, or DXVK matrix.

The first run displayed `PBVP UI LAYER` near the upper-left of the Pip-Boy. The normal frame and controls stayed visible and usable. Neither the private surface nor the green checkerboard appeared. The plugin found the named `TileImage`, confirmed matching game and Direct3D callback thread IDs, and then reported `TileImage texture unavailable` across repeated openings.

The packaged DDS passed the 32-bit DirectX 9 image-info call used by this game build. The next candidate clears and restores the same private filename through `Tile::SetStringValue` after MapMenu becomes live. It will also report whether a known probe image has a non-null field at the reviewed texture offset.

Second run: filename refresh rejected

The follow-up build found the expected filename, cleared and restored it once through `Tile::SetStringValue`, and exited cleanly. The visible result did not change. The user saw `PBVP UI LAYER`, the normal frame, and usable controls, but no dark purple surface or green checkerboard. Before the refresh and at the upload boundary, `TileImage + 0x3C` was null and `+0x40` held the same non-null object.

The null direct member did not prove that the image lacked a render texture. Maintained JIP LN NVSE layouts identify `+0x40` as `TileShaderProperty` and its source texture at offset `0x60`. Psycho's decompiled `TileImage` node-building path also sends `+0x40` through the render-object setup. The next candidate removes the refresh and verifies that shader-property chain with exact vtables before attempting an upload.

Third run: texture upload verified, root depth rejected

The shader-source build resolved a 256x256 `A8R8G8B8` managed texture, verified its Direct3D device, and uploaded the checkerboard in 21.0 to 25.8 microseconds. The user saw the checkerboard for about one second while switching from Items to Data, then it disappeared. Repeated MapMenu openings produced the same valid texture profile and upload. The plugin exited cleanly.

This result verifies the engine-owned upload path and rejects the prefab root's default sibling depth. The active Vanilla UI Plus MapMenu places map content as high as depth 8, headline cards at depth 15, and the tab line at depth 22. The next candidate sets `PBVP_Root` to depth 10 so the surface clears ordinary page content without covering the existing headline and tab controls.

Fourth run: parent-only depth rejected

Setting `PBVP_Root` to depth 10 did not change the steady result. Opening the Pip-Boy directly to Data showed no PBVP layer. Switching away and back exposed the checkerboard briefly, with `PBVP UI LAYER` correctly above it, before both disappeared behind the completed page. The log recorded a valid surface and successful uploads from 21.8 to 31.1 microseconds on each return to MapMenu.

Vanilla UI Plus assigns depth to drawable children rather than relying on a container to carry it. Its map-marker shadow, for example, explicitly copies the parent image depth and subtracts `0.02`. The next candidate keeps the root at depth 10 and assigns explicit depths 10, 11, and 12 to the surface, black probe, and text probe. This is an inference from the active XML and still requires an in-game check.

Fifth run: explicit drawable depths accepted

The follow-up VNV Extended run kept the green checkerboard, black probe, and `PBVP UI LAYER` visible together. The user reported that the result looked good. The log recorded the expected managed surface and successful uploads from 22.6 to 30.7 microseconds across the initial Data display and several MapMenu returns.

This accepts explicit drawable depths 10 through 12 for the active fullscreen 1920x1080 Vanilla UI Plus stack. It does not yet prove input behavior, device recreation, other resolutions or display modes, base VNV, or DXVK. Those checks remain open in the matrix below.

Sixth run: input and repeat-cycle checks

The user confirmed that keyboard and mouse input still worked while the engine-owned checkerboard was visible. Five Alt+Tab cycles with Data open and ten Pip-Boy close and reopen cycles looked correct. A controller was not connected, so controller navigation and input-method switching were not tested.

The session logged 19 successful surface uploads and no plugin errors. No `NiDX9Renderer::Recreate` callback appeared, so the Alt+Tab result verifies visual persistence but not the forced device-recreation path. The user also requested that the temporary status strip move to the lower-left of the playback area so it does not sit over the top of radio station entries.

Seventh run: placement scope corrected

The lower-left status-strip candidate moved only the black probe and label within the unchanged video rectangle. The user expected the checkerboard to move as part of the UI and reported that it stayed in place. This rejects the narrow interpretation of the placement request. The next candidate anchors the complete `PBVP_VideoRect` to the lower-left of its parent while preserving its tested size and depths.

Eighth run: full-size lower-left anchor visually rejected

The complete rectangle anchor changed the resolved bounds from `110,108` through `670,423` to `42,276` through `602,591`. This proves that the relative XML expression moved the live tile 68 units left and 168 units down. The user still perceived the checkerboard as top-left.

The candidate kept the original 560 by 315 size, so its upper edge still occupies much of the Pip-Boy content area even when its bottom edge is anchored. The placement is not accepted. The next change requires a choice between a smaller lower-left video panel and the documented full-glass playback stage.

## Required profiles

| Profile | Purpose |
| --- | --- |
| Vanilla plus xNVSE and plugin | Finds hidden dependencies on VNV additions |
| Base VNV | Main stability and bug-fix baseline |
| VNV Extended | Required target with the normal UI and gameplay additions |
| VNV Extended without Pip-Boy UI Tweaks | Isolates the UI integration |
| VNV Extended with DXVK | Tests the D3D9 API through a translation layer |
| VNV Extended with a supported handheld Pip-Boy | Tests screen rectangle and animation assumptions |

ENB and New Vegas Reloaded begin as unsupported configurations. They may enter the matrix after the native D3D9 path is stable and a maintainer can reproduce them.

## Graphics matrix

Test native D3D9 and DXVK where VNV supports it. Each graphics path covers:

- full-screen and borderless or windowed modes used by the guide;
- 1280x720, 1920x1080, 2560x1440, 3440x1440, and one 4:3 resolution;
- game caps of 30, 60, 90, and 120 FPS;
- VSync on and off where the profile permits it;
- Alt+Tab during idle, buffering, playback, pause, and seek;
- resolution or display-mode reset where the game exposes it;
- device loss while decoded frames are queued.

Pass criteria:

- no crash or failed reset;
- no persistent black texture after recovery;
- no changed game render state outside the video rectangle;
- correct clipping and aspect ratio;
- controls render above the video;
- no audio continuing while presentation is suspended beyond the documented grace period.

## UI matrix

Test these layouts independently:

- vanilla UI;
- Vanilla UI Plus;
- Vanilla UI Plus with Clean Vanilla HUD;
- the complete VNV Extended UI stack;
- each claimed Pip-Boy replacer;
- mouse and keyboard only;
- controller only;
- switching between mouse and controller while the page is open.

Test catalog sizes of zero, one, ten, one hundred, and five hundred files. Long titles must clip or scroll without moving the video rectangle. Empty and error states must leave the ordinary Data page usable.

## Media fixture set

All fixtures must be created for testing or come from a redistribution-safe source. Keep the small deterministic fixtures in a separate test-data package if repository size becomes a problem.

Required cases:

| Category | Cases |
| --- | --- |
| H.264 profile | Baseline, Main, High |
| Frame rate | 23.976, 24, 25, 29.97, 30, 60, variable |
| Resolution | 320x240, 640x360, 720p, 1080p, portrait |
| Aspect | 4:3, 16:9, ultrawide sample, rotated phone video |
| Audio | silent, mono, stereo, 44.1 kHz, 48 kHz, multichannel downmix |
| Timeline | nonzero start, B-frames, edit list, timestamp gap, long duration |
| Metadata | no title, Unicode title, oversized title, malformed text |
| Failure | truncated file, random bytes, zero length, invalid dimensions, unsupported codec, DRM marker |

Optional codec fixtures may document observed behavior, but the UI must not call them supported until they are part of the required release matrix.

## Functional scenarios

### Normal playback

Open the Pip-Boy, enter Videos, select a file, wait for buffering, play to completion, and return to the list. Repeat after loading a save and after changing cells.

### Navigation

Pause and resume. Seek forward and backward near the start, middle, and end. Stop during buffering. Leave the Videos page during playback. Close the Pip-Boy during a seek. Open a modal menu over paused playback.

### Lifecycle

Load another save while idle and while playing. Return to the main menu. Start a new game. Exit to desktop. Confirm that audio stops, handles close, workers exit, and no callback reaches released objects.

### Fault injection

Remove or rename a selected media file before open. Deny read access. Simulate an FFmpeg open failure, decoder failure, allocation failure, audio-device loss, queue starvation, device loss, Reset failure, missing UIO traits, and incompatible FFmpeg DLL versions.

### Repetition

Automate or manually script the acceptance loops from the project scope. Capture process memory, handle count, thread count, frame time, audio underruns, video drops, and clock error.

## Performance measurements

Record at least:

- time from selection to first frame;
- decode-worker CPU time;
- render-thread upload and draw time at median and 95th percentile;
- audio queue depth and underrun count;
- video queue depth and dropped-frame count;
- audio and video clock difference;
- working set, private bytes, and virtual address space;
- handle and thread counts before and after playback;
- cost of the chosen Direct3D state preservation method.

Results need the CPU, GPU, operating system, display mode, graphics path, source media properties, game FPS cap, and VNV profile.

## Stability soak

The release candidate runs a two-hour mixed test: repeated short clips, one 30-minute clip, seeks, pauses, Pip-Boy closures, cell changes between sessions, and regular Alt+Tab cycles. Memory and handle graphs must level off after warm-up. Any crash, deadlock, stuck audio, failed reset, or continuing growth blocks release.

## Save safety

The planned plugin is ESP-less and stores no playback state in saves or xNVSE co-saves. Test a save before installation, with the mod installed, and after removal. The save should load in each case without missing-form warnings or persistent menu state.

## Bug report data

A useful report contains the plugin log, crash log if any, VNV profile and versions, graphics path, display mode, input method, and a media probe summary that omits the file itself. Users should not upload copyrighted or private videos to demonstrate a bug.
