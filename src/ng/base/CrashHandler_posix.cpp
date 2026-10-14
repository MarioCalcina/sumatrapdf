/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

// A signal handler that writes a text crash report (the app's getCrashComment
// plus the stacks) and logs it.

#include "base/Base.h"

#include "base/File.h"
#include "base/CrashHandler.h"

#include <csignal>
#include <unistd.h>
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

// first line of every report; cmd/ng-crashes.ts looks for it
#define kCrashReportMagic "Crash report v1\n"

constexpr int kAltStackSize = 128 * 1024;
// a handler stuck on a lock the faulting code held must still end the process
constexpr int kHandlerTimeoutSecs = 20;

void MacAppendSignalInfo(str::Builder& b, int sig, void* sigInfo);
void MacAppendStacks(str::Builder& b, void* uctx);
TempStr MacSysInfoTemp();

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
    Str path = gCfg.reportPath;
    if (len(path) > 0) {
        dir::CreateForFile(path);
        file::WriteFile(path, report);
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
    raise(sig);
}

NO_INLINE void _writeDebugReport(Str condStr, Str fileLine, bool isCrash) {
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

void InstallCrashHandler(const CrashHandlerConfig& cfg) {
    if (gCrashHandlerArena || DebuggerAttached()) {
        return;
    }
    gCrashHandlerArena = ArenaNew();
    gCfg = cfg;
    gCfg.reportPath = str::Dup(gCrashHandlerArena, cfg.reportPath);
    gSysInfo = str::Dup(gCrashHandlerArena, MacSysInfoTemp());

    SetSignalHandlers();
}

