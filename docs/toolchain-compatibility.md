# Win95 Toolchain Compatibility Matrix

This matrix validates C compilers for producing Win32 PE-i386 GUI binaries that can run on Windows 95. Open Watcom C/C++ is the project baseline. The smoke-test source is expected at `tools/win95_smoke/main.c` and intentionally uses only ANSI Win32 APIs, including `RegisterClassExA`, `CreateWindowExA`, `GetMessageA`, and `DispatchMessageA`.

Create `build\` before running the Windows command lines below. The batch scripts do this automatically.

## Summary

| Toolchain | PE-i386 for Win95 | Runtime DLL requirements | NT-only imports avoidable? | Static/minimal CRT option | YAAT status |
| --- | --- | --- | --- | --- | --- |
| Open Watcom C/C++ | Primary | Normally only Win95 system DLLs when statically linking the Watcom runtime | Yes, with Win95-era SDK/import libraries and ANSI APIs | Yes; static runtime is the normal safe choice | Preferred baseline |
| MinGW.org / old MinGW targeting `msvcrt.dll` | Yes | `MSVCRT.DLL` plus Win95 system DLLs; optionally `libgcc` if not statically linked | Yes, if source and startup/import libraries stay Win95-safe | `-static-libgcc`; CRT itself remains `MSVCRT.DLL` | Fallback experiment |
| `tcc` | Fallback candidate, if using the 32-bit Windows target and old enough import libraries | Usually `KERNEL32.DLL`, `USER32.DLL`, `GDI32.DLL`, and `MSVCRT.DLL` | Yes for this smoke test, but verify imports because bundled import libraries vary | Minimal startup is possible; full static CRT is not the normal Windows `tcc` mode | Fallback experiment |
| MSVC 4.x/5.x/6.x | Yes, these were contemporary Win32 i386 compilers | `/MT` avoids VC runtime DLLs; `/MD` requires VC runtime DLL deployment | Yes, when linking subsystem version 4.00 and using Win95-era APIs | `/MT` static CRT; `/NODEFAULTLIB` custom startup is possible but not preferred initially | Fallback experiment |

## Source-code compatibility rules

Keep engine and smoke-test C source friendly to Open Watcom and Windows 95:

- Use ANSI Win32 APIs explicitly (`RegisterClassExA`, `CreateWindowExA`, `GetMessageA`, `DispatchMessageA`, etc.).
- Do not require Unicode entry points or NT-only APIs.
- Keep source broadly C89-compatible where practical: declare variables near the start of a block, avoid C99-only library calls, and avoid compiler-specific extensions unless guarded.
- Include project headers through `-i=src` / `-I src` rather than absolute paths.
- Keep runtime code independent of modern CRT calls that may not exist on a Windows 95 target.

## Open Watcom C/C++ installation

Open Watcom must be installed with the 32-bit Windows target support and Win32 import libraries.

Typical setup on Windows:

```bat
C:\WATCOM\OWSETENV.BAT
wcl386 -?
wlink ?
```

`OWSETENV.BAT` should set `WATCOM`, update `PATH`, and add Watcom include/library directories to `INCLUDE` and `LIB`. If `wcl386` cannot locate `windows.h`, `user32.lib`, or `gdi32.lib`, the Win32 target support is missing or the environment script has not been run in the current shell.

## Open Watcom C/C++ engine build

The Open Watcom engine script compiles the Win32 shell, renderer, tokenizer, and runtime asset-loader scaffolding:

```bat
scripts\build_engine_openwatcom.bat
```

Equivalent direct command:

```bat
mkdir build
wcl386 -q -bt=nt -i=src -os -w3 -l=win95 -fe=build\yaat_engine_openwatcom.exe src\main_win32.c src\platform\win32\gdi_renderer.c src\script_tokenizer.c src\runtime\asset_loader.c src\runtime\zip_archive.c src\third_party\miniz\miniz.c src\third_party\miniz\miniz_zip.c src\third_party\miniz\miniz_tinfl.c user32.lib gdi32.lib
```

Source list:

```text
src\main_win32.c
src\platform\win32\gdi_renderer.c
src\script_tokenizer.c
src\runtime\asset_loader.c
src\runtime\zip_archive.c
src\third_party\miniz\miniz.c
src\third_party\miniz\miniz_zip.c
src\third_party\miniz\miniz_tinfl.c
```

Link libraries:

```text
user32.lib
gdi32.lib
```

## Open Watcom C/C++ smoke-test build

Use the smoke test before accepting a new Open Watcom install, SDK library set, or compiler-option change:

```bat
scripts\build_win95_smoke_openwatcom.bat
```

Equivalent direct command:

```bat
mkdir build
wcl386 -q -bt=nt -os -w3 -l=win95 -fe=build\win95_smoke_openwatcom.exe tools\win95_smoke\main.c user32.lib gdi32.lib
```

## MinGW.org / old MinGW targeting `msvcrt.dll`

- **Can emit PE-i386 binaries runnable on Windows 95:** Yes, when using the original 32-bit MinGW.org toolchain/runtime that targets `msvcrt.dll`. This is now a fallback compiler, not the YAAT baseline. Do not assume modern MinGW-w64 configurations remain Win95-compatible.
- **Runtime DLL requirements:** The executable normally imports `MSVCRT.DLL` plus Win95 system DLLs such as `KERNEL32.DLL`, `USER32.DLL`, and `GDI32.DLL`. Add `-static-libgcc` to avoid a separate `libgcc` DLL when the compiler supports that option.
- **Can avoid NT-only imports:** Yes if code avoids newer APIs and the MinGW startup/runtime being used does not introduce NT-only imports. Inspect imports for functions absent from Windows 95 before accepting a toolchain version.
- **Smoke-test compile command:**

```bat
scripts\build_win95_smoke_mingw.bat
```

## `tcc`

- **Can emit PE-i386 binaries runnable on Windows 95:** Yes as a fallback candidate, provided the 32-bit Windows `tcc` driver is used and the generated executable imports only Win95-available functions. Treat current TinyCC releases as needing import-table verification before adoption.
- **Runtime DLL requirements:** Typical output depends on `KERNEL32.DLL`, `USER32.DLL`, `GDI32.DLL`, and `MSVCRT.DLL`. The exact import set depends on the `tcc` distribution and its startup objects/import libraries.
- **Can avoid NT-only imports:** Yes for engine code that uses only ANSI Win95 APIs. Verify with `objdump -p`, `dumpbin /imports`, or Dependency Walker because a newer `tcc` bundle could introduce newer CRT or kernel imports through startup code.
- **Smoke-test compile command:**

```bat
scripts\build_win95_smoke_tcc.bat
```

## MSVC 4.x/5.x/6.x

- **Can emit PE-i386 binaries runnable on Windows 95:** Yes. MSVC 4.x, 5.x, and 6.x are period-appropriate Win32 i386 compilers and are fallback choices if available.
- **Runtime DLL requirements:** Use `/MT` so the C runtime is statically linked into the executable. Avoid `/MD` unless the matching VC runtime DLL is explicitly deployed and tested on Windows 95.
- **Can avoid NT-only imports:** Yes. Link with `/SUBSYSTEM:WINDOWS,4.00`, use ANSI Win32 APIs, and build against an SDK/import library set that still supports Windows 95.
- **Smoke-test compile command:**

```bat
scripts\build_win95_smoke_msvc.bat
```

## Acceptance checklist

For every compiler or Open Watcom option change, build the smoke test and inspect the import table. Accept the toolchain only if the executable is PE-i386, uses a Windows GUI subsystem compatible with Windows 95, and imports no APIs newer than Windows 95.

Suggested import checks:

```bat
dumpbin /headers build\win95_smoke_*.exe
dumpbin /imports build\win95_smoke_*.exe
```

or:

```sh
objdump -p build/win95_smoke_*.exe
```
