# Win95 Toolchain Compatibility Matrix

This matrix validates C compilers for producing Win32 PE-i386 GUI binaries that can run on Windows 95. The smoke-test source is expected at `tools/win95_smoke/main.c` and intentionally uses only ANSI Win32 APIs, including `RegisterClassExA`, `CreateWindowExA`, `GetMessageA`, and `DispatchMessageA`.

Create `build\` before running the Windows command lines below.

## Summary

| Toolchain | PE-i386 for Win95 | Runtime DLL requirements | NT-only imports avoidable? | Static/minimal CRT option |
| --- | --- | --- | --- | --- |
| MinGW.org / old MinGW targeting `msvcrt.dll` | Primary | `MSVCRT.DLL` plus Win95 system DLLs; optionally `libgcc` if not statically linked | Yes, if source and startup/import libraries stay Win95-safe | `-static-libgcc`; CRT itself remains `MSVCRT.DLL` |
| Open Watcom C | Yes, with Win32/Win95 target/linker settings | Normally only Win95 system DLLs when statically linking the Watcom runtime | Yes, with Win95-era SDK/import libraries and ANSI APIs | Yes; static runtime is the normal safe choice |
| `tcc` | Fallback candidate, if using the 32-bit Windows target and old enough import libraries | Usually `KERNEL32.DLL`, `USER32.DLL`, `GDI32.DLL`, and `MSVCRT.DLL` | Yes for this smoke test, but verify imports because bundled import libraries vary | Minimal startup is possible; full static CRT is not the normal Windows `tcc` mode |
| MSVC 4.x/5.x/6.x | Yes, these were contemporary Win32 i386 compilers | `/MT` avoids VC runtime DLLs; `/MD` requires VC runtime DLL deployment | Yes, when linking subsystem version 4.00 and using Win95-era APIs | `/MT` static CRT; `/NODEFAULTLIB` custom startup is possible but not preferred initially |

## Smoke-test source

Use the repository path:

```text
tools/win95_smoke/main.c
```

The source must remain C89-friendly and must not call Unicode-only or NT-only APIs. It should define an ANSI `WinMain`, register a `WNDCLASSEXA`, create a window with `CreateWindowExA`, and run a `GetMessageA` / `DispatchMessageA` loop.

## MinGW.org / old MinGW targeting `msvcrt.dll`

- **Can emit PE-i386 binaries runnable on Windows 95:** Yes, when using the original 32-bit MinGW.org toolchain/runtime that targets `msvcrt.dll`. This is the primary compiler baseline. Do not assume modern MinGW-w64 configurations remain Win95-compatible.
- **Runtime DLL requirements:** The executable normally imports `MSVCRT.DLL` plus Win95 system DLLs such as `KERNEL32.DLL`, `USER32.DLL`, and `GDI32.DLL`. Add `-static-libgcc` to avoid a separate `libgcc` DLL when the compiler supports that option.
- **Can avoid NT-only imports:** Yes if code avoids newer APIs and the MinGW startup/runtime being used does not introduce NT-only imports. Inspect imports for functions absent from Windows 95 before accepting a toolchain version.
- **Static or minimal CRT options:** `-static-libgcc` is recommended. The Microsoft CRT dependency remains `MSVCRT.DLL`; fully replacing CRT startup is possible but should be treated as a separate experiment.
- **Smoke-test compile command:**

```bat
mkdir build
mingw32-gcc -mwindows -march=i386 -Os -static-libgcc -o build\win95_smoke_mingw.exe tools\win95_smoke\main.c -luser32 -lgdi32
```

## Open Watcom C

- **Can emit PE-i386 binaries runnable on Windows 95:** Yes. Open Watcom can target 32-bit Windows PE and can link using Win95-compatible settings.
- **Runtime DLL requirements:** With the static runtime, the executable should require only Win95 system DLLs such as `KERNEL32.DLL`, `USER32.DLL`, and `GDI32.DLL` for this smoke test. Avoid Watcom DLL runtime options for the initial baseline.
- **Can avoid NT-only imports:** Yes. Use `-bt=nt` with the Win95 GUI linker system and avoid newer SDK imports. Confirm with an import-table inspection.
- **Static or minimal CRT options:** Static Watcom runtime is the preferred baseline. A custom startup may be possible later, but the ordinary static runtime is simpler and Win95-appropriate.
- **Smoke-test compile command:**

```bat
mkdir build
wcl386 -q -bt=nt -l=win95 -fe=build\win95_smoke_ow.exe tools\win95_smoke\main.c user32.lib gdi32.lib
```

## `tcc`

- **Can emit PE-i386 binaries runnable on Windows 95:** Yes as a fallback candidate, provided the 32-bit Windows `tcc` driver is used and the generated executable imports only Win95-available functions. Treat current TinyCC releases as needing import-table verification before adoption.
- **Runtime DLL requirements:** Typical output depends on `KERNEL32.DLL`, `USER32.DLL`, `GDI32.DLL`, and `MSVCRT.DLL`. The exact import set depends on the `tcc` distribution and its startup objects/import libraries.
- **Can avoid NT-only imports:** Yes for engine code that uses only ANSI Win95 APIs. Verify with `objdump -p`, `dumpbin /imports`, or Dependency Walker because a newer `tcc` bundle could introduce newer CRT or kernel imports through startup code.
- **Static or minimal CRT options:** `tcc` is attractive for small binaries and simple startup, but the common Windows target links against `msvcrt.dll` rather than a fully static CRT. If CRT imports are a problem, use a custom/minimal startup only after the normal smoke test passes.
- **Smoke-test compile command:**

```bat
mkdir build
tcc -m32 -Wl,-subsystem=windows -o build\win95_smoke_tcc.exe tools\win95_smoke\main.c -luser32 -lgdi32
```

If the installed binary is named for the cross target, use this equivalent:

```sh
i386-win32-tcc -Wl,-subsystem=windows -o build/win95_smoke_tcc.exe tools/win95_smoke/main.c -luser32 -lgdi32
```

## MSVC 4.x/5.x/6.x

- **Can emit PE-i386 binaries runnable on Windows 95:** Yes. MSVC 4.x, 5.x, and 6.x are period-appropriate Win32 i386 compilers and are strong fallback choices if available.
- **Runtime DLL requirements:** Use `/MT` so the C runtime is statically linked into the executable. Avoid `/MD` unless the matching VC runtime DLL is explicitly deployed and tested on Windows 95.
- **Can avoid NT-only imports:** Yes. Link with `/SUBSYSTEM:WINDOWS,4.00`, use ANSI Win32 APIs, and build against an SDK/import library set that still supports Windows 95.
- **Static or minimal CRT options:** `/MT` is the preferred static CRT option. A hand-written entry point with `/NODEFAULTLIB` is possible for very small binaries, but it increases risk and should not be the first baseline.
- **Smoke-test compile command:**

```bat
mkdir build
cl /nologo /W3 /O1 /MT tools\win95_smoke\main.c /link /SUBSYSTEM:WINDOWS,4.00 /OUT:build\win95_smoke_msvc.exe user32.lib gdi32.lib
```

## Acceptance checklist

For every candidate compiler, build the smoke test and inspect the import table. Accept the toolchain only if the executable is PE-i386, uses GUI subsystem version 4.00 or otherwise runs on Windows 95, and imports no APIs newer than Windows 95.

Suggested import checks:

```bat
dumpbin /headers build\win95_smoke_*.exe
dumpbin /imports build\win95_smoke_*.exe
```

or:

```sh
objdump -p build/win95_smoke_*.exe
```
