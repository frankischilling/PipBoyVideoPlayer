# Research notes

Research date: August 9, 2026

These notes separate facts used by the plan from design inferences that still need an implementation test.

## Prior art

[Pip-Flicks 3000](https://www.nexusmods.com/newvegas/mods/59173) proves that a Pip-Boy page can present moving pictures. It swaps DDS images several times per second and plays separately extracted audio. Its page documents a 60 FPS synchronization requirement and a manual MP4-to-frames workflow. This project does not plan to reuse its scripts or assets. It addresses a different technical path: native decoding with an audio clock.

## xNVSE and VNV

The [xNVSE releases](https://github.com/xNVSE/NVSE/releases/) provide the current loader and plugin platform. The release page recommends Viva New Vegas for a stable setup.

The current [Viva New Vegas utilities guide](https://vivanewvegas.moddinglinked.com/utilities.html) includes xNVSE, JIP LN, JohnnyGuitar, ShowOff, NVTF, and UIO. It describes NVTF as supporting frame rates up to roughly 120 FPS and UIO as the UI composition layer.

The local VNV installation was inspected on August 9, 2026. The selected profile is Viva New Vegas Extended. The installation also contains the base Viva New Vegas profile. No game, profile, mod, or save files were changed during inspection.

The installed versions relevant to the plugin are xNVSE 6.4.5, JIP LN 57.30, JohnnyGuitar 5.20, ShowOff 1.82, NVTF 10.61, UIO 2.30, Vanilla UI Plus 9.48, Clean Vanilla Hud f1.01, Pip-Boy UI Tweaks 5.2.1, and Fallout Shader Loader 1.32.

The active `FalloutNV.exe` reports runtime 1.4.0.525 and has SHA-256 `518C87F58A6C4D9826E9EF8FBB7F4213882FA70822675610D45AEA2464502A57`. The retained backup executable has SHA-256 `3A87F92F011E5DC9179DDF733CF08BE2B39EA6E5B7A8A9E3A9A72DAFCC1B104D`. The active executable is large-address aware and contains code changes beyond the PE header. Runtime checks must therefore validate every patched function before installation. The game root contains no `d3d9.dll` or `dxgi.dll`, so the inspected profiles currently use native Direct3D 9.

Inference to test: an ESP-less native plugin plus UIO prefab should fit the VNV install model without a load-order patch.

## UIO

The [UIO documentation](https://www.nexusmods.com/newvegas/mods/57174) says it registers UI extensions before menus load and avoids modifying game UI files. Its public registration format can inject a prefab into a named game menu or XML component.

Inference to test: a prefab can expose a stable rectangle and controls inside the Pip-Boy Data flow across the required UI variants. UIO solves XML composition, but it does not by itself solve native texture placement or draw order.

The installed Pip-Boy UI Tweaks registration files confirm UIO's target syntax. A line such as `PipBoyVideoPlayer\Player.xml::MapMenu::MM_MainRect` injects the prefab below the named tile without replacing the complete menu. The injected prefab will expose `PBVP_Root` and `PBVP_VideoRect` for native lookup.

## FFmpeg

The [FFmpeg library documentation](https://www.ffmpeg.org/doxygen/trunk/index.html) assigns container I/O and demuxing to `libavformat`, decoding to `libavcodec`, scaling and pixel conversion to `libswscale`, and audio conversion to `libswresample`. Library major versions may include incompatible API changes, so the build must pin and validate a specific set.

The [FFmpeg README](https://ffmpeg.org/doxygen/8.0/md_README.html) states that the codebase is mainly LGPL with optional GPL components. The actual configuration controls the resulting obligations. The package plan therefore excludes optional components until their licenses and need are reviewed.

Inference to test: a minimal x86 build with MOV/MP4, H.264, and AAC support is small enough in code and address-space cost for the target VNV profile.

## Direct3D 9

Microsoft's [`IDirect3DTexture9::LockRect` documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dtexture9-lockrect) documents `D3DLOCK_DISCARD` for dynamic texture updates and notes the restrictions on default-pool textures. The [`D3DPOOL` documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpool) states that default-pool resources must be released before `Reset` and recreated afterward.

Inference to test: a dynamic A8R8G8B8 default-pool texture is the lowest-cost reliable upload path for the chosen hook. The spike must also compare a system-memory staging texture plus `UpdateTexture` if direct locks stall or behave poorly through DXVK.

The Fallout NV performance guide discusses [DXVK and display modes](https://performance.moddinglinked.com/falloutnv.html). DXVK support is therefore part of the planned test matrix, not an assumption.

Microsoft's Direct3D 9 documentation confirms that default-pool resources and state blocks must be released before `Reset`. `D3DSBT_ALL` captures vertex and pixel state, textures, streams, indices, viewport, scissor state, transforms, and material. Render targets are not part of that documented list, so the plugin will retain and restore render target and depth surface references explicitly.

The xNVSE 6.4.5 source dispatches `kMessage_OnFramePresent` on the render thread immediately before the engine's frame presentation call. The message is sent for both the main loop and loading screens, with a loading-screen flag in the message data. This provides a presentation boundary without patching `Present`, `EndScene`, or a live Direct3D device vtable.

The maintained [Psycho source](https://github.com/WallSoGB/psycho) identifies `NiDX9Renderer::DisplayScene` at `0x00E75000`, `NiDX9Renderer::Recreate` at `0x00E73EB0`, and the Direct3D device at renderer offset `0x288` for Fallout NV 1.4.0.525. Its comments warn against rewriting a live device vtable. These addresses are evidence for the supported Steam runtime, not permission to patch blindly. The plugin must verify the live bytes and device interface before use.

Psycho's checked-in Ghidra audit records the `NiDX9Renderer::Recreate` entry as `SUB ESP,0x38; PUSH ESI; PUSH EDI; MOV EDI,ECX; MOV ECX,[EDI+0x884]; MOV EAX,[ECX]`. The corresponding relocation-free bytes are `83 EC 38 56 57 8B F9 8B 8F 84 08 00 00 8B 01 8B`. Its call-site audit also shows the two request values left on the stack before the call, which agrees with the maintained two-argument hook declaration. This exact 16-byte sequence is the first supported reset-hook signature. The plugin still logs and compares the decrypted live entry before installing MinHook.

[MinHook 1.3.4](https://github.com/TsudaKageyu/minhook/releases/tag/v1.3.4), commit `c3fcafdc10146beb5919319d0683e44e3c30d537`, is the selected reset-hook dependency for the Phase 1 spike. It supports x86, has a small API surface, and uses the BSD 2-Clause license. The plugin will use it only after its own signature and conflict checks accept the original `NiDX9Renderer::Recreate` entry point.

The current xNVSE source was inspected at commit `625db7e60007fbcceab755650ed479b5c337717c`. The installed 6.4.5 release corresponds to commit `fa1ab4d0d49516ebcc7a69e5d6e075976acca061`. The current Fallout Shader Loader source was inspected at commit `12fdf8d84a8f54763625091f37d538e0bbca988f`, and Psycho was inspected at commit `22b0030cd48d190a0cd9a0b4a945ebc2585b338e`.

## Audio and timing

Microsoft's [`XAUDIO2_VOICE_STATE` documentation](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/ns-xaudio2-xaudio2_voice_state) defines `SamplesPlayed` as the number of decoded samples processed by a source voice. The [XAudio2 streaming guide](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-streaming-audio-data) recommends queued buffers and a worker thread for long audio streams, and warns that underruns create audible gaps.

[QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter) provides a high-resolution interval clock for silent media.

Inference to test: `SamplesPlayed`, adjusted for seek and pre-roll, can provide stable audio-led synchronization inside the paused Pip-Boy menu.

## Mod Organizer 2

MO2's [USVFS repository](https://github.com/ModOrganizer2/usvfs) describes a process-local virtual filesystem implemented through Windows API hooks. It supports x86 applications, but it also notes that dependent DLL loading can occur before virtualization becomes active.

Inference to test: media enumeration and the custom FFmpeg I/O bridge see files from an MO2 media mod, while private FFmpeg DLL loading remains deterministic. Dependency DLLs should be loaded from a verified private path rather than relying on VFS search order.

## Research still needed

- the exact Pip-Boy menu draw order and tile-to-backbuffer transform;
- confirmation that the patched local executable exposes the reviewed `NiDX9Renderer::Recreate` prologue after runtime decryption;
- native Direct3D 9 and DXVK reset behavior at the selected frame boundary;
- XAudio2 version behavior on the minimum Windows target;
- a reproducible minimal FFmpeg x86 build;
- codec patent review for binary distribution;
- permission and naming checks before a public mod page is created.
