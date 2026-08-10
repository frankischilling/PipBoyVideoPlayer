# Playback design

## Opening media

Selection moves the player from `Idle` to `Opening`. The worker opens the file through a custom FFmpeg I/O bridge backed by Windows file handles. This makes path handling, MO2 testing, cancellation, file-size checks, and logging behavior explicit instead of leaving them to FFmpeg's default URL layer.

The open step rejects:

- files outside the configured media root after path normalization;
- non-regular files and reparse-point escapes;
- zero-length files;
- files larger than a configurable safety limit;
- media with no decodable video stream;
- source dimensions above the configured cap;
- dimensions or strides that overflow 32-bit size calculations;
- encrypted or unsupported streams.

The initial dimension cap should be 1920 by 1080. A later release can raise it after memory and decoder tests. Rotation metadata should be honored so phone videos do not appear sideways.

## Buffering

The player waits for a minimum amount of decoded audio and at least one displayable video frame. It then submits the first audio buffers, records the playback origin, and enters `Playing`.

The buffering threshold must be small enough for a responsive Pip-Boy and large enough to avoid immediate XAudio2 starvation. Native x86 tests compare 100, 200, and 300 milliseconds of PCM. All three completed without an underrun on the current reference system. The implementation keeps 200 milliseconds as its provisional default until the live MO2 run measures decoder and game-thread scheduling together.

Video-only files start against QueryPerformanceCounter after the first frame is ready. Audio-only MP4 files are outside the first-release scope because the feature is a video player.

## Timestamp model

All packet and frame timestamps are converted from stream time bases into signed microseconds relative to the selected playback origin. Missing timestamps use FFmpeg's best-effort timestamp where available. Files with no usable presentation timeline fail with a clear error rather than inventing a fixed frame rate.

Variable frame rate playback follows presentation timestamps. The game frame rate does not advance the video clock.

For media with audio:

`media time = seek origin + played PCM samples / output sample rate`

For silent media:

`media time = seek origin + elapsed high-resolution counter time`

The implementation should account for audio samples queued before the source voice starts and reset its sample origin after each seek.

## Frame selection

On each eligible render callback, the renderer reads the master media time and examines the front of the video queue.

- Frames too early remain queued.
- The newest frame at or before the presentation deadline becomes visible.
- Older eligible frames are dropped.
- The last visible frame remains on screen while the next frame is early.
- A large discontinuity requests rebuffering instead of discarding the entire queue in one callback.

The initial late threshold is one video-frame duration plus 20 milliseconds. Variable frame rate streams use adjacent timestamps when available. This threshold needs tests at 30, 60, 90, and 120 game FPS, plus forced low-frame-rate cases.

## Pause and resume

Pause stops the XAudio2 source voice without flushing queued samples and freezes the fallback QPC clock. The decoder may continue until both bounded queues reach their pause limits, then it waits.

Resume restarts the voice and reestablishes the clock origin. A long pause must not cause the first visible frame to be treated as late.

Opening another menu above the Pip-Boy, losing focus, or starting a save load uses a policy setting:

- Pip-Boy navigation away from Videos stops playback.
- A temporary modal menu pauses playback.
- Save load, new game, main menu, or exit stops playback and closes the file.
- Device loss pauses presentation. Audio pauses once the loss is confirmed so it cannot run far ahead of the screen.

## Seeking

A seek command carries a target media timestamp and a monotonically increasing generation number. The decode worker discards older seek commands, asks the demuxer to seek to a suitable preceding point, flushes both decoders, clears converted queues, and decodes forward until it reaches the target.

The game thread stops and flushes the XAudio2 source voice before the new generation becomes active. Old frames and audio buffers include their generation number and are ignored if they arrive after a seek.

The UI shows `Seeking` until one target video frame and the new audio pre-roll are ready. If the container cannot seek, the controls are disabled for that file.

## End of stream

The worker drains both decoders after demuxer end of file. The final XAudio2 buffer carries an end-of-stream marker. Playback ends after the audio cursor reaches the last sample and the final video duration expires. Silent media ends from its last known presentation time.

The first release returns to the selected file in the catalog. Automatic replay is an option, off by default.

## Pixel and color handling

FFmpeg converts decoded video to BGRA for Direct3D upload. The first release targets SDR output. It reads color-range and matrix metadata where FFmpeg exposes it, then converts to an 8-bit presentation surface. Missing metadata uses a documented SD or HD default based on frame size.

The player has two display modes:

- Full color shows the converted frame with ordinary alpha blending disabled inside the video rectangle.
- Pip-Boy tint converts luminance to the active Pip-Boy color and may apply a restrained scanline texture.

HDR transfer functions are unsupported in the first release. The player should warn rather than show a badly clipped picture and call it supported.

## Audio handling

The media core outputs 48 kHz stereo interleaved signed 16-bit PCM. Mono input is mapped to both channels, and multichannel input is downmixed through FFmpeg's channel-layout API. The native Phase 3 pipeline test accepted that format for 44.1 kHz stereo, 48 kHz mono, and 48 kHz 5.1 sources, so no second output format is needed.

Volume changes apply to the source voice. Muting audio keeps the audio cursor active so video timing does not change. Files with no audio use the QPC clock.

Audio device removal, source-voice failure, or repeated underruns stops playback with an error. A future fallback to silent playback can be considered after the failure can be detected consistently.

## Resource limits

Every limit belongs in one configuration structure and is logged at startup:

| Limit | Planning default |
| --- | ---: |
| Maximum source width | 1920 |
| Maximum source height | 1080 |
| Maximum video queue | 3 frames |
| Maximum converted video bytes | 32 MiB |
| Target queued audio | 200 ms |
| Maximum queued audio | 500 ms |
| Maximum media file size | 32 GiB |
| Maximum catalog entries | 500 |
| Maximum metadata string | 512 UTF-8 bytes |

These values are starting points. Multiplication and alignment must use checked `size_t` arithmetic even though the process is 32-bit.
