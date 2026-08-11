# Phase 5 live test guide

Use only the isolated PBVP profiles. Do not load a normal save or change a normal VNV profile. The profile setup script refuses a target that already contains save data.

## Prepare the profiles

Close Fallout: New Vegas and Mod Organizer 2. From the repository root, run:

```powershell
$instance = Read-Host 'VNV MO2 instance path'
& .\scripts\prepare-phase5-ui-profiles.ps1 `
    -InstanceRoot $instance `
    -CreateMissing `
    -SelectProfile 'PBVP Phase 5 Extended'
& .\scripts\prepare-phase5-ui-profiles.ps1 `
    -InstanceRoot $instance `
    -VerifyOnly
```

The command creates four profiles from the accepted Phase 1 isolation profiles. Each copy enables the development plugin, generated catalog, and save guard. It disables the other PBVP fixture mods.

## Check Fit, Fill, and idle reload

Start with `AspectMode=Fit` in the development mod's `Config\PipBoyVideoPlayer.ini`.

The synthetic `Aspect Mode 4x3.mp4` file is a local test artifact and is not part of the repository or release package. Before launch, verify that the copy under the catalog test mod has SHA-256 `5FF551A7C3B482CD042391AEE810EE5D08AAA77417EA6908572F5D291B67F4DF`:

```powershell
$aspectFixture = Join-Path $instance `
    'mods\Pip-Boy Video Player - Catalog Test\NVSE\Plugins\PipBoyVideoPlayer\Videos\Aspect Mode 4x3.mp4'
Get-FileHash -LiteralPath $aspectFixture -Algorithm SHA256
```

1. Launch the `PBVP Phase 5 Extended` profile.
2. Open the Pip-Boy, select Data, open `VIDEOS`, and play `Aspect Mode 4x3`.
3. Confirm that the whole 4:3 picture is visible with black bars at the left and right.
4. Stop playback and return to the ordinary Data page so the player is idle.
5. Change the development INI to `AspectMode=Fill`.
6. Open the game console and run `ReloadPluginConfig PipBoyVideoPlayer`.
7. Reopen Data and `VIDEOS`, then play `Aspect Mode 4x3` again.
8. Confirm that the picture fills the 16:9 stage and crops the top and bottom. It must not draw over the Pip-Boy frame or map buttons.
9. Exit the game normally.

Copy the finished plugin log to an ignored test-artifact path, then run the checker:

```powershell
$gameRoot = Read-Host 'Fallout New Vegas game directory'
Copy-Item `
    -LiteralPath (Join-Path $gameRoot 'PipBoyVideoPlayer.log') `
    -Destination '.\build-host\phase5-aspect-reload-accepted.log'
& .\scripts\check-phase5-aspect-reload-log.ps1 `
    -LogPath '.\build-host\phase5-aspect-reload-accepted.log'
```

Restore `AspectMode=Fit` after the test.

## Check controller actions and prompt switching

Connect an Xbox-compatible controller before launching the game. Use the Extended test profile and start with the keyboard or mouse prompt visible.

1. Open `VIDEOS` with the mouse or keyboard.
2. Press D-pad Down and confirm that the selection moves and the prompt changes to controller labels.
3. Press A to play the selected file and wait until the picture and audio begin.
4. Press X to pause and X again to resume.
5. Use the left and right bumpers to seek in both directions.
6. Press Y and confirm that the tint mode changes.
7. Press B to stop and return to the catalog.
8. Move the mouse and confirm that the prompt changes back to keyboard and mouse labels.
9. Press B again to return to Data, then exit normally.

Preserve that run as `build-host\phase5-controller-accepted.log` and check it:

```powershell
& .\scripts\check-phase5-controller-log.ps1 `
    -LogPath '.\build-host\phase5-controller-accepted.log'
```

The checker proves that controller input opened a file and that keyboard or mouse input later became active. It cannot see the labels or confirm every button action, so record the visual result separately.

## Check the four UI profiles

Test these profiles one at a time:

| Test label | MO2 profile |
| --- | --- |
| Base | `PBVP Phase 5 Base` |
| Vanilla UI Plus | `PBVP Phase 5 VUI Plus` |
| Clean Vanilla HUD | `PBVP Phase 5 Extended No Pip-Boy Tweaks` |
| Extended | `PBVP Phase 5 Extended` |

Before selecting another profile, exit the game and close MO2. Select the next profile with:

```powershell
& .\scripts\prepare-phase5-ui-profiles.ps1 `
    -InstanceRoot $instance `
    -SelectProfile 'PBVP Phase 5 Base'
```

For each profile, open Data and `VIDEOS`, inspect all visible rows, play one generated file, wait for a decoded frame and audio, stop, return to Data, and exit normally. Confirm that the Pip-Boy frame and ordinary controls remain visible and usable. Preserve each finished log under `build-host` with the names below:

- `phase5-ui-base.log`
- `phase5-ui-vui-plus.log`
- `phase5-ui-clean-vanilla-hud.log`
- `phase5-ui-extended.log`

After all four runs, execute:

```powershell
& .\scripts\check-phase5-ui-matrix-logs.ps1 `
    -BaseLog '.\build-host\phase5-ui-base.log' `
    -VuiPlusLog '.\build-host\phase5-ui-vui-plus.log' `
    -CleanVanillaHudLog '.\build-host\phase5-ui-clean-vanilla-hud.log' `
    -ExtendedLog '.\build-host\phase5-ui-extended.log'
```

Do not record a profile as passed unless its visual and input checks passed, its log passes the checker, and its isolated `saves` directory is empty after exit.
