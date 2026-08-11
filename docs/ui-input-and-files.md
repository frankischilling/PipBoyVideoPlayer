# UI, input, and files

## Pip-Boy entry

The implemented layout adds `VIDEOS` to the Data section instead of adding a fourth top-level Pip-Boy tab. A Data entry is less likely to collide with replacers that assume the Stats, Items, and Data tabs are fixed.

Selecting `VIDEOS` opens an eight-row scrolling catalog. Starting a video replaces the catalog with a playback stage that fills the usable Pip-Boy glass. The physical Pip-Boy frame remains visible. Title, time, state, and control prompts stay in small overlays at the top and bottom of the picture.

This playback layout follows the useful visual cue from Pip-Flicks 3000: the
picture belongs on the Pip-Boy display, not in a small permanent side panel.
Pip-Flicks remains a design and compatibility reference rather than a runtime
dependency. Pip-Boy Video Player keeps its ESP-less UIO entry and native media
pipeline.

Phase 1 verified the injection target and panel geometry against vanilla UI, Vanilla UI Plus, Clean Vanilla HUD, and Pip-Boy UI Tweaks. Phase 5 must repeat those profiles with the catalog and playback controls active. UIO registration owns the prefab insertion. The package does not overwrite a full menu XML file.

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

The title and time appear along the top of the playback stage. State and control text use the lower inset. Both overlays are children of the named video rectangle, so they follow UI scaling and replacement layouts. They exist only while the Videos page owns focus and cannot cover Radio, Map, Quests, or Notes content.

The current playback stage uses a 384 by 216 rectangle raised above the Local Map and World Map buttons. The rectangle establishes the local coordinate origin for its image and status elements. The position passed the Phase 1 UI profiles and the full Extended Phase 5 catalog run. The other three Phase 5 profiles remain release gates.

## Controls

Default actions:

| Action | Keyboard and mouse | Controller |
| --- | --- | --- |
| Select or play | Enter or left click | A |
| Pause or resume | Space | X |
| Stop or back | Escape, Backspace, or right click | B |
| Seek backward | Left Arrow | Left bumper |
| Seek forward | Right Arrow | Right bumper |
| Previous item | Up Arrow or wheel up | D-pad up |
| Next item | Down Arrow or wheel down | D-pad down |
| Toggle presentation mode | T | Y |

The binding layer must use game control state or verified menu input events, not a global low-level keyboard hook. Input is consumed only while the Videos page has focus. Holding a seek button does not produce an unbounded command stream; it repeats at a controlled interval.

The implementation receives keyboard actions from the live MapMenu's 32-bit `HandleKeyboardInput` callback and controller edges from XInput. It translates configured DirectInput scan codes into Fallout's printable or high-bit special-key values, so the shipped INI remains the source of keyboard bindings. Backspace is also accepted as a close key. The tested VNV stack did not expose menu keyboard presses through the xNVSE input singleton when PBVP polled it on the game thread. PBVP still uses that singleton for filtered mouse state and as a secondary keyboard source.

A private virtual-table copy on the live MapMenu instance consumes clicks and keyboard calls while Videos owns focus. Before attaching it, PBVP validates the MapMenu ID, readable table storage, and executable protection for all 15 entries. `HandleClick` must belong to `FalloutNV.exe`. `HandleKeyboardInput` may belong to the game or the exact Stewie Tweaks 9.80 Menu Search handler reviewed for the target VNV stack. PBVP calls that Stewie handler unchanged while Videos is inactive. Changes to unrelated entries are also copied without modification. Any unknown input handler keeps the Videos layer hidden and leaves ordinary Pip-Boy input unchanged.

Keyboard settings under `[Input]` are DirectInput scan codes, not Windows virtual-key numbers. Codes must be between 1 and 255, and the eight actions must use different codes. An out-of-range or duplicate value resets all keyboard actions to their shipped defaults so a partial configuration cannot bind one key to two actions.

Catalog and playback prompts use the active keyboard bindings. Pressing a controller button switches them to controller labels. Pressing a keyboard key, clicking, scrolling, or moving the mouse switches them back.

