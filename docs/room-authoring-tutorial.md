# Room Authoring Tutorial: Populating a Complete YAAT Scene

This tutorial walks through building a complete point-and-click room in YAAT: metadata, background graphics, visible objects, invisible hotspots, inventory links, and script functions/procedures. It is written as a practical recipe you can copy into a new `game/rooms/<room_id>/` folder and adapt.

The example room is a **workshop** scene with:

- a painted background and palette;
- several visible objects, including a key, a toolbox, a lamp, and a machine;
- hotspot-only regions, including a poster, a locked hatch, and an exit;
- object graphics with transparency metadata;
- room entry logic, reusable script procedures, inventory checks, animation/sprite swaps, sound effects, camera shake, and room transitions.

## 1. Pick IDs and create the room folder

Use lowercase ASCII IDs and short paths so the room works in loose development trees and packed Win95-friendly archives.

```text
game/
  rooms/
    room003_workshop/
      room.ini
      background.bmp
      hotspots.ini
      objects.ini
      exits.ini
      script.yaat
      objects/
        tiny_key.bmp
        toolbox_closed.bmp
        toolbox_open.bmp
        lamp_0.bmp
        lamp_1.bmp
        lamp_2.bmp
        machine_idle.bmp
        machine_on.bmp
```

Recommended naming rules:

- Room folder: `roomNNN_short_name`, for example `room003_workshop`.
- Object IDs: noun phrases such as `tiny_key`, `toolbox`, `bench_lamp`.
- Hotspot IDs: target names such as `safety_poster`, `locked_hatch`, `hall_exit`.
- Inventory item IDs: match the pickup where possible, for example `tiny_key`.

## 2. Add room metadata in `room.ini`

`room.ini` tells the engine how to draw and initialize the room. Keep paths relative to the room folder unless they point to shared assets.

```ini
; game/rooms/room003_workshop/room.ini
[id]
name=room003_workshop
label=Workshop

[display]
width=320
height=200
background=background.bmp
palette=../../palettes/main.pal

[scale]
near_y=190
near_scale=1.15
far_y=80
far_scale=0.65

[audio]
music=../../audio/music/intro.mid

[script]
file=script.yaat
```

Field notes:

- `[id].name` should match the folder and the `room <id>` block in `script.yaat`.
- `[display].background` is the room-sized BMP backdrop.
- `[display].palette` points at the shared game palette when using indexed BMP art.
- `[scale]` gives the renderer simple near/far player scaling hints.
- `[audio].music` starts or changes background MIDI music for this room.
- `[script].file` is the room-local YAAT script.

## 3. Prepare the background graphic

For the current YAAT baseline, use BMP for runtime graphics. A safe first target is:

- `320x200` pixels;
- 8-bit indexed BMP using `game/palettes/main.pal`, or 24-bit BMP if palette sharing is not important yet;
- no alpha channel for backgrounds;
- the exact filename referenced by `room.ini`, usually `background.bmp`.

Authoring tip: draw your background first, then record object rectangles from the image editor. YAAT coordinates use the upper-left corner as `0,0`.

## 4. Define visible room objects in `objects.ini`

Objects are drawn sprites that can also be clicked. Put per-room sprites under `objects/` to keep the room self-contained.

```ini
; game/rooms/room003_workshop/objects.ini
[tiny_key]
name=Tiny Key
sprite=objects/tiny_key.bmp
transparency=color-key
transparent_color=#ff00ff
x=62
y=154
width=16
height=8
walk_x=70
walk_y=166
visible=true
inventory_item=tiny_key
script_event=on_click

[toolbox]
name=Toolbox
sprite=objects/toolbox_closed.bmp
transparency=color-key
transparent_color=#ff00ff
x=112
y=138
width=42
height=26
walk_x=132
walk_y=168
visible=true
script_event=on_click

[bench_lamp]
name=Bench Lamp
sprite=objects/lamp_0.bmp
transparency=color-key
transparent_color=#ff00ff
animation=flicker
animation_frames=objects/lamp_0.bmp;objects/lamp_1.bmp;objects/lamp_2.bmp
animation_fps=6
x=202
y=76
width=22
height=44
walk_x=214
walk_y=150
visible=true
script_event=on_look

[repair_machine]
name=Repair Machine
sprite=objects/machine_idle.bmp
transparency=color-key
transparent_color=#ff00ff
x=232
y=104
width=54
height=62
walk_x=256
walk_y=170
visible=true
script_event=on_use
```

