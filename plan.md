# Win95 Point-and-Click Adventure Engine Plan

## Goal

Build a lightweight point-and-click adventure engine that targets Win95 and newer systems using C, Win32 ANSI APIs, and GDI, while avoiding modern runtime assumptions and heavyweight dependencies.

## Core Technical Direction

```yaml
compiler:
  primary_option: evaluate_tcc
  fallback_options:
    - Open Watcom C
    - old MinGW
    - MSVC 6
  language: C89/C99-compatible C
  target: Win32 PE-i386
  compatible_with_win95: required

runtime:
  crt: minimal_or_static
  api_policy: ANSI WinAPI only
  forbidden:
    - Unicode-only APIs
    - DirectX
    - layered windows
    - modern CRT-only calls
    - NT-only Win32 APIs

graphics:
  backend: GDI
  backbuffer: CreateDIBSection
  present:
    - BitBlt
    - StretchBlt
  internal_format: 32-bit BGRA/BGRX

assets:
  images:
    authoring_format: PNG
    runtime_options:
      - predecoded packed BGRA
      - optional runtime PNG via spng + miniz
  audio:
    first_format: PCM WAV
    compressed_candidate: IMA ADPCM WAV
  scripts:
    format: simple ASCII custom scripting

platform:
  windowing:
    - RegisterClassExA
    - CreateWindowExA
    - GetMessageA
    - DispatchMessageA
  input:
    - WM_MOUSEMOVE
    - WM_LBUTTONDOWN
    - WM_LBUTTONUP
  timing:
    preferred: timeGetTime
    fallback: GetTickCount
```

## Task List

### 1. Create a Win95-compatible compiler validation matrix

Add a `docs/toolchain-compatibility.md` document that evaluates candidate C compilers for Win95 PE-i386 output.

Include at minimum:

1. `tcc`
2. Open Watcom C
3. MinGW.org / old MinGW targeting `msvcrt.dll`
4. MSVC 4.x/5.x/6.x, if available

For each compiler, document:

- Whether it emits PE-i386 binaries runnable on Windows 95.
- Required runtime DLLs.
- Whether it imports only Win95-era APIs.
- Whether it supports static linking or minimal CRT startup.
- Exact test command for building a minimal `WinMain` using `CreateWindowExA`.

Use distinctive test source name: `tools/win95_smoke/main.c`.

### 2. Prototype a dependency-isolated PNG decoder module

Create a small image-loading abstraction under:

- `src/platform/win32/png_loader.c`
- `src/platform/win32/png_loader.h`

Requirements:

1. Hide `spng` and `miniz` behind an engine-local API such as `yaat_load_png_rgba8`.
2. Ensure the module does not call Unicode WinAPI functions.
3. Avoid file APIs inside the decoder itself; pass memory buffers into the PNG loader.
4. Add a compile-time switch such as `YAAT_USE_SPNG`.
5. Document all external symbols required by `spng` and `miniz`.

Use search strings:

- `spng_ctx`
- `spng_decode_image`
- `mz_uncompress`

### 3. Define a GDI DIB backbuffer rendering path

Add a renderer design document or implementation skeleton for a Win32 GDI backend.

Target files:

- `src/platform/win32/gdi_renderer.c`
- `src/platform/win32/gdi_renderer.h`

Include:

1. `CreateDIBSection`-based software backbuffer.
2. 32-bit BGRA or BGRX internal pixel format.
3. Palette/low-color fallback notes for Windows 95-era displays.
4. Final presentation through `BitBlt` or `StretchBlt`.
5. No dependency on DirectDraw or DirectX.

Important identifiers:

- `CreateDIBSection`
- `BITMAPINFO`
- `DIB_RGB_COLORS`
- `BitBlt`
- `StretchBlt`

### 4. Design a Win95-safe audio backend abstraction

Create an audio design note and optional backend skeleton under:

- `docs/audio-win95.md`
- `src/platform/win32/audio_winmm.c`
- `src/platform/win32/audio_winmm.h`

Cover these options:

1. `PlaySoundA` for the simplest blocking/asynchronous WAV playback.
2. `waveOutOpen` / `waveOutWrite` for engine-controlled PCM mixing.
3. IMA ADPCM WAV as a lightweight compression option.
4. Optional MCI playback for background music where acceptable.
5. Avoid MP3/OGG unless a decoder is proven to build without modern CRT assumptions.

