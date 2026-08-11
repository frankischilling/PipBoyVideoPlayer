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
.\scripts\package.ps1 -Version 0.1.0 -Configuration Release
$instance = Read-Host 'VNV MO2 instance path'
& .\scripts\install-dev.ps1 `
  -MO2ModsDirectory (Join-Path $instance 'mods') `
  -UpdateExisting
```

Create and select a separate Phase 6 profile:

```powershell
& .\scripts\prepare-phase6-profile.ps1 `
  -InstanceRoot $instance `
  -SelectProfile
& .\scripts\prepare-phase6-profile.ps1 `
  -InstanceRoot $instance `
  -VerifyOnly
```

The profile is copied from `PBVP Phase 5 Extended`. It enables the development mod, catalog fixtures, 30-minute fixture, and save guard. It disables the older armed diagnostic mods. The setup refuses a profile that contains a save or co-save.

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

## Run the two-hour soak

Start the process sampler in a separate PowerShell window before launching FalloutNV:

```powershell
.\scripts\measure-phase6-process.ps1 `
  -OutputPath '.\build-host\phase6-soak-process.json' `
  -IntervalMilliseconds 5000 `
  -WarmupSeconds 300
```

Keep the game running for at least two hours. Include all of these actions during the session:

- play several short generated clips;
- play `PBVP-Phase4-30Minute` from start to finish once;
- pause and resume while audio is active;
- seek in both directions and wait for playback after each seek;
- stop once during buffering;
- close the Pip-Boy during active playback and reopen it;
- move between cells while playback is idle;
- open and close the Pip-Boy repeatedly;
- use ordinary windowed Alt+Tab changes at regular intervals.

Do not include repeated native fullscreen Alt+Tab. The matched PBVP-disabled control reproduced the same NVIDIA driver crash, so that path is outside the supported configuration.

Exit FalloutNV normally after two hours. The sampler stops when the process exits. Check its output:

```powershell
& .\scripts\check-phase6-process-metrics.ps1 `
  -MetricsPath '.\build-host\phase6-soak-process.json'
```

Copy the matching plugin log to `build-host\phase6-soak.log`. Inspect it for user-visible faults, then check the session records with thresholds that match the actions completed during the soak:

```powershell
& .\scripts\check-phase6-repetition-log.ps1 `
  -LogPath '.\build-host\phase6-soak.log' `
  -MinimumPlaybackSessions 10 `
  -MinimumForwardSeeks 5 `
  -MinimumBackwardSeeks 5
```

The process checker requires two hours of samples, a five-minute warm-up, at least 80 percent sample coverage, private-memory growth below 128 MiB, no more than 32 extra handles, and no more than eight extra threads. Keep both result files together.

## Finish the run

Close Mod Organizer 2 before verifying the profile again:

```powershell
& .\scripts\prepare-phase6-profile.ps1 `
  -InstanceRoot $instance `
  -VerifyOnly
```

The profile's `saves` directory must remain empty. Preserve the accepted logs and process JSON under `build-host`; that directory is ignored by Git and excluded from release packaging.
