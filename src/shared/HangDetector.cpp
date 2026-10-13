/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Detects a blocked UI thread and logs what every one of our threads was doing.
//
// A watchdog thread pings the UI thread with a message and times the round
// trip. If the UI thread doesn't pump messages within kHangThresholdMs, all
// other threads are frozen, a raw callstack is captured from each, they're
// resumed, and only then are the addresses symbolized and logged (symbolizing
// while threads are frozen could block on a heap or loader lock one of them
// holds).

#include "base/Base.h"

#include "HangDetector.h"

#include "base/UITask.h"

constexpr int kHangThresholdMs = 3000;
constexpr int kPingIntervalMs = 1000;
constexpr int kMaxHangReports = 10;

static ThreadHandle gWatchdogThread = nullptr;
static AtomicBool gShouldStop = 0;
static AtomicBool gPingPending = 0;
static AtomicInt gHeartbeat = 0;
static int gHangReportsLeft = kMaxHangReports;

bool IsUiHangDetectorRunning() {
    return gWatchdogThread != nullptr;
}

static void HangDetectorPingUi() {
    AtomicIntInc(&gHeartbeat);
    AtomicBoolSet(&gPingPending, false);
}

static void ReportHang() {
    if (gHangReportsLeft <= 0) {
        return;
    }
    gHangReportsLeft--;
    logf("UI thread blocked for at least %d ms (report %d of %d)\n", kHangThresholdMs,
         kMaxHangReports - gHangReportsLeft, kMaxHangReports);
}

static void HangDetectorThreadFunc() {
    int lastHeartbeat = AtomicIntGet(&gHeartbeat);
    int blockedMs = 0;
    bool inHang = false;
    while (!AtomicBoolGet(&gShouldStop)) {
        if (!AtomicBoolGet(&gPingPending)) {
            AtomicBoolSet(&gPingPending, true);
            uitask::Post(MkFunc0Void(HangDetectorPingUi), "HangDetectorPing");
        }
        SleepInMs(kPingIntervalMs);
        if (AtomicBoolGet(&gShouldStop)) {
            return;
        }
        int heartbeat = AtomicIntGet(&gHeartbeat);
        if (heartbeat != lastHeartbeat) {
            if (inHang) {
                log(StrL("UI thread is responsive again\n"));
            }
            lastHeartbeat = heartbeat;
            blockedMs = 0;
            inHang = false;
            continue;
        }
        blockedMs += kPingIntervalMs;
        if (!inHang && blockedMs >= kHangThresholdMs) {
            inHang = true;
            ReportHang();
        }
    }
}

void StartUiHangDetector() {
    if (IsUiHangDetectorRunning()) {
        return;
    }
    gHangReportsLeft = kMaxHangReports;
    AtomicBoolSet(&gShouldStop, false);
    AtomicBoolSet(&gPingPending, false);
    AtomicIntSet(&gHeartbeat, 0);
    gWatchdogThread = StartThread(MkFunc0Void(HangDetectorThreadFunc), StrL("HangDetector"));
    if (!gWatchdogThread) {
        return;
    }
    logf("StartUiHangDetector: threshold %d ms, up to %d reports\n", kHangThresholdMs, kMaxHangReports);
}

void StopUiHangDetector() {
    AtomicBoolSet(&gShouldStop, true);
    if (gWatchdogThread) {
        JoinThread(&gWatchdogThread, -1);
    }
}
