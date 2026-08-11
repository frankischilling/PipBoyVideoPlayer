# Installation and use

Pip-Boy Video Player 0.1.0-rc.1 is a private test candidate for the Viva New Vegas setup documented in this repository. Install it as a normal Mod Organizer 2 mod. Do not extract its FFmpeg DLLs into the game directory.

## Requirements

- Windows 10 or Windows 11
- Fallout: New Vegas runtime 1.4.0.525
- xNVSE 6.4.5 or newer
- UIO 2.30
- Mod Organizer 2
- The tested Viva New Vegas Base or Extended setup
- Native Direct3D 9 in windowed mode

DXVK, controller input, and repeated native fullscreen Alt+Tab are not supported by this candidate.

## Install the player

1. In Mod Organizer 2, choose **Install a new mod from an archive**.
2. Select `PipBoyVideoPlayer-0.1.0-rc.1.zip`.
3. Accept the archive layout. `NVSE`, `menus`, `textures`, `uio`, and `Config` must be at the mod root.
4. Name the mod `Pip-Boy Video Player` and enable it in the left pane.
5. Keep UIO enabled. Launch Fallout: New Vegas through the xNVSE entry in Mod Organizer 2.

The player has no ESP and does not add anything to the plugin load order.

## Add videos

Keep personal media in a separate MO2 mod:

1. Right-click the MO2 left pane and select **Create empty mod**.
2. Name it `Pip-Boy Videos`.
3. Open that mod in Explorer.
4. Create `NVSE\Plugins\PipBoyVideoPlayer\Videos` inside it.
5. Copy MP4 files directly into the `Videos` directory.
6. Enable `Pip-Boy Videos`, then restart the game through MO2.

The complete path has this form:

```text
<MO2 instance>\mods\Pip-Boy Videos\NVSE\Plugins\PipBoyVideoPlayer\Videos
```

The catalog scans only files directly inside `Videos`. It does not scan subdirectories. The supported first-release format is MP4 with H.264 video and optional AAC audio. Sources may be no larger than 1920 by 1080. The default file limit is 32 GiB.

## Open the player

1. Open the Pip-Boy and select Data.
2. Select `VIDEOS`.
3. Choose a catalog entry with the mouse or Up and Down Arrow keys.
4. Press Enter or left-click the selected entry.

Default controls:

| Action | Input |
| --- | --- |
| Select or play | Enter or left click |
| Pause or resume | Space |
| Stop or go back | Escape, Backspace, or right click |
| Seek backward | Left Arrow |
| Seek forward | Right Arrow |
| Previous item | Up Arrow or wheel up |
| Next item | Down Arrow or wheel down |
| Toggle Pip-Boy tint or full color | T |

Closing the Pip-Boy or leaving the Videos page stops playback. The player does not save a resume position.

## Settings

The settings file is `Config\PipBoyVideoPlayer.ini` inside the installed mod. It controls volume, mute, seek length, Fit or Fill display, Pip-Boy tint or full color, catalog limits, resource limits, keyboard scan codes, and logging detail.

Changed settings can be loaded while playback is idle with this xNVSE console command:

```text
ReloadPluginConfig "Pip-Boy Video Player"
```

If playback is active, the reload is refused and the current settings stay in use.

## Remove or reinstall

Close Fallout: New Vegas and Mod Organizer 2 before changing the installation. Disable or remove the `Pip-Boy Video Player` mod in MO2. Personal videos remain in the separate `Pip-Boy Videos` mod.

The player is ESP-less and stores no playback state in saves or xNVSE co-saves. Reinstall the archive as the same MO2 mod, enable it, and launch through xNVSE. Removing the player also removes the `VIDEOS` entry on the next launch.
