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

The base VNV mod list keeps UIO but enables no visual UI replacer, so it provides the vanilla UI case without removing PBVP's required injection layer. The Extended list enables Vanilla UI Plus, Clean Vanilla HUD, and Pip-Boy UI Tweaks. Four save-free test profiles now isolate the base visual UI, Vanilla UI Plus alone, Vanilla UI Plus with Clean Vanilla HUD, and the full Extended stack. Creating the fourth profile left MO2's selected Extended profile unchanged.

The installed NVTF 10.61 configuration sets `iMaxFPSTolerance=300` and marks it as a value not to edit. It is not a general frame limiter. The maintained [Fallout NV Performance Guide](https://github.com/ModdingLinked/FalloutNV-Performance-Guide/blob/main/falloutnv.html) states that VSync is not a limiter and recommends a FalloutNV.exe profile in RivaTuner Statistics Server. RTSS is installed locally, and its existing FalloutNV.exe profile uses front-edge sync and passive waiting with the limit disabled. The Phase 1 matrix can set that profile to 30, 60, 90, or 120 FPS while retaining a restorable copy of its original contents.

The official [DXVK configuration reference](https://github.com/doitsujin/dxvk/blob/master/dxvk.conf) provides `d3d9.maxFrameRate`, but its own comments recommend an external limiter when one is available. Using the same RTSS profile for native Direct3D 9 and any later DXVK test avoids changing the limiter between graphics paths. This does not establish DXVK support. A separate isolated DXVK installation and in-game result are still required before making that claim.

A binary-content scan of the Phase 1 Release outputs found the absolute local PDB path inside `PipBoyVideoPlayer.dll`. The full PDB contains repository, build, compiler, and temporary paths, including the local Windows user profile. The earlier package audit only checked archive entries and ordinary text, so its clean-path conclusion was incorrect. Microsoft's [`/PDBALTPATH` reference](https://learn.microsoft.com/en-us/cpp/build/reference/pdbaltpath-use-alternate-pdb-path) confirms that a linker can write a path-independent PDB name into the DLL. Passing its `%_PDB%` placeholder through the CMake Visual Studio generator produced the literal CodeView name `%%%PipBoyVideoPlayer.pdb%%%`, while the Ninja generator produced the intended name. The project must pass the explicit stable filename to avoid generator-specific percent escaping. Microsoft's [`/PDBSTRIPPED` reference](https://learn.microsoft.com/en-us/cpp/build/reference/pdbstripped-strip-private-symbols) defines a second distributable PDB containing public symbols, contributing object records, and frame pointer optimization data without source line, type, local, or static symbol details.

The raw stripped PDB still contained 493 absolute `Module` and `ObjFile` fields. This follows Microsoft's documented requirement to retain contributing object records. The [official LLVM `llvm-pdbutil` reference](https://llvm.org/docs/CommandGuide/llvm-pdbutil.html) documents PDB inspection and conversion. An LLVM 22.1.0 YAML round trip removed the paths and retained all 4,114 public name, flag, and address tuples, but it dropped the FPO and section-contribution streams. It is not suitable for release symbols.

A format-aware test instead replaced the absolute names in the DBI logical stream with equal-length path-neutral names, then wrote that unchanged-size stream back to the same MSF blocks. It also found stale linker paths in unreferenced blocks and unused block tails. The cleanup zeros those areas while preserving the superblock, free-page maps, directory map, directory data, and every byte within a live stream length. On the current x86 Release PDB, it cleaned 493 DBI path fields, six unreferenced blocks, and 345,801 slack bytes. The result kept the original byte size, GUID, age, stripped status, public-symbol dump, FPO dump, and section-contribution dump. It contained no drive-qualified path and loaded through LLVM's DIA-compatible reader. This supports an automated packaging-only cleanup with strict post-write comparison. The full private PDB remains untouched.

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

The official release index identified FFmpeg 8.1.2 as the current stable release on August 10, 2026. The source archive was downloaded from `https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz`. Its SHA-256 is `464BEB5E7BF0C311E68B45AE2F04E9CC2AF88851ABB4082231742A74D97B524C`. GnuPG accepted the detached signature from release key fingerprint `FCF986EA15E6E293A5644F10B4322F04D67658D8`.

The minimal i686 configuration enables five shared libraries, the MOV demuxer, H.264 and AAC decoding and parsing, Windows threads, software scaling, and audio resampling. It disables programs, documentation, networking, protocols, encoders, muxers, filters, devices, hardware acceleration, and automatic external dependency detection. The configuration reports LGPL version 2.1 or later.

The first build imported `libwinpthread-1.dll` from `avutil-60.dll` for `clock_gettime32` and `nanosleep32`. A second profile linked only those functions from the pinned static winpthreads archive. The resulting five DLLs import only one another and Windows system libraries. The installed MSYS2 package is `mingw-w64-i686-winpthreads` version `13.0.0.r505.g7d006b2ea-1`, based on source commit `7d006b2ea4b17da66e515f4494b86cc1adb52f24`. Its terms are MIT and BSD-3-Clause-Clear.

A path scan then found absolute source names in FFmpeg's runtime diagnostic strings even though debug symbols were disabled. GCC file-prefix mapping replaces the local checkout with `pbvp` without adding an absolute path to FFmpeg's published configure string. Two clean builds with source epoch `1781662671` produced the same five DLL hashes. The audit confirmed i386 machine type, exact imports, exact hashes, and no repository, MSYS2, or user-profile path.

GCC emitted upstream warnings in AAC spectral band replication, frame-worker naming, VLC table analysis, and unused transform initialization functions. The build completed without a local FFmpeg patch. These warnings remain part of the dependency review record and will be checked again before an update.

The current five DLLs total 6,056,006 bytes on disk. This resolves build size, not runtime address-space cost. Phase 2 still needs a 1080p decode measurement inside the target VNV process.

The plugin loader builds the runtime directory from the xNVSE game path and the fixed private suffix `Data\NVSE\Plugins\PipBoyVideoPlayer\bin`. It passes each absolute DLL name to `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` and `LOAD_LIBRARY_SEARCH_SYSTEM32`. It then checks the loaded module path, resolves version and configuration functions by name, and compares the five exact library versions with the build headers. It also rejects a configuration missing the minimal libraries or containing GPL, nonfree, or version 3 switches.

The Win32 loader test rejects a relative directory, accepts the pinned runtime, verifies all five numeric versions, rejects a second load, unloads cleanly, and rejects an incomplete private directory. Packaging audits the source DLLs again, copies only the manifest entries, includes the verified license files, scans every staged file for local path markers, and requires the final DLL inventory to contain only the plugin and the five private libraries.

The custom AVIO bridge opens one direct-child media name through `CreateFileW`. It rejects relative or network roots, traversal, subdirectories, reparse points, empty files, and files beyond the configured 64-bit limit. Reads use an overlapped file handle and wait on either the read completion event or a manual cancellation event. A cancellation request signals the waiter and calls `CancelIoEx`, so shutdown does not depend on an ordinary synchronous read returning by itself.

The Win32 test opens a generated 128 KiB fixture with a Unicode name. It verifies buffered reads, an absolute seek, data after the seek, the size query, and cancellation. It also exercises the path, file type, file size, and AVIO buffer failures. This proves the Windows and FFmpeg callback contract outside the game. It does not prove that `CreateFileW` sees a file supplied only by the active MO2 virtual filesystem.

The bounded media tests on August 10, 2026 passed all 7 host tests and all 12 Win32 Release tests. The x86 run exercised the 64-bit to 32-bit size rejection used by the game build. Queue tests filled both count and byte capacity, advanced a seek generation while a producer was blocked, and closed a full queue while another producer was blocked. Both waiters woke with the expected refusal instead of publishing stale data or waiting through shutdown.

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

The first Base VNV visual check used native Direct3D 9 windowed mode at 1920x1080. The checkerboard appeared and the user reported that the game otherwise looked and ran correctly. The Base layout places its Local Map and World Map buttons beneath the panel's accepted Extended position, so the checkerboard covered them. The next shared candidate increases the bottom inset from 12 to 64 logical units, moving the full locus-owned panel up by 52 units without changing its size or horizontal anchor.

The isolated Base profile had no save directory before launch. MO2 created an empty directory during the session, and no save or co-save file appeared. Later inspection found the Phase 1 save guard disabled in the Base mod list. MO2 had remained open while the new local mod was installed and wrote its unchecked state when the user switched profiles. The empty result therefore cannot verify the guard. Profile and display-case scripts now refuse mutations while the selected instance's Mod Organizer process is running. They permit an empty local save directory but continue to reject any entry inside it.

The repaired follow-up ran with one enabled save-guard entry and no disabled duplicate. The raised checkerboard position cleared the Base map buttons, and the user reported that it looked good. After normal exit, the isolated save directory remained empty. This accepts the raised position for Base VNV. It verifies the guard only against the Base preset, whose main INI already has improved autosaves disabled.

Stewie Tweaks added one blank line to the small multi-INI guard during that run. The section and three zero-valued settings remained unchanged, and no new setting appeared. Guard verification now checks the INI structure and values instead of requiring identical whitespace. It still rejects extra sections, unknown settings, duplicate keys, missing keys, and nonzero values.

The full Extended regression accepted the raised panel visually. The user reported that it looked good with the normal Extended UI stack, so the 64-unit bottom inset is now shared by the tested Base and Extended profiles.

The same Extended exit disproved the guard assumption. One enabled guard entry and three zero-valued lines were present, but the isolated profile received a 3,628,509-byte named save and a 1,945-byte co-save. Their SHA-256 values are `DFAB52DC08E49EFB0F24641941A32FC114A5DA485E6AAB816810411C1078B7B3` and `B09B176849B72F49BA68E1565C1D1A2725C5230801A7612FD6EA57A2B70E47DF`. They were moved together to `build-host/quarantine/phase1-extended-raised-20260810-074245` and were not deleted.

Local inspection identified the configuration error. The guard put `bImprovedAutoSave`, `bSaveOnExitGame`, and `iAutoSaveTimer` under `[Tweaks]`. Stewie Tweaks 9.80 documents section-aware multi-INI replacement. Its active Extended INI stores the first key under `[Tweaks]` and the other two under `[Save Manager]`. The named save was written after the exit-game message, which matches the active Extended `bSaveOnExitGame = 1` setting. The guard must use both sections and pass another Extended exit before the save-isolation claim is restored.

The corrected Extended verification passed. The guard kept `bImprovedAutoSave` under `[Tweaks]` and moved `bSaveOnExitGame` and `iAutoSaveTimer` to `[Save Manager]`. It remained enabled exactly once, and the local save directory stayed empty after the user entered the Goodsprings test world and exited through the game menu. DiaMove recorded `Received exit game message` with no save-game message. The PBVP log resolved the accepted raised rectangle at `42,323` through `426,539`, completed two uploads from 24.30 to 28.70 microseconds, recorded no failure, and wrote both session summaries and the shutdown record. The strict Phase 1 log check passed.

The isolated Vanilla UI Plus profile then passed at native Direct3D 9 windowed 1920x1080 with VSync on and the RTSS limit unchanged. This profile keeps Vanilla UI Plus and disables Clean Vanilla HUD and both Pip-Boy UI Tweaks mods. The user reported that the checkerboard looked good and that mouse and keyboard input worked. The log resolved `42,323` through `426,539`, completed one 22.70-microsecond upload, recorded no failure, and shut down cleanly. The strict log check passed, and the isolated save directory remained empty.

The Extended profile without Pip-Boy UI Tweaks also passed at native Direct3D 9 windowed 1920x1080 with VSync on and the RTSS limit unchanged. It keeps Vanilla UI Plus and Clean Vanilla HUD while disabling both Pip-Boy UI Tweaks mods. The user reported that placement, visibility, mouse, and keyboard all looked and worked correctly. The log resolved the same accepted rectangle, completed one 26.30-microsecond upload, recorded no failure, and shut down cleanly. The strict log check passed, and the isolated save directory remained empty. All four isolated UI profiles now pass at this resolution.

The first capped resolution row used the full Extended profile at native Direct3D 9 windowed 1280x720, VSync off, and an RTSS limit of 30 FPS. The user reported that the panel and input looked good. The device log confirmed a 1280x720 backbuffer and immediate presentation interval. One checkerboard upload completed in 25.70 microseconds with no failure. Six three-second cadence samples measured 30.00 to 30.01 FPS, and the summary average was 30.00 FPS. The strict log check passed, shutdown was clean, and the isolated save directory remained empty.

The 1280x960, VSync-on, RTSS 60 FPS row needed two runs. The first passed visual placement and input, but focus or menu pauses produced cadence samples from 15.25 to 60.00 FPS and a 41.97 FPS average. The strict checker rejected it. During the timing-only retry, the user left the Data page focused and untouched. Eight samples measured 59.98 to 60.04 FPS with a 60.00 FPS average. The device log confirmed the 1280x960 backbuffer and a 1280x960 logical canvas. One upload completed in 26.30 microseconds, no upload failed, shutdown was clean, and the isolated save directory remained empty. The strict check passed.

The 2560x1440, VSync-off, RTSS 90 FPS row passed on the same full Extended profile. The active monitor remained 1920x1080, so the larger window was clipped at its right and bottom edges. The user reported that the visible panel looked good. The device log confirmed a 2560x1440 backbuffer. Eight cadence samples measured 89.99 to 90.04 FPS with a 90.01 FPS average. One upload completed in 25.70 microseconds, no upload failed, shutdown was clean, and the isolated save directory remained empty. The strict check passed.

The final capped resolution row used native windowed 3440x1440, VSync on, and RTSS at 120 FPS. The physical display again clipped the oversized window, and the user reported that the visible panel looked good. The device log confirmed a 3440x1440 backbuffer and a 2293.33x960 logical canvas. Eight cadence samples measured 119.89 to 120.19 FPS with a 120.01 FPS average. One upload completed in 24.90 microseconds, no upload failed, shutdown was clean, and the isolated save directory remained empty. The strict check passed. The native windowed matrix now covers every documented resolution and FPS cap, with the stated full-window limitation at the two sizes above the monitor's native resolution.

The maintained [Psycho source](https://github.com/acidpointer/psycho) identifies `NiDX9Renderer::DisplayScene` at `0x00E75000`, `NiDX9Renderer::Recreate` at `0x00E73EB0`, and the Direct3D device at renderer offset `0x288` for Fallout NV 1.4.0.525. Its comments warn against rewriting a live device vtable. These addresses are evidence for the supported Steam runtime, not permission to patch blindly. The plugin must verify the live bytes and device interface before use.

Psycho's checked-in Ghidra audit records the `NiDX9Renderer::Recreate` entry as `SUB ESP,0x38; PUSH ESI; PUSH EDI; MOV EDI,ECX; MOV ECX,[EDI+0x884]; MOV EAX,[ECX]`. The corresponding relocation-free bytes are `83 EC 38 56 57 8B F9 8B 8F 84 08 00 00 8B 01 8B`. Its call-site audit also shows the two request values left on the stack before the call, which agreed with the retired two-argument hook declaration. This remains research evidence, but PBVP no longer detours the function.

The repository moved from the earlier `WallSoGB` owner to `acidpointer`; the old URL now returns 404. The current head inspected on August 9, 2026 is commit `85c96c1415b636051dff690036b510761de25d7a`. Its renderer audit proves that `NiDX9Renderer::Recreate` returns `0` on failure, `1` when the requested reset fails but the original presentation parameters recover, and `2` when the requested parameters succeed. The native caller treats either nonzero value as usable. PBVP now classifies only those documented values, requires a published device after either success, and leaves uploads disabled for every other result.

The moved `PBVP-deps` checkout was inspected again at commit `22b0030cd48d190a0cd9a0b4a945ebc2585b338e`. Its current Ghidra output identifies `0x004DC360` as the renderer recreation and window resize helper. The main loop checks byte `0x011C6FBB` at `0x0086EDF5`, calls the helper at `0x0086EE00` when the byte is nonzero, and clears it at `0x0086EE05`. The cross-reference report lists only that read and clear. The helper requires nonzero requested width and height values at `0x011C70E0` and `0x011C70E4`, performs the surrounding engine notifications, calls `NiDX9Renderer::Recreate`, updates the window after success, and runs the post-recreation work. Fallout's active render width and height are separate globals at `0x0118947C` and `0x01189480`.

This makes the deferred byte a narrower private test control than calling the renderer owner or Direct3D device directly. The diagnostic build verifies the full 23-byte gate before setting it, schedules only one request after the surface path has passed its thread and device checks, and leaves the option disabled in normal builds. This is still a candidate until the game log records the native recreation and successful texture reacquisition.

The installed armed DLL is dated 10:19:51 PM on August 9, while the most recent game log ended at 9:38:52 PM. That log also lacks the armed-build banner and scheduled-request record compiled into the installed DLL. The timestamps and missing records prove that the installed recreation candidate has not run yet. The controlled acceptance command now requires both records in addition to a successful recreation and clean exit.

The armed VNV Extended run on August 10, 2026 disproved the assumption that writing the deferred byte is enough to exercise `NiDX9Renderer::Recreate`. The plugin logged the armed banner, verified hook installation, a successful first upload, and one request write. It recorded no detour entry or recreation result. About ten seconds later the MapMenu snapshot became unavailable, followed by a second upload to the available engine texture. The user reported that the checkerboard returned, but the log cannot attribute the intervening UI transition to the request. It also lacks the xNVSE exit message and renderer summaries, so it does not count as a controlled recreation pass.

The follow-up diagnostic reads the request byte on later frame callbacks without adding another hook. It logs the first pending observation, one consumption, an unexpected value, or a five-second timeout. Before writing, it also requires a non-null renderer singleton, nonzero requested width and height, and a working QueryPerformanceCounter. Consumption would prove that the exact reviewed gate completed its call-and-clear sequence. The existing renderer detour remains the separate proof that the helper reached `NiDX9Renderer::Recreate`. No direct reset call is justified by the current result.

The follow-up VNV Extended run kept the checkerboard visible for the full test and exited cleanly. The log recorded one successful upload at 28.50 microseconds, no upload or recreation failures, eight cadence samples from 131.22 to 144.21 FPS, and the orderly renderer summary. It also recorded that the helper preconditions were unavailable and confirmed that the request was not written. The successful device validation used the same renderer singleton address, so the remaining failed check is one or both requested-size globals. The next bounded diagnostic will report each read and value separately and will continue to refuse the request while either value is zero.

The per-field run resolved the remaining ambiguity. The renderer read succeeded with value `0x18467138`, and both requested-size reads succeeded with values of zero. The same session validated a 1920x1080 fullscreen backbuffer, kept the checkerboard visible, recorded one 30.00 microsecond upload with no failure, and exited cleanly. This proves that the earlier request was consumed by the main-loop gate before the recreation helper returned at its zero-size checks.

Psycho's maintained display implementation identifies `0x011C73DC` and `0x011C718C` as the `iSize W` and `iSize H` setting objects. The audited recreation helper copies the transient requested values into those setting objects before it calls `NiDX9Renderer::Recreate`. The active render dimensions remain separate at `0x0118947C` and `0x01189480`. The next private test may populate only the transient requested values from a matching active render size and backbuffer. It must start from zero values, enforce bounded dimensions, write the request last, and restore the original values after consumption, timeout, or shutdown.

The staged same-size test on August 10, 2026 invalidated that candidate. The plugin verified a 1920x1080 active size and backbuffer, staged 1920x1080, set the request, and entered the `NiDX9Renderer::Recreate` detour. Its last record says that PBVP cleared its transient surface state before recreation. The detour never recorded a return from the original engine call, request consumption, value restoration, a renderer summary, or orderly shutdown. The user reported a freeze. CrashLogger produced no crash dump, which is consistent with a hang rather than an exception. The known-safe normal DLL was restored after FalloutNV closed.

The checked-in Ghidra audit shows that the native recreation call traverses renderer-owned resources and registered reset callbacks before and after the Direct3D reset. The private request reproduced the consumer's visible width, height, and request inputs, but it did not reproduce an audited producer for those values or prove that all required transition state was present. The freeze therefore rejects synthetic deferred recreation as a PBVP test method. It does not identify a specific game component or graphics plugin as the cause. PBVP must use a naturally initiated display transition or validate its engine-owned managed texture without forcing a reset.

The accepted engine texture profile reports pool value `1`, the Direct3D 9 value for `D3DPOOL_MANAGED`. Microsoft's Direct3D 9 pool contract says managed resources survive device loss through runtime-managed copying, while default-pool resources must be released before `Reset`. PBVP does not create this texture and retains no COM reference after an upload. Its stored device and surface values are non-owning identities used only to decide when validation and upload must run again. These facts support removing the reset detour, provided the runtime path rejects every texture pool except `D3DPOOL_MANAGED`.

The first hook-free run on August 10, 2026 validated that replacement in the active VNV Extended profile at fullscreen 1920x1080 through native Direct3D 9. The user reported no freeze, a correct panel, and a successful Alt+Tab return. The log identified the 256x256 `A8R8G8B8` surface in `D3DPOOL_MANAGED`, recorded one successful upload at 26.30 microseconds and no failures, then wrote its renderer summary and shutdown record. The strict log check passed. No device change was observed, so this is evidence for the hook-free managed-texture presentation path and clean lifecycle, not proof of a natural Direct3D recreation.

The next hook-free stress run lasted almost ten minutes. The user estimated that they completed more than 44 Alt+Tab cycles but did not record the exact count. The checkerboard remained usable. The log recorded 21,933 frame callbacks, ten successful surface uploads from 24.60 to 47.70 microseconds, no upload failure, and an orderly shutdown. The strict log check passed. This supports the presentation path under repeated focus changes, but it cannot satisfy the exact 50-cycle acceptance target. A separate foreground-process counter now measures loss and return cycles without installing a hook or changing FalloutNV.

The first counter-measured run crashed after four completed focus cycles. The result file recorded 1,254 foreground samples, away times from 1.53 to 5.42 seconds, an incomplete final loss, and a process exit before the 50-cycle target. PBVP completed three managed-texture uploads from 45.50 to 50.20 microseconds without logging a failed upload. Its orderly shutdown records are absent because the process terminated. CrashLogger placed the access violation on an NVIDIA Direct3D worker thread and showed no PBVP stack frame.

A March 14, 2026 crash report predates PBVP and uses the same NVIDIA driver version, 32.0.15.8142. Its `nvd3dum` call trace ends with the same offsets `0x1134C9E6`, `0x114CD94A`, `0x1144BBD0`, `0x11457097`, and `0x1177FCA3` as the measured PBVP run. The newer report adds one driver frame above a nearby `0x113C8E36` frame. This is evidence for an existing driver failure pattern, but one historical report cannot exclude PBVP as a trigger. A matched test with the PBVP development mod disabled is required before the renderer changes.

The requested PBVP-enabled retry crashed after nine completed focus cycles. Its plugin log contains one 27.90 microsecond upload, no upload failure, and no orderly shutdown. This time the driver call trace matches all six `nvd3dum` frames in the March pre-PBVP report exactly, from `0x113C8CAA` through `0x1177FCA3`. Reproducing a historical driver signature twice while PBVP is loaded still does not isolate the trigger. The matched disabled control is now required before another enabled run or renderer change.

The disabled control reached all 50 completed focus cycles, with 1,898 samples and away times from 0.77 to 4.49 seconds. FalloutNV crashed one second after the target with the same six-frame `nvd3dum` trace. The PBVP log timestamp and SHA-256 did not change, and the crash module list omits PBVP. This proves that the active native fullscreen VNV stack reproduces the driver failure without PBVP. It does not turn the control into a clean pass because the process crashed immediately after the measured target.

The maintained [FNV Performance Guide](https://performance.moddinglinked.com/falloutnv.html) describes legacy fullscreen mode as having slow and unstable Alt+Tab behavior. It documents that enabling NVTF's default-pool texture option breaks fullscreen Alt+Tab, while windowed mode and DXVK avoid that device-loss case. The installed VNV 16.2.26 NVTF INI has `bModifyDirectXBehavior = 1`, `bUseFlipModel = 0`, and `bUseDefaultPoolForTextures = 0`. That differs from the guide's pool-change failure case, so it cannot be cited as the direct cause. The broader fullscreen warning and the clean variable isolation justify a native windowed test before any PBVP architecture change.

The matched native windowed run completed the exact 50-cycle target with PBVP enabled. The counter recorded 1,478 samples, no incomplete focus loss, and away times from 110.97 to 5,247.61 milliseconds. The plugin completed three managed-surface uploads from 25.00 to 51.20 microseconds, reported no upload failure, passed the strict log check, and shut down normally. CrashLogger produced no new report. This separates a clean windowed result from the native fullscreen baseline failure without changing the PBVP renderer.

That run also exposed a test-profile side effect. The active Extended Stewie Tweaks INI enables `bImprovedAutoSave`, `bSaveOnExitGame`, and a 600-second autosave timer. Exit created one new save and co-save in the isolated profile. The files were moved to the ignored test quarantine and were not deleted. Stewie Tweaks 9.80 documents that `bMultiINISupport = 1` lets files under `NVSE\Plugins\Tweaks\INIs` override the main INI. The installed VNV INIs enable that setting. A separate local Phase 1 mod now uses this supported mechanism to set only the three save-related values to zero in PBVP test profiles. The shared VNV mods and normal profiles remain unchanged, and the override is excluded from release data.

The first in-process run on August 9, 2026 produced the same 16 bytes from the patched local executable. The reset detour installed successfully with the full VNV Extended plugin stack loaded, including Fallout Shader Loader, Depth Resolve, and HD Pip-Boy. The game reached the main menu and exited normally. This confirms the local signature and an unoccupied hook entry. Actual device recreation remains a separate test.

[MinHook 1.3.4](https://github.com/TsudaKageyu/minhook/releases/tag/v1.3.4), commit `c3fcafdc10146beb5919319d0683e44e3c30d537`, was used for the retired reset-hook spike. The managed-texture design removed MinHook from the build and package.

The current xNVSE source was inspected at commit `625db7e60007fbcceab755650ed479b5c337717c`. The installed 6.4.5 release corresponds to commit `fa1ab4d0d49516ebcc7a69e5d6e075976acca061`. The current Fallout Shader Loader source was inspected at commit `12fdf8d84a8f54763625091f37d538e0bbca988f`, and Psycho was inspected at commit `85c96c1415b636051dff690036b510761de25d7a`.

The final local graphics inventory found no DXVK DLL, DXVK configuration, DXVK mod, Root Builder plugin, or enabled root-management mod in the target VNV instance. Installing DXVK directly would require the root `d3d9.dll` path excluded by the project architecture. Phase 1 therefore makes no DXVK support claim.

The native windowed focus path did not expose a repeatable device recreation, and the maintained executable audit found no verified safe in-process display toggle. The accepted resource contract uses only the engine-owned `D3DPOOL_MANAGED` texture, releases every temporary COM reference inside the callback, and treats a changed device or surface identity as new. Together with the clean 50-cycle windowed run, this closes the Phase 1 lifecycle gate without claiming that a natural Direct3D Reset occurred.

The final RTSS cleanup restored `Limit=0` and `LimitDenominator=1`, then closed RTSS. A line and byte comparison against the saved control copy found one difference: RTSS replaced the `[Info]` timestamp. Both files are 1,394 bytes, and the nine differing bytes belong only to that timestamp. Manual UI restoration therefore needs a functional comparison instead of an exact hash comparison.

## Audio and timing

Microsoft's [`XAUDIO2_VOICE_STATE` documentation](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/ns-xaudio2-xaudio2_voice_state) defines `SamplesPlayed` as the number of decoded samples processed by a source voice. The [XAudio2 streaming guide](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-streaming-audio-data) recommends queued buffers and a worker thread for long audio streams, and warns that underruns create audible gaps.

[QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter) provides a high-resolution interval clock for silent media.

Inference to test: `SamplesPlayed`, adjusted for seek and pre-roll, can provide stable audio-led synchronization inside the paused Pip-Boy menu.

## Mod Organizer 2

MO2's [USVFS repository](https://github.com/ModOrganizer2/usvfs) describes a process-local virtual filesystem implemented through Windows API hooks. It supports x86 applications, but it also notes that dependent DLL loading can occur before virtualization becomes active.

Inference to test: media enumeration and the custom FFmpeg I/O bridge see files from an MO2 media mod, while private FFmpeg DLL loading remains deterministic. Dependency DLLs should be loaded from a verified private path rather than relying on VFS search order.

## Research still needed

- DXVK behavior at the selected frame boundary if support is later claimed;
- XAudio2 version behavior on the minimum Windows target;
- a reproducible minimal FFmpeg x86 build;
- codec patent review for binary distribution;
- permission and naming checks before a public mod page is created.
