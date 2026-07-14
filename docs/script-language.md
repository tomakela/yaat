# YAAT Script Language, Version 0

This document defines the first deliberately small scripting syntax for the YAAT C point-and-click engine. The goal is a readable format that can be parsed and interpreted by a simple hand-written C parser without implementing a general-purpose programming language.

## File extension

YAAT script files use the `.yaat` extension.

Example:

```text
intro.yaat
```

## Design goals

Version 0 keeps the language intentionally constrained:

- Declarations are block based and use `{` and `}`.
- Strings are double quoted.
- Identifiers are simple names such as `hall`, `brass_key`, or `door_locked`.
- Commands are statement oriented and end at the line break.
- There are no user-defined functions, loops, arithmetic expressions, or complex data types.
- The only control flow is `if` / `else` over boolean flags and equality checks.

## Lexical conventions

### Identifiers

Identifiers name rooms, objects, hotspots, actors, items, and flags.

Recommended identifier format:

```text
[a-zA-Z_][a-zA-Z0-9_]*
```

Examples:

```text
hall
closet
brass_key
door_locked
player
```

### Strings

Strings are enclosed in double quotes:

```text
"A small brass key."
"assets/rooms/hall.png"
```

For version 0, implementations may keep escaping minimal. A first parser can support only plain characters up to the next `"`.

### Numbers

Numbers are non-negative decimal integers used for coordinates and sizes:

```text
at 120, 80
size 32, 48
```

### Comments

Line comments start with `#` and continue to the end of the line:

```text
# This is a comment.
```

## Top-level declarations

A script file contains optional global variables followed by top-level `room` declarations. The current parser skips top-level `object` and `hotspot` blocks rather than preserving them in the script package.

```text
var door_locked = true
var has_seen_intro = false

room hall {
  background "assets/rooms/hall.png"
}

object brass_key {
  name "Brass Key"
  sprite "assets/items/brass_key.png"
}

hotspot locked_door {
  name "Locked Door"
}
```

Top-level reusable command blocks may also be declared with `event` or `proc`:

```text
event <id> {
  ...
}

proc <id> {
  ...
}
```

Both forms define a global command block named by `<id>`. These blocks do not run by themselves; invoke them from another command block with `call <id>`.

```text
event show_locked_message {
  say player "The door is locked."
}

proc unlock_door {
  set door_locked = false
  say narrator "The lock clicks open."
}

hotspot locked_door {
  on look {
    call show_locked_message
  }

  on use brass_key {
    call unlock_door
  }
}
```

For version 0, `event` and `proc` are parser aliases for the same global command-block storage and runtime `call` behavior. The intended convention is to use `event` for externally meaningful story or game events and `proc` for helper procedures shared by multiple handlers. The parser entry point `yaat_parse_script_text_into` accepts either a tokenized `event` keyword (`SCRIPT_TOKEN_KEYWORD_EVENT`) or identifier text `proc` at the top level and passes both forms to the same event parser.

### Global variables

Global variables define initial flag or simple value state.

```text
var <id> = <value>
```

Version 0 values are limited to:

- `true`
- `false`
- integer numbers
- quoted strings
- identifiers, for simple symbolic states

Examples:

```text
var door_locked = true
var coins = 0
var current_mood = calm
var player_name = "Alex"
```

Global `var` declarations are optional.

### Rooms

Rooms define places the player can visit.

```text
room <id> {
  ...
}
```

Rooms may contain room fields, event handlers, and nested object or hotspot declarations.

### Objects

Objects define interactive things. In the current parser, only objects nested inside a `room` are preserved in the script package; top-level `object` blocks are skipped.

```text
object <id> {
  ...
}
```

A nested object starts in the room where it is declared. Current parser support for object data is limited to the object id, `name`, `at`, `size`, and `on ... { ... }` event blocks; other fields are skipped.

### Hotspots

Hotspots define interactive regions that usually do not need a separate inventory identity.