Mouse buttons use unique MapMenu IDs. Nested button visuals are not mouse targets. If the engine still reports one of those children, PBVP walks no more than eight parent links and accepts only exact button names from its own prefab. Vanilla UI Plus can report the radio-list target or produce no callback where the injected entry is visible. The scoped MapMenu callback handles an overlapping target by checking exact PBVP bounds against the game's `InterfaceManager::cursorX` and `cursorY` fields. It obtains the button origin from the game's locus-adjusted tile routines instead of adding parent positions. If no callback occurs, the game thread checks one filtered xNVSE left-button edge for `PBVP_OpenButton` only. Catalog and playback mouse input still requires Videos focus. PBVP does not read cursor tile traits, use a Windows mouse hook, map visible text, or accept another mod's tile name.

## Focus and menu behavior

The list owns focus while idle. Starting playback moves focus to the playback stage and remembers the selected catalog entry. Stop returns to that entry. Focus stays with the playback stage until playback stops. Back from the catalog returns to the Data page.

Closing the Pip-Boy always stops playback. Switching to another Pip-Boy page also stops it.

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

MO2's user-space virtual filesystem overlays mod directories for processes launched through MO2. A Phase 2 integration run opened and decoded a fixture that existed only in an enabled media mod. Phase 5 also enumerated and played files from a separate generated catalog mod.

## Filenames and metadata

The catalog uses Unicode Windows paths internally and shows the filename without its extension as soon as the page opens. Selecting a file lets the decoder inspect its file-level title. Valid UTF-8 metadata within the string cap replaces that row for the current session. Missing, malformed, oversized, blank, or control-character metadata leaves the filename in place.

Sorting is case-insensitive and natural, so `Episode 2` appears before `Episode 10`. Duplicate display names remain distinct. The UI may add a short disambiguator, but it never exposes a full absolute path.

Filename tests must include spaces, apostrophes, non-Latin text, combining characters, very long names, and malformed metadata. Unsupported characters in the game font use a replacement glyph without changing the actual file path.

## Configuration

The configuration file is:

`Data\Config\PipBoyVideoPlayer.ini`

It contains presentation mode, aspect mode, volume, resource limits, logging detail, and input bindings. Defaults ship in the mod. User changes written through the virtual filesystem will normally land in MO2's Overwrite unless a dedicated settings mod captures them.

The shipped INI lists every supported setting. `Volume` accepts 0.0 through 1.0, and `SeekSeconds` accepts 1 through 60. `AspectMode` accepts `Fit` or `Fill`. `TintMode` accepts `PipBoy` or `FullColor`. `Detail` accepts `Normal` or `Diagnostic`. The `[Input]` values follow the DirectInput scan-code rules in the Controls section.

Resource values may lower the compiled limits, but they cannot raise the supported 1920 by 1080 source limit, 512-pixel queued-video edge, 32 GiB file limit, or 500-entry catalog limit. Invalid values keep their compiled defaults. Unknown keys and malformed lines are ignored and summarized once in the log without printing the configuration path.

`ReloadPluginConfig "Pip-Boy Video Player"` applies a changed INI only while playback is idle. The quotation marks are required because xNVSE resolves the registered plugin name as one string. A successful reload returns to the ordinary Data page, where `VIDEOS` can be opened again with a fresh catalog scan. A reload request during opening, buffering, playback, pause, or an error leaves the settings, Videos page, and playback state unchanged.

An MCM page is optional for a later release. The first version uses the INI and exposes tint switching in the player.

## Logs and privacy

The runtime log belongs beside other xNVSE plugin logs. Normal logging includes the plugin version, supported runtime, dependency versions, selected media basename, stream summary, timing warnings, and errors.

Normal mode records only the selected media basename. Diagnostic mode may record a validated relative catalog name. If a value is absolute or contains traversal, PBVP strips it to the basename. File contents and embedded metadata are not used for log names. Shared logs can reveal filenames in either mode.
