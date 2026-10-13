/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

.section __TEXT,__const
#define SYMBOL(x) _##x

.balign 8
.globl SYMBOL(gEmbeddedLzsa)
.globl SYMBOL(gEmbeddedLzsaEnd)
#if defined(__wasm__)
.type gEmbeddedLzsa,@object
.type gEmbeddedLzsaEnd,@object
#endif
SYMBOL(gEmbeddedLzsa):
.incbin EMBEDDED_LZSA
SYMBOL(gEmbeddedLzsaEnd):

#if defined(__wasm__)
.size gEmbeddedLzsa,gEmbeddedLzsaEnd-gEmbeddedLzsa
.size gEmbeddedLzsaEnd,0
#endif