```text
hotspot <id> {
  ...
}
```

Only hotspots nested inside a `room` are preserved by the current parser. Top-level `hotspot` blocks are skipped. Nested hotspots are the usual form for exits, doors, signs, scenery, and other room-specific regions. Current parser support for hotspot data is limited to the hotspot id, `name`, `at`, `size`, and `on ... { ... }` event blocks; other fields are skipped.

## Room fields

### Background

The language reserves this syntax for a room background image path, but the current parser does not preserve `background` in `YaatScriptPackage` or `YaatRoom`:

```text
background "path"
```

Example:

```text
background "assets/rooms/hall.png"
```

### Enter handler

A room can run commands when the player enters it:

```text
on enter {
  say narrator "You step into the hall."
}
```

### Nested objects and hotspots

Rooms can declare local objects and hotspots. The current parser preserves only nested room entities, their `name`, `at`, and `size` fields, and their event blocks; fields such as `sprite` are skipped:

```text
room hall {
  background "assets/rooms/hall.png"

  object key {
    name "Small Key"
    sprite "assets/items/key.png"
    at 96, 132
    size 16, 8
  }

  hotspot door {
    name "Door"
    at 280, 64
    size 32, 96
  }
}
```

## Object and hotspot fields

Objects and hotspots share the same small parser-supported field set: `name`, `at`, `size`, and event blocks. Other fields may be present in scripts but are skipped by the current parser.

### Display name

```text
name "Display Name"
```

Example:

```text
name "Brass Key"
```

### Sprite

```text
sprite "path"
```

Example:

```text
sprite "assets/items/brass_key.png"
```

The current parser skips `sprite` and does not store it in `YaatEntity`; runtime sprite changes are currently represented only by commands such as `set_sprite`. A hotspot may omit `sprite` if it is only an invisible clickable region.

### Position

```text
at x, y
```

Example:

```text
at 140, 112
```

Coordinates are integer screen or room-space coordinates. Version 0 leaves the exact coordinate origin to the engine, but the recommended origin is the upper-left corner.

### Size

```text
size w, h
```

Example:

```text
size 32, 48
```

The size field defines the clickable or visible rectangle for hit testing.

### Event handlers

Event handlers attach command blocks to player actions.

```text
on <event> {
  ...
}
```

Room object and hotspot handlers may optionally choose whether the player walks
to the target before the handler runs:

```text
on <event> walk {
  ...
}

on <event> nowalk {
  ...
}
```

`walk` is the default for room object and hotspot verb handlers. Use `nowalk`
(`no_walk` or `immediate` are accepted aliases) for handlers that should fire
in place, such as reading a far-away sign or saying a quick rejection line.

Supported version 0 events:

```text
on look { ... }
on use { ... }
on use <item> { ... }
on talk { ... }
on click { ... }
```

Examples:

```text
on look {
  say player "It looks old."
}

on use brass_key {
  set door_locked = false
  say player "The key turns in the lock."
}
```

Event meaning:

- `on look`: runs when the player selects the `look` verb and clicks the object, hotspot, or inventory item.
- `on use`: runs when the player selects the `use` verb and clicks the object, hotspot, or inventory item without a selected inventory item.
- `on use <item>`: runs when the player selects the `use` verb, selects an inventory item, and then clicks an object, hotspot, or another inventory item. For inventory combinations, `<item>` is the already-selected ingredient and the handler is attached to the target inventory item.
- `on talk`: runs when the player selects the `talk` verb and clicks the object or hotspot.
- `on take`: runs when the player selects the `take` verb and clicks the object or hotspot.
- `on open`: runs when the player selects the `open` verb and clicks the object or hotspot.
- `on close`: runs when the player selects the `close` verb and clicks the object or hotspot.
- `on click`: runs for a generic click or as the default fallback when the clicked target does not define the selected verb event.
- `on longclick`: runs when the player right-clicks an object, hotspot, or inventory item equivalent target. If this handler is absent, right-click defaults to `on look`; if neither handler exists, the normal look fallback sentence is shown.