Object field checklist:

- `name`: display name in UI/tooltips.
- `sprite`: initial BMP path.
- `transparency`: usually `color-key` for Win95-era BMP sprites or `alpha` for 32-bit BMPs in newer builds.
- `transparent_color`: the keyed-out color when using `color-key`; magenta `#ff00ff` is the common placeholder.
- `x`, `y`, `width`, `height`: draw rectangle and click rectangle.
- `walk_x`, `walk_y`: where the player should stand before interacting, when the runtime supports walk-to interaction.
- `visible`: initial visibility. Hidden objects can be shown later with `show`.
- `inventory_item`: item added when `pickup <object_id> <item_id>` runs.
- `script_event`: legacy data hint for tools; the script file still defines the real event handlers.
- `animation_*`: optional static-frame animation data; keep `sprite=` as the safe fallback frame.

## 5. Define invisible clickable regions in `hotspots.ini`

Hotspots are clickable rectangles without their own sprite. Use them for doors, signs, machinery panels, exits, and background details.

```ini
; game/rooms/room003_workshop/hotspots.ini
[safety_poster]
name=Safety Poster
rect=18,34,62,48
cursor=look
script_event=on_look

[locked_hatch]
name=Locked Hatch
rect=286,92,30,72
cursor=use
script_event=on_use

[hall_exit]
name=Exit to Hall
rect=0,84,20,96
cursor=use
script_event=on_click
```

Hotspot field checklist:

- `rect=x,y,w,h`: clickable rectangle.
- `cursor`: suggested verb or cursor, such as `look`, `use`, or `talk`.
- `script_event`: legacy data hint; script dispatch is still controlled by `on look`, `on use`, and related handlers.

## 6. Add optional data-driven exits in `exits.ini`

Use `exits.ini` for simple transitions that tools can inspect. Keep scripted exits in `script.yaat` when the exit depends on a puzzle state.

```ini
; game/rooms/room003_workshop/exits.ini
[hall_exit]
target_room=room001_intro
rect=0,84,20,96
walk_x=24
walk_y=158
```

In the example below, `hall_exit` also has script handlers so it can play dialogue or sound before leaving.

## 7. Write the complete room script

The script is where the room becomes interactive. It should repeat the object and hotspot IDs from `objects.ini` and `hotspots.ini`, then attach event handlers.

