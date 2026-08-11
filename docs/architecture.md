# Architecture

## Runtime boundary

The project is one x86 xNVSE plugin plus data files installed under `Data`. It does not use a `d3d9.dll` proxy and does not require an ESP. The plugin locates the game renderer after xNVSE load. UIO provides an engine-owned `TileImage` for the video surface so Gamebryo controls its clipping and layer order. The plugin updates that image's reviewed Direct3D texture chain from the render thread and does not issue a separate screen-space draw.

The implementation targets FalloutNV 1.4.0.525 and rejects unknown executables. Reviewed engine addresses and object layouts are tied to that runtime. Steam, GOG, and patched Epic executables need separate verification records if their code layouts differ.

## Components

### Plugin lifecycle

The lifecycle owner handles xNVSE query and load, messaging registration, configuration, log startup, game-ready transitions, save-load transitions, and process shutdown. It is the only component allowed to start or stop the player as a whole.

It owns a small state machine:

`Unavailable -> Idle -> Opening -> Buffering -> Playing -> Paused -> Stopping -> Idle`

Any state may enter `Error`, which records the problem, drains owned queues, releases the file, and returns to `Idle` after the UI acknowledges it. Shutdown bypasses user-facing recovery and joins all workers before library unload.

### Media catalog

The catalog enumerates regular `.mp4` files directly below the configured Videos directory. Recursive scanning is disabled for the first release. Each entry stores a display name, normalized relative path, file size, and a stable session identifier. The Win32 search handle has scoped ownership, so allocation and filename conversion failures close it before the scan returns an error.

The catalog does not open every file during enumeration. Rows initially use the filename without its extension. Duration, codec details, and file-level title metadata are loaded on selection, which prevents a folder full of damaged media from delaying the Pip-Boy menu. A valid bounded title replaces that row for the rest of the session without reordering the open catalog.

### Demux and decode worker

One worker thread owns FFmpeg format and codec contexts. It opens the selected file, chooses the default or best video and audio streams, reads packets, sends them to decoders, and converts decoded output into bounded queues.

The worker never calls Direct3D. It never edits menu tiles. Seeking is serialized through a command queue so codec flushes and demuxer repositioning happen on the same thread that owns the FFmpeg objects.

The private FFmpeg runtime contains:

- `libavformat` for MP4 demuxing and file I/O;
- `libavcodec` for audio and video decoding;
- `libavutil` for timestamps, channel layouts, frames, and allocation helpers;
- `libswscale` for conversion to BGRA and presentation scaling;
- `libswresample` for conversion to interleaved PCM accepted by XAudio2.

`libavfilter`, `libavdevice`, and the command-line programs are not needed for the first release.

### Video queue

The video queue contains presentation timestamps, dimensions, stride, and owned pixel storage. It is bounded to 12 ready frames and 32 MiB. For the tested 720p30 profile, 12 items cover 400 milliseconds and allow the 16-item decoded audio queue to become the interleaved worker's backpressure point. The byte cap remains authoritative for large frames.

When either decoder queue is ahead, its fixed item or byte limit blocks the worker. When rendering is late, the presentation side discards frames whose timestamps are behind the clock and keeps the newest eligible frame. It never lets a queue grow to absorb a slow game.

### Audio queue and output

The decode worker resamples audio to a fixed PCM format selected during stream setup. An audio feeder maintains a small XAudio2 buffer queue from a fixed pool. Completed buffers return to that pool through voice callbacks. Each callback performs only atomic state or counter updates. It does not allocate, log, decode, acquire a PBVP mutex, call game code, or perform blocking work.

XAudio2 `SamplesPlayed` is the master clock once playback starts. The clock subtracts known pre-roll and seek offsets. For silent videos, a QueryPerformanceCounter timeline becomes the master.

The player owns a system XAudio2 2.9 engine, mastering voice, source voice, callback target, and buffer pool. The audio owner initializes COM before creating XAudio2 and balances any successful COM initialization on that same thread after the voices and engine are gone. Using a separate engine avoids altering Fallout's audio objects. The source voice uses the default output device so Windows can follow ordinary device changes. The player volume follows its own setting in the first release; integration with game effect-volume settings is a later decision.

### Renderer