## Verb selection and dispatch

The Win32 runtime exposes a small verb menu. The default verb list is configurable in `game/actions.ini` and currently contains:

```ini
[verbs]
verb0=look
verb1=use
verb2=talk
verb3=take
verb4=open
verb5=close
```

Runtime dispatch uses this order:

1. If the click is in the verb menu, the runtime updates the selected verb and does not dispatch a script event.
2. If the click is on an inventory item while `use` is selected and no inventory item is selected yet, the runtime stores that inventory item as the selected item.
3. If the click is on a different inventory item while `use` already has a selected inventory item, the runtime looks for `on use <selected-item>` on the clicked target inventory item.
4. If the clicked inventory item does not define that exact combination handler, it becomes the newly selected inventory item instead.
5. If the click is on a room object or hotspot, the runtime first looks for `on <selected-verb>`. Missing verb handlers immediately use the default "I can't..." feedback instead of walking to the target. A right-click is treated as `on longclick`, falling back to `on look` by default.
6. If `use` has a selected inventory item, the runtime first looks for `on use <item>` before trying plain `on use`.
7. If the selected verb handler exists and is marked `walk` or has no walk modifier, the player walks to the target and then dispatches it.
8. If the selected verb handler exists and is marked `nowalk`, the runtime dispatches it immediately without changing the walk target.

### Inventory item scripts and combinations

Inventory behavior can live in `game/scripts/inventory.yaat`. Inventory items can also opt into a script by setting `script=<path>` in `game/inventory/items.ini`; paths are resolved from the `game/` directory. Top-level `object` blocks in those scripts are treated as inventory item definitions, so their IDs should match item IDs from `items.ini`. The parser stores those top-level objects in an internal synthetic room for dispatch; that room ID is an implementation detail and scripts should not reference it.

```text
object fixed_key {
  name "Fixed Key"

  on look {
    say player "The two pieces fit together now."
  }
}

object key_teeth {
  name "Key Teeth"

  on use key_bow {
    remove_inventory key_bow
    remove_inventory key_teeth
    take fixed_key
    say player "I join the bow and teeth into a complete key."
  }
}
```

In the example, the player selects `use`, clicks `key_bow`, then clicks `key_teeth`. The runtime dispatches `on use key_bow` on `key_teeth`. Combination handlers can spend both ingredients with `remove_inventory` or `consume`, then add the result with `take`.

## Built-in commands

Commands appear inside event handlers or control-flow blocks. Version 0 commands are single-line statements.

### say

Show dialogue or narration.

```text
say <actor> "text"
```

Examples:

```text
say player "I need a key."
say narrator "The lock opens with a click."
```

### goto

Move the player to another room.

```text
goto <room>
```

Example:

```text
goto cellar
```

### take

Add an item to inventory. This legacy command does not change room entity visibility; prefer `pickup` when the script is moving a visible room object into inventory.

```text
take <item>
```

Example:

```text
take brass_key
```

### pickup

Move an object in the current room into inventory. The runtime hides the named room object and adds the inventory item.

```text
pickup <object_id> <item_id>
```

Example:

```text
pickup brass_key brass_key
```

### drop

Move an inventory item back into the current room. The runtime removes the inventory item, clears it if selected, and shows a predeclared room object at its authored position. Scripts should declare the droppable object in the room, usually hidden until `drop` runs.

```text
drop <item_id> <object_id>
```

Example:

```text
drop brass_key brass_key
```

### remove_inventory

Remove an item from inventory if the player has it. The runtime also clears the selected inventory item when it removes the selected item. Missing items are ignored.

Shared behavior note: `remove_inventory` and `consume` use the same inventory-removal behavior. `drop` also removes the inventory item through that behavior, but only in its dedicated move-to-room command path where the target room object is shown after the item is present and removed.

```text
remove_inventory <item>
```

