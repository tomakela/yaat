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

## Building the Win32 engine shell

The current engine shell is a Win32/GDI GUI program. It opens a basic YAAT window; the full game runtime is not wired into the message loop yet.

The primary Windows 95 compatibility baseline is the original MinGW.org 32-bit toolchain that targets `msvcrt.dll` (commonly GCC 3.x/4.x-era MinGW.org distributions). Do not assume modern MinGW-w64 builds are Windows 95-compatible: verify that the generated executable is PE-i386, links only Win95-era system imports, and does not introduce NT-only startup/runtime imports. Use `-march=i386` for conservative CPU compatibility and `-static-libgcc` when supported so the executable does not require a separate `libgcc` DLL. The executable may still depend on `MSVCRT.DLL`, which must be present on the target Windows 95 system.

From a Windows shell with `mingw32-gcc` on `PATH`, either run the helper script:

```bat
scripts\build_engine_mingw.bat
```

or compile directly:

```bat
if not exist build mkdir build
mingw32-gcc -I src -mwindows -march=i386 -Os -static-libgcc -o build\yaat_engine_mingw.exe src\main_win32.c src\platform\win32\gdi_renderer.c -luser32 -lgdi32
```

On a Unix-like host with a compatible MinGW cross-compiler named `mingw32-gcc`, the repository `Makefile` uses the same compiler shape:

```sh
make
```

Override `CC` if your validated Win95-capable cross-compiler has a different name:

```sh
make CC=i386-mingw32-gcc
```

For alternate compilers and smoke-test commands, see [Toolchain compatibility](docs/toolchain-compatibility.md).

## Running the Win32 engine shell

After building, run the generated executable on Windows:

```bat
build\yaat_engine_mingw.exe
```

If built with `make`, the default output name is:

```bat
build\yaat.exe
```

For Windows 95 testing, copy the executable to the target machine or VM together with any required runtime DLLs from the validated toolchain. With the MinGW.org baseline and `-static-libgcc`, expect Windows system DLLs plus `MSVCRT.DLL`; inspect imports with `dumpbin /imports`, `objdump -p`, or Dependency Walker before treating a build as Win95-compatible.

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
