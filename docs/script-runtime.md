# Script Runtime Implementation Plan

This document describes the initial C architecture for the YAAT script language runtime. The first implementation should prioritize deterministic, data-driven scripts that are straightforward to parse, validate, and execute from engine events.

## Initial scope

The first milestone deliberately excludes user-defined functions, loops, and arbitrary expressions. Scripts should be limited to:

- Room, object, and hotspot declarations.
- Event blocks attached to rooms, objects, and hotspots.
- Ordered command lists.
- Literal values and simple identifiers.
- Simple `if` statements with narrowly defined conditions, such as checking inventory state, boolean flags, equality against simple values, or current room/object state.

This keeps execution deterministic, avoids unbounded runtime behavior, and makes save/load state easier to reason about.

## 1. Tokenizer module

Suggested files:

- `src/script_tokenizer.c`
- `src/script_tokenizer.h`

The tokenizer should convert source text into a stream of tokens with line and column metadata for diagnostics. It should avoid interpreting higher-level syntax and only classify lexical units.

Recommended token categories:

- **Identifiers**: room names, object names, hotspot names, event names, command names, and variable-like references.
- **Strings**: quoted text for dialogue, labels, asset paths, and room transition targets.
- **Numbers**: integer values for coordinates, dimensions, priorities, or future deterministic counters.
- **Braces and punctuation**: `{`, `}`, `(`, `)`, `[`, `]`, `,`, `:`, and statement terminators if the language adopts them.
- **Operators**: `=` and `==` for version 0. Reserve other operators for future versions instead of tokenizing them as accepted syntax.
- **Keywords**: `room`, `object`, `hotspot`, `event`, `on`, `if`, `else`, `true`, `false`, and any reserved command-like words that should not be accepted as identifiers.
- **End-of-file**: an explicit EOF token to simplify parser termination.

Implementation notes:

- Store token text as slices into the original source or as owned strings, but keep ownership rules explicit in `script_tokenizer.h`.
- Track `line` and `column` on every token.
- Support the language-defined `#` line comments so authored scripts can be documented consistently.
- Report unterminated strings and unexpected characters as tokenizer diagnostics instead of allowing the parser to fail later with vague errors.

## 2. Parser module

Suggested files:

- `src/script_parser.c`
- `src/script_parser.h`

The parser should consume tokenizer output and build validated runtime-facing structures or an intermediate AST that can be converted into runtime data. The initial grammar should stay small and predictable.

The parser should support:

- `room` declarations with unique identifiers.
- `object` declarations, either nested in rooms or defined globally and referenced by rooms.
- `hotspot` declarations with geometry or named regions.
- Event blocks such as `on enter`, `on look`, `on click`, `on use`, `on use <item>`, and `on talk`.
- Command invocations with positional or named arguments.
- Simple `if` statements whose condition grammar is intentionally limited.

Parser responsibilities:

- Enforce required fields for rooms, objects, hotspots, and events.
- Preserve command order exactly as written.
- Attach source spans to parsed nodes so runtime and validation errors can refer back to script locations.
- Reject unsupported syntax explicitly, especially loops, user-defined functions, and arbitrary arithmetic or expression trees.

A minimal parse flow can be:

1. Tokenize the complete source.
2. Parse top-level declarations.
3. Validate identifier uniqueness and references.
4. Lower parsed declarations into runtime structures.
5. Return a script package that the room loader can attach to engine data.

## 3. Runtime data structures

The runtime model should be plain C structures with clear allocation and teardown ownership. Suggested public types include:

- `ScriptRoom`
  - Room identifier, display name, asset references, room-level events, objects, and hotspots.
- `ScriptObject`
  - Object identifier, initial state, sprite or asset path, inventory metadata, and object-level events.
- `ScriptHotspot`
  - Hotspot identifier, bounds or polygon data, cursor/action metadata, and hotspot-level events.
- `ScriptEvent`
  - Event type, optional trigger metadata, source span, and an ordered list of commands.
- `ScriptCommand`
  - Command opcode or command name, argument list, source span, and prevalidated target references where possible.
