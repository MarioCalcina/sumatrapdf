/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"

#include "Settings.h"
#include "DocController.h"
#include "Commands.h"
#include "VirtKeys.h"
#include "ShortcutParse.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "Screenshot.h"
#include "AppSettings.h"
#include "gui/AppShell.h"

#include "GlobalHotkeys.h"
#include "GlobalHotkeysCommon.h"

// Cocoa has no global-hotkey abstraction in gpui yet.
void ReRegisterGlobalHotkeys() {}