Example:

```text
remove_inventory brass_key
```

### consume

Remove an item from inventory after it is used. `consume` has the same runtime behavior as `remove_inventory`; use this name when the script is spending or destroying the item as part of an interaction.

```text
consume <item>
```

Example:

```text
consume apple
```

### hide

Hide an object or hotspot.

```text
hide <id>
```

Example:

```text
hide brass_key
```

### show

Show an object or hotspot.

```text
show <id>
```

Example:

```text
show open_door
```

### set

Set a global variable or flag.

```text
set <flag> = <value>
```

Examples:

```text
set door_locked = false
set door_state = open
set visits = 1
```

Version 0 does not include arithmetic, so `set visits = visits + 1` is intentionally invalid.

### move

Move an object or hotspot in the current room to an absolute room position. Coordinates are integer pixels in the room coordinate system.

```text
move <object_id> <x>, <y>
```

Example:

```text
move crate 160, 96
```

### set_sprite

Change an object's sprite path at runtime. The path must be a quoted string and is loaded as the object's new sprite.

```text
set_sprite <object_id> "path"
```

Example:

```text
set_sprite door "assets/rooms/hall/door_open.png"
```


### title_card / cutscene_text

Show a full black-screen cutscene card with large blue text for a fixed duration. `cutscene_text` is an alias for `title_card`. While the card is visible, the current script event pauses and resumes automatically after the duration expires.

```text
title_card "text" <duration_ms>
cutscene_text "text" <duration_ms>
```

Example:

```text
title_card "Three days later..." 2500
```

### wait

Pause the current script event for a fixed duration before continuing with the next command. This lets room-based cutscenes sequence dialogue, actor motion, and automatic room transitions.

```text
wait <duration_ms>
```

Example:

```text
say narrator "The guard steps aside."
wait 1200
goto courtyard
```

### move_player

Place the player at an absolute room position and stop any current walk target. This is intended for room cutscenes and scripted blocking. Coordinates are integer pixels in the room coordinate system.

```text
move_player <x>, <y>
```

Example:

```text
move_player 96, 160
```

### hide_player / show_player

Hide or show the player sprite. Use these commands for rooms that function as cutscene stages, title rooms, establishing shots, or other scenes where no player should appear.

```text
hide_player
show_player
```

Example:

```text
on enter {
  hide_player
  say narrator "Only the empty hallway remains."
  wait 2000
  goto next_room
}
```

### play_sound

Play a sound effect.

```text
play_sound "path"
```

Example:

```text
play_sound "assets/sounds/unlock.wav"
```

### shake

Temporarily shake the in-room camera/render layer. The dialogue and UI layer remains steady.

```text
shake <duration_ms> <magnitude>
```

`duration_ms` is clamped by the runtime to a safe maximum and `magnitude` is clamped to keep offsets inside the backbuffer.

Example:

```text
shake 350 6
```

### call

Run a global event or procedure by id. `call` looks up an `event` declared at the top level of the script package and executes its commands immediately. Calls are limited by the runtime's script call-depth guard to prevent runaway recursion.

```text
call <event_or_proc_id>
```

Example:

```text
call unlock_door
```

## Control flow

The control-flow construct in version 0 is `if` / `else`.

```text
if <condition> {
  ...
} else {
  ...
}
```

The `else` block is optional:

```text
if <condition> {
  ...
}
```

### Boolean flags

A bare identifier condition checks whether a flag is true:

```text
if door_locked {
  say player "It is locked."
}
```

This is equivalent to checking `door_locked == true`.

### Inventory conditions

The parser and Win32 runtime also support a special inventory condition form:

```text
if has <item> {
  ...
}
```

This condition is true when `<item>` is currently in the player's inventory. The keyword `inventory` is accepted as an alias for `has`.

Examples:

```text
if has brass_key {
  say player "I have the key."
}

if inventory lantern {
  say player "The lantern should help."
} else {
  say player "I need a light source first."
}
```

