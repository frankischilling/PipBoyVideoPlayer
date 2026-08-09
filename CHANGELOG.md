# Changelog

## 0.1.0 - Unreleased

- Added the Win32 xNVSE plugin scaffold and strict FalloutNV 1.4.0.525 runtime check.
- Added xNVSE lifecycle logging and frame-present diagnostics through `kMessage_OnFramePresent`.
- Added a UIO prefab with a named video rectangle and a game-thread bridge that publishes rectangle snapshots.
- Added the unconditional UIO registration condition required to inject the prefab.
- Added a Direct3D 9 checkerboard renderer with full state capture and bounded default-pool resource ownership.
- Added an overlapping UIO label for testing whether XML controls render above the native video draw.
- Added device-profile, texture-upload, draw-cost, and device-recreation diagnostics for compatibility testing.
- Added one-time diagnostics for game-thread UI polling, MapMenu lookup, frame presentation, and D3D device discovery.
- Corrected the native height and width trait IDs and now source them from the official xNVSE `Tile` definitions.
- Resolved the logical UI canvas from the bounded MapMenu ancestor chain instead of assuming the menu root has screen dimensions.
- Removed untouched render-target and depth-surface rebinds that could reset the restored Direct3D viewport.
- Added a reset-hook probe that rejects occupied or unknown function entries before MinHook can patch them.
- Added a checked normal-frame call replacement that draws immediately before the engine UI routine and restores the original call during shutdown.
- Added relative-call encoding, decoding, and conflict-classification tests for the pre-UI draw boundary.
- Added checked dependency downloads for xNVSE 6.4.5 and MinHook 1.3.4.
- Added host tests, Win32 tests, DLL export checks, and PE architecture checks.
- Added a data-contract test for the UIO registration and named prefab tiles.
- Added RadioCaptions-style build, test, package, and MO2 development-install scripts.
