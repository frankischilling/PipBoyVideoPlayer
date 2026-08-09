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
