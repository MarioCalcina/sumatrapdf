/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's installer window is win32 (a yellow frame it paints with GDI+,
// real Button / Edit / Checkbox controls over it). Here it is a gpui window of
// the same size with the same rows, texts and behaviour. Gone with the
// features they installed (step 17a): the lzsa payload, the "Let Windows show
// previews" / "Let Windows Desktop Search search" checkboxes and everything
// that unregistered PdfFilter.dll / PdfPreview.dll before an upgrade - a
// static exe installs itself by copying.

#include "gui/GpuiBridge.h"

#include "base/File.h"
#include "base/Timer.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Flags.h"
#include "Version.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "gui/DialogWidgets.h"
#include "Installer.h"
#include "InstallerUtil.h"
#include "UpdateCheck.h"

#include "SumatraLog.h"

int RunInstaller(Flags*) {
    log(StrL("-install is not supported on this platform\n"));
    return 1;
}

