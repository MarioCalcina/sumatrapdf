/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

.section __TEXT,__const
#define SYMBOL(x) _##x

.balign 8
.globl SYMBOL(gEmbeddedLzsa)
.globl SYMBOL(gEmbeddedLzsaEnd)
SYMBOL(gEmbeddedLzsa):
.incbin EMBEDDED_LZSA
SYMBOL(gEmbeddedLzsaEnd):

