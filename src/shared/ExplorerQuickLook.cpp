/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "VirtKeys.h"

#include "Settings.h"
#include "DocController.h"
#include "Commands.h"
#include "Flags.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "AppSettings.h"
#include "gui/AppShell.h"
#include "ExplorerQuickLook.h"

MainWindow* FindExplorerQuickLookWindow() {
    for (MainWindow* win : gWindows) {
        if (win->isQuickLook && IsMainWindowValidAndNotClosing(win)) {
            return win;
        }
    }
    return nullptr;
}

bool ExplorerQuickLookOnKeyDown(MainWindow* win, int key, bool hasModifiers) {
    if (!win || !win->isQuickLook) {
        return false;
    }
    if (key == VK_ESCAPE || key == VK_SPACE) {
        CloseWindow(win, true, false);
        return true;
    }
    if (hasModifiers) {
        return false;
    }
    // the About tab eats the keys without acting on them
    if (key == VK_LEFT || key == VK_PRIOR) {
        if (!win->IsCurrentTabAbout()) {
            OpenNextPrevFileInFolder(win, false);
        }
        return true;
    }
    if (key == VK_RIGHT || key == VK_NEXT) {
        if (!win->IsCurrentTabAbout()) {
            OpenNextPrevFileInFolder(win, true);
        }
        return true;
    }
    return false;
}

// Explorer integration is Windows only
void ApplyExplorerQuickLookChrome(MainWindow*) {}

void ShowExplorerQuickLook(Str) {}

void ExplorerQuickLookApplyFromSettings() {}

void ExplorerQuickLookRemoveRunKey() {}

bool RunExplorerQuickLookAgentLoop() {
    return false;
}

