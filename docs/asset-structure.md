# Win95 Point-and-Click Adventure Asset Structure

This document defines a practical, human-editable asset layout for a small Windows 95-compatible point-and-click adventure built with YAAT. It favors simple directories, short ASCII names, and formats that can be loaded with Win32 ANSI APIs and modest C code.

## Goals

- Keep assets easy for artists, writers, and programmers to find and edit.
- Use Windows 95-friendly runtime formats such as BMP, WAV, MIDI, INI, and plain text.
- Allow richer authoring formats offline, but convert them before release when needed.
- Avoid relying on long Unicode paths, case-sensitive names, or modern filesystem behavior.

## Recommended folder tree

```text
game/
  game.ini
  rooms/
    room001_hall/
      room.ini
      background.bmp
      walkmask.bmp
      zmask.bmp
      hotspots.ini
      objects.ini
      exits.ini
      script.yaat
    room002_cellar/
      room.ini
      background.bmp
      hotspots.ini
      objects.ini
      script.yaat
  graphics/
    cursors/
      arrow.cur
      use.cur
      talk.cur
    ui/
      panel.bmp
      inventory_slot.bmp
    sprites/
      player_idle.bmp
      player_walk_left.bmp
      player_walk_right.bmp
  audio/
    music/
      intro.mid
      cellar.mid
    sfx/
      door_open.wav
      pickup.wav
    speech/
      narrator_001.wav
  scripts/
    startup.yaat
    global.yaat
    inventory.yaat
  dialogs/
    guard_dialog.ini
  inventory/
    items.ini
    icons/
      brass_key.bmp
      map.bmp
  fonts/
    ui_font.fnt
  palettes/
    main.pal
  strings/
    en.ini
    de.ini
  movies/
  save/
  tools/
  packed/
    game.dat
```

## Top-level files and folders

- `game.ini`: Global game metadata, screen size, first room, default language, and optional packed-data settings.
- `rooms/`: One subfolder per playable room or scene. Room folders keep local scripts, masks, object lists, exits, and backgrounds together.
- `graphics/`: Shared art that is not owned by one room, including cursors, UI panels, and reusable sprites.
- `audio/`: Music, sound effects, and optional speech lines grouped by use.
- `scripts/`: Global YAAT scripts used by startup logic, inventory behavior, and shared events.
- `dialogs/`: Reusable dialogue-tree data for conversations that are too large to keep inline in room scripts.
- `inventory/`: Item definitions and inventory icon art.
- `fonts/`: Bitmap fonts or Win95-compatible font resources used by the UI.
- `palettes/`: Shared palette files for 8-bit or low-color display modes.
- `strings/`: Localization files with translated UI and dialogue text.
- `movies/`: Optional cutscene files. Keep this empty unless a proven Win95-safe playback path exists.
- `save/`: Default save-game location for unpacked development builds. Installers may redirect this to a user-writable path.
- `tools/`: Offline converters, validators, and packers. Runtime code should not depend on this folder.
- `packed/`: Optional release archives created from the loose asset tree.

## Room folder structure

Each room folder should be self-contained enough to preview and test independently.

```text
rooms/room001_hall/
  room.ini
  background.bmp
  walkmask.bmp
  zmask.bmp
  hotspots.ini
  objects.ini
  exits.ini
  script.yaat
```

Recommended room files:

- `room.ini`: Room metadata, dimensions, music, palette, and default script.
- `background.bmp`: Final runtime background image.
- `walkmask.bmp`: Optional mask describing where the player can walk.
- `zmask.bmp`: Optional depth/occlusion mask for drawing the player behind foreground objects.
- `hotspots.ini`: Clickable regions and cursor/action hints.
- `objects.ini`: Initial room objects, sprites, positions, visibility, and inventory links.
- `exits.ini`: Simple room-to-room transitions for data-driven exits.
- `script.yaat`: Room-specific script using the YAAT script language.

### Example room metadata

```ini
; rooms/room001_hall/room.ini
[id]
name=room001_hall
label=Front Hall

[display]
width=320
height=200
background=background.bmp
palette=../../palettes/main.pal

[audio]
music=../../audio/music/intro.mid

[script]
file=script.yaat
```

### Example hotspot data

```ini
; rooms/room001_hall/hotspots.ini
[locked_door]
name=Locked Door
rect=284,52,42,116
cursor=use
script_event=on_click
```

### Example room script

```text
# rooms/room001_hall/script.yaat

room room001_hall {
  background "rooms/room001_hall/background.bmp"

  hotspot locked_door {
    name "Locked Door"
    at 284, 52
    size 42, 116

    on click {
      if door_locked {
        say player "The door is locked."
      } else {
        goto room002_cellar
      }
    }
  }
}
```

## Graphics and sprites

Use BMP for runtime images unless the engine build explicitly includes another Win95-safe decoder. Recommended defaults:

- Backgrounds: 320x200 or 640x480 BMP, 8-bit indexed or 24-bit BGR.
- UI art: BMP with a documented transparent color if alpha is unavailable.
- Sprites: BMP sprite sheets plus INI metadata for frame rectangles.
- Cursors: `.cur` files where possible; BMP cursor sheets are acceptable if converted by the engine.

Example sprite metadata:

```ini
; graphics/sprites/player_walk.ini
[walk_down]
image=player_walk_right.bmp
frames=0,0,32,48;32,0,32,48;64,0,32,48
fps=8
```

## Audio

Recommended runtime formats:

- Music: MIDI (`.mid`) for small looping background tracks.
- Sound effects: PCM WAV (`.wav`), 8-bit or 16-bit mono/stereo at modest sample rates.
- Speech: PCM WAV or IMA ADPCM WAV if compression is needed and supported by the target system.

Avoid MP3 and OGG for the first runtime unless a decoder is validated against Windows 95 and the chosen compiler/runtime.

## Scripts

Use `.yaat` for YAAT scripts. Keep scripts ASCII-only unless the engine has an explicit localization encoding plan. Global scripts belong in `scripts/`; room-specific scripts belong beside their room data.

Typical script files:

- `scripts/startup.yaat`: Initial flags, first room, and boot events.
- `scripts/global.yaat`: Shared helpers expressed as data/events, not user-defined code functions.
- `scripts/inventory.yaat`: Inventory item events.
- `rooms/<room>/script.yaat`: Room declarations and local interactions.

## Dialog trees

Small interactions may live directly in `.yaat` event blocks. Larger branching conversations should use plain INI dialog files.

```ini
; dialogs/guard_dialog.ini
[start]
speaker=guard
text=No one enters the cellar without a key.
choices=ask_key,leave

[ask_key]
text=Where can I find the key?
reply=I heard something drop in the front hall.
next=end

[leave]
text=Never mind.
reply=Move along, then.
next=end
```

## Inventory data

Keep item definitions centralized so UI code, scripts, and save data can agree on stable item IDs.

```ini
; inventory/items.ini
[brass_key]
name=Brass Key
icon=icons/brass_key.bmp
description=A small brass key with worn teeth.
stackable=false
script=scripts/inventory.yaat

[map]
name=Map
icon=icons/map.bmp
description=A rough map of the old house.
stackable=false
```

## UI, fonts, palettes, and localization

- Put reusable panels, buttons, and inventory slots in `graphics/ui/`.
- Put cursors in `graphics/cursors/` and name them after actions, such as `look.cur`, `use.cur`, and `talk.cur`.
- Store bitmap fonts or legacy `.fnt` files in `fonts/`.
- Store shared palettes in `palettes/` and reference them from room metadata.
- Store localized strings in simple key/value files under `strings/`.

Example localization file:

```ini
; strings/en.ini
ui.start=Start
ui.load=Load
ui.quit=Quit
item.brass_key.name=Brass Key
```

## Save data

Save files should store stable IDs and state, not copied asset data. Prefer a small binary or INI-like format with an explicit version number.

Recommended contents:

- Save format version.
- Current room ID.
- Global flags and integer variables.
- Inventory item IDs.
- Object visibility or state overrides.
- Optional play time and thumbnail path.

## Optional packed release data

Development builds should load loose files first because they are easier to inspect. Release builds may pack assets into `packed/game.dat` with a table of contents and fixed-width little-endian fields. Keep the loose folder structure as the canonical source, then generate packed output with an offline tool.

## Naming and Win95 compatibility notes

- Use lowercase ASCII names with letters, numbers, and underscores.
- Keep IDs stable once scripts or save files refer to them.
- Prefer paths under 120 characters and avoid spaces in runtime asset paths.
- Avoid relying on case-sensitive filenames.
- Use forward slashes in data files if the engine normalizes them, or document backslashes consistently.
- Keep individual files small enough for old machines; split oversized speech or animation assets.
- Validate the final asset tree on a clean Windows 95-compatible runtime target or emulator before release.

## Release asset archives

The loose `game/` directory is the canonical source for YAAT assets. Developers
should edit and review files in that tree, then generate release artifacts from
it as a separate packaging step.

Use the development-time packer in `tools/asset_pack/` to create ZIP-format
`.dat` archives while preserving logical paths relative to the packed folder:

```sh
cc -std=c99 -Wall -Wextra -pedantic -o tools/asset_pack/asset_pack tools/asset_pack/asset_pack.c
./tools/asset_pack/asset_pack game game.dat
```

Patch folders can be packed the same way:

```sh
./tools/asset_pack/asset_pack patch_folder patch0000.dat
```

Generated `game.dat` and `patchNNNN.dat` files are release artifacts, not the
canonical asset source. The packer writes a manifest report next to each archive
as `<output>.manifest.txt`, listing the normalized logical paths, byte sizes,
and CRC-32 values of packed files.
## Initial packed `.dat` format

YAAT's first packed runtime asset format is a ZIP archive renamed with a `.dat`
extension, for example `game/packed/game.dat`. This keeps the packer format
inspectable with ordinary ZIP tools while the engine owns all runtime access
through `src/runtime/zip_archive.c` and `src/runtime/zip_archive.h`.

The initial runtime reader intentionally supports only a conservative subset:

- stored and deflated file entries
- central-directory table-of-contents lookup
- relative printable ASCII paths normalized to `/`
- no encryption, no ZIP64, no symlinks, and no absolute paths or `.`/`..` path segments
- caller-supplied uncompressed-size limits before allocating entry buffers

Game/runtime parsers should request files through the runtime archive wrapper and
must not parse ZIP records directly. Loose development assets may continue to use
the folder structure documented above until the engine wiring selects a `.dat`
archive at startup.
