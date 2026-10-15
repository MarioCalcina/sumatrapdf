/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Window;
}

// called when a drag-out is over; `dropped`: another app took it
typedef void (*DragOutEndFn)(void* user, bool dropped);

bool MacDragOutText(gpui::Window* gw, Str text, DragOutEndFn onEnd, void* user);
bool MacDragOutImage(gpui::Window* gw, Str png, Str fileName, DragOutEndFn onEnd, void* user);
TempStr MacPasteboardTemp(bool drag);
