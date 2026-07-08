# PNG Library Suitability for Win95 Runtime Use

This note evaluates practical PNG decoder choices for the YAAT Win95 point-and-click engine. The engine may still prefer offline conversion to packed BGRA assets, but this document records when a runtime PNG path is reasonable.

## Recommendation

Use **offline PNG decoding during asset packing** as the default runtime strategy, and keep **libspng + miniz** as the preferred optional runtime decoder behind a compile-time flag such as `YAAT_USE_RUNTIME_PNG` or `YAAT_USE_SPNG`.

Runtime PNG support is suitable only if the decoder is isolated behind an engine API that accepts in-memory data and returns 32-bit pixels. The decoder must not perform file I/O itself, must not call Unicode-only Win32 APIs, and must be easy to disable for minimal Win95 builds.

## Evaluation criteria

A library is suitable for this project if it meets these constraints:

- Builds as C source for 32-bit Windows without C++ runtime dependencies.
- Can be compiled with older or simple C toolchains used for Win95-era PE-i386 targets.
- Does not require modern Windows APIs or Unicode-only Win32 APIs.
- Can decode from memory buffers owned by the engine or asset pack loader.
- Can output 8-bit RGBA or a predictable format that can be converted to the engine's 32-bit BGRA/BGRX backbuffer.
- Has a small enough dependency surface to audit and vendor.
- Allows custom allocation or at least centralizes allocation enough for deterministic failure handling.
- Has licensing compatible with source vendoring in the engine.

## Candidate comparison

| Candidate | Suitability | Strengths | Risks / concerns | Verdict |
| --- | --- | --- | --- | --- |
| libspng + miniz | High for optional runtime path | PNG-focused C API; can use miniz instead of zlib; small vendored set of `spng.c`, `spng.h`, `miniz.c`, and `miniz.h`; decodes from memory; supports explicit output formats | Must test with the exact Win95 toolchain; more code than LodePNG; dynamic allocation and PNG edge cases still need guardrails | Preferred optional runtime decoder |
| LodePNG | Medium-high for tools and fallback runtime | ISO C90-compatible C option; no zlib/libpng dependency; only `lodepng.c`/`lodepng.h` needed after renaming from `.cpp`; very simple vendoring | Often slower than optimized decoders; all-in-one implementation may be less attractive for security-sensitive untrusted PNG input | Good fallback, especially for offline tools |
| libpng + zlib | Medium | Mature, ANSI C, widely tested, PNG reference ecosystem | More build system and dependency complexity; larger API surface; easier to accidentally depend on external DLLs or mismatched CRTs | Acceptable for offline asset tools; less attractive for Win95 runtime |
| stb_image | Medium-low | Single-header convenience; supports many formats; easy RGBA decode | Multi-format parser increases attack surface; PNG behavior is less strict than dedicated libraries; SIMD/preprocessor paths need careful old-CPU/toolchain settings | Avoid for engine runtime unless already used elsewhere |
| fpng | Low | Very fast for a restricted subset; dependency-light | C++ source and intentionally limited PNG subset make it a poor general PNG decoder for authored assets | Not recommended |

## libspng + miniz suitability details

libspng is the best fit for an optional runtime decoder because it is dedicated to PNG decoding, can be linked with miniz as a zlib replacement, and has a memory-buffer API that matches the desired engine boundary. The official build documentation states that using `SPNG_USE_MINIZ` lets libspng be embedded with `spng.c`, `miniz.c`, and their headers. Miniz itself is a single-source zlib/Deflate implementation, which is a good match for vendored Win32 builds.

A YAAT wrapper should expose a narrow API, for example:

```c
int yaat_load_png_rgba8(
    const unsigned char *png_data,
    unsigned long png_size,
    unsigned char **out_rgba,
    unsigned long *out_width,
    unsigned long *out_height);
```

The wrapper should then convert RGBA to the renderer's BGRA/BGRX format outside libspng, or request a suitable output format if that remains portable across the chosen library version.

### Required isolation rules

