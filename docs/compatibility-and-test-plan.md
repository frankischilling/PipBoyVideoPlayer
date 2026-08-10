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

An explicit game-initiated display-mode or resolution transition is still required to check whether the device or surface identity changes and receives fresh validation. The full 50-cycle acceptance test also remains open.

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

Eighth run: container anchor did not move its children

The complete rectangle anchor changed the resolved bounds from `110,108` through `670,423` to `42,276` through `602,591`. The relative XML expression moved the container traits 68 units left and 168 units down, but the user still saw the checkerboard at the upper-left.

The first analysis blamed the 560 by 315 footprint. The later compact test disproved that explanation because resizing changed the image size without moving its origin. The rectangle did not establish a locus for its children.

Ninth run: compact rectangle isolated the missing locus

The 320 by 180 candidate resolved at `42,411` through `362,591`, but the user saw a smaller checkerboard at the same upper-left origin. The size traits worked. The child position did not follow the container.

Tenth run: locus-corrected placement accepted

Adding `locus = 1` to `PBVP_VideoRect` made the surface, black strip, and text use the rectangle as their local origin. The user reported that the complete layer looked great in its lower-left position.

Eleventh run: modest size increase accepted

The user asked for the accepted lower-left panel to be slightly larger. The next candidate increased it to 384 by 216, which is 20 percent larger in both dimensions. The anchor, locus, texture path, and draw depths remained unchanged.

The run resolved the panel at `42,375` through `426,591` on the 1706.67 by 960 logical canvas. The managed texture upload took 27.3 microseconds. The user reported that the larger panel looked good. This accepts the size and position for the active fullscreen 1920x1080 VNV Extended profile. It does not extend that result to other resolutions or UI profiles.

Automated coordinate matrix

The host and Win32 suites convert the accepted logical rectangle through representative 4:3, 16:9, 16:10, and 3440x1440 ultrawide canvases. They cover the required 1280x720, 1920x1080, 2560x1440, and 3440x1440 backbuffers plus 1280x960 and 1920x1200. Invalid and nonfinite geometry fails with an empty output rectangle. These tests verify the pure conversion math only. Each resolution still needs an in-game visual check because the active UI mod supplies the logical canvas.

Automated recreation result contract

The earlier host and Win32 suites verified the audited `NiDX9Renderer::Recreate` return values while the observation detour was present. The managed-texture decision removes that detour because PBVP owns no reset-sensitive resource. The replacement contract requires `D3DPOOL_MANAGED`, rejects every other pool, and treats a changed device or surface as a fresh validation and upload. The target game exposes no verified safe in-process display toggle, so Phase 1 accepts the managed-resource ownership result without claiming that a natural Reset was observed.

### Retired controlled engine recreation candidate

The normal game flow has not exposed a repeatable recreation action. The current executable audit found one main-loop read and one clear of the deferred request byte. When set, the main loop calls the complete renderer and window helper. That helper requires separate nonzero requested width and height values before it reaches `NiDX9Renderer::Recreate`.

The repository temporarily had a private build option that scheduled this request once after a successful checkerboard upload and shared-thread validation. The first request lacked the transient size fields and returned early. A later guarded build staged matching 1920x1080 values and reached the native recreation call, where the game froze. The option and its installer are retired. Do not build or run this diagnostic again.

This failure does not prove that normal game-initiated recreation is unsafe. It proves that PBVP cannot safely synthesize the transition from the audited consumer inputs alone. Future lifecycle testing must observe a transition initiated by the game or use the documented managed-resource ownership result. Direct renderer calls, direct Direct3D resets, device-vtable patches, and synthetic request writes remain prohibited.

No executable hook surface

The current renderer uses xNVSE's frame-present notification and installs no executable or device-vtable hook. The earlier hook classifier, MinHook fixture, and reset detour were removed with the managed-texture decision. Runtime rejection now covers the FalloutNV version, engine object layouts, Direct3D device identity, texture dimensions, pixel format, and `D3DPOOL_MANAGED` requirement.

Twelfth run: hook-free managed texture accepted

Date: August 10, 2026

Profile: Viva New Vegas Extended, native Direct3D 9, fullscreen 1920x1080

The first run after removing the recreation hook displayed the checkerboard in the accepted position. The user reported that the game did not freeze, the panel looked correct, and it remained correct after Alt+Tab. The plugin logged the hook-free presentation path, a 256x256 `A8R8G8B8` texture in `D3DPOOL_MANAGED`, and one successful upload at 26.30 microseconds. It recorded zero upload failures and wrote both renderer and cadence summaries before normal process shutdown.