```text
# game/rooms/room003_workshop/script.yaat

var workshop_seen = false
var toolbox_open = false
var machine_powered = false
var hatch_unlocked = false

proc describe_power_state {
  if machine_powered {
    say narrator "The repair machine hums with a steady electric note."  # @rooms.room003_workshop.power_on
  } else {
    say narrator "The repair machine is silent. Its power socket is empty."  # @rooms.room003_workshop.power_off
  }
}

proc open_toolbox {
  if toolbox_open {
    say player "The toolbox is already open."  # @rooms.room003_workshop.toolbox_already_open
  } else {
    set toolbox_open = true
    set_sprite toolbox "rooms/room003_workshop/objects/toolbox_open.bmp"
    play_sound "audio/sfx/start_click.wav"
    say narrator "The toolbox lid pops open."  # @rooms.room003_workshop.toolbox_opened
    show tiny_key
  }
}

proc unlock_hatch {
  if hatch_unlocked {
    say player "The hatch is already unlocked."  # @rooms.room003_workshop.hatch_already_unlocked
  } else {
    set hatch_unlocked = true
    consume tiny_key
    play_sound "audio/sfx/door_unlock.wav"
    shake 350 6
    say player "The tiny key fits the hatch lock."  # @rooms.room003_workshop.hatch_unlocked
  }
}

room room003_workshop {
  background "rooms/room003_workshop/background.bmp"

  on enter {
    show_player
    if workshop_seen {
      say narrator "You are back in the cluttered workshop."  # @rooms.room003_workshop.enter_again
    } else {
      set workshop_seen = true
      title_card "The Workshop" 1200  # @rooms.room003_workshop.title
      say narrator "Oil, dust, and old tools fill the little workshop."  # @rooms.room003_workshop.enter_first
    }
  }

  object tiny_key {
    name "Tiny Key"
    sprite "rooms/room003_workshop/objects/tiny_key.bmp"
    at 62, 154
    size 16, 8

    on look {
      say player "A tiny key is hidden near the bench leg."  # @rooms.room003_workshop.key_look
    }

    on take {
      pickup tiny_key tiny_key
      play_sound "audio/sfx/pickup_key.wav"
      say player "I pocket the tiny key."  # @rooms.room003_workshop.key_take
    }

    on click {
      pickup tiny_key tiny_key
      play_sound "audio/sfx/pickup_key.wav"
      say player "I pocket the tiny key."  # @rooms.room003_workshop.key_click
    }
  }

  object toolbox {
    name "Toolbox"
    sprite "rooms/room003_workshop/objects/toolbox_closed.bmp"
    at 112, 138
    size 42, 26

    on look {
      if toolbox_open {
        say player "The open toolbox smells like old iron."  # @rooms.room003_workshop.toolbox_look_open
      } else {
        say player "The toolbox is shut, but the latch looks loose."  # @rooms.room003_workshop.toolbox_look_closed
      }
    }

    on open {
      call open_toolbox
    }

    on use {
      call open_toolbox
    }

    on click {
      call open_toolbox
    }
  }

  object bench_lamp {
    name "Bench Lamp"
    sprite "rooms/room003_workshop/objects/lamp_0.bmp"
    at 202, 76
    size 22, 44

    on look {
      say player "The lamp flickers just enough to make the shadows move."  # @rooms.room003_workshop.lamp_look
    }

    on use {
      play_sound "audio/sfx/start_click.wav"
      say player "The switch clicks, but the lamp keeps flickering."  # @rooms.room003_workshop.lamp_use
    }
  }

  object repair_machine {
    name "Repair Machine"
    sprite "rooms/room003_workshop/objects/machine_idle.bmp"
    at 232, 104
    size 54, 62

    on look {
      call describe_power_state
    }

    on use {
      if machine_powered {
        say player "The machine is already running."  # @rooms.room003_workshop.machine_already_on
      } else {
        set machine_powered = true
        set_sprite repair_machine "rooms/room003_workshop/objects/machine_on.bmp"
        play_sound "audio/sfx/start_click.wav"
        say narrator "The repair machine coughs, then starts."  # @rooms.room003_workshop.machine_started
      }
    }
  }

  hotspot safety_poster {
    name "Safety Poster"
    at 18, 34
    size 62, 48

    on look {
      say narrator "The poster reads: keep small keys away from large machines."  # @rooms.room003_workshop.poster_look
    }
  }

  hotspot locked_hatch {
    name "Locked Hatch"
    at 286, 92
    size 30, 72

    on look {
      if hatch_unlocked {
        say player "The hatch is unlocked and ready to open."  # @rooms.room003_workshop.hatch_look_unlocked
      } else {
        say player "A small hatch blocks the way. It has a tiny keyhole."  # @rooms.room003_workshop.hatch_look_locked
      }
    }

    on use {
      if hatch_unlocked {
        goto room002_exit
      } else {
        if has tiny_key {
          call unlock_hatch
        } else {
          say player "I need a tiny key for this tiny lock."  # @rooms.room003_workshop.hatch_need_key
        }
      }
    }

    on use tiny_key {
      call unlock_hatch
    }

    on open {
      if hatch_unlocked {
        goto room002_exit
      } else {
        say player "It will not open while it is locked."  # @rooms.room003_workshop.hatch_open_locked
      }
    }

    on click {
      if hatch_unlocked {
        goto room002_exit
      } else {
        say player "Locked."  # @rooms.room003_workshop.hatch_click_locked
      }
    }
  }

  hotspot hall_exit {
    name "Exit to Hall"
    at 0, 84
    size 20, 96

    on look {
      say player "That way leads back to the intro room."  # @rooms.room003_workshop.exit_look
    }

    on click {
      goto room001_intro
    }

    on use {
      goto room001_intro
    }
  }
}
```

### Why the script is structured this way

