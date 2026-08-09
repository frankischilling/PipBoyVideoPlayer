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

Status: awaiting an in-game run

The candidate build checks the five-byte relative call at `0x00870403` and accepts it only when it still targets `0x00709B40`. Its replacement draws the checkerboard and then calls the original engine routine. The frame-present callback remains active for diagnostics but does not draw.

Open the Pip-Boy Data tab and wait at least ten seconds. A useful result must report all three visual layers separately:

- whether the checkerboard appears inside the intended screen rectangle;
- whether the black strip appears over the checkerboard;
- whether `PBVP UI LAYER` appears on the strip.

Also report whether the ordinary Pip-Boy frame and controls remain usable. The candidate passes layer order only if the checkerboard appears below both UIO probe elements without covering the rest of the Pip-Boy.

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
