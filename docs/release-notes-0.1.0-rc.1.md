# Pip-Boy Video Player 0.1.0-rc.1

This private release candidate plays local MP4 files inside the Fallout: New Vegas Pip-Boy. It uses a 32-bit xNVSE plugin, a private FFmpeg 8.1.2 runtime, an engine-owned Direct3D 9 texture, and the Windows XAudio2 2.9 runtime.

## Supported configuration

- FalloutNV runtime 1.4.0.525
- xNVSE 6.4.5 or newer
- UIO 2.30
- Viva New Vegas Base and Extended profiles
- native Direct3D 9 in windowed mode
- mouse and keyboard input
- MP4 with H.264 video and optional AAC audio
- sources up to 1920 by 1080

## Verification summary

- All 27 portable tests and all 36 Win32 Release tests pass.
- The 30-minute 720p30 synchronization run finished with 28.125 milliseconds of audio-to-video error, zero audio underruns, 41,811,968 bytes of peak private-memory growth, and a 150.40 microsecond maximum texture upload.
- The five-minute 10 FPS run dropped late frames without cumulative audio-clock drift and finished with zero underruns.
- The native x86 lifecycle test passed 100 open and stop cycles. Handles remained at 184, threads remained at 6, and retained private memory was 765,952 bytes.
- A fresh native session passed 20 forward and 20 backward seeks with zero underruns.
- The Base, Vanilla UI Plus, Clean Vanilla HUD, and Extended UI profiles passed their catalog, playback, mouse, keyboard, layout, and shutdown checks.
- Native Direct3D 9 windowed mode passed 50 measured focus-return cycles. A matched PBVP-disabled control also passed 50 cycles.
- A three-session release-build smoke accounted for all 79 playback frame uploads, reported zero underruns, and shut down normally.
- Native tests cover valid media, empty and random files, truncation, unsupported video and audio codecs, encrypted media, rotation, variable frame rate, mono, stereo, and 5.1 downmix.
- The runtime and symbols archives pass exact inventory, dependency, timestamp, entry safety, expansion, local-path, personal-media, save, log, dump, and unexpected-DLL checks.
- The runtime archive passes an isolated install, removal, and reinstallation check without changing the adjacent save sentinel.

## Known limitations

- The two-hour mixed soak was not run. The project owner waived it for this private candidate on August 11, 2026. This is not a soak-tested build.
- DXVK is not tested or supported.
- Repeated native fullscreen Alt+Tab is not supported. The reference system reproduced its graphics failure with PBVP disabled.
- Controller and gamepad input are not supported.
- The catalog scans only direct-child MP4 files and does not scan subdirectories.
- Other containers and codecs are not supported even if a different FFmpeg build could decode them.
- Closing the Pip-Boy or leaving Videos stops playback. Resume positions are not saved.
- The reference windowed focus path did not expose a natural Direct3D Reset. PBVP uses an engine-owned managed texture and retains no Direct3D resource between callbacks.

## Publication status

This candidate and its repository remain private. No project license has been selected. Do not publish the binary or source as a public release until the owner chooses a project license, FFmpeg and codec distribution obligations are reviewed, and the owner approves publication.
