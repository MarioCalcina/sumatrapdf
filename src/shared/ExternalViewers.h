/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct WindowTab;

bool CanSendAsEmailAttachment(WindowTab* tab = nullptr);
bool SendAsEmailAttachment(WindowTab* tab);
Str FilePathForOtherApps(WindowTab* tab);

bool CouldBePDFDoc(WindowTab*);
bool IsPdfDoc(WindowTab*);
bool PathMatchFilter(Str path, Str filter);

bool RunWithExe(WindowTab* tab, Str cmdLine, Str filter);
