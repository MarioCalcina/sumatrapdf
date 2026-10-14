/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the version is the day of the build, yy.mm.dd[.n] (e.g. 26.10.03.1).
// The build passes it on the command line (cmd/helper/ng-version.ts), and
// only to SumatraConfig.cpp, so that a new day recompiles one file:
//   SUMATRA_VER=26.10.03.1 GIT_COMMIT_ID=<sha1>
// Everything else reads currentVersion, BuiltOnDate() and gitCommidId
// (SumatraConfig.h).

#define _QUOTEME(x) #x
#define QM(x) _QUOTEME(x)

#ifdef SUMATRA_VER
#define CURR_VERSION_STRA QM(SUMATRA_VER)
#endif

#define kAppName "SumatraPDF"