Use Win95-era APIs:

- `PlaySoundA`
- `waveOutOpen`
- `waveOutPrepareHeader`
- `waveOutWrite`
- `waveOutUnprepareHeader`

### 5. Add a Win95 API compatibility allowlist

Create `docs/win95-api-allowlist.md`.

Include sections for:

1. Window creation and message loop.
2. GDI rendering.
3. File I/O.
4. Timers.
5. Input.
6. Audio.
7. Memory allocation.

For each API, mark:

- Required Windows version.
- ANSI/Unicode variant policy.
- Whether it is allowed, discouraged, or forbidden.

Seed the allowlist with:

- `CreateWindowExA`
- `RegisterClassExA`
- `GetMessageA`
- `DispatchMessageA`
- `PeekMessageA`
- `BitBlt`
- `StretchBlt`
- `CreateDIBSection`
- `PlaySoundA`
- `waveOutOpen`
- `CreateFileA`
- `ReadFile`
- `SetTimer`
- `timeGetTime`

### 6. Specify a minimal adventure scripting format

Create `docs/script-format.md` describing the initial scripting language.

The format should support:

1. Rooms/scenes.
2. Background image references.
3. Clickable hotspots with rectangles or polygons.
4. State flags and integer variables.
5. Inventory item checks.
6. Dialogue text.
7. Simple commands such as `goto`, `set`, `if`, `play_sound`, `show`, `hide`.

Keep the parser C-friendly:

- ASCII text.
- Line-oriented or simple token format.
- No dynamic code execution.
- No dependency on regex libraries.
- Deterministic memory limits.

Include example room file name: `assets/scripts/room_intro.adv`.

### 7. Define an offline asset packing format

Create `docs/asset-pack-format.md`.

Specify a simple packed asset container with:

1. Header magic, version, and table of contents.
2. Asset type IDs for image, sound, script, and metadata.
3. Little-endian fixed-width fields.
4. Optional compression flag per asset.
5. CRC or checksum field if desired.
6. Runtime loader using `CreateFileA` and `ReadFile`.

Mention that PNG decoding may happen either:

- Offline during packing, producing raw BGRA frames.
- At runtime only when `YAAT_USE_RUNTIME_PNG` is enabled.

### 8. Document Win32 mouse input handling for the engine

Create `docs/input-win32.md`.

Cover:

1. `WM_MOUSEMOVE`
2. `WM_LBUTTONDOWN`
3. `WM_LBUTTONUP`
4. `WM_LBUTTONDBLCLK`
5. `SetCapture` / `ReleaseCapture`
6. Client-coordinate to virtual-scene-coordinate conversion.
7. Cursor changes using `SetCursor` and `LoadCursorA`.
8. Behavior when the window is stretched.

Mention target source module: `src/platform/win32/input_win32.c`.

### 9. Add a Win95-safe timing policy

Create `docs/timing-win95.md` and define the engine timing strategy.

Include:

1. Primary timer candidate: `timeGetTime`.
2. Optional fallback: `GetTickCount`.
3. Optional guarded use of `QueryPerformanceCounter`.
4. Fixed timestep versus variable timestep policy.
5. Expected frame target such as 30 FPS or 60 FPS.
6. Notes about linking against `winmm`.

Mention target implementation path: `src/platform/win32/time_win32.c`.

### 10. Add a PE import compatibility check

Create a script or documentation entry under `tools/check_win95_imports`.

The check should inspect the final `.exe` and list:

1. Imported DLLs.
2. Imported symbols.
3. Any forbidden APIs.
4. Any unexpected CRT dependency.
5. Any dependency on Unicode-only or NT-only APIs.

The allowlist should reference `docs/win95-api-allowlist.md`.

If implementing as a script, keep it host-side only; it does not need to run on Windows 95.

## Initial Audio Recommendation

Start with audio support in this order:

1. PCM WAV for the simplest and safest initial implementation.
2. IMA ADPCM WAV for lightweight compression.
3. A custom simple ADPCM decoder if deterministic behavior is needed.
4. Avoid MP3 and OGG initially unless their decoders are proven to build without modern CRT assumptions.
