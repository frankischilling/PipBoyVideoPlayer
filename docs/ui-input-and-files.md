# UI, input, and files

## Pip-Boy entry

The preferred layout adds `VIDEOS` to the Data section instead of adding a fourth top-level Pip-Boy tab. A Data entry is less likely to collide with replacers that assume the Stats, Items, and Data tabs are fixed.

Selecting `VIDEOS` opens a scrollable catalog. Starting a video replaces the
catalog with a playback stage that fills the usable Pip-Boy glass. The physical
Pip-Boy frame remains visible. Title, time, state, and control prompts appear as
a small overlay while paused or after input, then clear from the picture.

This playback layout follows the useful visual cue from Pip-Flicks 3000: the
picture belongs on the Pip-Boy display, not in a small permanent side panel.
Pip-Flicks remains a design and compatibility reference rather than a runtime
dependency. Pip-Boy Video Player keeps its ESP-less UIO entry and native media
pipeline.

The exact injection target must be verified against vanilla UI, Vanilla UI Plus, Clean Vanilla HUD, and Pip-Boy UI Tweaks. UIO registration owns the prefab insertion. The package must not overwrite a full menu XML file.

## Layout contract

The injected prefab exposes named traits for the native plugin:

| Trait group | Purpose |
| --- | --- |
| Video rectangle | Resolved x, y, width, height, visibility, and clipping state |
| Player state | Idle, buffering, playing, paused, seeking, ended, or error |
| Media text | Display title, current time, duration, and error message |
| Catalog | Visible rows, selected index, scroll position, and empty state |
| Input prompts | Current keyboard or controller labels |
| Presentation | Aspect mode, tint mode, opacity, and safe-area adjustment |

Names and numeric state values become a versioned bridge between XML and the DLL. Changing them after release needs a bridge version bump and a compatibility check at startup.

The native plugin treats missing or malformed traits as a disabled UI. It logs the missing name and shows one short message through a safe existing menu path. It does not guess screen coordinates.

The playback stage defines the largest safe rectangle inside the active
Pip-Boy glass. Aspect fit may leave bars inside that rectangle. Aspect fill may
crop the source, but it must never draw over the physical Pip-Boy frame. UI
variants and handheld replacers may supply different named bounds through their
prefabs.

The title, state, and control overlay starts at the lower-left inset of the playback stage. Its position is derived from the named video rectangle so it follows UI scaling and replacement layouts. The overlay fades when playback is stable. It exists only while the Videos page owns focus, so it cannot cover Radio, Map, Quests, or Notes content.

The Phase 1 diagnostic stage uses a 384 by 216 rectangle with a 12-unit lower-left inset inside the active Pip-Boy content rectangle. The rectangle establishes the local coordinate origin for its image and status elements. This size and location are provisional. Phase 5 must test the final playback-stage size and page-specific visibility against every supported UI profile before treating the diagnostic geometry as a release contract.

## Controls

Default actions:

| Action | Keyboard and mouse | Controller |
| --- | --- | --- |
| Select or play | Enter or left click | A |
| Pause or resume | Space | X |
| Stop or back | Escape or right click | B |
| Seek backward | Left Arrow | Left bumper |
| Seek forward | Right Arrow | Right bumper |
| Previous item | Up Arrow or wheel up | D-pad up |
| Next item | Down Arrow or wheel down | D-pad down |
| Toggle presentation mode | T | Y |

The binding layer must use game control state or verified menu input events, not a global low-level keyboard hook. Input is consumed only while the Videos page has focus. Holding a seek button does not produce an unbounded command stream; it repeats at a controlled interval.

The implementation reads keyboard and mouse edges from xNVSE's filtered game-input state and controller edges from XInput. A private virtual-table copy on the live MapMenu instance consumes clicks and keyboard calls while Videos owns focus. Before attaching it, PBVP validates the MapMenu ID and requires the original table and every original function pointer to reside in `FalloutNV.exe`. If that check fails, the Videos layer stays hidden and ordinary Pip-Boy input continues unchanged.

