/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <zlib.h>
#include "base/File.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

Kind kindEnginePostScript = "enginePostScript";

// Ghostscript discovery and process launching use Win32 APIs.

TempStr GetGhostscriptPathTemp() {
    return {};
}

EngineBase* CreateEnginePsFromFile(Str) {
    return nullptr;
}

bool IsEnginePsAvailable() {
    return false;
}

bool IsEnginePsSupportedFileType(FileType) {
    return false;
}

