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

The earlier host and Win32 suites verified the audited `NiDX9Renderer::Recreate` return values while the observation detour was present. The managed-texture decision removes that detour because PBVP owns no reset-sensitive resource. The replacement tests require `D3DPOOL_MANAGED`, reject every other pool, and verify that a changed device or surface receives fresh validation and upload. An in-game natural display transition is still required.

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

Each profile enables only the separate `Pip-Boy Video Player - Dev` mod on top of its source profile. The setup copies no save directory and does not change MO2's selected profile. Because the original base profile has an empty `falloutprefs.ini`, its test copy uses the Extended profile's known 1920x1080 fullscreen display preferences. The base mod list remains the source for every other setting.

Create these profiles once, or verify them later:

```powershell
.\scripts\create-phase1-profiles.ps1 `
  -InstanceRoot 'C:\path\to\Viva New Vegas' `
  -Confirm:$false

.\scripts\create-phase1-profiles.ps1 `
  -InstanceRoot 'C:\path\to\Viva New Vegas' `
  -VerifyOnly
```

The normal creation command refuses to replace an existing target. If a later project update adds another isolated profile, `-CreateMissing` validates every existing target and creates only the missing profiles. The automated test builds a temporary MO2 fixture, confirms that saves are not copied, verifies each disabled UI layer, checks the replacement refusal, and recreates one missing profile without touching the others.

### Reversible display and frame-rate cases

Exit FalloutNV before changing a test case. Create a FalloutNV.exe application profile in RTSS, then configure one isolated MO2 profile:

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

The restore operation puts the saved bytes back and removes its temporary state. Before creating a backup, the script checks that every profile and RTSS file is writable. It refuses a protected file without changing settings or leaving temporary state. It also refuses a second profile while another profile has an active case, profiles with saves, a missing development mod, an unexpected RTSS filename, or an undocumented resolution. Its automated fixture covers the protected-file refusal, two successive cases, other rejection behavior, and byte-for-byte restoration. This tooling prepares repeatable cases but does not count as an in-game result.

For a display or VSync check that must leave RTSS untouched, omit `-RtssProfilePath` and add `-SkipFrameCap`. The script then changes only the three INI files inside the selected save-free PBVP profile. It stores the restore state in that profile and reports that the frame cap is unchanged. A display-only result cannot be used as evidence for any FPS-cap row.

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

The current development DLL installs no recreation detour and has SHA-256 `0091FF35675409A8612B24A113AC1571C9D5EFE6E55939813E12EF8565C8F547`. The reviewed managed-texture decision establishes that PBVP retains no reset-sensitive resource to release. The first hook-free run passed its visual, Alt+Tab, log, and clean-shutdown checks. A game-initiated transition is still needed to test fresh validation after a device or surface change. None of this evidence permits a DXVK or untested display-mode claim.

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
