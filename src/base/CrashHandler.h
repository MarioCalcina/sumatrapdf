/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Everything app-specific the crash handler needs. Callbacks are optional
// (null is fine) and run on the crashing thread, so they must not allocate
// from the process heap.
struct CrashHandlerConfig {
    Str reportPath;  // the text report
    bool forTesting; // terminate on a debug report so automated tests fail

    // The app's part of a report. condStr/fileLine describe the ReportIf() that
    // fired and are empty for a crash. Everything it returns must be allocated
    // from a, which is the crash arena.
    Str (*getCrashComment)(Arena* a, Str condStr, Str fileLine, bool isCrash);
    void (*onCrashBegin)();
};

void InstallCrashHandler(const CrashHandlerConfig& cfg);

// pre-allocated, so that a crash doesn't have to touch the process heap
Arena* CrashHandlerArena();

// os, cpu, memory and graphics driver info, gathered at install time so that
// getCrashComment() can use it without asking the system anything
Str CrashHandlerSystemInfo();