- `ScriptValue`
  - Tagged values for strings, numbers, booleans, identifiers, asset paths, and future deterministic value kinds.

Recommended ownership approach:

- Keep all script-owned allocations within a `ScriptPackage` or arena-style allocator.
- Expose explicit load/free functions, such as `script_package_load_from_file()` and `script_package_free()`.
- Avoid exposing mutable internal arrays directly unless the engine has a clear ownership contract.

## 4. Interpreter module

Suggested files:

- `src/script_vm.c`
- `src/script_vm.h`

The interpreter should execute command lists in response to engine events. It should not evaluate arbitrary code; it should dispatch known commands through a fixed command table.

Core responsibilities:

- Accept a `ScriptEvent` and an execution context containing current room, inventory, flags, dialogue services, asset services, and transition services.
- Iterate commands in source order.
- Evaluate simple `if` conditions using deterministic engine state.
- Dispatch known commands such as dialogue display, flag updates, inventory changes, object visibility changes, sound playback, and room transitions.
- Return structured execution results so the engine can react to requested transitions or blocking dialogue.

Recommended API shape:

- `script_vm_execute_event(ctx, event)` for event-driven execution.
- `script_vm_execute_commands(ctx, commands, count)` for shared command-list execution.
- `script_vm_register_builtin_commands()` if command dispatch is table-driven.

The VM should keep command execution deterministic by avoiding timers, random numbers, filesystem access, network access, or other nondeterministic side effects inside script code. If the engine needs such behavior later, it should be exposed as explicit engine-controlled commands with deterministic save/load semantics.

## 5. Integration points with the game engine

The first integration layer should connect parsed script data to existing engine systems without making scripts responsible for engine ownership.

Required integration points:

- **Room loading**
  - Load and validate a script package when a room or adventure file is loaded.
  - Resolve room asset paths and report missing asset path warnings.
  - Attach `ScriptRoom` data to engine room instances.
- **Hotspot click handling**
  - Map click coordinates to `ScriptHotspot` entries.
  - Execute the hotspot `on click` event or a more specific action event.
- **Inventory actions**
  - Route inventory use/combine/select events to object or hotspot events.
  - Provide deterministic inventory predicates for simple `if` conditions.
- **Dialogue display**
  - Expose a command that passes dialogue text and optional speaker metadata to the engine dialogue UI.
  - Return control according to the engine's existing blocking or non-blocking dialogue model.
- **Room transitions**
  - Expose a command for transitioning to another room by identifier.
  - Validate transition targets at load time when possible.
  - Return a transition request to the engine instead of directly mutating unrelated engine state from the VM.

## 6. Error handling and diagnostics

Diagnostics should be useful to script authors and precise enough for tests.

Required diagnostics:

- **Line/column diagnostics**
  - Every tokenizer, parser, validation, and VM error should include a source line and column when it originated from script text.
- **Unknown command errors**
  - The parser or validator should reject command names that are not in the initial built-in command table.
- **Missing asset path warnings**
  - During validation or room loading, asset references should be checked and reported as warnings so authors can fix broken paths without confusing them with syntax errors.
- **Duplicate identifier checks**
  - Duplicate room, object, and hotspot identifiers should be reported before runtime execution begins.

Error model recommendations:

- Use a `ScriptDiagnostic` type with severity, message, source span, and optional related identifier.
- Accumulate multiple diagnostics during validation when possible instead of stopping after the first non-fatal issue.
- Treat syntax errors, duplicate identifiers, unknown commands, and invalid event targets as fatal load errors.
- Treat missing optional assets as warnings unless the engine requires the asset to run.

## Milestone sequence

1. Implement tokenizer with token tests and diagnostics.
2. Implement parser for declarations, event blocks, command calls, and simple `if` statements.
3. Define runtime data structures and ownership/free APIs.
4. Add validation for identifiers, command names, event targets, and asset paths.
5. Implement the VM command dispatcher and deterministic execution context.
6. Integrate room loading, hotspot clicks, inventory actions, dialogue display, and room transitions.
7. Add end-to-end script fixtures that exercise a room with objects, hotspots, inventory checks, dialogue, and a transition.