Keyboard settings under `[Input]` are DirectInput scan codes, not Windows virtual-key numbers. Codes must be between 1 and 255, and the eight actions must use different codes. An out-of-range or duplicate value resets all keyboard actions to their shipped defaults so a partial configuration cannot bind one key to two actions.

Catalog and playback prompts use the active keyboard bindings. Pressing a controller button switches them to controller labels. Pressing a keyboard key, clicking, scrolling, or moving the mouse switches them back.

Controller prompts must come from the active input method. Mouse movement should switch to mouse prompts, and a controller action should switch back.

## Focus and menu behavior

The list owns focus while idle. Starting playback moves focus to the playback
stage and remembers the selected catalog entry. Stop returns to that entry.
Controls fade when they are not needed, but focus stays with the playback stage
until playback stops. Back from the catalog returns to the Data page.

Closing the Pip-Boy always stops playback. Switching to another Pip-Boy page also stops it. Modal confirmation dialogs pause playback and restore focus when dismissed.

The player must not interfere with ordinary Pip-Boy hotkeys, item use, radio controls, map controls, or search features from Pip-Boy UI Tweaks.

## Media directory

The default virtual path is:

`Data\NVSE\Plugins\PipBoyVideoPlayer\Videos`

Users should keep personal files in a separate MO2 mod. A typical installed layout is:

```text
Pip-Boy Videos\
  NVSE\
    Plugins\
      PipBoyVideoPlayer\
        Videos\
          Sample.mp4
```

The release package should include the empty Videos directory or a short text instruction, but no media.

MO2's user-space virtual filesystem overlays mod directories for processes launched through MO2. File discovery and custom FFmpeg I/O still need an integration test because not every library reaches files through the same Windows calls.

## Filenames and metadata

The catalog uses Unicode Windows paths internally. Display names come from MP4 title metadata when it is valid UTF-8 and within the string cap. Otherwise, the filename without its extension is used.

Sorting is case-insensitive and natural, so `Episode 2` appears before `Episode 10`. Duplicate display names remain distinct. The UI may add a short disambiguator, but it never exposes a full absolute path.

Filename tests must include spaces, apostrophes, non-Latin text, combining characters, very long names, and malformed metadata. Unsupported characters in the game font use a replacement glyph without changing the actual file path.

## Configuration

The planned configuration file is:

`Data\Config\PipBoyVideoPlayer.ini`

It contains presentation mode, aspect mode, volume, resource limits, logging detail, and input bindings. Defaults ship in the mod. User changes written through the virtual filesystem will normally land in MO2's Overwrite unless a dedicated settings mod captures them.

The shipped INI lists every supported setting. `Volume` accepts 0.0 through 1.0, and `SeekSeconds` accepts 1 through 60. `AspectMode` accepts `Fit` or `Fill`. `TintMode` accepts `PipBoy` or `FullColor`. `Detail` accepts `Normal` or `Diagnostic`. The `[Input]` values follow the DirectInput scan-code rules in the Controls section.

Resource values may lower the compiled limits, but they cannot raise the supported 1920 by 1080 source limit, 512-pixel queued-video edge, 32 GiB file limit, or 500-entry catalog limit. Invalid values keep their compiled defaults. Unknown keys and malformed lines are ignored and summarized once in the log without printing the configuration path.

`ReloadPluginConfig PipBoyVideoPlayer` applies a changed INI only while playback is idle. A successful reload returns to the ordinary Data page, where `VIDEOS` can be opened again with a fresh catalog scan. A reload request during opening, buffering, playback, pause, or an error leaves the settings, Videos page, and playback state unchanged.

An MCM page is optional for a later release. The first version may keep settings in the INI and expose only common presentation toggles in the player.

Unknown settings are ignored with one warning. Invalid limits fall back to safe compiled bounds. A config reload is allowed only while the player is idle.

## Logs and privacy

The runtime log belongs beside other xNVSE plugin logs. Normal logging includes the plugin version, supported runtime, dependency versions, selected media basename, stream summary, timing warnings, and errors.

Absolute media paths, file contents, and metadata comments are omitted by default. Diagnostic mode may include more detail after warning the user that shared logs can reveal filenames.