The strict Phase 1 log check passed with one validated device, one successful upload, a 1920x1080 fullscreen backbuffer, and a clean exit. This accepts the hook-free managed-texture path for the tested VNV Extended configuration. It does not prove a Direct3D device recreation occurred during Alt+Tab, and it does not extend the result to other profiles, resolutions, display modes, controllers, or DXVK.

### Repeatable diagnostic log check

Maintainers can check a Phase 1 game log from the repository root:

```powershell
.\scripts\check-phase1-log.ps1 `
  -LogPath '...\PipBoyVideoPlayer.log' `
  -ExpectedWidth 1920 `
  -ExpectedHeight 1080 `
  -ExpectedFps 60 `
  -MinimumUploads 1
```

The check requires a plugin load record, the hook-free presentation-path record, a resolved UIO rectangle, a validated Direct3D device, and the requested number of successful texture uploads. When `-ExpectedFps` is present, it also requires at least one visible-frame cadence sample. The default allowance is the greater of 2 FPS or 5 percent of the requested cap. `-FpsTolerance` can set a different allowance for a recorded case.

The plugin measures up to eight three-second windows while the Pip-Boy is visible. It resets a partial window when the menu hides, the device is unavailable, or the performance counter regresses. Each log record contains the frame count, elapsed time, and calculated FPS. The checker recalculates that FPS before comparing it with the requested cap.

The checker rejects plugin errors, inconsistent cadence records, and a backbuffer that does not match the expected dimensions. Add `-RequireCleanExit` when the test includes an orderly process exit. A clean-exit check requires the renderer summary and, when samples exist, the cadence summary.

The accepted hook-free 1920x1080 VNV Extended run passes with one validated device and one upload at 26.30 microseconds. The log contains no plugin errors and ends with the renderer summary, cadence summary, and shutdown record.

CTest runs the checker against generated logs. The fixtures cover a normal clean exit and a 60 FPS cadence. They also confirm rejection of an error record, a wrong backbuffer, a wrong frame rate, inconsistent cadence arithmetic, and a clean exit without the renderer summary. Portable texture-contract tests accept the two supported 256x256 color formats in the managed pool and reject wrong dimensions, unsupported formats, and every other pool. The generated fixtures contain no game data or personal media.

The normal installed build records fixed-size session totals for frame callbacks, visible frames, device validations, texture uploads, and cadence samples. It also reports minimum, average, and maximum successful upload time and visible FPS. The August 10 hook-free run verified these summaries during an orderly in-game exit.

### Measured Alt+Tab cycles

Run the external focus counter before launching FalloutNV:

```powershell
.\scripts\measure-phase1-focus-cycles.ps1 `
  -OutputPath .\build-host\phase1-focus-cycles.json `
  -TargetCycles 50 `
  -StopAtTarget
```

The counter waits for one FalloutNV process, then polls the foreground window's process ID. A cycle begins when FalloutNV loses the foreground and completes when it becomes the foreground process again. Samples before the game's first foreground window do not count, and an unfinished final focus loss does not count. The script writes the completed count and away-time range to JSON. It does not install a Windows event hook, inspect input, inject code, or change game files.

The user later performed what they estimated as more than 44 additional Alt+Tab cycles on the hook-free build. The checkerboard remained usable, and the session log passed with 21,933 callbacks, ten successful surface uploads, zero upload failures, and a normal shutdown. Uploads ranged from 24.60 to 47.70 microseconds. This is a clean extended stress result, but the estimate is not an exact 50-cycle measurement. The external counter must supply that final acceptance record.

The first measured run ended after four completed focus cycles because FalloutNV crashed. The counter recorded `EndReason` as `process-exit`, an incomplete final focus loss, and a missed 50-cycle target. The plugin log recorded three successful managed-texture uploads from 45.50 to 50.20 microseconds and no upload failure, but it ended without the renderer or shutdown summary. CrashLogger reported an access violation on an NVIDIA Direct3D worker thread.

The crash report contains no PBVP frame. A March 14, 2026 crash from before PBVP development used the same NVIDIA driver version and the same final five `nvd3dum` call frames. That history makes the cause uncertain. It does not convert the failed measured run into a pass. The next test must repeat the same counter and Alt+Tab procedure with only the separate PBVP development mod disabled. PBVP must then be re-enabled for a matched test. Another driver crash in the control run is baseline evidence. A control pass followed by repeated PBVP failures requires further renderer isolation.

