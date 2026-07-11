# YAAT script bytecode package (`.yaatbc`)

YAAT bytecode is an offline-compiled package for Win95-era ANSI C loaders. Version 5 is intentionally simple: fixed-size records, little-endian integers, no pointer-sized fields, and no required unaligned integer reads.

## Header

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 8 | Magic bytes `YAATBC\0\0` |
| 8 | 2 | Format version, currently `5` |
| 10 | 2 | Flags, currently `0` |
| 12 | 2 | Variable count |
| 14 | 2 | Room count |
| 16 | 2 | Command count |
| 18 | 2 | Global event count |

## String table and strings

Version 5 stores strings inline as fixed-width NUL-terminated byte arrays rather than a separate offset table. Loaders must clamp the final byte to `NUL` after reading. Future versions may add a string table section while preserving the header magic/version gate.

## Records

Records are serialized in this order: variables, commands, global events, rooms. Room records contain room events followed by entities; entity records contain entity events.
Version 5 stores strings inline as fixed-width NUL-terminated byte arrays rather than a separate offset table. Loaders must clamp the final byte to `NUL` after reading. Commands also include a `string_id[64]` field for runtime string-table lookups; when no external string is referenced, the inline command string remains the fallback text.

## Records

Records are serialized in this order: variables, commands, global events, then rooms. Global event records and room/entity event records use the same event layout. Room records contain room events followed by entities; entity records contain entity events.

* Variable: `name[32]`, typed value (`kind:u16`, `bool:u16`, `int:u32`, `string[96]`).
* Command: `opcode:u16`, `a[96]`, `b[96]`, `string_id[64]`, legacy `bool:u16`, legacy `int:u16`, typed value (`kind:u16`, `bool:u16`, `int:u32`, `string[96]`), `condition_op:u16`, `first_child:u16`, `child_count:u16`, `first_else_child:u16`, `else_child_count:u16`.
* Event: `name[32]`, optional inventory `item[32]`, `first_command:u16`, `command_count:u16`.
* Room: `id[32]`, `label[64]`, `color:u32`, `event_count:u16`, `entity_count:u16`, then events and entities.
* Command: `opcode:u16`, `a[96]`, `b[96]`, `bool:u16`, `int:u16`, `first_child:u16`, `child_count:u16`, `first_else_child:u16`, `else_child_count:u16`.
* Command: `opcode:u16`, `a[96]`, `b[96]`, `string_id[64]`, legacy `bool:u16`, legacy `int:u16`, typed value (`kind:u16`, `bool:u16`, `int:u32`, `string[96]`), `condition_op:u16`, `first_child:u16`, `child_count:u16`, `first_else_child:u16`, `else_child_count:u16`.

## Command opcodes

Serialized opcodes map deterministically to runtime command kinds: `0=YAAT_CMD_SAY`, `1=YAAT_CMD_SET`, `2=YAAT_CMD_GOTO`, `3=YAAT_CMD_PLAY_SOUND`, `4=YAAT_CMD_TAKE`, `5=YAAT_CMD_HIDE`, `6=YAAT_CMD_IF`, `7=YAAT_CMD_SHAKE`, `8=YAAT_CMD_PICKUP`, `9=YAAT_CMD_DROP`, `10=YAAT_CMD_REMOVE_INVENTORY`, `11=YAAT_CMD_CONSUME`, `12=YAAT_CMD_CALL`, `13=YAAT_CMD_SHOW`, `14=YAAT_CMD_MOVE_OBJECT`, `15=YAAT_CMD_SET_OBJECT_SPRITE`, `16=YAAT_CMD_TITLE_CARD`, `17=YAAT_CMD_WAIT`, `18=YAAT_CMD_MOVE_PLAYER`, `19=YAAT_CMD_SET_PLAYER_VISIBLE`, `20=YAAT_CMD_DIALOG`, `21=YAAT_CMD_CHOICE`, and `22=YAAT_CMD_ANIMATE_OBJECT`.
* Entity: `kind:u16` (`0` hotspot, `1` object, `2` NPC), `id[32]`, `name[64]`, `x/y/w/h:u16`, `visible:u16`, `event_count:u16`, then events.

## Command opcodes

Serialized opcodes map directly to the declaration order of `YaatCommandKind`:

