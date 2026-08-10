# Research notes

Research date: August 9, 2026

These notes separate facts used by the plan from design inferences that still need an implementation test.

## Prior art

[Pip-Flicks 3000](https://www.nexusmods.com/newvegas/mods/59173) proves that the
usable Pip-Boy glass can present moving pictures while the physical device frame
stays visible. Its author screenshot uses the Items screen as a full playback
surface rather than placing the picture in a permanent side panel. Its player
is selected by equipping an Apparel item, swaps 320x240 DDS frames, and plays a
separately extracted audio track. The documented design requires a steady 60
FPS and cannot stop the audio when the visual playback closes.

The project owner directed Pip-Boy Video Player to use Pip-Flicks 3000 as a
design, interaction, and technical reference wherever useful. The new player
will carry forward the full-glass playback idea, straightforward catalog
selection, support for replacer layouts, and immediate stop on Pip-Boy or page
closure. It will not carry forward the ESP requirement, frame extraction,
frame-rate clock, or audio lifecycle defect.

The [Pip-Flicks Modder's Resource article](https://www.nexusmods.com/newvegas/articles/54735)
permits quest mods to call its player and manage shown or hidden video entries.
That is a narrow integration permission, not a general source or asset license.
No maintained source repository or broader author permission was found. The
4 KB main package is listed on Nexus Mods but requires an authenticated download
before its ESP and scripts can be inspected locally. Until broader permission is
verified, this project may study behavior and interfaces but will not copy or
redistribute Pip-Flicks code, XML, scripts, assets, or media.

## xNVSE and VNV

The [xNVSE releases](https://github.com/xNVSE/NVSE/releases/) provide the current loader and plugin platform. The release page recommends Viva New Vegas for a stable setup.

The current [Viva New Vegas utilities guide](https://vivanewvegas.moddinglinked.com/utilities.html) includes xNVSE, JIP LN, JohnnyGuitar, ShowOff, NVTF, and UIO. It describes NVTF as supporting frame rates up to roughly 120 FPS and UIO as the UI composition layer.

The local VNV installation was inspected on August 9, 2026. The selected profile is Viva New Vegas Extended. The installation also contains the base Viva New Vegas profile. No game, profile, mod, or save files were changed during inspection.

The installed versions relevant to the plugin are xNVSE 6.4.5, JIP LN 57.30, JohnnyGuitar 5.20, ShowOff 1.82, NVTF 10.61, UIO 2.30, Vanilla UI Plus 9.48, Clean Vanilla Hud f1.01, Pip-Boy UI Tweaks 5.2.1, and Fallout Shader Loader 1.32.

The installed NVTF 10.61 configuration sets `iMaxFPSTolerance=300` and marks it as a value not to edit. It is not a general frame limiter. The maintained [Fallout NV Performance Guide](https://github.com/ModdingLinked/FalloutNV-Performance-Guide/blob/main/falloutnv.html) states that VSync is not a limiter and recommends a FalloutNV.exe profile in RivaTuner Statistics Server. RTSS is installed locally, and its existing FalloutNV.exe profile uses front-edge sync and passive waiting with the limit disabled. The Phase 1 matrix can set that profile to 30, 60, 90, or 120 FPS while retaining a restorable copy of its original contents.

The official [DXVK configuration reference](https://github.com/doitsujin/dxvk/blob/master/dxvk.conf) provides `d3d9.maxFrameRate`, but its own comments recommend an external limiter when one is available. Using the same RTSS profile for native Direct3D 9 and any later DXVK test avoids changing the limiter between graphics paths. This does not establish DXVK support. A separate isolated DXVK installation and in-game result are still required before making that claim.

The active `FalloutNV.exe` reports runtime 1.4.0.525 and has SHA-256 `518C87F58A6C4D9826E9EF8FBB7F4213882FA70822675610D45AEA2464502A57`. The retained backup executable has SHA-256 `3A87F92F011E5DC9179DDF733CF08BE2B39EA6E5B7A8A9E3A9A72DAFCC1B104D`. The active executable is large-address aware and contains code changes beyond the PE header. Runtime checks must therefore validate every patched function before installation. The game root contains no `d3d9.dll` or `dxgi.dll`, so the inspected profiles currently use native Direct3D 9.

Inference to test: an ESP-less native plugin plus UIO prefab should fit the VNV install model without a load-order patch.

## UIO

The [UIO documentation](https://www.nexusmods.com/newvegas/mods/57174) says it registers UI extensions before menus load and avoids modifying game UI files. Its public registration format can inject a prefab into a named game menu or XML component.

Inference to test: a prefab can expose a stable rectangle and controls inside the Pip-Boy Data flow across the required UI variants. UIO solves XML composition, but it does not by itself solve native texture placement or draw order.

The installed Pip-Boy UI Tweaks registrations and UIO's bundled author instructions confirm the public registration format. The first Phase 1 test used the correct target line, `PipBoyVideoPlayer\Player.xml::MapMenu::MM_MainRect`, but omitted the required condition line below it. UIO found the registration and prefab but did not add them. Its log contained a check entry without a matching add or inject entry, and `PBVP_Root` was absent from generated menu XML. The corrected record retains the target and adds `true` on the second line. A new in-game run must confirm the correction before the injection is treated as portable.

The second VNV Extended run confirmed the correction. UIO logged both `Adding to 'Main\map_menu.xml' @ MM_MainRect` and `Injecting 'PipBoyVideoPlayer\Player.xml' @ MM_MainRect`. No checkerboard appeared, and the plugin logged no resolved rectangle or Direct3D activity. UIO composition is therefore working, while the native tile bridge or its upstream frame callbacks still need diagnosis.

The diagnostic run confirmed that game-thread polling, non-loading frame presentation, native Direct3D 9 device discovery, MapMenu visibility, and `PBVP_VideoRect` lookup all work. Rectangle resolution stopped at the height and width traits. The copied trait constants were one slot early because Fallout's standard trait enumeration skips `0xFA5`. The bridge now uses `Tile::kTileValue_height` and `Tile::kTileValue_width` from the official xNVSE 6.4.5 headers instead of local numeric copies.

The next run resolved the video tile dimensions and stopped because `MapMenu` has no positive width and height pair. The active Vanilla UI Plus globals describe `screen` as the logical UI canvas and derive the physical screen dimensions from its width, height, and resolution converter. The native bridge now searches the bounded ancestor chain from `MapMenu` for the first positive width and height pair. It does not substitute the physical client size, which would mix backbuffer pixels with logical UI coordinates.

The following VNV Extended run produced the first visible checkerboard on the Pip-Boy Data tab at fullscreen 1920x1080 under native Direct3D 9. The native bridge resolved a 1706.67x960 logical canvas and video bounds of 110,108 through 670,423. The generated 256x256 upload took 149.10 microseconds. State preservation and drawing averaged 91.72 microseconds over 300 frames, with a recorded maximum of 647.70 microseconds. Layer order, state isolation, device recreation, and the remaining compatibility matrix are still open.

After the viewport restoration correction, the checkerboard stayed visible and
looked correct through five user-run Alt+Tab cycles. The reset detour did not log
a recreation during those switches. The result is evidence for presentation
persistence only. It does not yet prove the pre-Reset release and post-Reset
resource path.

The layer-order run used an updated prefab with a black image and the text
`PBVP UI LAYER` inside `PBVP_VideoRect`. The current `ui_organizer.log` records
both the add and inject operations for that prefab. The plugin log records the
same rectangle and successful checkerboard draws. The user saw the checkerboard
cover the Pip-Boy and saw neither UIO probe element. This rejects
`kMessage_OnFramePresent` as the final playback draw point. The callback remains
useful for presentation diagnostics, but the video draw needs an engine-owned
location before menu rendering.

## FFmpeg

The [FFmpeg library documentation](https://www.ffmpeg.org/doxygen/trunk/index.html) assigns container I/O and demuxing to `libavformat`, decoding to `libavcodec`, scaling and pixel conversion to `libswscale`, and audio conversion to `libswresample`. Library major versions may include incompatible API changes, so the build must pin and validate a specific set.

The [FFmpeg README](https://ffmpeg.org/doxygen/8.0/md_README.html) states that the codebase is mainly LGPL with optional GPL components. The actual configuration controls the resulting obligations. The package plan therefore excludes optional components until their licenses and need are reviewed.

Inference to test: a minimal x86 build with MOV/MP4, H.264, and AAC support is small enough in code and address-space cost for the target VNV profile.

## Direct3D 9

Microsoft's [`IDirect3DTexture9::LockRect` documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dtexture9-lockrect) documents `D3DLOCK_DISCARD` for dynamic texture updates and notes the restrictions on default-pool textures. The [`D3DPOOL` documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpool) states that default-pool resources must be released before `Reset` and recreated afterward.

Inference to test: a dynamic A8R8G8B8 default-pool texture is the lowest-cost reliable upload path for the chosen hook. The spike must also compare a system-memory staging texture plus `UpdateTexture` if direct locks stall or behave poorly through DXVK.

The Fallout NV performance guide discusses [DXVK and display modes](https://performance.moddinglinked.com/falloutnv.html). DXVK support is therefore part of the planned test matrix, not an assumption.

Microsoft's Direct3D 9 documentation confirms that default-pool resources and state blocks must be released before `Reset`. `D3DSBT_ALL` captures vertex and pixel state, textures, streams, indices, viewport, scissor state, transforms, and material. Render targets are not part of that documented list. The checkerboard path does not change the render target or depth surface, so it leaves both untouched. Microsoft also documents that [`SetRenderTarget`](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget) resets the viewport to the full target. Rebinding an unchanged target after applying the state block could therefore overwrite the restored viewport. [`CreateStateBlock`](https://learn.microsoft.com/en-us/windows/win32/direct3d9/state-blocks-save-and-restore-state) captures the selected state immediately, so a second `Capture` call is unnecessary for a one-use block.

The xNVSE 6.4.5 source dispatches `kMessage_OnFramePresent` on the render thread immediately before the engine's frame presentation call. The message is sent for both the main loop and loading screens, with a loading-screen flag in the message data. This provides a presentation boundary without patching `Present`, `EndScene`, or a live Direct3D device vtable.

Psycho's checked-in Ghidra audit records the normal-frame sequence after rendered-menu work. The call at `0x00870403` targets the 10-byte routine at `0x00709B40`; later work reaches the xNVSE frame-present site at `0x0087055E`. JohnnyGuitar NVSE commit `155679e1ec84c597cfe631b73666fc22d661609a` separately identifies the earlier `Main::Render` call at `0x008702A9` as the rendered-menu update path. Together, these maintained sources make `0x00870403` a credible pre-UI candidate, but only the in-game overlap probe can establish its visible layer order.

The candidate implementation checks the live call opcode and decodes its target before writing anything. It refuses the site if the target is not `0x00709B40`, writes a relative call to the plugin only after the reset hook is ready, calls the original routine after drawing, and restores the original bytes during orderly shutdown if no later code changed the site. The frame-present callback now logs its availability without issuing a draw.

The in-game test rejected that candidate. The current log contains the expected `E8 38 97 E9 FF` bytes, decodes the target as `0x00709B40`, and records successful installation. The user saw the checkerboard hovering over the Pip-Boy but saw neither overlapping UIO probe. The ordinary Pip-Boy frame and controls stayed visible and usable. The log recorded average draw costs between 47.08 and 63.36 microseconds across ten 300-frame samples. No orderly exit message appeared in that log, so restoration of the five-byte call was not observed directly.

Maintained JIP LN NVSE source defines `TileImage::texture` at offset `0x3C` and its shader property at `0x40`. It defines `NiTexture::textureData` at offset `0x24` and `NiDX9TextureData::d3dBaseTexture` at offset `0x64`. Psycho's checked-in `FNV NITEXTURE D3D CHAIN AUDIT` independently verifies the latter two offsets. JIP's `TileImage::SetTexture` wrapper targets `0x00A20610`, and its alpha-texture helper uses the engine's reference-counted replacement path. The fallback will not call those mutators or borrow a shared game texture. UIO will load a private DDS, and the plugin will take only a temporary Direct3D reference while validating and updating that image.

The package script now generates that private DDS from fixed values instead of storing or borrowing an asset. The output has a 128-byte DDS header, 256x256 BGRA pixels, and a total size of 262272 bytes. The local `ffprobe` check recognized it as a 256x256 BGRA DDS. Its initial dark purple color distinguishes a successful UIO asset load from the green checkerboard written by the plugin.

The first private-image run proved that UIO created `PBVP_VideoSurface` as the reviewed `TileImage` type and that the game and Direct3D callbacks share one operating-system thread. The image's reference at offset `0x3C` stayed null through repeated Pip-Boy openings. The user saw the text probe and ordinary controls, but no purple surface or green checkerboard.

The packaged DDS was then passed to `D3DXGetImageInfoFromFileInMemory` from 32-bit PowerShell using the installed `d3dx9_38.dll`, matching the game import. The call returned success and reported a 256x256, one-level `A8R8G8B8` 2D DDS. The engine's static XML filename path, rather than basic DDS parsing, is now the active failure. Maintained xNVSE headers expose `Tile::SetStringValue` at `0x00A01350`, and audited game call sites use it with the standard filename trait `0xFCC`. The setter's maintained decompilation compares the prior string and propagates whether the value changed. The next candidate clears and restores the private filename once on the game thread and does not write engine reference-counted fields directly.

That filename-refresh candidate did not change the result. Its log showed one successful clear-and-restore request, followed by the same null `TileImage + 0x3C` member. The `+0x40` member was non-null and stable. The user again saw only `PBVP UI LAYER` inside the normal usable Pip-Boy.

The earlier field interpretation was incomplete. JIP LN NVSE commit `5a30ac4356ea0e93b9ff357b5031b1e420240a4d` identifies `TileImage + 0x40` as `TileShaderProperty`, its exact vtable as `0x010B9D28`, and `TileShaderProperty::srcTexture` at offset `0x60`. Its `SetAlphaTexture` implementation separately replaces the texture at shader-property offset `0x64`, which confirms that `+0x60` is the ordinary source slot. Psycho's decompilation of the `TileImage` node-building routine at `0x00A1FD50` passes `TileImage + 0x40` to the engine render-object setup at `0x00439410`. The next implementation will verify those exact types and read the `+0x60` source. It will not reinterpret the shader property as a texture or write an engine object field.

The shader-source run verified that chain at runtime. The surface was a 256x256 `A8R8G8B8` texture in the managed pool, each checkerboard update took 21.0 to 25.8 microseconds, and the image appeared briefly during the Items-to-Data transition. It disappeared when the Data page finished drawing. The active Vanilla UI Plus `map_menu.xml` uses depths up to 8 for page content, 15 for headline cards, and 22 for the tab line. `PBVP_Root` had no explicit depth, so the next prefab places it at depth 10.

The [Pip-Flicks 3000 mod page](https://www.nexusmods.com/newvegas/mods/59173) describes an older background-frame player and explicitly offers version 0.2 and later as a modder's resource. Its visible behavior is tied to the Items screen: switching to Stats or Data stops the video, while extracted DDS frames replace the Pip-Boy background. This is evidence that engine-owned Pip-Boy textures can present video below native controls. It does not supply a reusable Data-page hierarchy, synchronization design, or redistributable media for PBVP. Nexus's file preview lists one ESP in the current reference package. No Pip-Flicks code or assets are copied into this project.

Giving `PBVP_Root` depth 10 did not change the visible result. Opening directly to Data showed no PBVP layer. Returning to Data exposed the surface and label briefly, then the completed page covered both. Each return produced a valid managed texture and a successful upload between 21.8 and 31.1 microseconds. The active Vanilla UI Plus XML assigns depth to drawable nodes, including a map-marker shadow that copies its parent image depth and subtracts `0.02`. The next candidate therefore gives the PBVP surface and probes explicit depths from 10 through 12 rather than relying on the container depth to propagate.

The explicit drawable-depth run displayed the green checkerboard with the black and text probes above it, and the user reported that the result looked good. The same session returned to MapMenu several times. Every return resolved the same reviewed surface profile and completed an upload between 22.6 and 30.7 microseconds. This establishes a usable engine-owned presentation path for the active fullscreen 1920x1080 VNV Extended UI stack. Input, reset behavior, base VNV, other display profiles, and DXVK still need independent evidence.

The next manual session passed keyboard and mouse input, five visual Alt+Tab cycles with Data open, and ten Pip-Boy close and reopen cycles. The log contained 19 successful uploads and no plugin errors. The texture address changed often enough to trigger another guarded upload, but the verified renderer recreation detour did not run. These Alt+Tab cycles therefore prove visual persistence of the engine-owned managed texture path, not release and recreation of plugin-owned default-pool resources. Controller input was not tested because no controller was connected.

The first placement adjustment anchored only the status strip inside `PBVP_VideoRect`. The user clarified that the checkerboard itself was expected to move. The next candidate replaces the rectangle's fixed `x = 80` and `y = 65` with a 12-unit lower-left inset derived from `PBVP_Root`. It does not alter the verified rectangle size, texture path, or draw depths.

The live bridge resolved that candidate at `42,276` through `602,591`, compared with `110,108` through `670,423` before the change. The container traits moved, but the user still described the checkerboard as top-left. The first analysis blamed the 560 by 315 footprint. The later compact run disproved that explanation because the child image stayed at the same origin after its size changed.

The user then clarified that the full checkerboard should clear the radio station list at the upper-left. The next diagnostic size is 320 by 180. With the same verified bottom anchor, this should lower the surface's top edge by 135 logical units. The generated engine texture remains 256 by 256 and will be stretched by the UI image for this placement test. Final decoded frames will use the configured aspect-fit or aspect-fill rules inside the selected playback viewport.

The compact run resolved `PBVP_VideoRect` at `42,411` through `362,591`, but the user saw a smaller checkerboard at the same upper-left location. The previous conclusion confused valid container traits with a child rendering origin. The active Vanilla UI Plus `MM_MainRect` and its positioned nested containers use `locus = 1`. The installed B42 Recoil prefab also assigns a locus to a window before placing its child images in local coordinates. The next candidate gives `PBVP_VideoRect` a locus so the surface at local `0,0` and both local status elements move with it.

The locus candidate moved the surface and status elements together, and the user accepted the lower-left position. The next candidate increased the rectangle to 384 by 216 at the user's request. This kept the same 16:9 shape and raised the top edge by only 36 logical units compared with the accepted 320 by 180 placement. The fresh run resolved `42,375` through `426,591`, uploaded the texture in 27.3 microseconds, and produced no plugin error. The user accepted the larger size.

The maintained [Psycho source](https://github.com/acidpointer/psycho) identifies `NiDX9Renderer::DisplayScene` at `0x00E75000`, `NiDX9Renderer::Recreate` at `0x00E73EB0`, and the Direct3D device at renderer offset `0x288` for Fallout NV 1.4.0.525. Its comments warn against rewriting a live device vtable. These addresses are evidence for the supported Steam runtime, not permission to patch blindly. The plugin must verify the live bytes and device interface before use.

Psycho's checked-in Ghidra audit records the `NiDX9Renderer::Recreate` entry as `SUB ESP,0x38; PUSH ESI; PUSH EDI; MOV EDI,ECX; MOV ECX,[EDI+0x884]; MOV EAX,[ECX]`. The corresponding relocation-free bytes are `83 EC 38 56 57 8B F9 8B 8F 84 08 00 00 8B 01 8B`. Its call-site audit also shows the two request values left on the stack before the call, which agrees with the maintained two-argument hook declaration. This exact 16-byte sequence is the first supported reset-hook signature. The plugin still logs and compares the decrypted live entry before installing MinHook.

The repository moved from the earlier `WallSoGB` owner to `acidpointer`; the old URL now returns 404. The current head inspected on August 9, 2026 is commit `85c96c1415b636051dff690036b510761de25d7a`. Its renderer audit proves that `NiDX9Renderer::Recreate` returns `0` on failure, `1` when the requested reset fails but the original presentation parameters recover, and `2` when the requested parameters succeed. The native caller treats either nonzero value as usable. PBVP now classifies only those documented values, requires a published device after either success, and leaves uploads disabled for every other result.

The moved `PBVP-deps` checkout was inspected again at commit `22b0030cd48d190a0cd9a0b4a945ebc2585b338e`. Its current Ghidra output identifies `0x004DC360` as the renderer recreation and window resize helper. The main loop checks byte `0x011C6FBB` at `0x0086EDF5`, calls the helper at `0x0086EE00` when the byte is nonzero, and clears it at `0x0086EE05`. The cross-reference report lists only that read and clear. The helper reads the active width and height globals, performs the surrounding engine notifications, calls `NiDX9Renderer::Recreate`, updates the window after success, and runs the post-recreation work.

This makes the deferred byte a narrower private test control than calling the renderer owner or Direct3D device directly. The diagnostic build verifies the full 23-byte gate before setting it, schedules only one request after the surface path has passed its thread and device checks, and leaves the option disabled in normal builds. This is still a candidate until the game log records the native recreation and successful texture reacquisition.

The first in-process run on August 9, 2026 produced the same 16 bytes from the patched local executable. The reset detour installed successfully with the full VNV Extended plugin stack loaded, including Fallout Shader Loader, Depth Resolve, and HD Pip-Boy. The game reached the main menu and exited normally. This confirms the local signature and an unoccupied hook entry. Actual device recreation remains a separate test.

[MinHook 1.3.4](https://github.com/TsudaKageyu/minhook/releases/tag/v1.3.4), commit `c3fcafdc10146beb5919319d0683e44e3c30d537`, is the selected reset-hook dependency for the Phase 1 spike. It supports x86, has a small API surface, and uses the BSD 2-Clause license. The plugin will use it only after its own signature and conflict checks accept the original `NiDX9Renderer::Recreate` entry point.

The current xNVSE source was inspected at commit `625db7e60007fbcceab755650ed479b5c337717c`. The installed 6.4.5 release corresponds to commit `fa1ab4d0d49516ebcc7a69e5d6e075976acca061`. The current Fallout Shader Loader source was inspected at commit `12fdf8d84a8f54763625091f37d538e0bbca988f`, and Psycho was inspected at commit `85c96c1415b636051dff690036b510761de25d7a`.

## Audio and timing

Microsoft's [`XAUDIO2_VOICE_STATE` documentation](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/ns-xaudio2-xaudio2_voice_state) defines `SamplesPlayed` as the number of decoded samples processed by a source voice. The [XAudio2 streaming guide](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-streaming-audio-data) recommends queued buffers and a worker thread for long audio streams, and warns that underruns create audible gaps.

[QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter) provides a high-resolution interval clock for silent media.

Inference to test: `SamplesPlayed`, adjusted for seek and pre-roll, can provide stable audio-led synchronization inside the paused Pip-Boy menu.

## Mod Organizer 2

MO2's [USVFS repository](https://github.com/ModOrganizer2/usvfs) describes a process-local virtual filesystem implemented through Windows API hooks. It supports x86 applications, but it also notes that dependent DLL loading can occur before virtualization becomes active.

Inference to test: media enumeration and the custom FFmpeg I/O bridge see files from an MO2 media mod, while private FFmpeg DLL loading remains deterministic. Dependency DLLs should be loaded from a verified private path rather than relying on VFS search order.

## Research still needed

- the exact Pip-Boy menu draw order and tile-to-backbuffer transform;
- native Direct3D 9 and DXVK reset behavior at the selected frame boundary;
- XAudio2 version behavior on the minimum Windows target;
- a reproducible minimal FFmpeg x86 build;
- codec patent review for binary distribution;
- permission and naming checks before a public mod page is created.
