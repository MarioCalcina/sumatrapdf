/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Everything application-specific that base/CrashHandler_posix.cpp asks for:
// where the report goes and the app's part of its text.

#include "base/Base.h"
#include "base/File.h"
#include "base/CrashHandler.h"
#include "base/SettingsUtil.h"

#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"

#include "SumatraCrashHandler.h"
#include "SumatraCrashCommon.h"

#include "SumatraLog.h"

// the crash info goes next to the screenshots, under the application data directory
TempStr GetCrashInfoDirTemp() {
    TempStr dir = GetAppDataDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    return path::JoinTemp(dir, StrL("crashinfo"));
}

static void GetProgramInfo(str::Builder& b) {
    TempStr exePath = GetSelfExePathTemp();
    b.Append(fmt("Exe: %s %s\n", exePath, GetFileSizeAsStrTemp(exePath)));
    b.Append(fmt("BuiltOn: %s\n", BuiltOnDate()));
    Str instType = IsRunningInPortableMode() ? StrL("portable") : StrL("installed");
    b.Append(fmt("ExeType: %s\n", instType));
    b.Append(fmt("Ver: %s", currentVersion));
    if (gIsPreReleaseBuild) {
        b.Append(StrL(" pre-release"));
    }
    if (gIsDebugBuild) {
        if (!str::Contains(currentVersion, StrL(" (dbg)"))) {
            b.Append(StrL(" (dbg)"));
        }
    }
    b.Append(StrL("\n"));

    if (gitCommidId) {
        b.Append(fmt("Git: %s\n", gitCommidId));
    }
}

// What we are, what tripped, and the log and settings; the stacks follow.
// Runs on the crashing thread, so everything is allocated from a.
static Str GetCrashComment(Arena* a, Str condStr, Str fileLine, bool isCrash) {
    str::Builder b(a);
    b.Reserve(16 * 1024);
    b.Append(isCrash ? StrL("Type: crash\n") : StrL("Type: debug report (not crash)\n"));
    if (condStr) {
        b.Append(str::Format(a, "Cond: %s @ %s\n", condStr, fileLine));
    }
    GetProgramInfo(b);
    AppendUncaughtMupdfError(a, b);
    Str sysInfo = CrashHandlerSystemInfo();
    if (sysInfo) {
        b.Append(sysInfo);
        b.Append(StrL("\n"));
    }
    AppendLogAndSettings(b);
    return ToStr(b);
}

// FileStates are the largest part and we don't need them in a crash report
static void CaptureSettings() {
    TempStr path = GetSettingsPathTemp();
    // can be empty on first run but that's fine because then we know it has default values
    Str prefsData = file::ReadFile(path);
    if (len(prefsData) == 0) {
        return;
    }
    Settings* gp = NewSettings(prefsData);
    DeleteFileStates(gp->fileStates);
    gp->fileStates = new Vec<FileState*>();
    Str d = SerializeSettings(gp, {});
    CrashHandlerSetSettings(d);
    str::Free(d);
    DeleteSettings(gp);
    str::Free(prefsData);
}

void InstallSumatraCrashHandler() {
    if (gIsAsanBuild) {
        return;
    }

    CrashHandlerConfig cfg{};
    cfg.reportPath = path::JoinTemp(GetCrashInfoDirTemp(), StrL("sumatrapdfcrash.txt"));
    cfg.forTesting = gForTesting;
    cfg.getCrashComment = GetCrashComment;
    cfg.onCrashBegin = OnCrashBegin;

    InstallCrashHandler(cfg);
    logf("InstallSumatraCrashHandler: report '%s'\n", cfg.reportPath);
    CaptureSettings();
}
