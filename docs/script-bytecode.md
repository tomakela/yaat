# YAAT script bytecode package (`.yaatbc`)

YAAT bytecode is an offline-compiled package for Win95-era ANSI C loaders. Version 3 is intentionally simple: fixed-size records, little-endian integers, no pointer-sized fields, and no required unaligned integer reads.

## Header

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 8 | Magic bytes `YAATBC\0\0` |
| 8 | 2 | Format version, currently `3` |
| 10 | 2 | Flags, currently `0` |
| 12 | 2 | Variable count |
| 14 | 2 | Room count |
| 16 | 2 | Command count |
| 18 | 2 | Reserved, must be `0` |

## String table and strings

Version 3 stores strings inline as fixed-width NUL-terminated byte arrays rather than a separate offset table. Loaders must clamp the final byte to `NUL` after reading. Future versions may add a string table section while preserving the header magic/version gate.

## Records

Records are serialized in this order: variables, commands, rooms. Room records contain room events followed by entities; entity records contain entity events.

* Variable: `name[32]`, typed value (`kind:u16`, `bool:u16`, `int:u32`, `string[96]`).
* Event: `name[32]`, optional inventory `item[32]`, `first_command:u16`, `command_count:u16`.
* Entity: `kind:u16` (`0` hotspot, `1` object), `id[32]`, `name[64]`, `x/y/w/h:u16`, `visible:u16`, `event_count:u16`, then events.
* Room: `id[32]`, `label[64]`, `color:u32`, `event_count:u16`, `entity_count:u16`, then events and entities.
* Command: `opcode:u16`, `a[96]`, `b[96]`, `bool:u16`, `int:u16`, `first_child:u16`, `child_count:u16`, `first_else_child:u16`, `else_child_count:u16`.
* Command: `opcode:u16`, `a[96]`, `b[96]`, legacy `bool:u16`, legacy `int:u16`, typed value (`kind:u16`, `bool:u16`, `int:u32`, `string[96]`), `condition_op:u16`, `first_child:u16`, `child_count:u16`, `first_else_child:u16`, `else_child_count:u16`.

## Command opcodes

Serialized opcodes map deterministically to runtime command kinds: `0=YAAT_CMD_SAY`, `1=YAAT_CMD_SET`, `2=YAAT_CMD_GOTO`, `3=YAAT_CMD_PLAY_SOUND`, `4=YAAT_CMD_TAKE`, `5=YAAT_CMD_HIDE`, `6=YAAT_CMD_IF`, `7=YAAT_CMD_SHAKE`, `8=YAAT_CMD_PICKUP`, `9=YAAT_CMD_DROP`.

## Values

Version 2 value encoding is command-specific: booleans are `u16` `0` or `1`; identifiers, asset paths, speakers, and dialogue text are inline strings. `YAAT_CMD_IF` uses `a` as the variable name and child command ranges for branches. `YAAT_CMD_SHAKE` stores the duration in `bool_value` and the magnitude in `int_value`. `YAAT_CMD_PICKUP` stores `object_id` in `a` and `item_id` in `b`; `YAAT_CMD_DROP` stores `item_id` in `a` and `object_id` in `b`.
Version 3 stores script values as tagged values: `0=bool`, `1=int`, and `2=string`. `YAAT_CMD_SET` stores its right-hand value in the typed value field. `YAAT_CMD_IF` uses `a` as the variable name, `condition_op` as `0=truthy`, `1===`, `2!=`, `3<`, `4<=`, `5>`, or `6>=`, and the typed value field as the right-hand operand for comparison forms. `YAAT_CMD_SHAKE` keeps duration in the legacy `bool_value` field and magnitude in the legacy `int_value` field.

## Endian and alignment

All integers are unsigned little-endian 16-bit or 32-bit values. Files are byte-packed; readers must copy bytes and assemble integers manually instead of casting file bytes to C structs. No field requires more than byte alignment, which keeps loaders portable to older Win32 C compilers and avoids ABI padding differences.
