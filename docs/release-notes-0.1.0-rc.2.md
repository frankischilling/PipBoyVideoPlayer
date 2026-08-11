# Pip-Boy Video Player 0.1.0-rc.2

This private release candidate adds the normal Mod Organizer 2 gameplay setup and end-user documentation. The plugin binary and supported media format are unchanged from `0.1.0-rc.1`.

## Changes

- Added a guarded setup helper for a normal `Pip-Boy Video Player` MO2 profile.
- Added a separate `Pip-Boy Videos` mod and documented its MP4 folder.
- Replaced the development-phase README with installation, controls, settings, compatibility, and troubleshooting information.
- Documented the successful 1920 by 1080 native fullscreen playback checks. Windowed mode remains the recommended choice for frequent Alt+Tab use.
- Licensed original PBVP code and documentation under the MIT License.
- Added the MIT license to runtime and symbols packages.

## Known limits

- Controller input is not supported.
- DXVK is not supported.
- Videos must use H.264 in an MP4 container with optional AAC audio and must not exceed 1920 by 1080.
- Closing the Pip-Boy stops playback. Resume positions are not saved.
- The owner waived the two-hour mixed soak. It was not run for this candidate.

This candidate remains private. A public binary release still requires a review of H.264 and AAC distribution obligations and the owner's approval.
