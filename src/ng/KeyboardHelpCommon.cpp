/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Accelerators.h"
#include "Translations.h"
#include "Commands.h"
#include "KeyboardHelp.h"
#include "KeyboardHelpCommon.h"

bool IsHelpListedCmd(int cmdId) {
    for (const KbSectionDef& definition : kSections) {
        for (const int* id = definition.commands; *id; id++) {
            if (*id == cmdId) {
                return true;
            }
        }
    }
    return false;
}

