# Architecture

## Runtime boundary

The project is one x86 xNVSE plugin plus data files installed under `Data`. It does not use a `d3d9.dll` proxy and does not require an ESP. The plugin locates the game renderer after xNVSE load. UIO provides an engine-owned `TileImage` for the video surface so Gamebryo controls its clipping and layer order. The plugin updates that image's reviewed Direct3D texture chain from the render thread and does not issue a separate screen-space draw.

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

The renderer receives BGRA frames and uploads them on the game render thread. UIO loads a private, uncompressed DDS into `PBVP_VideoSurface`. The bridge validates the `TileImage`, `NiTexture`, `NiDX9TextureData`, and `IDirect3DTexture9` chain before each new surface is used. It also verifies dimensions, format, pool, device identity, and the shared game and render thread identity.

The renderer locks the engine-owned texture, copies rows using the returned Direct3D pitch, and releases its temporary COM reference before returning. It does not bind the texture or issue a primitive draw, so Gamebryo retains its normal UI render state and draws the surface in XML order. An upload at the final frame callback becomes visible on the next rendered frame. Frame selection must include that one-frame presentation offset.

The plugin does not retain ownership of the UI texture across callbacks. The engine owns its lifetime and reset behavior. The verified recreation hook remains during the spike to observe device recreation and reject an occupied lifecycle boundary. If testing proves that the engine texture is managed and no plugin-owned default-pool resource remains, a later decision may remove that detour.

### Pip-Boy presentation bridge

UIO injects a prefab into the selected Pip-Boy menu. The prefab owns the video rectangle, engine image, focus region, labels, control prompts, and state traits. The engine image places video between the Pip-Boy screen and the controls without requiring a frame-wide overlay.

The native bridge resolves the named image and follows the reviewed engine texture layout to its Direct3D resource. Gamebryo owns the texture and draws it. The plugin only updates its pixels while the image is live.

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

xNVSE 6.4.5's `kMessage_OnFramePresent` notification is the upload boundary for the engine-owned texture. It does not issue a primitive draw. The callback ignores loading screens and refuses the upload if its operating-system thread does not match the game thread that resolved the tile. The plugin does not patch `Present`, `EndScene`, the normal-frame UI call, or a Direct3D device vtable.

The engine's `NiDX9Renderer::Recreate` function remains the only MinHook detour during this spike. Before installing it, the plugin compares the live function entry with a reviewed signature table and rejects common jump stubs and unknown bytes. An unknown or occupied entry disables texture updates for the session instead of attempting to chain through another hook.

The selected boundaries still need:

- validation of the live `TileImage` texture chain;
- confirmation that the game-thread and upload callbacks use the same operating-system thread;
- in-game confirmation that the engine image sits above the Pip-Boy screen and below UIO controls;
- safe refusal when another plugin has already patched the reset target;
- Reset or lost-device coverage;
- conflict detection and a safe refusal path;
- a test result for native D3D9 and DXVK.

A proxy DLL is excluded because VNV users may already have root-level graphics wrappers. The normal package must remain installable through MO2.
