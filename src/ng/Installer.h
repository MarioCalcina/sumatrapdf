/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's installer is a second win32 UI inside the same exe, driven by an
// lzsa payload (libsumatrapdf.dll, PdfFilter.dll, PdfPreview.dll) appended to
// the installer build. This port links one static exe and dropped the
// previewer / search filter in step 17a, so the payload is gone: installing is
// copying ourselves plus the registry work. The window is gpui
// (Installer.cpp / Uninstaller.cpp), everything here is Windows-only.

constexpr int kInstallerWinDy = 340;

// DWORD 0|1 in the uninstall key: whether the install created a desktop shortcut
#define kRegDesktopShortcut "DesktopShortcut"

enum class PreviousInstallationType {
    None = 0,
    User = 1,
    Machine = 2,
    Both = 3
};

struct PreviousInstallationInfo {
    Str installationDir;
    PreviousInstallationType typ = PreviousInstallationType::None;
    bool allUsers = false;
    // installs before the DesktopShortcut registry value always created one
    bool desktopShortcut = true;

    PreviousInstallationInfo() = default;
    ~PreviousInstallationInfo();
};

struct Flags;

// ng: orig reads the global `gCli`; the port's parsed command line is private
// to SumatraPDF.cpp, so it is passed in and kept in `gCli` here
int RunInstaller(Flags* cli);
int RunUninstaller(Flags* cli);

inline bool IsOurExeInstalled() {
    return false;
}
inline void ReRegisterFileAssociations() {}

