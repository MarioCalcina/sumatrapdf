/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Crypto.h"
#include "base/Launch.h"

#include "gui/UIModels.h"

#include "SumatraConfig.h"
#include "Translations.h"
#include "Version.h"
#include "AppTools.h"

// Portable helpers shared with AppTools_posix.cpp.

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
TempStr FormatFileSizeShortTransTemp(i64 size) {
    Str units[3] = {Tr("GB"), Tr("MB"), Tr("KB")};
    return str::FormatSizeShortTemp(size, units);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTransTemp(i64 size) {
    if (size <= 0) {
        return fmt("%d", size);
    }
    TempStr n1 = FormatFileSizeShortTransTemp(size);
    TempStr n2 = str::FormatNumWithThousandSepTemp(size);
    return fmt("%s (%s %s)", n1, n2, Tr("Bytes"));
}

// returns true if file exists
bool LaunchFileIfExists(Str path) {
    if (len(path) == 0) {
        return false;
    }
    if (!file::Exists(path)) {
        logf("LaunchFileIfExists: !file::Exists('%s')\n", path);
        return false;
    }
    LaunchFileShell(path, {}, StrL("open"));
    return true;
}