### Equality checks

Simple comparisons compare a variable to a value:

```text
if <id> <op> <value> {
  ...
}
```

Examples:

```text
if door_locked == false {
  goto treasure_room
}

if door_state == open {
  say player "The door is already open."
}

if coins == 3 {
  say player "That should be enough."
}
```

Version 0 defines `==`, `!=`, `<`, `<=`, `>`, and `>=` comparisons. Boolean `and`, boolean `or`, and boolean `not` are intentionally not included.

## Complete sample script

The following complete `.yaat` script defines a two-room puzzle. The player starts in the hall, picks up a key, unlocks a door, and enters the treasure room.

```text
# two_room_key_puzzle.yaat

var door_locked = true
var took_key = false

room hall {
  background "assets/rooms/hall.png"

  on enter {
    say narrator "You are standing in a quiet hall."
  }

  object brass_key {
    name "Brass Key"
    sprite "assets/items/brass_key.png"
    at 96, 136
    size 18, 10

    on look {
      say player "A small brass key is lying on the floor."
    }

    on click {
      pickup brass_key brass_key
      set took_key = true
      say player "I picked up the brass key."
    }
  }

  hotspot locked_door {
    name "Locked Door"
    at 284, 52
    size 42, 116

    on look {
      if door_locked {
        say player "The door is locked. The keyhole is scratched."
      } else {
        say player "The door is unlocked now."
      }
    }

    on use {
      if door_locked {
        say player "It will not open. I need a key."
      } else {
        goto treasure_room
      }
    }

    on use brass_key {
      if door_locked {
        play_sound "assets/sounds/unlock.wav"
        set door_locked = false
        say player "The key fits. The door is unlocked."
      } else {
        say player "The door is already unlocked."
      }
    }

    on click {
      if door_locked {
        say player "Locked."
      } else {
        goto treasure_room
      }
    }
  }
}

room treasure_room {
  background "assets/rooms/treasure_room.png"

  on enter {
    say narrator "You step into a small room glittering with treasure."
  }

  hotspot exit_door {
    name "Door Back to the Hall"
    at 12, 54
    size 38, 112

    on look {
      say player "That door leads back to the hall."
    }

    on click {
      goto hall
    }
  }

  object treasure_chest {
    name "Treasure Chest"
    sprite "assets/objects/chest.png"
    at 156, 128
    size 64, 48

    on look {
      say player "This must be what I came for."
    }

    on click {
      say narrator "You found the treasure."
    }
  }
}
```

## Suggested parser notes

A first C implementation can parse this language with a small tokenizer and recursive-descent parser:

1. Tokenize identifiers, strings, integers, `{`, `}`, `,`, `=`, `==`, `!=`, `<`, `<=`, `>`, and `>=`.
2. Parse top-level declarations until end of file.
3. Parse blocks by reading declarations, fields, and handlers until `}`.
4. Parse command blocks as lists of commands and `if` statements.
5. Store declarations in simple structs keyed by identifier.
6. Resolve `goto`, `take`, `hide`, `show`, and `on use <item>` identifiers after parsing or at runtime.

The language intentionally avoids expression precedence, nested function calls, and arbitrary statements so that version 0 can fit a compact engine interpreter.

## Runtime string IDs for localization

Readable script text stays inline in English. Any text-producing command that should be localizable may add a same-line string-id comment after its normal arguments:

```yaat
say player "The door is locked."  # @intro.locked_door
title_card "Chapter One" 2500  # @intro.chapter_one
```

At runtime, YAAT keeps the English literal as the fallback. When the selected runtime language is not English, the engine looks up the `@` id in `game/strings/<lang>.ini`; if the id is absent, or the language file is missing, the inline English text is displayed unchanged. String ids are plain dotted identifiers by convention and should be stable once shipped.

The asset validator reports script text lines that do not have a `# @string.id` label so localizable text can be audited without making labels mandatory during early prototyping.
