# Risk register

This register tracks failures that can change the architecture, support promise, schedule, or release decision. Probability and impact are planning estimates until the relevant spike produces measurements.

| Risk | Probability | Impact | Current response | Trigger or evidence | Phase owner |
| --- | --- | --- | --- | --- | --- |
| No safe Pip-Boy render point exists | High | Project-blocking | Both frame-wide draw candidates failed layer order; use a private engine-owned `TileImage` and validate its native texture chain before updating pixels | The final-present and normal-frame candidates both showed the checkerboard above the intended XML probes | Phase 1 |
| UIO shader property has no usable source texture | High | Project-blocking | Remove the ineffective filename refresh; verify `TileImage + 0x40` as the exact `TileShaderProperty` type, then verify its `+0x60` source as `NiSourceTexture` before reading renderer data | Two runs left `TileImage + 0x3C` null, while the second recorded a stable non-null `+0x40` object before and after the filename refresh | Phase 1 |
| UIO surface is covered by MapMenu page content | Medium | High | Set the injected root to depth 10, above active page content through depth 8 and below headline and tab controls at depths 15 and 22 | A verified checkerboard upload appeared during the Items-to-Data transition and disappeared when the page finished drawing | Phase 1 |
| Direct3D device reset conflicts with another plugin | Low to medium | High | The Extended profile accepted the verified hook, and five Alt+Tab switches preserved the draw without invoking recreation; next force an actual display recreation and verify default-pool release | Failed Reset, black texture after a display transition, or a crash during another hook's reset work | Phase 1 |
| DXVK behaves differently from native D3D9 | Medium | High | Treat it as a separate graphics target and keep the upload path replaceable | Different device lifetime, texture lock failure, or state restoration bug | Phase 1 |
| UI rectangle is not portable across Pip-Boy layouts | Medium to high | High | Extended at fullscreen 1920x1080 is verified; test every required layout, resolution, and scale before narrowing the support list | The first visible draw resolved the named tile through a 1706.67x960 logical canvas | Phases 1 and 5 |
| FFmpeg consumes too much 32-bit address space | Medium | High | Cap dimensions, keep queues small, disable unused features, and measure virtual memory | Private bytes or address space grows past the agreed budget | Phase 2 |
| Damaged media crashes inside a codec | Low to medium | High | Use a minimal current FFmpeg build, allocation caps, corrupt fixtures, and outer error containment | Unhandled exception or memory corruption from a fixture | Phases 2 and 6 |
| Audio clock cannot be mapped cleanly after seek | Medium | High | Prototype XAudio2 sample-origin accounting before integration | Persistent clock jumps, stale samples, or drift after repeated seeks | Phase 3 |
| Audio underruns on slower systems | Medium | Medium | Measure several prebuffer depths and keep decode off game threads | Pops, silence gaps, or repeated buffer starvation | Phase 3 |
| Media files are invisible through MO2 custom I/O | Low to medium | High | Test Win32 file handles through the active USVFS session before locking the path contract | Catalog sees files but FFmpeg bridge cannot open them | Phase 2 |
| Private FFmpeg DLLs collide with other mod DLLs | Low | High | Use absolute paths, restricted DLL search, version checks, and a private directory | Wrong module path or mismatched library major version at runtime | Phases 2 and 6 |
| H.264 or AAC distribution terms block the planned package | Unknown | High | Review the exact build and intended distribution before public binaries | Counsel, host policy, or license review rejects the package | Phase 6 |
| Project license conflicts with contributions or third-party resources | Medium | High | Keep private original work all rights reserved; treat Pip-Flicks as a behavioral reference under its narrow quest-integration permission; wait for the owner to select a license | Code or assets are proposed for reuse without clear permission, or a public release is proposed before rights and terms are clear | Before publication |
| Game updates or patchers change the executable layout | Low | High | Verify runtime identity and fail closed on unknown builds | Signature mismatch or new executable hash | Every release |
| Rendering overhead harms large VNV lists | Medium | Medium | The first generated draw averaged 91.72 microseconds over 300 frames; measure real 1080p uploads and the 95th percentile | Upload or state preservation exceeds the acceptance budget | Phases 1 and 4 |
| Logs reveal private media information | Medium | Medium | Log basenames only, omit metadata comments and absolute paths by default | A normal log contains a user directory or embedded comment | Phases 4 and 6 |
| Feature growth prevents a stable first release | High | Medium | Enforce the first-release scope and move additions to later work | A milestone adds subtitles, streaming, hardware decode, or world screens | All phases |

## Stop conditions

Implementation should stop and return to design if any of these occur:

- the render hook cannot recover from device loss without affecting the game or another plugin;
- the only viable install method is a root `d3d9.dll` proxy;
- the UI needs full-file menu replacement in the required VNV profile;
- bounded playback cannot fit the agreed 32-bit memory budget;
- supported test media can execute an uncontained failure in the game process;
- the dependency package cannot meet its license obligations;
- synchronization cannot meet the 30-minute drift target without unbounded buffering.

A stop condition does not automatically cancel the project. It blocks the current design. The next decision entry should record whether the project changes its hook, narrows its support promise, moves decoding out of process, or ends development.

## Review cadence

Review this file at every phase exit and before a release candidate. Replace estimates with measurements as soon as they exist. Closed risks remain in the table with a link to the evidence or decision that closed them.