| Opcode | Runtime kind | Serialized operands |
| ---: | --- | --- |
| 0 | `YAAT_CMD_SAY` | `a=speaker`, `b=inline text`, optional `string_id` lookup key |
| 1 | `YAAT_CMD_SET` | `a=variable name`, typed `value` is the right-hand value; legacy `bool`/`int` mirror scalar values for compatibility |
| 2 | `YAAT_CMD_GOTO` | `a=room id` |
| 3 | `YAAT_CMD_PLAY_SOUND` | `a=sound asset id/path` |
| 4 | `YAAT_CMD_TAKE` | `a=inventory item id` |
| 5 | `YAAT_CMD_HIDE` | `a=object id` |
| 6 | `YAAT_CMD_IF` | `a=variable/condition name`, optional `b=inventory item`, `condition_op`, typed comparison `value`, child and else-child command ranges |
| 7 | `YAAT_CMD_SHAKE` | legacy `bool=duration_ms`, legacy `int=magnitude` |
| 8 | `YAAT_CMD_PICKUP` | `a=object id`, `b=inventory item id` |
| 9 | `YAAT_CMD_DROP` | `a=inventory item id`, `b=object id` |
| 10 | `YAAT_CMD_REMOVE_INVENTORY` | `a=inventory item id` |
| 11 | `YAAT_CMD_CONSUME` | `a=inventory item id` |
| 12 | `YAAT_CMD_CALL` | `a=global event name` |
| 13 | `YAAT_CMD_SHOW` | `a=object id` |
| 14 | `YAAT_CMD_MOVE_OBJECT` | `a=object id`, legacy `bool=x`, legacy `int=y` |
| 15 | `YAAT_CMD_SET_OBJECT_SPRITE` | `a=object id`, `b=sprite id/path` |
| 16 | `YAAT_CMD_TITLE_CARD` | `a=inline text`, optional `string_id` lookup key, legacy `int=duration_ms` |
| 17 | `YAAT_CMD_WAIT` | legacy `int=duration_ms` |
| 18 | `YAAT_CMD_MOVE_PLAYER` | legacy `bool=x`, legacy `int=y` |
| 19 | `YAAT_CMD_SET_PLAYER_VISIBLE` | legacy `bool=visible` (`0` hidden, nonzero shown) |
| 20 | `YAAT_CMD_DIALOG` | `a=dialog id` |
| 21 | `YAAT_CMD_CHOICE` | `a=choice id` |
| 22 | `YAAT_CMD_ANIMATE_OBJECT` | `a=object id`, `b=animation id` |

Version 2 value encoding is command-specific: booleans are `u16` `0` or `1`; identifiers, asset paths, speakers, and dialogue text are inline strings. `YAAT_CMD_IF` uses `a` as the variable name and child command ranges for branches. `YAAT_CMD_SHAKE` stores the duration in `bool_value` and the magnitude in `int_value`. `YAAT_CMD_PICKUP` stores `object_id` in `a` and `item_id` in `b`; `YAAT_CMD_DROP` stores `item_id` in `a` and `object_id` in `b`.
Version 5 stores script values as tagged values: `0=bool`, `1=int`, and `2=string`. `YAAT_CMD_SET` stores its right-hand value in the typed value field. `YAAT_CMD_IF` uses `a` as the variable name, `condition_op` as `0=truthy`, `1===`, `2!=`, `3<`, `4<=`, `5>`, or `6>=`, and the typed value field as the right-hand operand for comparison forms. `YAAT_CMD_SHAKE` keeps duration in the legacy `bool_value` field and magnitude in the legacy `int_value` field.
## Values and runtime notes

Current value encoding stores script values as tagged values: `0=bool`, `1=int`, and `2=string`. The typed value payload is always serialized as `kind:u16`, `bool:u16`, `int:u32`, and `string[96]`, even when a command does not use it.

`YAAT_CMD_SET` stores its right-hand value in the typed value field. `YAAT_CMD_IF` uses `a` as the variable or special condition name, `condition_op` as `0=truthy`, `1===`, `2!=`, `3<`, `4<=`, `5>`, or `6>=`, and the typed value field as the right-hand operand for comparison forms. Truthy inventory conditions may also use `b` for the inventory item id.

Several runtime commands still use the legacy scalar fields for compact numeric operands: `YAAT_CMD_SHAKE` stores duration in `bool_value` and magnitude in `int_value`; object/player movement stores `x` in `bool_value` and `y` in `int_value`; title cards and waits store duration in `int_value`; player visibility stores the visible flag in `bool_value`.

Global events are serialized after commands and before rooms. `YAAT_CMD_CALL` resolves its `a` field against this global event table and executes the referenced command range with runtime recursion-depth protection.

## Endian and alignment

All integers are unsigned little-endian 16-bit or 32-bit values. Files are byte-packed; readers must copy bytes and assemble integers manually instead of casting file bytes to C structs. No field requires more than byte alignment, which keeps loaders portable to older Win32 C compilers and avoids ABI padding differences.
