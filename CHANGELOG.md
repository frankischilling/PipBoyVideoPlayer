# Changelog

## 0.1.0 - Unreleased

- Added the Win32 xNVSE plugin scaffold and strict FalloutNV 1.4.0.525 runtime check.
- Added xNVSE lifecycle logging and a guarded texture-upload boundary through `kMessage_OnFramePresent`.
- Added a UIO prefab with a named video rectangle, private engine image, visible layer probes, and a game-thread bridge.
- Added the unconditional UIO registration condition required to inject the prefab.
- Added a deterministic 256x256 BGRA DDS generator for the private video surface.
- Added guarded checks for the reviewed `TileImage`, `TileShaderProperty`, `NiSourceTexture`, and `NiDX9SourceTextureData` layouts.
- Added diagnostics for the direct image texture, shader property, and shader source texture.
- Assigned the injected surface and probe drawables explicit depths between normal MapMenu page content and the existing headline and tab controls.
- Added a Direct3D 9 checkerboard upload that uses a temporary COM reference and leaves drawing to Gamebryo.
- Added device-profile, surface-profile, texture-upload, thread-identity, and device-recreation diagnostics.
- Added one-time diagnostics for game-thread UI polling, MapMenu lookup, frame presentation, and D3D device discovery.
- Corrected the native height and width trait IDs and now source them from the official xNVSE `Tile` definitions.
- Resolved the logical UI canvas from the bounded MapMenu ancestor chain instead of assuming the menu root has screen dimensions.
- Added a reset-hook probe that rejects occupied or unknown function entries before MinHook can patch them.
- Removed the rejected frame-present and normal-frame overlay draws. The plugin no longer issues a screen-space primitive or patches the normal-frame UI call.
- Added checked dependency downloads for xNVSE 6.4.5 and MinHook 1.3.4.
- Added host tests, Win32 tests, DLL export checks, and PE architecture checks.
- Added a data-contract test for the UIO registration, named prefab tiles, private surface path, and package generator.
- Added RadioCaptions-style build, test, package, and MO2 development-install scripts.
