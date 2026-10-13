/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's uninstaller window is win32 over the same painted frame as the
// installer's; here it is a gpui window with the same logo, message and single
// button. The previewer / search filter un-registration is gone with those
// features (step 17a).

#include "gui/GpuiBridge.h"

#include "base/File.h"
#include "base/Timer.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "SumatraConfig.h"
#include "Flags.h"
#include "Version.h"
#include "AppTools.h"
#include "Translations.h"
#include "Installer.h"
#include "InstallerUtil.h"

#include "SumatraLog.h"

int RunUninstaller(Flags*) {
    log(StrL("-uninstall is not supported on this platform\n"));
    return 1;
}

