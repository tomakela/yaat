# YAAT

YAAT is an experimental Windows 95-friendly point-and-click adventure engine project. The repository currently contains design documentation, a small C tokenizer for the first YAAT script syntax, Win32/GDI platform experiments, an offline asset validator, and a consistent placeholder game asset tree.

## Current repository contents

- `src/`: engine-side C experiments, including the script tokenizer and Win32/GDI support code.
- `game/`: a placeholder point-and-click demo asset tree using INI metadata, `.yaat` scripts, and stub runtime assets for validation.
- `tools/asset_validate/`: a standalone development-time checker for the loose `game/` asset tree.
- `tools/win95_smoke/`: a minimal Win95-compatible Win32 GUI smoke test source.
- `docs/`: project documentation for assets, scripting, runtime planning, toolchains, PNG support, and Win95 API policy.

## Documentation

- [Engine plan](plan.md)
- [Asset structure](docs/asset-structure.md)
- [Script language](docs/script-language.md)
- [Script runtime implementation plan](docs/script-runtime.md)
- [Toolchain compatibility](docs/toolchain-compatibility.md)
- [PNG library suitability](docs/png-library-suitability.md)
- [Win95 API allowlist](docs/win95-api-allowlist.md)

## Quick checks

Build and run the offline asset validator from the repository root:

```sh
cc -std=c99 -Wall -Wextra -pedantic -o tools/asset_validate/asset_validate tools/asset_validate/asset_validate.c
./tools/asset_validate/asset_validate game
```

Compile the script tokenizer as a standalone consistency check:

```sh
cc -std=c99 -Wall -Wextra -pedantic -c src/script_tokenizer.c -o /tmp/script_tokenizer.o
```
