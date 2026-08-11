# Troubleshooting

## The VIDEOS entry is missing

Confirm that both Pip-Boy Video Player and UIO are enabled in the active MO2 profile. Launch through xNVSE, not directly through `FalloutNV.exe`. This candidate accepts FalloutNV runtime 1.4.0.525 and refuses unknown runtimes.

An incompatible mod can replace the MapMenu input handler. PBVP recognizes the game handler and the tested Stewie Tweaks 9.80 Menu Search handler. It hides its page if the active handler does not match a reviewed layout. This protects the rest of the Pip-Boy from an unsafe input patch.

## The catalog is empty

Check that the media mod is enabled and that each file is directly inside:

```text
NVSE\Plugins\PipBoyVideoPlayer\Videos
```

Only `.mp4` files are listed. Restart Fallout: New Vegas after enabling the media mod so MO2 can expose it through the virtual filesystem.

## A video shows an error

The first candidate supports H.264 video in MP4 with optional AAC audio. It rejects unsupported video codecs, unsupported audio codecs, encrypted media, damaged files, empty files, sources larger than 1920 by 1080, and files above the configured size limit.

Try the same file in a desktop media probe and confirm its video codec, audio codec, resolution, and duration. Do not upload a private or copyrighted video with a bug report. A text-only probe summary is enough.

`VIDEO MEMORY ERROR` means a checked 32-bit allocation failed. Close other overlays or memory-heavy plugins and try a smaller source. Do not raise the resource limits beyond the shipped caps.

`AUDIO PLAYBACK ERROR` means the XAudio2 stream or default audio device failed. Close the Pip-Boy, confirm that Windows still has a working default output device, and open the video again.

`VIDEO DISPLAY ERROR` means the validated Pip-Boy texture path stopped accepting frames. Close the Pip-Boy and reopen it. If the error returns, keep the log and report the active UI and graphics mods.

## Video disappears after closing the Pip-Boy

This is expected. Closing the Pip-Boy or leaving the Videos page stops playback. Open `VIDEOS` and select the file again.

## Alt+Tab problems

Use native Direct3D 9 in windowed mode. The windowed path passed repeated focus changes. Repeated native fullscreen Alt+Tab is not supported because the same failure occurred with PBVP disabled on the reference system. DXVK was not installed or tested and is not claimed.

## Controller input does not work

Controller and gamepad input are not part of this release. Use the mouse or keyboard controls shown in the player.

## Find the log

The plugin writes `PipBoyVideoPlayer.log` in the Fallout: New Vegas game directory. Normal logging can include media basenames, so review the file before sharing it. It does not intentionally log absolute media paths or embedded title metadata.

The log path has this form:

```text
<Fallout New Vegas game directory>\PipBoyVideoPlayer.log
```

Your game may be installed elsewhere.
