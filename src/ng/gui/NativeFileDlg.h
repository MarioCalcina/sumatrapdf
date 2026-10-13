/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SavePathArgs;

#if OS_WIN
bool NativeFileDlgEnabled();
void NativeSaveFileDlg(SavePathArgs* args);
bool NativeOpenFileDlg(MainWindow* win, Str filter, Str initialPath, bool multiSelect, StrVec* pathsOut);
bool NativeOpenDocsDlg(MainWindow* win, StrVec* pathsOut);
TempStr NativeFileDlgTestTemp(MainWindow* win, Str what, Str arg);
#elif OS_DARWIN
bool NativeFileDlgEnabled();
void NativeFileDlgSetEnabled(bool enabled);
void NativeSaveFileDlg(SavePathArgs* args);
#else
inline bool NativeFileDlgEnabled() {
    return false;
}
#endif
