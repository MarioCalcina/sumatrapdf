/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the one win32 message path of the shell. gpui does not expose the native
// handle, and it has no hook for the messages a few services need
// (WM_HOTKEY for GlobalHotkeys, WM_COPYDATA for Explorer QuickLook,
// WM_ACTIVATE for the hotkey target window). We claim the gpui window of this
// thread by its class name and subclass it. OS file drops arrive as gpui
// ExternalPaths drops on the shell root.

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"
#include "ProgressUpdateUI.h"
#include "EngineBase.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "GlobalHotkeys.h"
#include "ReadAloud.h"
#include "SearchAndDDE.h"
#include "PdfTools.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "HomePage.h"
#include "SumatraConfig.h"
#include "RefHover.h"
#include "gui/DocCanvas.h"
#include "AnnotEditToolbar.h"
#include "SelectTextKeyboard.h"

void AppShellEnableFileDrop(MainWindow*) {}

void AppShellFrameChanged(MainWindow*) {}

void AppShellForgetNativeHwnd(MainWindow*) {}

// x, y are window dips from the top left. Hover may omit paths. Leave clears
// the Merge PDF insertion bar. A drop on the canvas opens the files; while
// Merge PDF is up the dialog takes a drop anywhere on the frame.
bool AppShellAcceptDrop(MainWindow* win, float x, float y, const Str* paths, int n, FileDropPhase phase) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return false;
    }
    if (phase == FileDropPhase::Leave || !CanAccessDisk()) {
        bool ignored = false;
        PdfToolDialogOnDragOver(win, nullptr, false, &ignored, DropHost::Frame);
        return false;
    }
    if (n < 0) {
        n = 0;
    }
    if (n > 0 && !paths) {
        return false;
    }

    bool hasPdf = n == 0 && phase == FileDropPhase::Hover;
    for (int i = 0; i < n; i++) {
        if (str::EndsWithI(paths[i], StrL(".pdf"))) {
            hasPdf = true;
            break;
        }
    }

    PointF pt{x, y};
    if (phase == FileDropPhase::Hover) {
        bool accept = false;
        if (PdfToolDialogOnDragOver(win, &pt, hasPdf, &accept, DropHost::Frame)) {
            return accept;
        }
    } else if (PdfToolDialogIsMerge(win)) {
        // orig accepts a drop anywhere on the frame while Merge PDF is up,
        // and the dialog consumes it instead of opening a tab
        if (n > 0) {
            PdfToolDialogOnDropFiles(win, paths, n, &pt, DropHost::Frame);
        }
        return n > 0;
    }

    Rect rc = win->canvasRc;
    bool onCanvas = x >= (float)rc.x && x < (float)(rc.x + rc.dx) && y >= (float)rc.y && y < (float)(rc.y + rc.dy);
    if (!onCanvas) {
        return false;
    }
    if (phase != FileDropPhase::Drop) {
        return true;
    }
    if (n <= 0) {
        return false;
    }
    logf("AppShellAcceptDrop: %d file(s)\n", n);
    OpenDroppedFiles(win, paths, n);
    return true;
}
