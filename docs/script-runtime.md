# Script Runtime Implementation Status

This document describes the current C implementation of the YAAT script runtime and calls out known limitations separately from future work. The canonical language syntax is defined in [`docs/script-language.md`](script-language.md), and the canonical bytecode package format is defined in [`docs/script-bytecode.md`](script-bytecode.md).

## Implemented files

The runtime is currently implemented across these files:

- `src/script_tokenizer.c` / `src/script_tokenizer.h` tokenize `.yaat` source text.
- `src/script_parser.c` / `src/script_parser.h` parse token streams into a `YaatScriptPackage`.
- `src/script_package.c` / `src/script_package.h` define fixed-size runtime data structures and package helpers.
- `src/script_bytecode.c` / `src/script_bytecode.h` read and write `.yaatbc` bytecode packages.
- `src/main_win32.c` owns the in-engine script command executor and event integration.

There is no separate `src/script_vm.c` or `src/script_vm.h` today; VM-like command dispatch is embedded in `src/main_win32.c`.

## Current implementation

### Tokenizer

The tokenizer converts source text into tokens with line and column metadata for diagnostics. It recognizes identifiers, strings, integer numbers, braces, parentheses, brackets, commas, colons, assignment and comparison operators, keywords, string-id labels, `#` line comments, and an explicit EOF token.

Tokenizer diagnostics currently cover lexical failures such as unexpected characters and unterminated strings. The tokenizer API returns owned token/diagnostic arrays that callers release with `script_tokenizer_result_free()`.

### Parser and package construction

The parser consumes the token stream and fills a fixed-size `YaatScriptPackage`. It currently supports:

- Global `var` declarations with boolean, integer, string, and identifier values.
- Top-level `room` declarations.
- Top-level `event` declarations and `proc` aliases as global command blocks.
- Room-level `on ... { ... }` event handlers.
- Nested `object`, `hotspot`, and `npc` declarations inside rooms.
- Top-level `object` declarations imported into an inventory-style script room.
- Entity fields for `name`, `at`, `size`, and `visible`.
- Command lists in source order.
- `if` / `else` blocks with truthy checks, inventory-style `has` / `inventory` checks, and comparison operators `==`, `!=`, `<`, `<=`, `>`, and `>=`.

Parsed data is stored in plain C structs in `YaatScriptPackage`; the package uses fixed arrays rather than dynamic allocation or an arena allocator.

### Bytecode package

`src/script_bytecode.c` serializes and deserializes `YaatScriptPackage` to `.yaatbc`. The bytecode format is intentionally fixed-width, little-endian, and pointer-free for old Win32-era C compatibility. See [`docs/script-bytecode.md`](script-bytecode.md) for the canonical field layout, version number, command opcode mapping, value encoding, and alignment rules.

The bytecode reader validates package counts, command kinds, condition operators, event command ranges, and child/else command ranges before accepting a package.

### Engine execution and integration

`src/main_win32.c` loads bytecode when available and falls back to parsing source scripts. It imports script package rooms, entities, globals, commands, and global events into engine runtime arrays.

Command execution currently happens through `yaat_execute_commands()` in `src/main_win32.c`. Implemented command behavior includes dialogue text, variable assignment, room transitions, sound playback, inventory updates, object visibility, object movement, sprite/animation changes, cutscene/title text, waits, player movement/visibility, dialogue node selection, global event calls, and screen shake.

Events are executed by engine actions such as room entry, hotspot/object interaction, inventory use, and global event calls.

## Current limitations

These are current implementation limits, not design goals for the language:

- **Fixed array limits:** script packages are capped by `YAAT_MAX_ROOMS` (`8`), `YAAT_MAX_ENTITIES` (`32` per room), `YAAT_MAX_EVENTS` (`8` per room/entity), `YAAT_MAX_GLOBAL_EVENTS` (`16`), `YAAT_MAX_COMMANDS` (`128`), `YAAT_MAX_VARS` (`64`), and `YAAT_MAX_INVENTORY` (`16`).
- **Fixed string widths:** identifiers and fields are copied into fixed buffers, such as 32-byte room/entity/event ids, 64-byte labels/names, 96-byte command arguments and string values, and 160-byte dialogue/runtime text buffers.
- **Unsupported or unpreserved fields are skipped:** the parser preserves only selected room/entity fields. For example, room `background` is tokenized but not stored in `YaatScriptPackage`; unsupported nested blocks are skipped; unsupported scalar fields are generally consumed or ignored.
- **Top-level declaration behavior is narrow:** top-level global `event`/`proc` blocks are stored, and top-level objects are imported into an inventory-style room. Top-level hotspots are not preserved as standalone reusable hotspot definitions.
- **Unknown commands are skipped rather than diagnosed:** if a command token is not recognized, the parser rolls back the just-created command slot and skips an attached block when present. This means command typos may not produce a fatal parse error today.
- **Diagnostics are incomplete:** tokenizer diagnostics include source locations, but parser and runtime validation do not yet provide a full accumulated `ScriptDiagnostic` stream with source spans for every error.
- **Validation is limited:** the bytecode loader validates counts and command ranges, but the source parser does not comprehensively reject duplicate identifiers, unresolved references, unknown event targets, missing assets, or unknown commands.
- **Command range limits:** command indices and event/branch ranges are serialized as 16-bit fields and must fit within `YAAT_MAX_COMMANDS`. Nested `if` children and `else` children are represented as contiguous ranges in the shared command array.
- **No standalone VM module:** there is no separate `script_vm.c`; deterministic execution is integrated directly into `main_win32.c`, making the runtime less reusable outside the Win32 engine executable.
- **Execution has engine side effects:** commands directly mutate engine state or call engine services. This is practical for the current game, but it is not an isolated, pure interpreter API.
- **Control flow remains intentionally small:** there are no user-defined functions beyond global `event`/`proc` command blocks, no loops, no arithmetic expression trees, and no arbitrary script-defined code execution.

## Future work

The following ideas are not implemented yet and should be treated as future work:

1. Split the embedded command dispatcher out of `src/main_win32.c` into a reusable `src/script_vm.c` / `src/script_vm.h` module with an explicit execution context.
2. Add a proper parser/validation diagnostic model that accumulates source-spanned errors and warnings for duplicate identifiers, unknown commands, invalid event targets, unresolved references, and missing assets.
3. Decide whether currently skipped fields such as room backgrounds and additional object/hotspot metadata should become stored package fields.
4. Replace or augment fixed-size arrays with a deliberate allocation strategy if larger adventures need more rooms, entities, events, commands, variables, or text.
5. Expand end-to-end fixture coverage for source parsing, bytecode round-trips, event dispatch, inventory checks, dialogue, transitions, and failure diagnostics.
6. Consider a more reusable command registry if script commands need to be shared by non-Win32 tools or tests.
7. Add deterministic save/load semantics for any future command that uses timers, randomness, filesystem access, or other side effects.
