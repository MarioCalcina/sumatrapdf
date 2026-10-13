/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

// ng: base/CrashHandler.cpp is win32 minidumps + dbghelp. POSIX has no
// equivalent, so this is the same API backed by a signal handler that writes
// a text crash report (the app's getCrashComment plus the stacks) next to
// where the .dmp would have gone, and logs it. A report meant for the server
// is also left as a pending file and POSTed to submitUrl on the next start:
// a crashed process is in no state to talk to the network.

#include "base/Base.h"

#include "base/File.h"
#include "base/Http.h"
#include "base/CrashHandler.h"

#include <csignal>
#include <unistd.h>
#include <execinfo.h>
#include <sys/sysctl.h>
#include <sys/types.h>

static Arena* gCrashHandlerArena = nullptr;
static CrashHandlerConfig gCfg{};
static Str gSysInfo;

Arena* CrashHandlerArena() {
    return gCrashHandlerArena;
}

Str CrashHandlerSystemInfo() {
    return gSysInfo;
}

constexpr int kMaxFrames = 64;

// first line of every report; the crash server rejects a body without it
#define kCrashReportMagic "Crash report v1\n"

constexpr int kAltStackSize = 128 * 1024;
// a handler stuck on a lock the faulting code held must still end the process
constexpr int kHandlerTimeoutSecs = 20;
#define kPendingSuffix "-pending.txt"
constexpr int kPendingMaxAgeSecs = 7 * 24 * 60 * 60;
constexpr int kHttpClientErrorMin = 400;
constexpr int kHttpClientErrorMax = 499;

void MacAppendSignalInfo(str::Builder& b, int sig, void* sigInfo);
void MacAppendStacks(str::Builder& b, void* uctx);
TempStr MacSysInfoTemp();

static void AppendBacktrace(str::Builder& b) {
    void* frames[kMaxFrames];
    int n = backtrace(frames, kMaxFrames);
    char** syms = backtrace_symbols(frames, n);
    if (!syms) {
        b.Append(StrL("(no backtrace)\n"));
        return;
    }
    for (int i = 0; i < n; i++) {
        b.Append(Str(syms[i]));
        b.Append(StrL("\n"));
    }
    free(syms);
}

// the report path is the .dmp path with the extension swapped: a POSIX crash
// has no minidump, only text
static TempStr ReportPathTemp(Str suffix) {
    if (len(gCfg.crashDumpPath) == 0) {
        return {};
    }
    TempStr path = str::DupTemp(gCfg.crashDumpPath);
    TempStr ext = path::GetExtTemp(path);
    if (len(ext) > 0) {
        path.len -= ext.len;
        path.s[path.len] = 0;
    }
    return str::JoinTemp(path, suffix);
}

static bool ShouldUpload(bool isCrash) {
    if (gCfg.localOnly || len(gCfg.submitUrl) == 0) {
        return false;
    }
    return isCrash ? gCfg.uploadCrashes : gCfg.uploadDebugReports;
}

// sig, sigInfo and uctx are the signal handler's arguments; 0 / null for a debug report
static void WriteCrashReport(Str condStr, Str fileLine, bool isCrash, int sig, void* sigInfo, void* uctx) {
    str::Builder b(gCrashHandlerArena);
    b.Reserve(16 * 1024);
    b.Append(StrL(kCrashReportMagic));
    if (sig != 0) {
        MacAppendSignalInfo(b, sig, sigInfo);
    }
    if (gCfg.getCrashComment) {
        b.Append(gCfg.getCrashComment(gCrashHandlerArena, condStr, fileLine, isCrash));
    }
    MacAppendStacks(b, uctx);
    Str report = ToStr(b);
    TempStr path = ReportPathTemp(StrL(".txt"));
    if (len(path) > 0) {
        dir::CreateForFile(path);
        file::WriteFile(path, report);
        if (ShouldUpload(isCrash)) {
            file::WriteFile(ReportPathTemp(StrL(kPendingSuffix)), report);
        }
    }
    logf("crash report written to '%s'\n%s\n", path, report);
}

