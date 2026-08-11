# Pip-Boy Video Player 0.1.0-rc.3

This private release candidate keeps a video playing after the Pip-Boy closes. The picture disappears with the menu, but audio, decoding, and the media clock continue while the player walks around. Opening the Pip-Boy again returns to the same video at its current point.

## Changes

- Removed Pip-Boy visibility as a playback stop condition.
- Kept hidden frames within the existing bounded queues and one-frame render mailbox.
- Kept Back and Stop as explicit stop actions. Save load, new game, main menu, and shutdown also end the session.
- Added Win32 regressions for hidden AAC playback and silent variable-frame-rate playback.
- Updated the usage, troubleshooting, scope, architecture, decision, and risk documents.

## Validation

- All 28 host tests passed.
- All 37 Win32 Release tests passed.
- The installed VNV build still needs a live close, walk, and reopen check before this candidate is accepted.

## Known limits

- Controller input is not supported.
- DXVK is not supported.
- Videos must use H.264 in an MP4 container with optional AAC audio and must not exceed 1920 by 1080.
- PBVP does not save a resume position after playback ends or a game lifecycle transition closes the session.
- The owner waived the two-hour mixed soak. It was not run for this candidate.

This candidate remains private. A public binary release still requires a review of H.264 and AAC distribution obligations and the owner's approval.
