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

A script file contains optional global variables followed by top-level `room`, `object`, and `hotspot` declarations.

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

Objects define interactive things. They can be declared at the top level or nested inside a room.

```text
object <id> {
  ...
}
```

A top-level object can represent an inventory item or a reusable object definition. A nested object starts in the room where it is declared.

### Hotspots

Hotspots define interactive regions that usually do not need a separate inventory identity.

```text
hotspot <id> {
  ...
}
```

Hotspots can be declared at the top level or nested inside a room. Nested hotspots are the usual form for exits, doors, signs, scenery, and other room-specific regions.

## Room fields

### Background

Each room can define a background image path:

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

Rooms can declare local objects and hotspots:

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

Objects and hotspots share the same small field set.

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

A hotspot may omit `sprite` if it is only an invisible clickable region.

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
- `on use`: runs when the player selects the `use` verb and clicks the object or hotspot without a selected inventory item.
- `on use <item>`: runs when the player selects the `use` verb, selects an inventory item, and then clicks the object or hotspot.
- `on talk`: runs when the player selects the `talk` verb and clicks the object or hotspot.
- `on take`: runs when the player selects the `take` verb and clicks the object or hotspot.
- `on open`: runs when the player selects the `open` verb and clicks the object or hotspot.
- `on close`: runs when the player selects the `close` verb and clicks the object or hotspot.
- `on click`: runs for a generic click or as the default fallback when the clicked target does not define the selected verb event.

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
2. If the click is on an inventory item while `use` is selected, the runtime stores that inventory item as the selected item.
3. If the click is on a room object or hotspot, the runtime first looks for `on <selected-verb>`.
4. If `use` has a selected inventory item, the runtime first looks for `on use <item>` before trying plain `on use`.
5. If the selected verb handler is missing, the runtime falls back to `on click`.
6. If no matching event exists, no commands run.

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
