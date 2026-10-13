/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PdfDate.h"

TempStr FormatPdfDateLocalTimeTemp(time_t secs) {
    if (secs == 0) {
        return {};
    }
    struct tm tm;
    if (!localtime_r(&secs, &tm)) {
        return {};
    }
    char buf[64];
    size_t n = strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return str::DupTemp(Str(buf, (int)n));
}
