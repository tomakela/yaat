# miniz

This directory vendors the miniz ZIP/inflate sources used by YAAT's internal
`.dat` archive wrapper. YAAT treats `.dat` files as renamed ZIP files and keeps
all ZIP-specific runtime logic isolated in `src/runtime/zip_archive.c`.

- Upstream: https://github.com/richgel999/miniz
- Vendored version: 3.1.2 source snapshot
- License: miniz includes permissive MIT-style text at the top of `miniz.c` and
  an Unlicense/public-domain dedication at the end of `miniz.c`; both are
  acceptable for this project.
- Compiler note: the vendored miniz headers use fixed-width integer typedefs
  from `stdint.h`. Keep the wrapper C89-friendly, but validate this third-party
  code with the selected Win95 compiler before relying on packed `.dat` archives
  in a release build.

YAAT compiles miniz with archive-writing, deflate-writing, time, and zlib
compatibility-name APIs disabled. The runtime only needs ZIP central-directory
reading plus stored/deflated entry extraction.
