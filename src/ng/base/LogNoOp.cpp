/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

// what SumatraLog.cpp defines, for the executables without a log
bool gLogToConsole = false;

void log(Str /*s*/) {}