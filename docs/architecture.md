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

The decode worker resamples audio to a fixed PCM format selected during stream setup. An audio feeder maintains a small XAudio2 buffer queue from a fixed pool. Completed buffers return to that pool through voice callbacks. Each callback performs only atomic state or counter updates. It does not allocate, log, decode, acquire a PBVP mutex, call game code, or perform blocking work.

XAudio2 `SamplesPlayed` is the master clock once playback starts. The clock subtracts known pre-roll and seek offsets. For silent videos, a QueryPerformanceCounter timeline becomes the master.

The player owns a system XAudio2 2.9 engine, mastering voice, source voice, callback target, and buffer pool. The audio owner initializes COM before creating XAudio2 and balances any successful COM initialization on that same thread after the voices and engine are gone. Using a separate engine avoids altering Fallout's audio objects. The source voice uses the default output device so Windows can follow ordinary device changes. The player volume follows its own setting in the first release; integration with game effect-volume settings is a later decision.

### Renderer

The renderer receives BGRA frames and uploads them on the game render thread. UIO declares a private, uncompressed DDS for `PBVP_VideoSurface`. The native bridge first checks the direct `TileImage::texture` member. When that member is null, as observed in the active VNV stack, it verifies `TileImage::shaderProp` and reads `TileShaderProperty::srcTexture`. Both paths require exact object vtables before the bridge continues through `NiTexture`, `NiDX9TextureData`, and `IDirect3DTexture9`. It also verifies dimensions, format, pool, device identity, and the shared game and render thread identity.

The renderer locks the engine-owned texture, copies rows using the returned Direct3D pitch, and releases its temporary COM reference before returning. It does not bind the texture or issue a primitive draw, so Gamebryo retains its normal UI render state and draws the surface in XML order. An upload at the final frame callback becomes visible on the next rendered frame. Frame selection must include that one-frame presentation offset.

The plugin does not retain ownership of the UI texture across callbacks. The engine owns its lifetime and reset behavior. PBVP accepts only `D3DPOOL_MANAGED` for this surface and rejects default-pool or unknown-pool textures. It stores non-owning device and surface identities so a later callback can detect a replacement, validate it, and upload again.

The renderer keeps fixed-size session counters for frame callbacks, visible frames, validated devices, upload attempts, successful uploads, and failures. It records the minimum, average, and maximum successful checkerboard upload time. An orderly shutdown writes one summary line. The counters do not allocate memory and the render callback does not write a line for each frame.

### Pip-Boy presentation bridge

UIO injects a prefab into the selected Pip-Boy menu. The prefab owns the video rectangle, engine image, focus region, labels, control prompts, and state traits. `PBVP_VideoRect` establishes a locus, so its surface and status elements use local coordinates and move with the viewport. In the reviewed Vanilla UI Plus MapMenu, each PBVP drawable has an explicit depth from 10 through 12. Normal map and list content reaches depth 8, while headline cards use depth 15 and the tab line uses depth 22. The parent root also uses depth 10, but the implementation does not rely on that value propagating to drawable children. This places the video above page content and below the existing navigation without a frame-wide overlay.

The native bridge resolves the named image and follows the reviewed engine texture layout to its Direct3D resource. It does not write the image's reference-counted fields or change its filename at runtime. Gamebryo owns the texture and draws it. The plugin only updates its pixels while the image is live.

Playback state and errors use the existing `PBVP_LayerProbe` text tile. On the game thread, the bridge verifies that the named tile has a string trait owned by that tile, then calls the `Tile::SetStringValue` entry point documented by the pinned xNVSE 6.4.5 source for runtime 1.4.0.525. The bridge writes only when the tile identity, playback state, or error changes. Workers and callbacks never touch the tile.

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

Current code may hold only one PBVP mutex at a time. A caller must release any player, command, or queue lock before it waits on a queue. Queue methods do not call external code while locked. XAudio2 callbacks acquire no PBVP mutex. The audio owner calls XAudio2 only from its owning thread and observes callback results through atomics. Any future need to hold two locks at once requires a documented order and a focused deadlock test before implementation.

The renderer mailbox follows the same rule. The game thread holds its mutex only long enough to replace or clear one owned CPU frame. The render callback moves that frame out, releases the mutex, and only then scales pixels, resolves the UI surface, or calls Direct3D. It never waits for the decoder or audio callback while holding the mailbox mutex.

No callback may wait on the game thread. A completed XAudio2 buffer is not reused until its `OnBufferEnd` notification arrives. Pause stops the source voice without flushing it. Seek and stop halt the source voice and flush pending buffers, then the owner waits outside any PBVP lock until XAudio2 reports no queued buffers. Shutdown destroys the source voice before releasing its callback target or buffer pool. XAudio2 documents that `DestroyVoice` returns only after audio processing is idle, so no callback or buffer read can reach those targets afterward. Shutdown then joins the decode worker before releasing libraries and hooks.

## Error containment

All external file and FFmpeg failures become structured playback errors. Windows structured exception handling may guard the outer render callback, but it is not a substitute for normal validation. If the renderer fails, it disables video drawing for the session and leaves the Pip-Boy usable.

The plugin log records component, state, error code, media basename, and timestamp. Full local paths are disabled by default. Repeated frame errors are rate limited so a bad file cannot generate an unbounded log.

## Hook policy

xNVSE 6.4.5's `kMessage_OnFramePresent` notification is the upload boundary for the engine-owned texture. It does not issue a primitive draw. The callback ignores loading screens and refuses the upload if its operating-system thread does not match the game thread that resolved the tile. The plugin does not patch `Present`, `EndScene`, the normal-frame UI call, or a Direct3D device vtable.

The Phase 1 diagnostic also uses QueryPerformanceCounter at this boundary to measure visible-frame cadence. It emits no more than eight three-second samples per process and resets a partial sample when the Pip-Boy hides or the device is unavailable. This metric verifies the configured game cap. It is not a playback clock and will not replace the audio-led media clock in later phases.

PBVP installs no executable detour and does not modify a Direct3D device vtable. MinHook is not a dependency. The xNVSE frame-present notification is the only render callback boundary, so another plugin cannot occupy a PBVP hook target. Unknown runtime versions are still rejected by `NVSEPlugin_Query`, and every engine object and texture profile is validated before use.

The active VNV Extended test has validated the live texture chain, matching callback thread IDs, managed texture pool, and intended layer order. Portable tests reject unsupported texture sizes, formats, and memory pools. The selected boundary still needs a natural display-transition test, the remaining native Direct3D matrix, and a separate DXVK result before any DXVK support claim.

PBVP does not request or force renderer recreation. The retired private request path froze after entering the native recreation call, so its build flag, request writer, observer, and installer were removed. Any remaining lifecycle test must begin with a display transition initiated by the game. Direct calls to the engine helper, the renderer owner, or `IDirect3DDevice9::Reset` are outside the supported design.

A proxy DLL is excluded because VNV users may already have root-level graphics wrappers. The normal package must remain installable through MO2.
