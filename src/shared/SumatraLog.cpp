/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "SumatraLog.h"

Str gLogAppName = StrL("SumatraPDF");

Mutex gLogMutex;

// we use a dedicated Arena so we can do logging during crash handling
// where we want to avoid allocator deadlocks by calling malloc()
Arena* gLogAllocator = nullptr;

str::Builder* gLogBuf = nullptr;
bool gLogToConsole = false;
// we always log if IsDebuggerPresent()
// this forces logging to debuger always
bool gLogToDebugger = false;
// meant to avoid doing stuff during crash reporting
// will log to debugger (if no need for formatting)
bool gReducedLogging = false;
// when main thread exists other threads might still
// try to log. when true, this stops logging
bool gDestroyedLogging = false;

// if true, doesn't log if the same text has already been logged
// reduces logging but also can be confusing i.e. log lines are not showing up
static bool gSkipDuplicateLines = false;

// ng: the logview pipe is a win32 named pipe
bool gLogToPipe = false;

Str gLogFilePath;

// 1 MB - 128 to stay under 1 MB even after appending (an estimate)
constexpr int kMaxLogBuf = (1024 * 1024) - 128;

static bool IsValidHandle(void*) {
    return false;
}
static void* hLogPipe = nullptr;

static void logToPipe(Str) {}

void log(Str s) {
    if (gDestroyedLogging) {
        return;
    }
    if (gReducedLogging) {
        // in reduced logging mode, we do want to log to at least the debugger
        // ng: no debugger output channel on POSIX; stderr is the equivalent
        if (gLogToDebugger || gReducedLogging) {
            fwrite(s.s, 1, (size_t)s.len, stderr);
            fflush(stderr);
        }
        // if the pipe already connected, do log to it even if disabled
        // we do want easy logging, just want to reduce doing stuff
        // that can break crash handling
        if (gLogToPipe && IsValidHandle(hLogPipe)) {
            logToPipe(s);
        }
        return;
    }

    ScopedMutex lock(&gLogMutex);
    if (gDestroyedLogging) {
        return;
    }

    bool skipLog = gSkipDuplicateLines && gLogBuf && str::Contains(*gLogBuf, s);
    if (!skipLog) {
        if (gLogToDebugger) {
            fwrite(s.s, 1, (size_t)s.len, stderr);
            fflush(stderr);
        }
    }

    AtomicIntInc(&gAllowAllocFailure);
    AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);

    if (!gLogBuf) {
        gLogAllocator = ArenaNew();
        gLogBuf = new str::Builder(gLogAllocator);
        gLogBuf->Reserve(32 * 1024);
    } else {
        if (len(*gLogBuf) > kMaxLogBuf) {
            // TODO: use gLogBuf->Clear(), which doesn't free the allocated space
            gLogBuf->Reset();
        }
    }

    size_t n = (size_t)s.len;

    // when skipping, we skip buf (crash reports) and console
    // but write to file and logview
    if (!skipLog) {
        gLogBuf->Append(s);
    }

    if (!skipLog && gLogToConsole) {
        fwrite(s.s, 1, (size_t)s.len, stdout);
        fflush(stdout);
    }

    if (gLogFilePath) {
        auto* f = fopen(gLogFilePath.s, "a");
        if (f != nullptr) {
            fwrite(s.s, 1, n, f);
            fflush(f);
            fclose(f);
        }
    }
    logToPipe(s);
}

void StartLogToFile(Str path, bool removeIfExists) {
    ReportIf(len(gLogFilePath) != 0);
    gLogFilePath = str::Dup(path);
    FileWatcherSetSkipPath(gLogFilePath);
    if (removeIfExists) {
        file::Delete(path);
    }
}

bool WriteCurrentLogToFile(Str path) {
    Str slice;
    {
        ScopedMutex lock(&gLogMutex);
        if (!gLogBuf) {
            return false;
        }
        Str current = ToStr(*gLogBuf);
        if (len(current) == 0) {
            return false;
        }
        slice = str::Dup(current);
    }
    AutoCall freeSlice((void (*)(Str))str::Free, slice);
    bool ok = dir::CreateForFile(path);
    if (!ok) {
        logf("WriteCurrentLogToFile: dir::CreateForFile('%s') failed\n", path);
        return false;
    }
    ok = file::WriteFile(path, slice);
    if (!ok) {
        logf("WriteCurrentLogToFile: file::WriteFile('%s') failed\n", path);
    }
    return ok;
}

void DestroyLogging() {
    gDestroyedLogging = true;
    {
        ScopedMutex lock(&gLogMutex);
        delete gLogBuf;
        gLogBuf = nullptr;
        ArenaDelete(gLogAllocator);
        gLogAllocator = nullptr;
        str::FreePtr(&gLogFilePath);
    }
    FileWatcherSetSkipPath(Str());
}

// ng: gpui calls base::log(); src/gui/GpuiLog.cpp forwards to here, because a
// file that includes gpui.h can't also include base/Base.h (name clashes)
void SumatraLogBytes(const char* s, int n) {
    log(Str((char*)s, n));
}

// --- parent process chain (startup diagnostics) --------------------------------

// ng: needs Toolhelp + NtQueryInformationProcess; POSIX would read /proc
void LogParentProcessChain() {}