The renderer receives BGRA frames and uploads them on the game render thread. UIO declares a private, uncompressed DDS for `PBVP_VideoSurface`. The native bridge first checks the direct `TileImage::texture` member. When that member is null, as observed in the active VNV stack, it verifies `TileImage::shaderProp` and reads `TileShaderProperty::srcTexture`. Both paths require exact object vtables before the bridge continues through `NiTexture`, `NiDX9TextureData`, and `IDirect3DTexture9`. It also verifies dimensions, format, pool, device identity, and the shared game and render thread identity.

The renderer locks the engine-owned texture, copies rows using the returned Direct3D pitch, and releases its temporary COM reference before returning. It does not bind the texture or issue a primitive draw, so Gamebryo retains its normal UI render state and draws the surface in XML order. An upload at the final frame callback becomes visible on the next rendered frame. Frame selection must include that one-frame presentation offset.

The CPU scaler treats the 256 by 256 backing texture and the named visible rectangle as separate dimensions. Fit and Fill use the visible rectangle's aspect ratio, then map the result into the square texture before upload. This keeps the accepted engine texture contract without stretching a frame when Gamebryo draws it in the 384 by 216 playback area.

The plugin does not retain ownership of the UI texture across callbacks. The engine owns its lifetime and reset behavior. PBVP accepts only `D3DPOOL_MANAGED` for this surface and rejects default-pool or unknown-pool textures. It stores non-owning device and surface identities so a later callback can detect a replacement, validate it, and upload again.

The renderer keeps fixed-size session counters for frame callbacks, visible frames, validated devices, video submissions, mailbox outcomes, upload attempts, successful uploads, and failures. It records the minimum, average, and maximum successful upload time. An orderly shutdown writes one summary line. The counters do not allocate memory and the render callback does not write a line for each frame.

### Pip-Boy presentation bridge

UIO injects a prefab into the selected Pip-Boy menu. The prefab owns the video rectangle, engine image, focus region, labels, control prompts, and state traits. `PBVP_VideoRect` establishes a locus, so its surface and status elements use local coordinates and move with the viewport. The playback prompt calculates its Y position from the status-strip height and its measured text height instead of assuming one font metric. In the reviewed Vanilla UI Plus MapMenu, each PBVP drawable has an explicit depth from 10 through 12. Normal map and list content reaches depth 8, while headline cards use depth 15 and the tab line uses depth 22. The parent root also uses depth 10, but the implementation does not rely on that value propagating to drawable children. This places the video above page content and below the existing navigation without a frame-wide overlay.

The native bridge resolves the named image and follows the reviewed engine texture layout to its Direct3D resource. It does not write the image's reference-counted fields or change its filename at runtime. Gamebryo owns the texture and draws it. The plugin only updates its pixels while the image is live.

Playback state and errors use the existing `PBVP_LayerProbe` text tile. On the game thread, the bridge verifies that the named tile has a string trait owned by that tile, then calls the `Tile::SetStringValue` entry point documented by the pinned xNVSE 6.4.5 source for runtime 1.4.0.525. The bridge writes only when the tile identity, playback state, or error changes. Workers and callbacks never touch the tile.

The Videos page copies the live MapMenu object's 15 virtual function pointers into static plugin storage. It changes only `HandleClick` and `HandleKeyboardInput` in that private copy, then assigns the copy to the current MapMenu instance. Before attachment, the bridge checks the menu ID, requires readable committed table storage, and verifies that every entry points to executable memory. `HandleClick` must point inside `FalloutNV.exe`. `HandleKeyboardInput` may point there or to the pinned Stewie Tweaks 9.80 Menu Search wrapper. The Stewie exception requires exact PE metadata, handler RVA, entry bytes, forwarding bytes, saved-pointer storage, and an executable saved target inside `FalloutNV.exe`. Unrelated entries may belong to another loaded component and are preserved exactly. The bridge does not patch executable code or write to a shared table. Any unknown input entry or invalid pointer hides PBVP and leaves the menu alone.

Keyboard actions come from the verified MapMenu `HandleKeyboardInput` callback while Videos owns focus. The callback carries a 32-bit value. Printable keys use their character values, while menu keys use Bethesda's high-bit encoding: Backspace is `0x80000000`, Left through Down are `0x80000001` through `0x80000004`, and Enter is `0x80000008`. PBVP translates configured DirectInput scan codes into those values. The tested VNV stack did not expose these keyboard presses through the xNVSE input singleton at the game-thread polling point.

