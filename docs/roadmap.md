# Roadmap

Work is organized around evidence gates. A phase does not begin because the calendar says so; it begins when the previous phase answers its risky questions.

## Phase 0: design repository

Deliverables:

- product scope and first-release boundary;
- architecture and thread ownership;
- playback, UI, file, and configuration contracts;
- dependency and licensing plan;
- VNV compatibility matrix;
- test fixtures and acceptance targets;
- open questions and decision log.

Exit criteria:

- documentation has no known contradiction about ownership or lifecycle;
- each external dependency has a reason and license task;
- the first implementation spike has a narrow pass or fail result;
- unresolved choices are recorded instead of hidden in prose.

## Phase 1: render and UI feasibility

This phase does not decode media.

Tasks:

1. Build the smallest x86 xNVSE plugin that loads and logs lifecycle events.
2. Review candidate Direct3D 9 presentation points from the current executable, xNVSE, and maintained open-source plugins.
3. Inject a UIO prefab that exposes a colored placeholder rectangle in the Videos page.
4. Present a generated checkerboard through a safe render-thread boundary.
5. Avoid changing game render state, or preserve and restore every state value that the chosen path changes.
6. Prove resource ownership across device loss and `Reset` without forcing a renderer transition.
7. Test native D3D9, DXVK, base VNV, and VNV Extended.
8. Document the chosen callback boundary, object validation, ordering, and conflict behavior.

Exit criteria:

- the texture appears at the correct layer and coordinates in every required resolution;
- fifty Alt+Tab cycles pass;
- no visible state leaks outside the rectangle;
- render cost is measured;
- the presentation path installs no executable or device-vtable hook that can conflict with another plugin;
- an unknown device or texture profile causes a safe refusal;
- the design has a credible path for each claimed Pip-Boy layout.

If this phase fails, stop. Revisit the presentation method before adding FFmpeg.

## Phase 2: standalone media core

Build and test the media pipeline outside the game process first.

Tasks:

- produce the minimal x86 FFmpeg build;
- open files through the planned custom I/O layer;
- select streams and normalize timestamps;
- decode H.264 frames to BGRA;
- decode and resample AAC to the selected PCM format;
- implement bounded queues, generation-based seeks, cancellation, and drain;
- run corrupt-file and resource-limit tests;
- record memory and CPU measurements.

Exit criteria:

- every required media fixture produces the expected frame and sample counts;
- variable frame rate timestamps remain ordered;
- seek generations cannot leak stale buffers;
- corrupt and oversized inputs fail cleanly;
- the build and license inventory are reproducible.

## Phase 3: audio and clock integration

Tasks:

- stream PCM through XAudio2;
- measure buffer latency and underruns;
- derive media time from `SamplesPlayed`;
- implement pause, resume, mute, volume, end of stream, and audio-device failure;
- use QPC for silent video;
- test 44.1 kHz, 48 kHz, mono, stereo, and downmix cases.

Exit criteria:

- the audio clock survives pause and seek without discontinuity;
- queued memory stays bounded;
- no callback performs blocking work or touches game objects;
- the selected buffering threshold passes the slow reference system.

## Phase 4: integrated playback

Tasks:

- connect decoder queues to the Direct3D renderer;
- implement audio-led frame selection and late-frame dropping;
- add player state to the Pip-Boy UI;
- stop or pause on every documented menu and game lifecycle event;
- add user-facing error states;
- add metrics needed by the test plan.

Exit criteria:

- the 30-minute synchronization target passes;
- game frame-rate changes do not create cumulative drift;
- seeks and stop during buffering are reliable;
- working memory stays within the agreed budget.

## Phase 5: catalog, controls, and settings

Tasks:

- enumerate MP4 files through MO2;
- add natural sorting and Unicode display;
- complete mouse, keyboard, and controller navigation;
- add presentation and volume settings;
- add safe config reload while idle;
- verify focus behavior with Pip-Boy UI Tweaks.

Exit criteria:

- catalog limits and filename fixtures pass;
- all actions work without global input hooks;
- switching input methods updates prompts;
- personal paths stay out of normal logs.

## Phase 6: hardening and release candidate

Tasks:

- complete the full compatibility matrix;
- run fault injection, repetition tests, and the soak test;
- freeze dependency revisions;
- finish project and FFmpeg licensing materials;
- create a clean MO2 package and uninstall test;
- write user installation, troubleshooting, and bug-report guides;
- publish a release candidate for a small test group.

Exit criteria:

- all first-release acceptance targets pass;
- supported and unsupported graphics configurations are explicit;
- the archive contains no game assets, personal media, or unexpected DLLs;
- install and removal do not affect saves;
- known failures have useful messages and logs.

## Later work

Possible later releases can consider subtitles, playlists, resume positions outside saves, additional containers and codecs, hardware decoding, terminal screens, or broader graphics-plugin compatibility. Each addition needs its own failure and test contract.
