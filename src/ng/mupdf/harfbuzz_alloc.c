/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <stdlib.h>

// Wrappers around harfbuzz allocators that use standard C malloc/free.
// With HAVE_ATEXIT defined, harfbuzz registers atexit handlers to free
// its singletons. During atexit, mupdf's fz_hb_lock hasn't been called
// so fz_hb_secret (the fz_context) is NULL and fz_hb_free would crash.
// We use plain malloc/free which is safe both during normal operation
// (mupdf's default allocator is malloc/free) and during atexit cleanup.

void* sumatra_hb_malloc(size_t size) {
    return malloc(size);
}
void* sumatra_hb_calloc(size_t n, size_t size) {
    return calloc(n, size);
}
void* sumatra_hb_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}
void sumatra_hb_free(void* ptr) {
    free(ptr);
}
