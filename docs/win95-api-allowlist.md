# Win95 API Compatibility Allowlist

This document records the initial Win32 API allowlist for YAAT's Windows 95 target. It favors Win32 ANSI entry points, GDI, WinMM, and simple kernel/user APIs that are available on Windows 95 and newer systems.

## Status definitions

- **Allowed**: Suitable for the Win95 runtime when used through YAAT platform wrappers.
- **Discouraged**: Available, but should be isolated or reserved for fallback/simple cases because it can complicate portability, determinism, or later engine control.
- **Forbidden**: Do not call from the Win95 runtime. This includes Unicode-only APIs, NT-only APIs, DirectX, layered-window APIs, and modern CRT-dependent replacements.

## ANSI/Unicode policy

- Use explicit `A` suffixed Win32 APIs whenever a text or path API has ANSI and Unicode variants.
- Do not call generic `TCHAR` macros such as `CreateWindowEx`, `RegisterClassEx`, or `CreateFile` from engine runtime code, because the selected variant depends on build flags.
- Do not call `W` suffixed APIs in the Win95 runtime. Windows 95 does not provide broad native Unicode Win32 API support.
- APIs without string parameters have no ANSI/Unicode variant and are considered variant-neutral.

## 1. Window creation and message loop

| API | Required Windows version | ANSI/Unicode policy | Status | Notes |
| --- | --- | --- | --- | --- |
| `RegisterClassExA` | Windows 95 or newer | Use the explicit ANSI `A` variant only; avoid `RegisterClassEx` and `RegisterClassExW`. | Allowed | Preferred window-class registration entry point for the Win32 runtime. |
| `CreateWindowExA` | Windows 95 or newer | Use the explicit ANSI `A` variant only; avoid `CreateWindowEx` and `CreateWindowExW`. | Allowed | Main game window creation API. Avoid extended styles that require newer shells or layered windows. |
| `GetMessageA` | Windows 95 or newer | Use the explicit ANSI `A` variant only; avoid `GetMessage` and `GetMessageW`. | Allowed | Preferred blocking message-loop API when the engine can wait for messages. |
| `DispatchMessageA` | Windows 95 or newer | Use the explicit ANSI `A` variant only; avoid `DispatchMessage` and `DispatchMessageW`. | Allowed | Dispatches messages to the ANSI window procedure. |
| `PeekMessageA` | Windows 95 or newer | Use the explicit ANSI `A` variant only; avoid `PeekMessage` and `PeekMessageW`. | Allowed | Suitable for active game loops that must update rendering/audio while polling messages. Keep CPU usage bounded. |

## 2. GDI rendering

| API | Required Windows version | ANSI/Unicode policy | Status | Notes |
| --- | --- | --- | --- | --- |
| `CreateDIBSection` | Windows 95 or newer | Variant-neutral; no string parameters. | Allowed | Preferred software backbuffer allocation path. Use compatible `BITMAPINFO` and test low-color display behavior. |
| `BitBlt` | Windows 95 or newer | Variant-neutral; no string parameters. | Allowed | Preferred 1:1 backbuffer presentation path. |
| `StretchBlt` | Windows 95 or newer | Variant-neutral; no string parameters. | Allowed | Allowed for scaling the 320x200 backbuffer. Validate stretch mode and palette behavior on Win95-era displays. |

## 3. File I/O

| API | Required Windows version | ANSI/Unicode policy | Status | Notes |
| --- | --- | --- | --- | --- |
| `CreateFileA` | Windows 95 or newer | Use the explicit ANSI `A` variant only; avoid `CreateFile` and `CreateFileW`. | Allowed | Primary file-open API for assets, saves, and packed data. Keep paths ANSI and avoid modern path prefixes. |
| `ReadFile` | Windows 95 or newer | Variant-neutral; no string parameters. | Allowed | Primary synchronous read API. Pair with bounded buffer sizes and explicit error handling. |

## 4. Timers

| API | Required Windows version | ANSI/Unicode policy | Status | Notes |
| --- | --- | --- | --- | --- |
| `timeGetTime` | Windows 95 or newer with WinMM | Variant-neutral; no string parameters. | Allowed | Preferred millisecond tick source for frame timing. Link against WinMM and handle wraparound. |
| `SetTimer` | Windows 95 or newer | Variant-neutral; no string parameters. | Discouraged | Available for UI-style timers, but avoid as the primary game clock because message delivery granularity and ordering can vary. |

## 5. Input

Input should be handled through Win32 messages received by the ANSI window procedure rather than by modern raw-input APIs.

| API or message | Required Windows version | ANSI/Unicode policy | Status | Notes |
| --- | --- | --- | --- | --- |
| `WM_MOUSEMOVE` | Windows 95 or newer | Variant-neutral message constant. | Allowed | Primary cursor tracking message for point-and-click interaction. |
| `WM_LBUTTONDOWN` | Windows 95 or newer | Variant-neutral message constant. | Allowed | Primary click-start message for interactions and drag state. |
| `WM_LBUTTONUP` | Windows 95 or newer | Variant-neutral message constant. | Allowed | Primary click-release message for interactions. |
| Raw Input APIs | Windows XP or newer | Usually variant-neutral, but not Win95-era APIs. | Forbidden | Do not use `RegisterRawInputDevices` or related raw-input APIs in the Win95 runtime. |

## 6. Audio

| API | Required Windows version | ANSI/Unicode policy | Status | Notes |
| --- | --- | --- | --- | --- |
| `PlaySoundA` | Windows 95 or newer with WinMM | Use the explicit ANSI `A` variant only; avoid `PlaySound` and `PlaySoundW`. | Discouraged | Acceptable for simple WAV playback and smoke tests, but isolate behind the audio backend because it offers limited engine control. |
| `waveOutOpen` | Windows 95 or newer with WinMM | Variant-neutral; no file/path string parameters. | Allowed | Preferred foundation for engine-controlled PCM mixing when paired with `waveOutPrepareHeader` and `waveOutWrite`. |

## 7. Memory allocation

Memory allocation should remain centralized behind YAAT allocation helpers so low-memory behavior can be tested and toolchain CRT dependencies remain visible.

| API | Required Windows version | ANSI/Unicode policy | Status | Notes |
| --- | --- | --- | --- | --- |
| `HeapAlloc` | Windows 95 or newer | Variant-neutral; no string parameters. | Allowed | Acceptable low-level allocator when wrapped by engine allocation helpers and paired with `HeapFree`. |
| `HeapFree` | Windows 95 or newer | Variant-neutral; no string parameters. | Allowed | Required pair for `HeapAlloc` allocations. |
| `GlobalAlloc` | Windows 95 or newer | Variant-neutral; no string parameters. | Discouraged | Available for compatibility cases, but prefer heap-backed engine allocation helpers for normal runtime memory. |
| `malloc` | C runtime dependent | Variant-neutral; no Win32 string policy. | Discouraged | Only use when the selected Win95-compatible toolchain and CRT strategy have been audited. Prefer engine allocation wrappers. |