- Keep all `spng_*` and `mz_*` symbols private to `src/platform/win32/png_loader.c` or a similarly isolated module.
- Do not allow `spng` or `miniz` calls outside the wrapper.
- Pass PNG bytes from the asset loader to the decoder; do not open files in the PNG module.
- Use compile-time switches:
  - `YAAT_USE_RUNTIME_PNG` for the engine feature.
  - `YAAT_USE_SPNG` for the concrete implementation.
  - `SPNG_USE_MINIZ` when compiling libspng against miniz.
- Ensure the fallback build stubs the loader with a deterministic unsupported-format error.

### External symbols to audit

When vendored, the runtime should audit imports and unresolved symbols around these names:

- `spng_ctx_new`
- `spng_ctx_free`
- `spng_set_png_buffer`
- `spng_get_ihdr`
- `spng_decoded_image_size`
- `spng_decode_image`
- `mz_uncompress`
- `mz_inflate`
- `mz_inflateInit`
- `mz_inflateEnd`
- CRT allocation and memory helpers used by both libraries, such as `malloc`, `free`, `memcpy`, `memset`, and `memcmp`

The expected result is no direct dependency on modern Win32 APIs. Any CRT dependency should be linked consistently with the selected Win95-compatible toolchain.

## Win95-specific risks

### Toolchain compatibility

The main unknown is not Win32 API usage but old compiler compatibility. Before enabling runtime PNG by default, compile the exact vendored versions with each target compiler and inspect the resulting PE imports. TCC, Open Watcom, old MinGW, and MSVC 6 may differ in C99 support, inline semantics, warning behavior, and CRT selection.

### CPU assumptions

Avoid decoder builds that assume SSE2 or newer x86 features. Windows 95 machines may be Pentium-class systems without SSE2. Build with conservative i386 flags and disable optional SIMD paths unless a runtime dispatch layer is proven safe.

### Memory footprint

PNG decode memory can exceed the final pixel buffer because filtering, interlacing, and color conversion require temporary storage. libspng documents row-buffer memory use and extra gamma-related memory in some formats. YAAT should enforce maximum dimensions, maximum decoded byte counts, and asset-pack validation before allocating.

### Security model

PNG is a complex compressed format. Runtime decoding should be limited to trusted shipped assets or validated packed assets. If the game accepts user-supplied PNGs, prefer decoding during an offline/import step on a modern host and shipping raw packed BGRA data to Win95.

## Practical policy

1. Author art as PNG for modern tool compatibility.
2. During packing, decode PNG to raw BGRA/BGRX frames whenever possible.
3. Ship packed raw frames for the default Win95 runtime.
4. Enable runtime PNG only for debug builds, modding builds, or games that explicitly accept the memory and compatibility cost.
5. If runtime PNG is enabled, use libspng + miniz first and keep LodePNG as the low-dependency fallback candidate.

## Verification checklist

Before accepting a vendored PNG decoder into the runtime, run these checks:

- Build `png_loader.c`, `spng.c`, and `miniz.c` with every supported compiler.
- Inspect imports for NT-only or Unicode-only APIs.
- Decode representative palette, grayscale, RGB, RGBA, transparent, and interlaced PNG files.
- Reject oversized dimensions before allocation.
- Convert decoded pixels to the GDI renderer's 32-bit BGRA/BGRX format.
- Run under a Windows 95 VM or emulator with the target CRT/DLL set.
- Keep a no-runtime-PNG build path that compiles without any vendored PNG sources.

## Source notes

- libspng build documentation describes `SPNG_USE_MINIZ` and the four-file vendored build shape: <https://libspng.org/docs/build/>.
- libspng decode documentation describes decoder memory behavior: <https://libspng.org/docs/decode/>.
- miniz describes itself as a single-source zlib/Deflate implementation: <https://github.com/richgel999/miniz>.
- LodePNG documents ISO C90 C support and no zlib/libpng dependency: <https://lodev.org/lodepng/>.
- libpng documents ANSI C source availability and zlib dependency: <https://www.libpng.org/pub/png/libpng.html>.
- stb documents single-file use and SIMD build considerations in its repository notes: <https://github.com/nothings/stb>.
