/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SavePathArgs;

bool NativeFileDlgEnabled();
void NativeFileDlgSetEnabled(bool enabled);
void NativeSaveFileDlg(SavePathArgs* args);
