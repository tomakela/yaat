# Task: Document Win95 Point-and-Click Adventure Asset Structure

## Goal

Create a Markdown document that defines the recommended folder and data structure for a simple Windows 95-compatible point-and-click adventure game.

## Background

The game needs an organized asset layout covering rooms, graphics, scripts, dialogs, audio, inventory, UI, fonts, palettes, localization strings, save data, and optional release packaging.

The target platform is Windows 95, so the structure should favor simple files and formats such as BMP, WAV, MIDI, INI, and TXT. Graphics should be designed around a fixed 320x200 resolution and a 256-color palette.

## Deliverable

Add a Markdown document, for example `docs/asset-structure.md`, that explains the proposed structure and includes examples.

## Suggested Contents

The document should cover:

- Overall project folder layout.
- Room-specific folder structure.
- Graphics and sprite organization.
- Audio organization for music, sound effects, and speech.
- Script file organization.
- Dialog tree data.
- Inventory item data and icons.
- UI assets and cursors.
- Fonts and palettes.
- Localization strings.
- Save-game data.
- Optional packed release data.
- Recommended Win95-compatible file formats.
- Naming conventions and compatibility notes.
- A clear technical limitation that target room/background art uses 320x200 resolution with a 256-color palette.

## Proposed Folder Structure

```text
game/
  game.ini
  rooms/
    room001/
      room.ini
      background.bmp
      walkmask.bmp
      zmask.bmp
      hotspots.ini
      objects.ini
      exits.ini
      script.txt
  graphics/
    cursors/
    ui/
    sprites/
  audio/
    music/
    sfx/
    speech/
  scripts/
    global.txt
    startup.txt
    inventory.txt
    dialogs/
  dialogs/
  inventory/
    items.ini
    icons/
  fonts/
  palettes/
  strings/
  movies/
  save/
  tools/
  packed/
```

## Acceptance Criteria

- The document is written in Markdown.
- The document includes at least one full example folder tree.
- The document explains what each top-level folder is for.
- The document includes example data snippets for at least one room, one inventory item, and one dialog tree.
- The document includes Win95 compatibility recommendations.
- The document is clear enough for artists, writers, and programmers to use as a shared reference.

## Notes

Keep the first version practical and human-editable. Prefer simple formats and examples over a complex custom asset pipeline.