xNVSE's filtered DirectInput state remains the mouse source and a secondary keyboard source. The returned `DIHookControl` singleton begins with its inherited x86 virtual-table pointer, and its key array begins at offset 4. PBVP reads only the filtered `gameState` member. Controller edges come from XInput loaded from the Windows system directory. PBVP polls these sources only while the Videos page is open, except for the single left-button entry edge described below. During Videos focus, the private handlers consume ordinary MapMenu clicks and keyboard calls. When the page is inactive, they pass every ordinary event to the original functions.

Click actions first use the unique numeric ID assigned to each PBVP hotrect. If MapMenu reports a nested text or image tile instead, the bridge inspects at most eight ancestors and accepts only fixed PBVP button names. Vanilla UI Plus can report its later radio-list target or produce no callback where a PBVP control draws above it. The validated MapMenu callback handles the first case by checking exact visible PBVP hotrect bounds against `InterfaceManager::cursorX` and `cursorY`, the game's menu-space cursor fields at x86 offsets `0x38` and `0x40`. Button origins come from the verified runtime 1.4.0.525 `Tile::GetLocusAdjustedPosX` and `GetLocusAdjustedPosY` routines, so the comparison includes parent and locus transforms. For the entry with no callback, the game thread reads one filtered xNVSE left-button edge while the MapMenu, PBVP layer, and open button are active. It queues the open action only when the cursor is inside `PBVP_OpenButton`. The edge detector waits for a release before arming. Catalog and playback mouse input stays in the MapMenu callback while Videos owns focus. PBVP does not sum tile positions, read cursor tile traits, use a Windows mouse hook, or match visible labels.

Phase 1 proved three presentation properties before decoder work began:

1. Tile coordinates can be converted to the correct backbuffer rectangle at 4:3, 16:9, 16:10, and ultrawide resolutions.
2. The engine-owned presentation point puts video above the Pip-Boy background and below player controls and labels.
3. The game and other plugins retain their Direct3D state after the hook returns.

The accepted path uses the engine-owned texture and xNVSE frame-present callback. Any future runtime or renderer path must pass the same checks before it can be supported.

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

The plugin log records component, state, error code, media basename, and timestamp. Normal mode passes only the selected basename to the logger. Diagnostic mode may pass a validated relative catalog name. Absolute paths and traversal reduce to the basename in both modes, and embedded metadata is not used for log names. Repeated frame errors are rate limited so a bad file cannot generate an unbounded log.

## Hook policy

xNVSE 6.4.5's `kMessage_OnFramePresent` notification is the upload boundary for the engine-owned texture. It does not issue a primitive draw. The callback ignores loading screens and refuses the upload if its operating-system thread does not match the game thread that resolved the tile. The plugin does not patch `Present`, `EndScene`, the normal-frame UI call, or a Direct3D device vtable.

The Phase 1 diagnostic also uses QueryPerformanceCounter at this boundary to measure visible-frame cadence. It emits no more than eight three-second samples per process and resets a partial sample when the Pip-Boy hides or the device is unavailable. This metric verifies the configured game cap. It is not a playback clock and will not replace the audio-led media clock in later phases.

PBVP installs no executable detour and does not modify a Direct3D device vtable. MinHook is not a dependency. The xNVSE frame-present notification is the only render callback boundary, so another plugin cannot occupy a PBVP render hook target. The scoped MapMenu input bridge preserves compatible changes to unrelated entries and recognizes one audited Stewie Menu Search keyboard chain. Every other occupied input entry is rejected. Unknown runtime versions are still rejected by `NVSEPlugin_Query`, and every engine object and texture profile is validated before use.

The active VNV tests validated the live texture chain, matching callback thread IDs, managed texture pool, intended layer order, native windowed resolution matrix, and repeated focus changes. Portable tests reject unsupported texture sizes, formats, and memory pools. Phase 6 still needs repeated game-initiated display transitions. A separate DXVK result is required before any DXVK support claim.

PBVP does not request or force renderer recreation. The retired private request path froze after entering the native recreation call, so its build flag, request writer, observer, and installer were removed. Any remaining lifecycle test must begin with a display transition initiated by the game. Direct calls to the engine helper, the renderer owner, or `IDirect3DDevice9::Reset` are outside the supported design.

A proxy DLL is excluded because VNV users may already have root-level graphics wrappers. The normal package must remain installable through MO2.
