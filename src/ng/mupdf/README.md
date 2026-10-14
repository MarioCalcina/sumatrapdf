# Our additions to mupdf

Files here are ours, not mupdf's: the `mupdf` target in
`cmd/helper/ng-targets.ts` compiles them, but they live outside the vendored
`ext/mupdf` tree so that they are not mistaken for upstream code and do not
need an `ext/patches/` entry. This directory is on the `mupdf` target's include
path, so the tools we patch inside `ext/mupdf` include them by bare name
(`#include "pkcs7_mac.h"`).

- `mupdf_load_system_font_mac.c` — finds installed fonts through Core Text for
  mupdf's font fallback
- `harfbuzz_alloc.c` — plain-malloc harfbuzz allocators, safe during atexit
- `noto_sumatra.[ch]` — compiled instead of mupdf's `noto.c`: the built-in fonts
  are not linked into the binary but fetched by file name through
  `fz_set_builtin_font_loader()`. SumatraPDF tries the `fonts/` entries of the
  embedded archive (`cmd/helper/embedded.ts`), then a cache directory, then the
  URL in `fonts_map.c`
- `pkcs7_mac.[ch]` — PDF signature verification and signing with a .pfx through
  the macOS Security framework (patch `0003` makes mupdf's `pdfsign` / `murun`
  use it too)
- `load-jxl.cpp` / `load-jxl.h` — JPEG XL metadata and pixel decoding through
  jxldec for MuPDF images and PDF `/JXLDecode` streams

_Changes_ to mupdf itself still go into `ext/mupdf` in place and get recorded as
a patch in `ext/patches/` (see its README). Put code here only when it is
entirely ours: a whole new file that mupdf's build does not know about.