- Global `var` declarations store puzzle state that must survive repeated interactions.
- `proc describe_power_state`, `proc open_toolbox`, and `proc unlock_hatch` are reusable command blocks. Handlers call them with `call <proc_id>` instead of duplicating dialogue and state changes.
- `on enter` handles first-visit and repeat-visit narration.
- Objects define `name`, `sprite`, `at`, and `size` in script so the parser can preserve interactive entity data. The INI files still remain useful to asset tools and runtime loaders.
- `pickup tiny_key tiny_key` hides the visible room object and adds the inventory item.
- `if has tiny_key` checks inventory before unlocking the hatch.
- `consume tiny_key` removes the key after it unlocks the hatch.
- `set_sprite` changes graphics after state changes, such as opening the toolbox or powering the machine.
- `play_sound`, `shake`, `title_card`, and `wait`-style sequencing commands give interactions feedback without requiring a full general-purpose language.
- `goto` transitions to another room after the puzzle is solved.

## 8. Add inventory metadata if the room gives the player an item

When an object can be picked up, make sure the inventory item exists in `game/inventory/items.ini` and has an icon.

```ini
; game/inventory/items.ini
[tiny_key]
name=Tiny Key
icon=icons/tiny_key.bmp
script=../scripts/inventory.yaat
```

Then add optional inventory-specific behavior in `game/scripts/inventory.yaat`:

```text
object tiny_key {
  name "Tiny Key"

  on look {
    say player "A very small key for a very small lock."  # @inventory.tiny_key.look
  }
}
```

If you only need the item to work with `on use tiny_key` handlers in rooms, the inventory script can stay minimal.

## 9. Connect the room from another room

A room is reachable only if the game starts there or another script transitions to it. Add a door, sign, or debug hotspot in an existing room:

```text
hotspot workshop_door {
  name "Workshop Door"
  at 220, 60
  size 36, 96

  on look {
    say player "The workshop is through there."  # @rooms.room001_intro.workshop_door_look
  }

  on click {
    goto room003_workshop
  }
}
```

For early testing, you can also temporarily set the first room in `game/game.ini` to `room003_workshop` if your build and tools support that flow.

## 10. Validate the room

From the repository root, build and run the asset validator after adding files:

```sh
cc -std=c99 -Wall -Wextra -pedantic -o tools/asset_validate/asset_validate tools/asset_validate/asset_validate.c
./tools/asset_validate/asset_validate game
```

Also compile or package scripts with the script tooling used by your current workflow. For parser-only checks, keep script syntax deliberately simple:

- one command per line;
- balanced `{` and `}` blocks;
- quoted strings for text and paths;
- no arithmetic expressions;
- no boolean `and`, `or`, or `not`;
- nested `object` and `hotspot` blocks inside the room, not only top-level blocks.

## Complete room checklist

Before considering a room complete, confirm each item below:

- [ ] Folder exists under `game/rooms/<room_id>/`.
- [ ] `room.ini` has matching `[id].name`, `[display]`, and `[script]` data.
- [ ] `background.bmp` exists and matches the configured dimensions.
- [ ] Every `objects.ini` sprite path exists.
- [ ] Every object has sensible `x`, `y`, `width`, and `height` values.
- [ ] Every hotspot rectangle covers the intended background detail.
- [ ] `script.yaat` contains `room <room_id> { ... }`.
- [ ] Script object and hotspot IDs match the INI IDs.
- [ ] Every pickup object has an inventory item definition.
- [ ] Every `goto` target room exists or is intentionally stubbed for later.
- [ ] Every sound path used by `play_sound` exists.
- [ ] Dialogue that should be localized has a stable `# @string.id` comment.
- [ ] The asset validator runs cleanly, or warnings are understood and tracked.

## Common mistakes

- **Mismatch between room IDs:** `room.ini`, folder name, and `room <id>` should agree.
- **Top-level object-only scripts:** put playable room objects and hotspots inside the `room` block so the current parser preserves them as room entities.
- **Missing inventory item:** `pickup tiny_key tiny_key` needs the second ID to exist as inventory data.
- **Wrong paths in script:** script asset paths are usually written from the `game/` root, such as `rooms/room003_workshop/objects/toolbox_open.bmp`.
- **Wrong paths in INI:** room INI paths are usually relative to the room folder, such as `objects/toolbox_closed.bmp`.
- **No fallback interaction:** add `on click` to important targets so generic clicks still do something useful.
- **Changing sprites without declaring the object:** `set_sprite toolbox "..."` only works if `toolbox` is a known room object.
