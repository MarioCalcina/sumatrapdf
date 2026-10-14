/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: launching another program and handing a path to the desktop.
// Launch_posix.cpp is fork + exec, with open for documents.

// runs `path` with `params` as its command line. A path that isn't an
// executable is handed to the desktop (open).
bool LaunchFileShell(Str path, Str params = {}, Str verb = {}, bool hidden = false);
bool LaunchFileShellArgs(const StrVec& args);
int RunAndWait(const StrVec& args);
bool LaunchBrowser(Str url);
// shows the file's directory in the desktop file manager
void OpenPathInDefaultFileManager(Str path);

// a program we start and later ask whether it is done (-regen-thumbnails).
// StartChildProcess returns nullptr when the program can't be started
struct ChildProcess;
ChildProcess* StartChildProcess(Str cmdLine);
bool ChildProcessHasExited(ChildProcess* p, int waitMs);
void CloseChildProcess(ChildProcess* p);
