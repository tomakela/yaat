# YAAT asset packer

`asset_pack.c` is a standalone development-time tool that packs a loose asset
folder into a ZIP-format `.dat` archive. Runtime engine code should not include
or link this tool.

`game/` remains the canonical source for editable assets. Generated `game.dat`
and `patchNNNN.dat` files are release artifacts and should be rebuilt from the
loose source tree when assets change.

Build from the repository root:

```sh
cc -std=c99 -Wall -Wextra -pedantic -o tools/asset_pack/asset_pack tools/asset_pack/asset_pack.c
```

Pack the canonical game tree:

```sh
./tools/asset_pack/asset_pack game game.dat
```

Pack a patch folder using the same logical path rules:

```sh
./tools/asset_pack/asset_pack patch_folder patch0000.dat
```

The archive preserves file paths relative to the input directory and writes a
manifest report next to the output archive as `<output>.manifest.txt`. Paths in
the manifest use normalized forward slashes.

Path rules are intentionally strict so archives are reproducible and safe for
runtime lookup:

* file paths must be relative to the input directory;
* path components named `.` or `..` are rejected;
* supported names use lowercase ASCII letters, digits, `_`, `-`, and `.`;
* duplicate logical paths are rejected after normalization;
* directories are traversal containers only and are not emitted as ZIP entries.

The packer stores files without compression. This keeps the tool dependency-free
and still produces a valid ZIP-format archive suitable for `.dat` release
artifacts.
