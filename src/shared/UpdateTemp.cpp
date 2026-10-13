/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/DirScan.h"

#include "UpdateTemp.h"

// GetTempFileName keeps only the first 3 letters of the prefix, so
// "sumatra-installer" is created as sum<4 hex>.tmp. The download is that
// path plus ".exe". The stub is unused. The exe is the installer.
bool IsUpdateTempFileName(Str name) {
    constexpr Str kPrefix = StrL("sum");
    constexpr int kHexLen = 4;
    int tailOff = len(kPrefix) + kHexLen;
    if (len(name) < tailOff + LenL(".tmp") || !str::StartsWithI(name, kPrefix)) {
        return false;
    }
    for (int i = len(kPrefix); i < tailOff; i++) {
        if (str::HexDigitVal(name.s[i]) < 0) {
            return false;
        }
    }
    Str tail(name.s + tailOff, len(name) - tailOff);
    return str::EqI(tail, StrL(".tmp")) || str::EqI(tail, StrL(".tmp.exe"));
}

void DeleteStaleUpdateTemps(Str, Str, int) {}
void NoteTempInstallerRelaunch() {}
void ScheduleDeleteTempInstaller() {}