static void SignalHandler(int sig, siginfo_t* info, void* uctx) {
    // restore the default so a fault while reporting terminates us
    signal(sig, SIG_DFL);
    alarm(kHandlerTimeoutSecs);
    if (gCfg.onCrashBegin) {
        gCfg.onCrashBegin();
    }
    WriteCrashReport(fmt("signal %d", sig), {}, true, sig, info, uctx);
    if (gCfg.showCrashMessage && !gCfg.localOnly) {
        gCfg.showCrashMessage();
    }
    raise(sig);
}

NO_INLINE void _uploadDebugReport(Str condStr, Str fileLine, bool isCrash) {
    if (!gCrashHandlerArena) {
        return;
    }
    WriteCrashReport(condStr, fileLine, isCrash, 0, nullptr, nullptr);
    if (gCfg.forTesting && !isCrash) {
        // a ReportIf() under -for-testing must fail the test run
        _exit(105);
    }
}

// lldb/gdb are already attached when ng-dbg launches the process. Leave the
// signal to them so the stop is the fault, not this handler.
static bool DebuggerAttached() {
    struct kinfo_proc info{};
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    size_t size = sizeof(info);
    if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0) {
        return false;
    }
    return (info.kp_proc.p_flag & P_TRACED) != 0;
}

static const int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};

static void SetSignalHandlers() {
    struct sigaction sa{};
    sa.sa_sigaction = SignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    // a stack overflow leaves no room on the thread's own stack
    static char altStack[kAltStackSize];
    stack_t ss{};
    ss.ss_sp = altStack;
    ss.ss_size = kAltStackSize;
    if (sigaltstack(&ss, nullptr) == 0) {
        sa.sa_flags |= SA_ONSTACK;
    }
    for (int sig : kSignals) {
        sigaction(sig, &sa, nullptr);
    }
}

struct PendingUpload {
    Str path;
    Str url;
};

static bool IsHttpClientError(int status) {
    return status >= kHttpClientErrorMin && status <= kHttpClientErrorMax;
}

static void UploadPendingThread(PendingUpload* d) {
    Str report = file::ReadFile(d->path);
    HttpRsp rsp;
    bool ok = len(report) > 0 && HttpPostUrl(d->url, StrL("text/plain"), {}, report, &rsp);
    int status = (int)rsp.httpStatusCode;
    logf("UploadPendingThread: ok=%d status=%d bytes=%d url=%s\n", (int)ok, status, len(report), d->url);
    // a report the server refused stays refused; an unreachable server gets another try
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    bool isOld = FileTimeDiffInSecs(now, file::GetModificationTime(d->path)) > kPendingMaxAgeSecs;
    if (ok || IsHttpClientError(status) || isOld || len(report) == 0) {
        file::Delete(d->path);
    }
    str::Free(report);
    str::Free(d->path);
    str::Free(d->url);
    delete d;
}

// sends the report the previous run left behind
static void UploadPendingReport() {
    if (gCfg.localOnly || len(gCfg.submitUrl) == 0) {
        return;
    }
    TempStr path = ReportPathTemp(StrL(kPendingSuffix));
    if (!file::Exists(path)) {
        return;
    }
    auto d = new PendingUpload();
    d->path = str::Dup(path);
    d->url = str::Dup(gCfg.submitUrl);
    RunAsync(MkFunc0<PendingUpload>(UploadPendingThread, d), StrL("UploadCrashReport"));
}

void InstallCrashHandler(const CrashHandlerConfig& cfg) {
    if (gCrashHandlerArena || DebuggerAttached()) {
        return;
    }
    gCrashHandlerArena = ArenaNew();
    gCfg = cfg;
    gCfg.crashDumpPath = str::Dup(gCrashHandlerArena, cfg.crashDumpPath);
    gCfg.submitUrl = str::Dup(gCrashHandlerArena, cfg.submitUrl);
    gCfg.fullDumpEnvVar = str::Dup(gCrashHandlerArena, cfg.fullDumpEnvVar);
    gSysInfo = str::Dup(gCrashHandlerArena, MacSysInfoTemp());

    SetSignalHandlers();
    UploadPendingReport();
}

void UninstallCrashHandler() {
    if (!gCrashHandlerArena) {
        return;
    }
    for (int sig : kSignals) {
        signal(sig, SIG_DFL);
    }
    ArenaDelete(gCrashHandlerArena);
    gCrashHandlerArena = nullptr;
}
