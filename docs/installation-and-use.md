# Installation and video tutorial

This guide installs Pip-Boy Video Player as a normal Mod Organizer 2 mod and keeps your MP4 files in a separate personal mod. Separating the two means you can replace or update the player without moving your videos.

## Before you start

- Windows 10 or Windows 11
- Fallout: New Vegas runtime 1.4.0.525
- Mod Organizer 2
- xNVSE 6.4.5 or newer
- UIO 2.30
- Viva New Vegas Base or Extended

The tested graphics path is native Direct3D 9. Fullscreen playback passed at 1920 by 1080. Windowed mode passed the repeated focus-loss test and is recommended if you switch away from the game often. DXVK and controller input are not supported.

Close Fallout: New Vegas before installing the archive or changing the active MO2 profile.

## Install the player

1. Open Mod Organizer 2.
2. Select **Install a new mod from an archive** in the toolbar.
3. Choose `PipBoyVideoPlayer-0.1.0-rc.2.zip`.
4. Confirm that `NVSE`, `menus`, `textures`, `uio`, and `Config` are at the archive root.
5. Name the mod `Pip-Boy Video Player`.
6. Enable it in the MO2 left pane.
7. Confirm that UIO is enabled in the same profile.
8. Launch Fallout: New Vegas through the xNVSE entry in MO2.

The player has no ESP, so it does not add an entry to the plugin load order.

Do not move any FFmpeg DLL into the Fallout: New Vegas game directory. Do not move them into the shared `NVSE\Plugins` directory either. The five FFmpeg DLLs must stay in:

```text
NVSE\Plugins\PipBoyVideoPlayer\bin
```

## Create the video mod

1. Right-click an empty area in the MO2 left pane.
2. Select **Create empty mod**.
3. Enter `Pip-Boy Videos` as the name.
4. Right-click `Pip-Boy Videos` and select **Open in Explorer**.
5. Create a folder named `NVSE`.
6. Inside `NVSE`, create `Plugins`.
7. Inside `Plugins`, create `PipBoyVideoPlayer`.
8. Inside `PipBoyVideoPlayer`, create `Videos`.
9. Return to MO2 and enable `Pip-Boy Videos`.

The finished path is:

```text
<MO2 instance>\mods\Pip-Boy Videos\NVSE\Plugins\PipBoyVideoPlayer\Videos
```

If MO2 already shows `Pip-Boy Videos`, use its existing folder. Do not create a second copy with a similar name.

## Add an MP4

1. Close Fallout: New Vegas.
2. Open the `Pip-Boy Videos` mod in Explorer.
3. Open `NVSE\Plugins\PipBoyVideoPlayer\Videos`.
4. Copy the MP4 into that folder.
5. Start the game through MO2.
6. Open the Pip-Boy, select Data, then select `VIDEOS`.

The catalog scans only the files directly inside `Videos`. A file inside `Videos\Movies`, for example, will not appear.

The supported format is:

- `.mp4` container
- H.264 video
- Optional AAC audio
- Maximum source size of 1920 by 1080
- Maximum file size of 32 GiB with the default settings

The filename becomes the catalog name unless the MP4 contains a usable title. Unicode filenames work. The player sorts numbered names naturally, so `Video 2.mp4` appears before `Video 10.mp4`.

If a file is not compatible, convert a copy to H.264 video with AAC audio in an MP4 container. Keep the original outside the MO2 mod until you have checked the converted copy.

## Open and control a video

1. Open the Pip-Boy.
2. Select Data.
3. Select `VIDEOS`.
4. Move through the catalog with the mouse, wheel, or Up and Down Arrow keys.
5. Press Enter or left-click a row to play it.

| Action | Input |
| --- | --- |
| Select or play | Enter or left click |
| Pause or resume | Space |
| Stop or return | Escape, Backspace, or right click |
| Seek backward | Left Arrow |
| Seek forward | Right Arrow |
| Previous video | Up Arrow or wheel up |
| Next video | Down Arrow or wheel down |
| Toggle tint or full color | T |

Closing the Pip-Boy or leaving the Videos page stops the current video. Reopen `VIDEOS` and select the file again to restart it.

## Change settings

The settings file is `Config\PipBoyVideoPlayer.ini` inside the `Pip-Boy Video Player` mod. It controls volume, mute, seek length, Fit or Fill mode, Pip-Boy tint, catalog limits, memory limits, key bindings, and logging.

Close the game before editing settings. If the game is already running, you can reload a saved configuration while playback is idle:

```text
ReloadPluginConfig "Pip-Boy Video Player"
```

The player refuses a reload while a video is opening, buffering, playing, paused, or stopping.

## Use the repository setup helper

Use the checked setup helper when working from this repository. Close FalloutNV and Mod Organizer 2, then run:

```powershell
.\scripts\setup-gameplay-profile.ps1 `
  -InstanceRoot '<MO2 instance>' `
  -RuntimeArchive '.\dist\PipBoyVideoPlayer-0.1.0-rc.2.zip' `
  -SelectProfile
```

The helper creates a profile named `Pip-Boy Video Player` from `Viva New Vegas Extended`. It does not copy source-profile saves. It enables the normal player and video mods, disables PBVP development fixtures, and leaves all existing profiles unchanged.

Running the helper again is safe. It checks the installed archive layout and preserves any saves in the gameplay profile, MP4 files in the personal media mod, and edits to `PipBoyVideoPlayer.ini`.

## Remove or reinstall

Close Fallout: New Vegas and Mod Organizer 2 before changing the installation.

To remove the player, disable or delete `Pip-Boy Video Player` in MO2. Leave `Pip-Boy Videos` enabled or disabled as you prefer. Its MP4 files are separate and are not removed with the player.

The player stores no playback state in saves or xNVSE co-saves. Removing it does not require a save cleaner. After removal, the `VIDEOS` entry disappears the next time the game starts.

To reinstall, install the archive again under the same `Pip-Boy Video Player` mod name, enable it, and launch through xNVSE.
