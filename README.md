# YAAT

YAAT is an experimental Windows 95-friendly point-and-click adventure engine project. The repository currently contains design documentation, a small C tokenizer for the first YAAT script syntax, Win32/GDI platform experiments, an offline asset validator, and a consistent placeholder game asset tree.

## Current repository contents

- `src/`: engine-side C experiments, including the script tokenizer, runtime asset-loading scaffolding, and Win32/GDI support code.
- `game/`: a placeholder point-and-click demo asset tree using INI metadata, `.yaat` scripts, and stub runtime assets for validation.
- `tools/asset_validate/`: a standalone development-time checker for the loose `game/` asset tree.
- `tools/win95_smoke/`: a minimal Win95-compatible Win32 GUI smoke test source.
- `scripts/`: Windows batch files for Open Watcom engine and smoke-test builds, plus fallback compiler experiments.
- `docs/`: project documentation for assets, scripting, runtime planning, toolchains, PNG support, and Win95 API policy.

## Documentation

- [Engine plan](plan.md)
- [Asset structure](docs/asset-structure.md)
- [Script language](docs/script-language.md)
- [Script runtime implementation plan](docs/script-runtime.md)
- [Toolchain compatibility](docs/toolchain-compatibility.md)
- [PNG library suitability](docs/png-library-suitability.md)
- [Win95 API allowlist](docs/win95-api-allowlist.md)

## Open Watcom installation baseline

Open Watcom C/C++ is the primary compiler baseline for YAAT's Windows 95 builds. Use the 32-bit Open Watcom toolchain, install the Win32 headers/libraries, and initialize the environment before compiling.

Typical Windows setup:

1. Install Open Watcom C/C++ 2.x or a validated 1.9-compatible release with the 32-bit Windows target selected.
2. Open a command prompt.
3. Run the Open Watcom environment script, for example:

```bat
C:\WATCOM\OWSETENV.BAT
```

4. Confirm that the compiler and linker are visible:

```bat
wcl386 -?
wlink ?
```

The required environment variables are normally configured by `OWSETENV.BAT`: `WATCOM`, `PATH`, `INCLUDE`, and `LIB`. If `wcl386` cannot find `windows.h`, `user32.lib`, or `gdi32.lib`, rerun the environment script or check that the Win32 target components were installed.

## Building the Win32 engine shell

The current engine shell is a Win32/GDI GUI program. It opens a basic YAAT window; the full game runtime is still being wired into the message loop.

From a Windows shell with Open Watcom configured, run:

```bat
scripts\build_engine_openwatcom.bat
```

The script compiles these engine sources:

```text
src\main_win32.c
src\platform\win32\gdi_renderer.c
src\script_tokenizer.c
src\runtime\asset_loader.c
```

and links them as a Windows 95 GUI executable with `user32.lib` and `gdi32.lib`:

```bat
wcl386 -q -bt=nt -i=src -os -w3 -l=win95 -fe=build\yaat_engine_openwatcom.exe src\main_win32.c src\platform\win32\gdi_renderer.c src\script_tokenizer.c src\runtime\asset_loader.c user32.lib gdi32.lib
```

On a host where GNU Make can invoke `wcl386`, the repository `Makefile` uses the same Open Watcom compiler shape:

```sh
make
```

The make target writes `build/yaat.exe`. Override `WCL386`, `WATCOM_CFLAGS`, `WATCOM_LDFLAGS`, or `WATCOM_LIBS` only after validating that the resulting binary remains Windows 95-compatible.

For fallback compiler experiments and smoke-test commands, see [Toolchain compatibility](docs/toolchain-compatibility.md).

## Runtime asset lookup and archives

YAAT keeps the loose `game/` tree as the canonical development source. Runtime lookup must use this final precedence order, from highest priority to lowest:

1. `game/` loose files.
2. The highest-numbered `patchNNNN.dat` archive in `packed/`.
3. Lower-numbered `patchNNNN.dat` archives, descending by patch number.
4. `packed/game.dat`.

Release `.dat` files are ZIP-compatible archives with YAAT restrictions for the first runtime: no ZIP64 records, no encrypted entries, forward-slash-only relative paths, ASCII entry names, and path lengths kept below the existing YAAT limits where possible. Prefer paths under 120 characters.

## Running the Win32 engine shell

After building, run the generated executable on Windows:

```bat
build\yaat_engine_openwatcom.exe
```

If built with `make`, the default output name is:

```bat
build\yaat.exe
```

For Windows 95 testing, copy the executable to the target machine or VM. The Open Watcom baseline should statically link the Watcom runtime for this project shape, leaving only Win95-era system DLL imports such as `KERNEL32.DLL`, `USER32.DLL`, and `GDI32.DLL`. Inspect imports with `wdis`, `dumpbin /imports`, `objdump -p`, or Dependency Walker before treating a build as Win95-compatible.

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
