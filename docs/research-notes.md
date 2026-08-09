# Research notes

Research date: August 9, 2026

These notes separate facts used by the plan from design inferences that still need an implementation test.

## Prior art

[Pip-Flicks 3000](https://www.nexusmods.com/newvegas/mods/59173) proves that a Pip-Boy page can present moving pictures. It swaps DDS images several times per second and plays separately extracted audio. Its page documents a 60 FPS synchronization requirement and a manual MP4-to-frames workflow. This project does not plan to reuse its scripts or assets. It addresses a different technical path: native decoding with an audio clock.

## xNVSE and VNV

The [xNVSE releases](https://github.com/xNVSE/NVSE/releases/) provide the current loader and plugin platform. The release page recommends Viva New Vegas for a stable setup.

The current [Viva New Vegas utilities guide](https://vivanewvegas.moddinglinked.com/utilities.html) includes xNVSE, JIP LN, JohnnyGuitar, ShowOff, NVTF, and UIO. It describes NVTF as supporting frame rates up to roughly 120 FPS and UIO as the UI composition layer.

The local VNV Extended installation was inspected for a reproducible planning snapshot. Its relevant versions are recorded in the compatibility plan. No files in that installation were modified.

Inference to test: an ESP-less native plugin plus UIO prefab should fit the VNV install model without a load-order patch.

## UIO

The [UIO documentation](https://www.nexusmods.com/newvegas/mods/57174) says it registers UI extensions before menus load and avoids modifying game UI files. Its public registration format can inject a prefab into a named game menu or XML component.

Inference to test: a prefab can expose a stable rectangle and controls inside the Pip-Boy Data flow across the required UI variants. UIO solves XML composition, but it does not by itself solve native texture placement or draw order.

## FFmpeg

The [FFmpeg library documentation](https://www.ffmpeg.org/doxygen/trunk/index.html) assigns container I/O and demuxing to `libavformat`, decoding to `libavcodec`, scaling and pixel conversion to `libswscale`, and audio conversion to `libswresample`. Library major versions may include incompatible API changes, so the build must pin and validate a specific set.

The [FFmpeg README](https://ffmpeg.org/doxygen/8.0/md_README.html) states that the codebase is mainly LGPL with optional GPL components. The actual configuration controls the resulting obligations. The package plan therefore excludes optional components until their licenses and need are reviewed.

Inference to test: a minimal x86 build with MOV/MP4, H.264, and AAC support is small enough in code and address-space cost for the target VNV profile.

## Direct3D 9

Microsoft's [`IDirect3DTexture9::LockRect` documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dtexture9-lockrect) documents `D3DLOCK_DISCARD` for dynamic texture updates and notes the restrictions on default-pool textures. The [`D3DPOOL` documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpool) states that default-pool resources must be released before `Reset` and recreated afterward.

Inference to test: a dynamic A8R8G8B8 default-pool texture is the lowest-cost reliable upload path for the chosen hook. The spike must also compare a system-memory staging texture plus `UpdateTexture` if direct locks stall or behave poorly through DXVK.

The Fallout NV performance guide discusses [DXVK and display modes](https://performance.moddinglinked.com/falloutnv.html). DXVK support is therefore part of the planned test matrix, not an assumption.

## Audio and timing

Microsoft's [`XAUDIO2_VOICE_STATE` documentation](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/ns-xaudio2-xaudio2_voice_state) defines `SamplesPlayed` as the number of decoded samples processed by a source voice. The [XAudio2 streaming guide](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-streaming-audio-data) recommends queued buffers and a worker thread for long audio streams, and warns that underruns create audible gaps.

[QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter) provides a high-resolution interval clock for silent media.

Inference to test: `SamplesPlayed`, adjusted for seek and pre-roll, can provide stable audio-led synchronization inside the paused Pip-Boy menu.

## Mod Organizer 2

MO2's [USVFS repository](https://github.com/ModOrganizer2/usvfs) describes a process-local virtual filesystem implemented through Windows API hooks. It supports x86 applications, but it also notes that dependent DLL loading can occur before virtualization becomes active.

Inference to test: media enumeration and the custom FFmpeg I/O bridge see files from an MO2 media mod, while private FFmpeg DLL loading remains deterministic. Dependency DLLs should be loaded from a verified private path rather than relying on VFS search order.

## Research still needed

- current open-source xNVSE plugins that own Direct3D 9 resources and their hook-conflict behavior;
- the exact Pip-Boy menu render path and tile-to-backbuffer transform;
- xNVSE lifecycle notifications appropriate for load, main menu, and exit;
- safe game-thread menu access from a native plugin;
- XAudio2 version behavior on the minimum Windows target;
- a reproducible minimal FFmpeg x86 build;
- codec patent review for binary distribution;
- permission and naming checks before a public mod page is created.
