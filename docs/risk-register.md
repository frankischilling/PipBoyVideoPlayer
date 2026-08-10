# Risk register

This register tracks failures that can change the architecture, support promise, schedule, or release decision. Probability and impact are planning estimates until the relevant spike produces measurements.

| Risk | Probability | Impact | Current response | Trigger or evidence | Phase owner |
| --- | --- | --- | --- | --- | --- |
| No safe Pip-Boy render point exists | Low after Phase 1 | Project-blocking | Keep the accepted engine-owned `TileImage` path and rerun its matrix if a later phase changes presentation ownership | Both frame-wide draw candidates failed layer order; the engine-owned image passed every Phase 1 UI profile | Phases 1 and 4 |
| UIO shader property has no usable source texture | Low after Phase 1 | Project-blocking | Keep the guarded `TileImage` texture-chain validation and refuse unknown object types | The reviewed shader source resolved to a managed Direct3D texture in every accepted in-game run | Phases 1 and 4 |
| Drawable depth differs across UI stacks | Low across tested stacks | High | Keep explicit depths 10 through 12 and repeat the matrix for any newly claimed Pip-Boy replacer | The accepted depth passed all four isolated UI profiles | Phases 1 and 5 |
| Direct3D device reset conflicts with the active renderer stack | Low for the accepted managed-resource path | High | Retain no Direct3D reference across callbacks, reject non-managed textures, and do not reinstall a reset detour | The synthetic reset froze inside native recreation. The accepted surface uses the managed pool, and PBVP owns no default-pool resource that needs a pre-reset callback | Phases 1 and 4 |
| Repeated focus changes trigger a graphics-driver crash | High in native fullscreen, low in tested windowed mode | High | Exclude unsafe fullscreen Alt+Tab from support unless the baseline is fixed; keep windowed and DXVK results separate | The native fullscreen control reproduced the pre-PBVP driver trace; the PBVP-enabled windowed run completed 50 cycles and shut down cleanly | Phase 1 |
| Isolated compatibility runs create game saves | Low after corrected Base and Extended checks | High | Keep MO2 local saves enabled, require section-aware Phase 1 overrides, refuse profiles that already contain saves, and quarantine unexpected test output | The corrected two-section guard left both tested isolated profiles empty after normal exit | Phases 1 and 6 |
| Running MO2 overwrites external test-profile edits | Medium | High | Refuse profile and display-case mutations while the selected instance's Mod Organizer process is running | MO2 wrote the newly installed Base save guard as disabled when the user switched profiles | Phase 1 |
| RTSS manual restoration changes the control-file hash | High when the UI is used | Low | Require all functional values to match the saved profile and allow only the managed `[Info]` timestamp to differ | Final cleanup restored the cap to zero and changed only nine timestamp bytes | Phase 1 |
| DXVK behaves differently from native D3D9 | Medium if support is added | High | Make no current DXVK claim; require a separate isolated installation and full matrix before adding one | The target VNV instance has no DXVK or root-management tool, and PBVP must not install a root proxy | Phase 6 |
| UI rectangle is not portable across Pip-Boy layouts | Low across tested sizes | High | Keep the accepted raised 384 by 216 locus-owned panel and repeat the matrix if a later phase changes its traits | The visible panel passed all target resolutions from 1280x720 through 3440x1440; the two larger windows were clipped by the physical monitor | Phases 1 and 5 |
| FFmpeg consumes too much 32-bit address space | Low for the measured Phase 2 limits | High | Keep the 1080p cap and bounded queues, and repeat the stable-baseline measurement after integrated audio | The x86 full-queue test added 65,880,064 private bytes; the delayed live run added 62,976,000 bytes after a zero-byte no-decode control | Phases 2 and 4 |
| FFmpeg runtime build drifts or exposes private paths | Low after the reproducible build check | High | Pin the source, signature, toolchain, static support archive, configure arguments, DLL hashes, imports, source epoch, and neutral file prefix | Two clean builds matched; the audit rejects changed files, imports, architecture, and local paths | Phases 2 and 6 |
| Damaged media crashes inside a codec | Low to medium | High | Use a minimal current FFmpeg build, allocation caps, corrupt fixtures, and outer error containment | Unhandled exception or memory corruption from a fixture | Phases 2 and 6 |
| Audio clock cannot be mapped cleanly after seek | Medium | High | Prototype XAudio2 sample-origin accounting before integration | Persistent clock jumps, stale samples, or drift after repeated seeks | Phase 3 |
| Audio underruns on slower systems | Medium | Medium | Measure several prebuffer depths and keep decode off game threads | Pops, silence gaps, or repeated buffer starvation | Phase 3 |
| Media files are invisible through MO2 custom I/O | Low after the isolated live test | High | Keep the tested direct-child virtual `Data` path and repeat with Unicode catalog names in Phase 5 | The custom AVIO worker decoded a 1080p fixture that existed only in the enabled MO2 media mod | Phases 2 and 5 |
| Private FFmpeg DLLs collide with other mod DLLs | Low | High | Use absolute paths, restricted DLL search, version checks, and a private directory | Wrong module path or mismatched library major version at runtime | Phases 2 and 6 |
| H.264 or AAC distribution terms block the planned package | Unknown | High | Review the exact build and intended distribution before public binaries | Counsel, host policy, or license review rejects the package | Phase 6 |
| Project license conflicts with contributions or third-party resources | Medium | High | Keep private original work all rights reserved; treat Pip-Flicks as a behavioral reference under its narrow quest-integration permission; wait for the owner to select a license | Code or assets are proposed for reuse without clear permission, or a public release is proposed before rights and terms are clear | Before publication |
| Game updates or patchers change the executable layout | Low | High | Verify runtime identity and fail closed on unknown builds | Signature mismatch or new executable hash | Every release |
| Rendering overhead harms large VNV lists | Medium | Medium | Phase 1 managed-texture uploads measured 22.70 to 51.20 microseconds; measure decoded-frame uploads and the 95th percentile during integrated playback | Upload or presentation work exceeds the acceptance budget | Phase 4 |
| Logs reveal private media information | Medium | Medium | Log basenames only, omit metadata comments and absolute paths by default | A normal log contains a user directory or embedded comment | Phases 4 and 6 |
| Debug symbols expose local build paths | High | High | Keep the full PDB private, apply equal-length cleanup to the stripped PDB's DBI stream, verify its identity and diagnostic streams with pinned LLVM tooling, write only the PDB filename into the DLL, and scan packaged binaries for absolute paths | The August 9 Phase 1 DLL contained its full local PDB path, and both full and raw stripped PDBs contained local paths | Phases 1 and 6 |
| Feature growth prevents a stable first release | High | Medium | Enforce the first-release scope and move additions to later work | A milestone adds subtitles, streaming, hardware decode, or world screens | All phases |

## Stop conditions

Implementation should stop and return to design if any of these occur:

- the presentation path cannot recover from device loss without affecting the game or another plugin;
- the only viable install method is a root `d3d9.dll` proxy;
- the UI needs full-file menu replacement in the required VNV profile;
- bounded playback cannot fit the agreed 32-bit memory budget;
- supported test media can execute an uncontained failure in the game process;
- the dependency package cannot meet its license obligations;
- synchronization cannot meet the 30-minute drift target without unbounded buffering.

A stop condition does not automatically cancel the project. It blocks the current design. The next decision entry should record whether the project changes its hook, narrows its support promise, moves decoding out of process, or ends development.

## Review cadence

Review this file at every phase exit and before a release candidate. Replace estimates with measurements as soon as they exist. Closed risks remain in the table with a link to the evidence or decision that closed them.
