# Phase 6 live test guide

Use only the isolated PBVP profiles and generated media mods. Do not load a normal save. Close Fallout: New Vegas and Mod Organizer 2 before a setup script changes the selected test profile or refreshes the development mod.

## Prepare the hardening profile

Build and test the release configuration first:

```powershell
.\scripts\configure.ps1 -Target plugin
.\scripts\build.ps1 -Configuration Release -Jobs 2
.\scripts\test.ps1 -Configuration Release
```

Package the current build, then copy its staged files into the existing development mod:

```powershell
.\scripts\package.ps1 -Version 0.1.0-rc.1 -Configuration Release
$instance = Read-Host 'VNV MO2 instance path'
& .\scripts\install-dev.ps1 `
  -MO2ModsDirectory (Join-Path $instance 'mods') `
  -UpdateExisting
```

Create and select a separate Phase 6 profile:

```powershell
& .\scripts\prepare-phase6-fault-fixtures.ps1 `
  -InstanceRoot $instance
& .\scripts\prepare-phase6-profile.ps1 `
  -InstanceRoot $instance `
  -SelectProfile
& .\scripts\prepare-phase6-profile.ps1 `
  -InstanceRoot $instance `
  -VerifyOnly
```

The profile is copied from `PBVP Phase 5 Extended`. It enables the development mod, catalog fixtures, 30-minute fixture, generated fault fixtures, and save guard. It disables the older armed diagnostic mods. The setup refuses a profile that contains a save or co-save.

## Run 100 playback sessions and 40 seeks

Launch `PBVP Phase 6 Hardening` through Mod Organizer 2. Enter the isolated test world without loading an existing save.

Use the generated `Aspect Mode 4x3` file for the short sessions:

1. Open the Pip-Boy, select Data, and open `VIDEOS`.
2. Start `Aspect Mode 4x3` and wait for `PLAYING`, a visible frame, and audio.
3. Press Backspace to stop and return to the catalog.
4. Repeat until the run has at least 100 successful starts and stops.
5. After each group of ten, return to Data, close the Pip-Boy, reopen it, and continue.

During one session, wait until playback has advanced beyond five seconds. Press Right, wait for `PLAYING`, press Left, and wait for `PLAYING` again. Complete 20 right and left pairs. The waits matter because they prove that each seek finished buffering before the next request.

Use mouse selection for some sessions and keyboard selection for the rest. Keep ordinary Pip-Boy controls usable throughout the run. Exit the game normally after the final session.

Copy and check the finished log:

```powershell
$gameRoot = Read-Host 'Fallout New Vegas game directory'
Copy-Item `
  -LiteralPath (Join-Path $gameRoot 'PipBoyVideoPlayer.log') `
  -Destination '.\build-host\phase6-repetition.log'
& .\scripts\check-phase6-repetition-log.ps1 `
  -LogPath '.\build-host\phase6-repetition.log'
```

The checker requires at least 100 numbered session summaries, a decoded and presented frame in every session, audio samples, zero underruns, 20 accepted seeks in each direction, bounded queues, complete renderer accounting, privacy-safe paths, and orderly shutdown.

## Check damaged and unsupported media

The generated fault mod contains one valid control followed by six failure cases. Open each entry in number order:

1. `00 Valid Control` must play video and audio.
2. `10 Empty File` must show a playback error without freezing the game.
3. `20 Random Bytes` must show a playback error without freezing the game.
4. `30 Truncated File` must show a playback error without freezing the game.
5. `40 Unsupported Video Codec` must show a playback error.
6. `50 Unsupported Audio Codec` must show a playback error.
7. `60 Encrypted Media` must show a playback error.
8. Play `00 Valid Control` again to confirm recovery after the failures.

The catalog, Back control, ordinary Pip-Boy controls, and game input must remain usable after every result. The generated files are deterministic and redistribution-safe. The native Win32 decoder tests verify the expected structured status for every case.

## Soak status for 0.1.0-rc.1

The project owner waived the two-hour mixed soak for this private candidate on August 11, 2026. It was not run and is not recorded as passed. The process sampler and strict metrics checker remain available for a later candidate. This candidate must stay private and should not be described as soak-tested.

## Finish the run

Close Mod Organizer 2 before verifying the profile again:

```powershell
& .\scripts\prepare-phase6-profile.ps1 `
  -InstanceRoot $instance `
  -VerifyOnly
```

The profile's `saves` directory must remain empty. Preserve the accepted logs and process JSON under `build-host`; that directory is ignored by Git and excluded from release packaging.
