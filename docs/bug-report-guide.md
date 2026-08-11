# Bug report guide

Keep reports specific and remove private information before sharing them.

Include:

- Pip-Boy Video Player version
- FalloutNV runtime and store version
- xNVSE and UIO versions
- VNV profile name, Base or Extended
- Windows version
- native Direct3D 9 or another graphics path
- window mode, resolution, and FPS cap
- active Pip-Boy, UI, graphics, and input mods
- the exact mouse or keyboard action that failed
- whether the problem happens again after restarting the game
- `PipBoyVideoPlayer.log`
- a crash log or dump if the game crashed
- a text media probe showing container, codecs, resolution, frame rate, audio layout, and duration

Do not attach the video unless you created it for testing and have permission to distribute it. Do not include saves unless a maintainer asks for one and you have reviewed its contents. Normal logs can contain media basenames, so rename or remove sensitive filenames before posting.

For a playback problem, describe what the player displayed, such as `BUFFERING`, `VIDEO DECODE ERROR`, or `AUDIO PLAYBACK ERROR`. Note whether audio continued, whether ordinary Pip-Boy input still worked, and whether closing and reopening the Pip-Boy restored the catalog.

For a rendering problem, include the active display mode and every wrapper or overlay that can touch Direct3D 9. Do not install a root `d3d9.dll` proxy only to reproduce a report.
