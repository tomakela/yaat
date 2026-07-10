# YAAT offline asset validator

`asset_validate.c` is a standalone development-time checker for YAAT assets
as described in `docs/asset-structure.md`. Runtime engine code should not
include or link this tool.

This is a modern host-only developer tool, not a Windows 95 runtime component.
It is supported with C99-capable host compilers on contemporary development
platforms such as Linux, macOS, and modern Windows environments. On Windows, the
directory traversal currently depends on POSIX-compatible `dirent.h` support
provided by the compiler/runtime distribution, or by a compatible `dirent.h`
layer supplied by the build environment.

Build and run from the repository root:

```sh
cc -std=c99 -Wall -Wextra -pedantic -o tools/asset_validate/asset_validate tools/asset_validate/asset_validate.c
./tools/asset_validate/asset_validate game
```

The validator accepts either a loose `game/` directory or a single `.dat` ZIP
archive. When pointed at a loose tree, it also discovers `game.dat` and
`patchNNNN.dat` files in `game/packed/` or beside `game/`, applies them in
ascending patch number, and validates the final effective asset view. Later
sources intentionally override earlier entries, so patch overrides are reported
as warnings for review.

Checks include required `game.ini`, `game.first_room`, first-room `room.ini`,
room metadata references, normalized path safety, lowercase/length/extension
recommendations, duplicate entries within archives, unsupported ZIP features,
and oversized files before decompression. ZIP archives are inspected offline
from their central directory; stored entries can also be read for effective
`game.ini` and `room.ini` checks without adding any runtime dependencies.
