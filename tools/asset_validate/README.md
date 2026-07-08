# YAAT offline asset validator

`asset_validate.c` is a standalone development-time checker for the loose `game/`
asset tree described in `docs/asset-structure.md`. Runtime engine code should not
include or link this tool.

Build and run from the repository root:

```sh
cc -std=c99 -Wall -Wextra -pedantic -o tools/asset_validate/asset_validate tools/asset_validate/asset_validate.c
./tools/asset_validate/asset_validate game
```

The validator checks for required top-level assets, room folders and metadata
references, lowercase ASCII path recommendations, the 120-character runtime path
recommendation, and a small allow-list of known runtime file extensions.
