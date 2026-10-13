/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Pixmap;

bool MacPasteboardSetPng(Str png);
Pixmap* MacFileIconPixmap(Str path, int px);
void MacPreventDisplaySleep(bool on);
bool MacComposeEmail(Str path);
bool MacIsDefaultPdfReader();
void MacMakeDefaultPdfReader();
bool MacStartInstanceServer(void (*onCmds)(Str));
bool MacSendToRunningInstance(Str cmds);
TempStr MacAppBundlePathTemp();
