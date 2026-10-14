/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// What the engines and the application model ask of the shell, for the
// targets that have none: the console tools and the Quick Look library.

#include "base/Base.h"
#include "base/SettingsUtil.h"
#include "base/GuessFileType.h"
#include "base/Archive.h"

#include "DocProperties.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "Translations.h"
#include "EmbeddedResources.h"
#include "SumatraPDF.h"

// EngineMupdf asks the app how the user configured ebook rendering
struct EBookUI;
struct FileEBookUI;

EBookUI* GetEBookUI() {
    return nullptr;
}

FileEBookUI* GetFileEBookUI(Str) {
    return nullptr;
}

TempStr GetSumatraDataDirTemp() {
    return {};
}

bool IsStressTesting() {
    return false;
}

// SumatraPDF.cpp
void InitializePolicies(bool) {}

bool HasPermission(Perm) {
    return true;
}

bool CanAccessDisk() {
    return true;
}

bool AnnotationsAreDisabled() {
    return false;
}

bool IsUIRtl() {
    return false;
}

bool SettingsUseTabs() {
    return true;
}

bool SettingsRememberOpenedFiles() {
    return true;
}

void SetCurrentLang(Str langCode) {
    trans::SetCurrentLangByCode(langCode);
}

void SetCurrentLanguageAndRefreshUI(Str langCode) {
    SetCurrentLang(langCode);
}

void UpdateDocumentColors() {}

void UpdateFixedPageScrollbarsVisibility() {}

void ApplySettingsToWindowsUi() {}

void ReloadSettingsUpdateWindows(bool) {}

void MaybeRedrawHomePage() {}

SeqStrings gScrollbarModeNames = "windows\0smart\0overlay\0hidden\0";
SeqStrings gToolbarModeNames = "show\0hide\0overlay\0";
SeqStrings gToolbarPositionNames = "top\0bottom\0";

// bodies as in SumatraPDF.cpp: the model asks for them
RenderCache* gRenderCache;

int ScrollbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gScrollbarModeNames, gSettings->scrollbars);
    if (idx < 0) {
        idx = kScrollbarWindows;
    }
    return idx;
}

bool ScrollbarsAreHidden() {
    return ScrollbarModeFromPrefs() == kScrollbarHidden;
}

bool ScrollbarsUseOverlay() {
    int mode = ScrollbarModeFromPrefs();
    return mode == kScrollbarSmart || mode == kScrollbarOverlay;
}

// Canvas.cpp
bool ShowTransparencyGrid() {
    return false;
}

// Print.cpp / TextViewWnd.cpp
void GetPrintersInfo(str::Builder&) {}

void ShowTextInWindow(MainWindow*, Str, Str) {}
void SetTextViewWindowText(Str) {}
bool IsTextViewWindowTitle(Str) {
    return false;
}
void CloseTextViewWindow() {}
bool IsTextViewWindowVisible() {
    return false;
}

// Menu.cpp
int CmdIdFromVirtualZoom(float) {
    return CmdZoomCustom;
}

// Favorites.cpp is UI now and lives in the SumatraPDF target;
// CommandAvailability only asks whether there are any
bool HasFavorites() {
    return false;
}

// HomePage.cpp; FileThumbnails.cpp is in the app lib now
void HomePageInvalidateLayoutCache() {}

struct FileState;
void HomePageThumbnailChanged(FileState*) {}

// SumatraPDFCommon.cpp, which only the app builds
void OnThumbnailsTypeChanged() {}

// Theme.cpp asks the UI to repaint; AppSettings owns the UI fonts
void UpdateAfterThemeChange() {}

void ThemeApplyPlatformColors() {}

// Notifications.cpp
void MaybeDelayedWarningNotification(Str msg) {
    logf("warning: %s\n", msg);
}

// gui/DocCanvas.cpp: RenderCache draws through the gpui canvas, which the
// console tools have no window for
struct BitmapCacheEntry;
namespace gpui {
struct PaintCtx;
}

bool CanvasDrawTile(gpui::PaintCtx*, BitmapCacheEntry*, Rect, Rect) {
    return false;
}

void CanvasDrawTileOutline(gpui::PaintCtx*, Rect) {}

void CanvasFreeTileImage(BitmapCacheEntry*) {}

// ExternalViewers.cpp is in the SumatraPDF target (it needs WindowTab); the
// command policy asks it whether a viewer's filter matches

bool PathMatchFilter(Str, Str) {
    return false;
}

// UpdateCheck.cpp is in the app target only
bool HasPendingPreReleaseUpdate() {
    return false;
}

// GlobalHotkeys.cpp is app-target only
void ReRegisterGlobalHotkeys() {}

// ReadAloud.cpp is app-target only; the settings apply the voice
bool ApplyReadAloudVoiceFromSettings() {
    return false;
}

// gui/NativeWindow_mac.mm is app-target only; Theme.cpp's System theme asks it
bool AppShellSystemIsDark() {
    return false;
}