The user requested one more PBVP-enabled attempt before the control. That retry crashed after nine completed focus cycles. PBVP had completed one managed-texture upload at 27.90 microseconds and logged no upload failure. The new CrashLogger trace exactly matches all six NVIDIA driver frames in the March pre-PBVP report, starting at `0x113C8CAA` and ending at `0x1177FCA3`. Two measured PBVP-enabled failures now make the disabled control mandatory.

The PBVP-disabled control completed all 50 measured focus cycles. It recorded 1,898 samples and away times from 0.77 to 4.49 seconds. One second after the counter reached its target, FalloutNV crashed with the same six-frame NVIDIA driver trace. PBVP was not loaded, its log timestamp and SHA-256 remained unchanged, and the crash report contains no PBVP module.

This is a failed native Direct3D 9 fullscreen baseline, not a PBVP-specific failure and not a clean 50-cycle pass. The maintained [FNV Performance Guide](https://performance.moddinglinked.com/falloutnv.html) describes legacy fullscreen Alt+Tab as slow and unstable. It also explains that the NVTF default-pool texture option makes fullscreen Alt+Tab unusable when enabled, while windowed mode and DXVK avoid that specific device-loss path. The active VNV profile leaves that option disabled, so the guide does not identify the exact cause of this crash. It supports testing native Direct3D 9 windowed mode next instead of changing PBVP around a driver crash that also occurs without the plugin.

### Native D3D9 windowed focus-cycle pass

Date: August 10, 2026

Profile: PBVP Phase 1 Extended, native Direct3D 9, windowed 1920x1080, VSync on, RTSS limit unchanged at zero

The external counter recorded all 50 completed focus cycles and stopped at the target. It collected 1,478 samples, recorded no incomplete focus loss, and measured away times from 110.97 to 5,247.61 milliseconds, with a 759.42 millisecond average. FalloutNV remained running when the counter finished.

The plugin log identified the windowed 1920x1080 backbuffer and the 256x256 managed `A8R8G8B8` UI surface. It recorded three successful uploads, no upload failures, and upload times from 25.00 to 51.20 microseconds. The run ended with renderer and cadence summaries followed by a clean shutdown. The strict Phase 1 log check passed, and CrashLogger produced no new report.

This accepts repeated focus loss and return for the tested native Direct3D 9 windowed configuration. It does not extend that result to native fullscreen, DXVK, another resolution, or another UI profile.

### Base VNV windowed placement result

Date: August 10, 2026

Profile: PBVP Phase 1 Base, native Direct3D 9, windowed 1920x1080, VSync on, RTSS limit unchanged

The first Base UI run displayed the checkerboard and otherwise looked and ran correctly. The user reported one layout problem: the 12-unit bottom inset placed the panel over the Local Map and World Map buttons. This rejects the Extended profile's accepted vertical position as a shared Base UI position. The next candidate keeps the 384 by 216 size and raises the complete locus-owned panel by 52 logical units.

MO2 created an empty local `saves` directory, but no save or co-save file appeared. Later inspection found that the open MO2 process had written the new save-guard mod as disabled when the user switched profiles. This run is not evidence that the guard worked, though it confirms that no save data was created. The guard still needs an in-game exit check after it is enabled with MO2 closed. Test tools now allow an empty MO2-created directory and continue to refuse any profile that contains save data.

Follow-up run: the raised candidate increased the bottom inset from 12 to 64 logical units without changing the panel size or horizontal anchor. The user reported that the new position looked good in the Base UI. This accepts the raised position for the tested Base VNV configuration.

The repaired save guard remained enabled for the follow-up run. After a normal game exit, the Base profile's local `saves` directory contained no files or subdirectories. This is the first in-game confirmation that the isolated multi-INI guard prevents save-on-exit output. It does not replace the later release-candidate save and uninstall tests.

### Extended raised-placement regression

Date: August 10, 2026

Profile: PBVP Phase 1 Extended, native Direct3D 9, windowed 1920x1080, VSync on, RTSS limit unchanged

The full Extended UI regression used the same 64-unit bottom inset and unchanged 384 by 216 panel. The user reported that it looked good. This accepts the raised position for both tested UI stacks.

The first save-isolation result failed. Normal exit created one named `.fos` save and its `.nvse` co-save inside the isolated profile. The pair was moved to the ignored quarantine and remains recoverable. Inspection found that the guard placed the exit-save and timer keys under `[Tweaks]`, while the active Stewie Tweaks INI defines them under `[Save Manager]`.

Corrected guard result: the two-section guard remained enabled exactly once during a fresh Goodsprings test-world session. Normal menu exit left the isolated save directory empty, and DiaMove recorded no save-game message. PBVP completed two uploads from 24.30 to 28.70 microseconds with no failure, wrote both summaries, and shut down cleanly. The strict Phase 1 log check passed. This accepts the test-only guard for the remaining isolated Phase 1 profiles.

### Vanilla UI Plus placement result

Date: August 10, 2026

Profile: PBVP Phase 1 VUI Plus, native Direct3D 9, windowed 1920x1080, VSync on, RTSS limit unchanged

This isolated profile keeps Vanilla UI Plus and disables Clean Vanilla HUD and both Pip-Boy UI Tweaks mods. The user reported that the raised checkerboard looked good and that mouse and keyboard input remained usable. The panel resolved at `42,323` through `426,539` on the 1706.67x960 logical canvas.

The plugin completed one texture upload in 22.70 microseconds, recorded no upload failure, wrote both session summaries, and shut down cleanly. The strict Phase 1 log check passed. The isolated save directory remained empty after exit.

### Extended without Pip-Boy UI Tweaks result

Date: August 10, 2026

Profile: PBVP Phase 1 Extended No Pip-Boy Tweaks, native Direct3D 9, windowed 1920x1080, VSync on, RTSS limit unchanged

This isolated profile keeps Vanilla UI Plus and Clean Vanilla HUD while disabling both Pip-Boy UI Tweaks mods. The user reported that the checkerboard placement and visibility looked good and that mouse and keyboard input worked correctly. The panel resolved at `42,323` through `426,539` on the 1706.67x960 logical canvas.

The plugin completed one texture upload in 26.30 microseconds, recorded no upload failure, wrote both session summaries, and shut down cleanly. The strict Phase 1 log check passed. The isolated save directory remained empty after exit. All four isolated UI profiles now pass at 1920x1080.

### 1280x720 and 30 FPS result

Date: August 10, 2026

Profile: PBVP Phase 1 Extended, native Direct3D 9, windowed 1280x720, VSync off, RTSS 30 FPS

The user reported that the panel looked good and input remained usable. The plugin confirmed the requested 1280x720 backbuffer with immediate presentation. It completed one checkerboard upload in 25.70 microseconds and recorded no failure.

Six visible cadence samples measured 30.00 to 30.01 FPS, with a 30.00 FPS session average. Both summaries and the clean shutdown record were present, the strict log check passed, and the isolated save directory remained empty. This accepts the 1280x720 resolution, 30 FPS cap, and VSync-off rows for native windowed mode.

### 1280x960 and 60 FPS result

Date: August 10, 2026

Profile: PBVP Phase 1 Extended, native Direct3D 9, windowed 1280x960, VSync on, RTSS 60 FPS

The first run passed visual placement and input but did not pass timing. Focus or menu pauses produced cadence samples from 15.25 to 60.00 FPS and lowered the session average to 41.97 FPS. The strict checker rejected that result.

The timing-only retry kept the Data page focused and untouched. Eight cadence samples measured 59.98 to 60.04 FPS, with a 60.00 FPS average. The plugin confirmed the requested 1280x960 backbuffer and logical canvas, completed one upload in 26.30 microseconds, recorded no failure, and shut down cleanly. The strict check passed, and the isolated save directory remained empty. This accepts the required 4:3 resolution, 60 FPS cap, and VSync-on rows for native windowed mode.

### 2560x1440 and 90 FPS result

Date: August 10, 2026

Profile: PBVP Phase 1 Extended, native Direct3D 9, windowed 2560x1440, VSync off, RTSS 90 FPS

The active monitor was 1920x1080, so the larger game window extended beyond its right and bottom edges. The user reported that the visible panel looked good. The plugin confirmed the requested 2560x1440 backbuffer and completed one upload in 25.70 microseconds with no failure.

Eight cadence samples measured 89.99 to 90.04 FPS, with a 90.01 FPS session average. Both summaries and the clean shutdown record were present, the strict check passed, and the isolated save directory remained empty. This accepts the 2560x1440 resolution and 90 FPS cap rows for native windowed mode. It does not claim full-window visual coverage on the smaller physical display.

### 3440x1440 and 120 FPS result

Date: August 10, 2026

Profile: PBVP Phase 1 Extended, native Direct3D 9, windowed 3440x1440, VSync on, RTSS 120 FPS

The active monitor was 1920x1080, so the ultrawide game window extended beyond its right and bottom edges. The user reported that the visible panel looked good. The plugin confirmed the requested 3440x1440 backbuffer, resolved a 2293.33x960 logical canvas, and completed one upload in 24.90 microseconds with no failure.

Eight cadence samples measured 119.89 to 120.19 FPS, with a 120.01 FPS session average. Both summaries and the clean shutdown record were present, the strict check passed, and the isolated save directory remained empty. This accepts the 3440x1440 resolution and 120 FPS cap rows for native windowed mode. It does not claim full-window visual coverage on the smaller physical display.

### Phase 1 graphics support boundary

Date: August 10, 2026

The supported Phase 1 graphics path is native Direct3D 9 in windowed mode. The required native resolutions, aspect ratios, VSync states, frame caps, UI profiles, keyboard and mouse checks, exact 50-cycle focus test, upload measurements, and clean shutdown checks passed. Native fullscreen remains usable for ordinary presentation, but repeated focus changes are excluded because the same NVIDIA driver crash occurred with PBVP disabled.

DXVK and a safe root-management tool are absent from the target VNV instance. PBVP does not install a root `d3d9.dll` proxy, so Phase 1 makes no DXVK claim. Controller navigation and input-method switching belong to Phase 5, where the controls are implemented.

The native windowed focus path did not expose a device recreation. PBVP creates no Direct3D resource, accepts only the engine's managed texture, and releases every temporary COM reference before returning from the callback. A changed device or surface identity triggers validation before use. This satisfies the Phase 1 resource-ownership gate without claiming that a natural Direct3D Reset passed.

Final environment cleanup restored the RTSS FalloutNV limit to zero, preserved its denominator at one, and left RTSS closed. Comparison with the saved control profile found no functional setting difference. RTSS updated only the `[Info]` timestamp, so the final file hash differs from the control copy even though the cap and every other setting match.

## Required profiles

| Profile | Purpose |
| --- | --- |
| Vanilla plus xNVSE and plugin | Finds hidden dependencies on VNV additions |
| Base VNV | Main stability and bug-fix baseline |
| VNV Extended | Required target with the normal UI and gameplay additions |
| VNV Extended without Pip-Boy UI Tweaks | Isolates the UI integration |
| VNV Extended with DXVK | Tests the D3D9 API through a translation layer |
| VNV Extended with a supported handheld Pip-Boy | Tests screen rectangle and animation assumptions |

### Local test profile isolation

The local MO2 instance has four PBVP-specific Phase 1 profiles:

- `PBVP Phase 1 Base`
- `PBVP Phase 1 VUI Plus`
- `PBVP Phase 1 Extended`
- `PBVP Phase 1 Extended No Pip-Boy Tweaks`

The Base profile supplies the vanilla visual UI with UIO still active for PBVP injection. The VUI Plus profile keeps Vanilla UI Plus but disables Clean Vanilla HUD and both Pip-Boy UI Tweaks mods. The no-tweaks Extended profile keeps Vanilla UI Plus and Clean Vanilla HUD while disabling only the Pip-Boy tweaks. The full Extended profile retains its normal UI stack.

Each profile enables the separate `Pip-Boy Video Player - Dev` mod and the local `Pip-Boy Video Player - Phase 1 Save Guard` mod on top of its source profile. The guard uses Stewie Tweaks multi-INI support to disable improved autosaves, save-on-exit, and the autosave timer only in these test profiles. It is not part of the PBVP release data. The setup copies no save data and does not change MO2's selected profile. Because the original base profile has an empty `falloutprefs.ini`, its test copy uses the Extended profile's known 1920x1080 fullscreen display preferences. The base mod list remains the source for every other setting.

Create these profiles once, or verify them later:

```powershell
.\scripts\create-phase1-profiles.ps1 `
  -InstanceRoot 'C:\path\to\Viva New Vegas' `
  -Confirm:$false

.\scripts\create-phase1-profiles.ps1 `
  -InstanceRoot 'C:\path\to\Viva New Vegas' `
  -VerifyOnly
```

Existing PBVP profiles created before the save guard can be updated once:

```powershell
.\scripts\create-phase1-profiles.ps1 `
  -InstanceRoot 'C:\path\to\Viva New Vegas' `
  -InstallSaveGuard `
  -Confirm:$false
```

The normal creation command refuses to replace an existing target. If a later project update adds another isolated profile, `-CreateMissing` validates every existing target and creates only the missing profiles. The automated test builds a temporary MO2 fixture, confirms that saves are not copied, verifies each disabled UI layer, checks the replacement refusal, recreates one missing profile without touching the others, and verifies save-guard installation and repair. Verification accepts an empty `saves` directory created by MO2 but refuses the profile as soon as that directory contains a file or subdirectory.

The first windowed Extended run exposed the source profile's enabled save-on-exit setting. It created one `.fos` file and its `.nvse` co-save inside the isolated profile at shutdown. Both files were new test output. They were moved together to the ignored `build-host/quarantine` area and remain recoverable. No original VNV save was read, changed, or removed. A later Extended run proved that the first guard layout did not override two `[Save Manager]` keys. The corrected guard must preserve the same profile-only design and use each setting's documented section.

### Reversible display and frame-rate cases

Exit FalloutNV and close Mod Organizer before changing a test case. The scripts refuse to change profile files while the Mod Organizer executable from the selected instance is running. Create a FalloutNV.exe application profile in RTSS, then configure one isolated MO2 profile:

```powershell
$rtssProfile = Join-Path ${env:ProgramFiles(x86)} `
  'RivaTuner Statistics Server\Profiles\FalloutNV.exe.cfg'

.\scripts\set-phase1-test-case.ps1 `
  -InstanceRoot 'C:\path\to\Viva New Vegas' `
  -ProfileName 'PBVP Phase 1 Extended' `
  -RtssProfilePath $rtssProfile `
  -Width 1920 -Height 1080 `
  -DisplayMode Fullscreen `
  -FpsCap 60 `
  -VSync On `
  -Confirm:$false
```

The script accepts only the five resolutions and four frame caps in the Phase 1 matrix. It updates `fallout.ini`, `falloutprefs.ini`, and `falloutcustom.ini` only inside the selected PBVP profile. It also updates the existing RTSS application profile. The first case saves all four original files. Later cases for the same profile reuse that baseline.

Restore the files before testing a different profile:

```powershell
.\scripts\set-phase1-test-case.ps1 `
  -InstanceRoot 'C:\path\to\Viva New Vegas' `
  -ProfileName 'PBVP Phase 1 Extended' `
  -RtssProfilePath $rtssProfile `
  -Restore `
  -Confirm:$false
```

The restore operation puts the saved bytes back and removes its temporary state. Before creating a backup, the script checks that every profile and RTSS file is writable. It refuses a protected file without changing settings or leaving temporary state. It also refuses a second profile while another profile has an active case, profiles with saves, a missing development mod, a missing save guard, an unexpected RTSS filename, or an undocumented resolution. Its automated fixture covers the protected-file refusal, save-guard refusal, two successive cases, other rejection behavior, and byte-for-byte restoration. This tooling prepares repeatable cases but does not count as an in-game result.

For a display or VSync check that must leave RTSS untouched, omit `-RtssProfilePath` and add `-SkipFrameCap`. The script then changes only the three INI files inside the selected save-isolated PBVP profile. It stores the restore state in that profile and reports that the frame cap is unchanged. A display-only result cannot be used as evidence for any FPS-cap row.

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

The August 10, 2026 private deferred-request run is inconclusive for device recreation. It produced two successful surface uploads and the user saw the checkerboard return, but the verified recreation detour recorded no entry or result. This run counts as surface recovery after a UI transition only. It does not satisfy the reset row of the graphics matrix.

The retired controlled recreation checker required exactly one observed request consumption and one verified restoration of the original requested-size values. No diagnostic run met those conditions. The checker and request path were removed after the native call froze.

The next clean follow-up run kept the checkerboard visible and produced an orderly renderer summary. It recorded one successful upload at 28.50 microseconds, no upload or recreation failures, and eight visible cadence samples from 131.22 to 144.21 FPS. The guarded diagnostic did not write the request because at least one helper precondition was unavailable. This is a stable uncapped render baseline, not a recreation pass.

The per-field follow-up found a valid renderer and readable requested-size fields, but both size values were zero. The validated backbuffer remained 1920x1080, the checkerboard stayed visible, and the clean summary recorded one 30.00 microsecond upload with no failures. This confirms that a request byte alone reaches only the helper's early return.

The guarded same-size candidate met those checks, staged 1920x1080, and entered the verified recreation detour. The game then froze before the original `NiDX9Renderer::Recreate` call returned. The log contains no request consumption, value restoration, renderer summary, or orderly shutdown record, and CrashLogger produced no dump. This is a failed compatibility result. The private forced-recreation path is retired and must not be installed or run again.

The current development DLL installs no recreation detour and has SHA-256 `0091FF35675409A8612B24A113AC1571C9D5EFE6E55939813E12EF8565C8F547`. The reviewed managed-texture decision establishes that PBVP retains no reset-sensitive resource to release. The hook-free path passed its visual, measured windowed Alt+Tab, log, and clean-shutdown checks. The game exposes no verified safe in-process display toggle in this setup, so the accepted result is the managed-resource ownership contract rather than an observed Reset. This evidence does not permit a DXVK claim.

## Phase 2 media-core evidence

The standalone Win32 Release test filled all three bounded 1920x1080 BGRA queue slots. Process private memory increased by 65,880,064 bytes, committed address space increased by 74,383,360 bytes, and the video queue held exactly 24,883,200 bytes. A separate full decode produced 30 frames and 48,128 audio samples in 165,342 microseconds of wall time and 312,500 microseconds of process CPU time.

The first live MO2 measurement used a baseline taken during `DeferredInit`. It decoded the expected output but reported 167,104,512 additional private bytes while other initialization was still active. That result failed the memory target and does not count as decoder memory evidence.

The corrected run waited five seconds, recorded a zero-byte private-memory change during a one-second no-decode control, and reset the baseline before starting the worker. The same 1080p fixture then added 62,976,000 private bytes, produced all 30 frames and 48,128 samples, and kept its video and audio queue peaks at 16,588,800 and 24,576 bytes. The worker joined before FFmpeg unloaded, and the log contained no absolute media path.

The fixture existed only in the enabled `Pip-Boy Video Player - Media Test` mod. It was absent from the physical game directory, the PBVP development mod, and MO2 overwrite. This proves custom AVIO visibility through the active USVFS session. It does not complete the larger release media matrix, long-duration stability tests, or integrated playback measurements assigned to later phases.

## Phase 3 audio and clock evidence

The native x86 stream test compared 100, 200, and 300 ms prebuffers. Each case reached end of stream without an underrun. It also passed pause, resume, mute, volume, stop and flush, sample-origin replacement, end of stream, healthy device reconstruction, 44.1 and 48 kHz mono and stereo output, QPC pause and seek behavior, pool exhaustion, foreign-thread refusal, and 25 complete stream lifetime cycles. The real decoder-to-audio test played generated 44.1 kHz stereo, 48 kHz mono, and 48 kHz 5.1 sources while muted. FFmpeg converted each source to stereo 48 kHz PCM, and XAudio2 reached the expected end time with zero underruns.

The live `PBVP Phase 3 Audio` MO2 profile supplied its generated MP4 through a separate test mod. The user heard the two-second tone at the main menu. The plugin submitted 96,967 samples, reported a final audio clock of 2,020,125 microseconds, and reached one end-of-stream callback with zero underruns. Its fixed PCM pool was 262,144 bytes. The decoder worker joined before FFmpeg unload, and the source voice, callback targets, and pool were released before process shutdown. The strict log checker passed and found no absolute media path.

This result selects a 200 ms default for the current reference system. The Phase 4 automation now covers the 30-minute scheduler target, low-FPS frame dropping, menu lifecycle behavior, and repeated integrated seeks. The live 30-minute run and full in-game matrix remain open. Phase 6 retains device-removal fault injection and the full stability matrix.

## Phase 4 integrated playback evidence

All 14 portable host tests and all 23 Win32 Release tests pass. The integrated x86 controller test delivered 18 video frames after two startup drops, played all 96,967 audio samples, reached 2,020,125 microseconds, and reported zero underruns. Separate tests cover pause and resume, forward and backward seeks, stop during buffering, Pip-Boy closure, game transitions, foreign-thread refusal, silent QPC playback, and orderly teardown. A simulated 30-minute 30 FPS stream at a 10 FPS game cadence showed bounded late-frame dropping without cumulative clock drift.

The live `PBVP Phase 4 Playback` profile used the generated two-second H.264 and AAC fixture from its separate MO2 media mod. At 1920x1080 native D3D9 fullscreen, the user saw the video and state text, heard synchronized audio, and reported correct behavior after Alt+Tab. The plugin decoded 20 frames, presented 18, dropped two at startup, and uploaded every submitted frame. Upload time was 20.10 microseconds minimum, 27.52 microseconds average, and 59.40 microseconds maximum. The mailbox replaced no frame, XAudio2 reported no underrun, and the process shut down after the decoder joined.

This accepts the short integrated path for the tested profile. It does not close the live 30-minute synchronization test, low-FPS in-game playback, pause and seek input scenarios, controller coverage, DXVK, other UI profiles, or the two-hour stability soak.

The live 30-minute test now has a reproducible private fixture and strict checker. The pinned generator produced an exact 1,800-second, 54,000-frame, 1280x720 H.264 stream with stereo 48 kHz AAC. The 15,720,894 byte file reproduced SHA-256 `431220B5D0F941E85E44671CDC46F04E43C3D6FA5AFA04988B35073E5C2FA239`. The armed build records progress every five minutes and measures final synchronization error, private-memory growth, queue peaks, upload cost, underruns, and shutdown order. Packaging rejects that build. This describes prepared test infrastructure, not a passed live result.

The first in-game attempt began decoding, audio playback, and texture upload, then stopped after about 340 milliseconds at the controller's video staging capacity check. The decoder and audio backend still reported `ok`. Native reproduction found that the byte counter subtracted the BGRA size after moving the frame, when the source vector was already empty. The corrected controller passed a complete 1080p playback regression and a five-second startup run against the real long fixture. The 30-minute in-game row remains open until the corrected build completes it.

The second in-game attempt used the corrected staging build but stopped at a different boundary about 39 milliseconds after opening. XAudio2 had not initialized and no decoded frame reached the renderer. The worker reported `allocation_failed`, and the controller observed it at `decoder_state_before_drain`. The standalone x86 decoder still completes the identical file. The next short run must capture the new fixed allocation-site code and the process address-space snapshot before any queue, container, or decoded-frame layout changes.

The third attempt captured `video_pixel_buffer`. The 1280 by 720 container opened, playback reached buffering, and the audio backend reported `ok`, but the first 3,686,400-byte BGRA conversion buffer threw an allocation exception. Process private memory was 1,590,300,672 bytes at failure. Its largest free virtual-memory region was 1,536,557,056 bytes, so contiguous address-space exhaustion does not explain the failed allocation. The next build keeps the 1920 by 1080 input cap but scales queued BGRA output into a 512 by 512 bound. A short live startup check must pass before another 30-minute run begins.

The bounded-output native build passed its first gate. A 1080p source produced 512 by 288 queued frames, and all three decoder slots held 1,769,472 bytes. The real long fixture stayed in playback for five seconds with 150 decoded frames, 143 delivered frames, six late drops, 245,760 submitted audio samples, a 4,940,000-microsecond clock, and no failure site. This is native process evidence only. The short in-game startup check remains open.

The first bounded-output in-game attempt played and uploaded video for about four seconds, then failed at `video_pixel_buffer` again. The 589,824-byte payload failed while the process had a 1,547,436,032-byte contiguous free region. This rejects payload size and virtual address-space exhaustion as sufficient explanations. The next candidate moves only video pixel storage from the C++ heap to checked `VirtualAlloc` regions. It must pass a new short live run before the 30-minute row resumes.

The virtual-memory payload candidate passes its native gates. A stress case completed 512 payload allocation, move, and release cycles while retaining less than 8 MiB after the loop. The 1080p queue and complete decode passed. The real long fixture also stayed in playback for five seconds with 150 decoded frames, 143 delivered frames, six late drops, 245,760 audio samples, a 4,940,000-microsecond clock, and no failure site. A live startup run remains required.

The `VirtualAlloc` payload build passed its short live startup gate. The user saw continuous video for about 56 seconds and reported no playback error. The shutdown summary counted 1,659 submitted frames, 1,659 uploaded frames, and zero upload failures. Upload time was 18.00 microseconds minimum, 25.65 microseconds average, and 98.10 microseconds maximum. Playback reached buffering as the session ended, then stopped audio and joined the decoder worker during process shutdown. The complete 30-minute run is still pending.

The user did not hear the fixture's tone during this run. The long diagnostic uses 3 percent source-voice volume, compared with 10 percent in the audible Phase 3 test. The log contained no audio-device or playback error. This run confirms the short live allocation fix but does not add an audible-audio result.

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
