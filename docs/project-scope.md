# Project scope

## Goal

The mod will play local MP4 files inside the Fallout: New Vegas Pip-Boy. Playback happens in the game process through a native xNVSE plugin. The user should not need to extract frames, transcode audio, or run a converter before each video.

The first supported environment is Viva New Vegas on Windows 10 or Windows 11, launched through Mod Organizer 2. Steam and GOG installations are in scope. Epic Games support is conditional on testing the executable produced by the VNV-recommended patcher.

## First public release

The first release is deliberately narrow:

- MP4 files stored beneath one mod-owned video directory;
- H.264/AVC video with 8-bit YUV 4:2:0 as the required video path;
- AAC-LC stereo audio as the required audio path;
- MP4s with no audio;
- constant and variable frame rate timestamps;
- one video and one audio stream at a time;
- software decoding through FFmpeg;
- playback while the Pip-Boy video page is visible;
- play, pause, restart, stop, seek backward 10 seconds, and seek forward 10 seconds;
- full-color or Pip-Boy-tinted output;
- aspect fit by default, with aspect fill as an option;
- keyboard, mouse, Xbox-style controller, and remappable bindings;
- user-facing errors for unsupported or damaged files;
- an ESP-less package installed as a normal MO2 mod.

Other codecs may work through FFmpeg, but they are not supported until they have fixtures and test results. The UI should label unverified formats as unsupported rather than imply that every MP4 codec combination will work.

## Out of scope for the first release

- web URLs, YouTube, network shares, and live streams;
- DRM-protected media;
- playlists, shuffle, or background playback outside the Pip-Boy;
- video projected onto world-space terminals or television meshes;
- subtitles and closed captions;
- HDR output and HDR-to-SDR tone mapping;
- hardware video decoding;
- more than one active player;
- frame interpolation;
- video recording or screenshots;
- a bundled library of films, trailers, or television clips;
- support for ENB, New Vegas Reloaded, or unknown graphics injectors at launch;
- save-file persistence for playback position;
- loading videos from arbitrary absolute paths.

## User story

The user creates a personal MO2 mod named something like `Pip-Boy Videos`. Inside it, the user creates `NVSE\Plugins\PipBoyVideoPlayer\Videos` and copies MP4 files there. After launching New Vegas through MO2, the Pip-Boy has a Videos entry. The entry lists media names without extensions. The user selects a video, watches it, pauses or seeks when needed, then leaves the page. Leaving the page stops playback and releases the open file.

The video list refreshes when the Videos page opens. Files added while a video is playing appear the next time the page is opened. The plugin does not watch the entire filesystem.

## Acceptance targets

The first release is ready only when all of these are true:

- A 30-minute 720p30 H.264/AAC test file remains within 50 milliseconds of the audio clock after initial buffering.
- A low game frame rate causes video frame drops instead of cumulative audio drift.
- The renderer adds less than 1 millisecond at the 95th percentile for upload and drawing at the default presentation size on the reference test system.
- Working memory remains bounded during repeated playback. The default target is less than 128 MiB above the post-load baseline for a 1080p source.
- One hundred open, play, close cycles complete without a crash, stuck input, orphaned audio, or growing worker count.
- Fifty full-screen Alt+Tab cycles complete without a failed Direct3D reset.
- Twenty seeks in both directions complete without stale audio, stale video, or deadlock.
- Corrupt files, zero-length files, and files with hostile dimensions fail without an unhandled exception.
- Base VNV and VNV Extended pass the release test matrix.

Targets may change after profiling, but a changed target needs recorded measurements and a decision entry.

## Product boundaries

The plugin reads media and its own configuration. It does not modify saves or game records. Playback position is session-only. A crash log may contain the media filename but should omit its absolute path by default.

The project will not attempt to make unsafe files playable. FFmpeg errors, allocation caps, timestamp discontinuities, and unsupported pixel formats are normal user-facing failures.
