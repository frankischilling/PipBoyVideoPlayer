# Architecture

## Runtime boundary

The project is one x86 xNVSE plugin plus data files installed under `Data`. It does not use a `d3d9.dll` proxy and does not require an ESP. The plugin locates the game renderer after xNVSE load. The Phase 1 candidate draws from a checked normal-frame call immediately before the engine UI routine at `0x00709B40`. A verified engine recreation hook handles default-pool resource teardown. The plugin restores the original call and disables its work during shutdown.

The implementation must target FalloutNV 1.4.0.525 and reject unknown executables. It should use verified relocations or signatures rather than naked absolute addresses. Steam, GOG, and patched Epic executables need separate verification records if their code layouts differ.

## Components

### Plugin lifecycle

The lifecycle owner handles xNVSE query and load, messaging registration, configuration, log startup, game-ready transitions, save-load transitions, and process shutdown. It is the only component allowed to start or stop the player as a whole.

It owns a small state machine:

`Unavailable -> Idle -> Opening -> Buffering -> Playing -> Paused -> Stopping -> Idle`

Any state may enter `Error`, which records the problem, drains owned queues, releases the file, and returns to `Idle` after the UI acknowledges it. Shutdown bypasses user-facing recovery and joins all workers before library unload.

### Media catalog

The catalog enumerates regular `.mp4` files directly below the configured Videos directory. Recursive scanning is disabled for the first release. Each entry stores a display name, normalized relative path, file size, and a stable session identifier.

The catalog does not open every file during enumeration. Duration and codec details are loaded on selection, which prevents a folder full of damaged media from delaying the Pip-Boy menu.

### Demux and decode worker

One worker thread owns FFmpeg format and codec contexts. It opens the selected file, chooses the default or best video and audio streams, reads packets, sends them to decoders, and converts decoded output into bounded queues.

The worker never calls Direct3D. It never edits menu tiles. Seeking is serialized through a command queue so codec flushes and demuxer repositioning happen on the same thread that owns the FFmpeg objects.

Planned FFmpeg libraries:

- `libavformat` for MP4 demuxing and file I/O;
- `libavcodec` for audio and video decoding;
- `libavutil` for timestamps, channel layouts, frames, and allocation helpers;
- `libswscale` for conversion to BGRA and presentation scaling;
- `libswresample` for conversion to interleaved PCM accepted by XAudio2.

`libavfilter`, `libavdevice`, and the command-line programs are not needed for the first release.

### Video queue

The video queue contains presentation timestamps, dimensions, stride, and owned pixel storage. It is bounded by frame count and bytes. A starting target is three ready frames with room for one frame under conversion. The exact limit is a profiling decision.

When the decoder is ahead, it waits. When rendering is late, the presentation side discards frames whose timestamps are behind the audio clock and keeps the newest eligible frame. It never lets the queue grow to absorb a slow game.

### Audio queue and output

The decode worker resamples audio to a fixed PCM format selected during stream setup. An audio feeder maintains a small XAudio2 buffer queue. Completed buffers return to a pool through voice callbacks; callback code only signals state and does not allocate, log, decode, or touch game objects.

XAudio2 `SamplesPlayed` is the master clock once playback starts. The clock subtracts known pre-roll and seek offsets. For silent videos, a QueryPerformanceCounter timeline becomes the master.

The player owns its XAudio2 engine and voices unless an implementation spike finds a safe game audio interface with equivalent timing data. Using a separate engine avoids altering Fallout's audio objects. The player volume follows its own setting in the first release; integration with game effect-volume settings is a later decision.

### Renderer

The renderer receives BGRA frames and uploads them on the game render thread. The preferred resource is a dynamic `D3DPOOL_DEFAULT` texture created with a lockable format. The implementation must copy rows using the returned Direct3D pitch rather than assuming tightly packed storage.

Before drawing, the renderer captures every Direct3D state it changes. It restores textures, samplers, shaders, vertex declarations, transforms, blend state, scissor state, and render targets before returning to the game. A state block may help, but the spike must measure its cost and confirm that it restores everything used by the selected hook point.

The renderer releases all default-pool resources before `Reset` and recreates them only after a successful reset. The upload queue remains in system memory during a lost-device interval. Playback may pause if the interval is long enough to exhaust bounded queues.

### Pip-Boy presentation bridge

UIO injects a prefab into the selected Pip-Boy menu. The prefab owns the video rectangle, focus region, labels, control prompts, and state traits. It does not attempt to display the decoded Direct3D texture itself.

The native renderer reads the resolved rectangle from the injected tile and draws the video into the same screen-space region at a verified point in the menu render order. This keeps layout decisions in XML while the native plugin owns the texture.

The bridge must prove three things before decoder work begins:

1. Tile coordinates can be converted to the correct backbuffer rectangle at 4:3, 16:9, 16:10, and ultrawide resolutions.
2. The chosen render hook puts video above the Pip-Boy background and below player controls and labels.
3. The game and other plugins retain their Direct3D state after the hook returns.

If those statements cannot be proven, the project needs a different presentation method before proceeding.

## Thread ownership

| Resource | Owning thread | Other access |
| --- | --- | --- |
| FFmpeg format and codec contexts | Decode worker | Commands through a synchronized queue |
| Converted video frames | Decode worker until queued | Render thread consumes owned frames |
| XAudio2 source buffers | Audio feeder | Voice callback returns buffer tokens |
| Direct3D device and resources | Game render thread | Other threads may publish CPU frame data only |
| Menu tiles and game objects | Game thread | Workers publish plain data only |
| Player state machine | Game thread | Workers post events without changing state directly |

Lock ordering must be written down before implementation. No callback may wait on the game thread. Shutdown signals workers, stops audio, joins the decode worker, and then releases libraries and hooks.

## Error containment

All external file and FFmpeg failures become structured playback errors. Windows structured exception handling may guard the outer render callback, but it is not a substitute for normal validation. If the renderer fails, it disables video drawing for the session and leaves the Pip-Boy usable.

The plugin log records component, state, error code, media basename, and timestamp. Full local paths are disabled by default. Repeated frame errors are rate limited so a bad file cannot generate an unbounded log.

## Hook policy

xNVSE 6.4.5's `kMessage_OnFramePresent` notification remains enabled for diagnostics, but it no longer draws video. Testing showed that it runs after the visible menu UI. The plugin does not patch `Present`, `EndScene`, or a Direct3D device vtable.

The Phase 1 candidate replaces the five-byte relative call at `0x00870403`, which targets the engine routine at `0x00709B40` in Fallout NV 1.4.0.525. The replacement draws the video rectangle and then calls the original routine. Before writing the call, the plugin decodes the live target and accepts only the reviewed original address. A changed opcode or target disables rendering for the session. The original bytes are restored during orderly shutdown only if the site still contains this plugin's replacement.

The engine's `NiDX9Renderer::Recreate` function remains the only MinHook detour. Before installing it, the plugin compares the live function entry with a reviewed signature table and rejects common jump stubs and unknown bytes. If either hook check fails, the plugin leaves the pre-UI call untouched and disables rendering.

The selected boundaries still need:

- in-game confirmation that the candidate runs on the render thread;
- in-game confirmation that the candidate sits above the Pip-Boy screen and below UIO controls;
- safe refusal when another plugin has already patched the reset target;
- Reset or lost-device coverage;
- conflict detection and a safe refusal path;
- a test result for native D3D9 and DXVK.

A proxy DLL is excluded because VNV users may already have root-level graphics wrappers. The normal package must remain installable through MO2.
