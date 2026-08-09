# Changelog

## 0.1.0 - Unreleased

- Added the Win32 xNVSE plugin scaffold and strict FalloutNV 1.4.0.525 runtime check.
- Added xNVSE lifecycle logging and a render-thread callback through `kMessage_OnFramePresent`.
- Added a UIO prefab with a named video rectangle and a game-thread bridge that publishes rectangle snapshots.
- Added the unconditional UIO registration condition required to inject the prefab.
- Added a Direct3D 9 checkerboard renderer with full state capture and explicit render-target restoration.
- Added device-profile, texture-upload, draw-cost, and device-recreation diagnostics for compatibility testing.
- Added one-time diagnostics for game-thread UI polling, MapMenu lookup, frame presentation, and D3D device discovery.
- Added a reset-hook probe that rejects occupied or unknown function entries before MinHook can patch them.
- Added checked dependency downloads for xNVSE 6.4.5 and MinHook 1.3.4.
- Added host tests, Win32 tests, DLL export checks, and PE architecture checks.
- Added a data-contract test for the UIO registration and named prefab tiles.
- Added RadioCaptions-style build, test, package, and MO2 development-install scripts.
