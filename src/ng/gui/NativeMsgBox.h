/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig asks with the system's MessageBoxW and, for "Unsaved changes", a
// task dialog. On Windows the port shows the same system dialogs; they are
// run from the ui task queue (their message loops let gpui render) and
// answer through the callback the port's own dialogs use. Under the
// automation channel they are off, so a script gets the port's dialogs,
// which it can answer; TestNativeMsgBox turns them on to look at them.

inline bool NativeMsgBoxEnabled() {
    return false;
}
