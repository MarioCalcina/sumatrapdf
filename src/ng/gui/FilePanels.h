/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class OpenPanelSelect {
    One,
    Many
};

bool MacOpenPanel(Str title, Str filter, Str initialPath, OpenPanelSelect select, StrVec* pathsOut);
