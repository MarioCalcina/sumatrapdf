/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the gpui side of the shell. Only forward declarations of gpui types, so
// a file that does not include gpui.h can still talk to it.

namespace gpui {
struct App;
struct Window;
} // namespace gpui

struct MainWindow;

// creates the gpui window, its root view and the MainWindow that owns them
MainWindow* AppShellCreateWindow(gpui::App* app, int dipW, int dipH);
// the app the first window was created with, so a second one needs no plumbing
gpui::App* AppShellGetApp();
// ng: the theme is installed before the first window opens, so the app has to
// be known before AppShellCreateWindow()
void AppShellSetApp(gpui::App*);
// runs the event loop until the last window closes
int AppShellRun(gpui::App* app);
// height of the menu bar row above the canvas, in dips. Zero where the
// menu bar is the OS one (macOS) rather than a row of the window.
constexpr int kMenuBarDy = 28;
// true when the menu bar is the OS one, not a row of the window
bool AppShellNativeMenu();
// install the OS menu bar from the window's model. No-op when the bar is
// drawn into the window, and when this window is not the front one.
void AppShellSyncMenu(MainWindow* win);
// the model was rebuilt; the next sync of the front window reinstalls it
void AppShellMenuRebuilt(MainWindow* win);

void AppShellInvalidate(MainWindow* win);
// one line of gpui frame timings since the last call, for the -dbg-control
// performance snapshot: "frames=N meanMs=.. p95Ms=.. maxMs=.. over16ms=N"
TempStr AppShellFrameStatsTemp(MainWindow* win, bool reset);
// -dbg-control's TestInput / TestUiState: input a posted window message cannot
// carry (modifiers, hover, a held button) and what the shell made of it
TempStr AppShellTestInput(MainWindow* win, Str kind, int a, int b, int c, int d);
// the same into any gpui window (a tool window's)
TempStr AppShellTestInputGpui(gpui::Window* gw, Str kind, int a, int b, int c, int d);
TempStr AppShellUiStateTemp(MainWindow* win);
TempStr AppShellFieldMenuTemp(MainWindow* win);
// orig's gSupressNextAltMenuTrigger: the Alt that is down was used for
// something (Alt + wheel), so its release must not enter the menu bar
void AppShellSuppressAltMenu(MainWindow* win);
void AppShellQuit();
// brings the window to the foreground (orig's MainWindow::Focus())
void AppShellActivateWindow(MainWindow* win);
bool AppShellIsFrameFocused(MainWindow* win);
void AppShellFocusFrame(MainWindow* win);
void AppShellAfterNativeDrag(MainWindow* win);
// closes the gpui window of `win`; the MainWindow itself is deleted by the
// caller (CloseWindow)
void AppShellCloseWindow(MainWindow* win);
void AppShellDeleteWindow(MainWindow* win);
// gpui has no "window closed" callback, so the shell's tick notices a window
// the user closed with the title bar and runs CloseWindow for it
void AppShellReapClosedWindows();
// the same prompt with a title of its own (certificate, image, ...); filter is
// orig's lpstrFilter with \1 for \0 ("PDF documents\1*.pdf\1")
TempStr AppShellPromptForPathTemp(MainWindow* win, Str title, Str filter, Str initialPath = {});
// File / Open style prompt taking several files where the platform can
bool AppShellPromptForFiles(MainWindow* win, Str filter, StrVec* pathsOut);
void AppShellSetTitle(MainWindow* win, Str title);
enum class FileDropPhase {
    Hover,
    Drop,
    Leave
};
bool AppShellAcceptDrop(MainWindow* win, float x, float y, const Str* paths, int n, FileDropPhase phase);
// orig's SetTabsInTitlebar: whether the frame has the port's caption (tab
// strip, menu button, caption buttons) in place of the system's title bar
void SetTabsInTitlebar(MainWindow* win, bool inTitleBar);
// where the caption puts the menu bar and the tab strip, and how tall it is;
// false when the frame has no caption of the port's (TestLayout)
bool AppShellCaptionRects(MainWindow* win, Rect* menuOut, Rect* tabsOut, int* dyOut);
// the border the frame keeps around its content while it has the caption
int AppShellFrameBorder(MainWindow* win);
// gpui's client-side title bar for the frame, without touching the flag
// (fullscreen has no caption of either kind)
void AppShellSetClientTitleBar(MainWindow* win, bool on);
// ng: gpui's WindowSetFullScreen, for orig's EnterFullScreen /
// ExitFullScreen on the native frame.
// `restoreMaximized` re-maximizes on the way out (orig: the window was
// maximized before presentation mode).
void AppShellSetFullScreen(MainWindow* win, bool fullScreen, bool restoreMaximized);
// screen rect of the window, for the -dbg-control layout snapshot
Rect AppShellWindowScreenRect(MainWindow* win);
// the canvas (MainWindow::canvasRc) in screen points
Rect AppShellCanvasScreenRect(MainWindow* win);
// moves / sizes the frame to `r` (outer rectangle, screen points; empty: leave
// it) and maximizes it
bool AppShellPlaceWindow(MainWindow* win, Rect r, bool maximize);
// Current: the canvas as the frame is now. DocumentTab: a document tab is
// about to exist, so a UseTabs strip counts before the tab does.
enum class CanvasPredict {
    Current,
    DocumentTab,
};
Rect AppShellPredictCanvasRc(MainWindow* win, CanvasPredict predict);
void AppShellSyncCanvasSize(MainWindow* win);
// work area of the monitor the frame is on. Primary work area when win is null.
Rect AppShellWorkArea(MainWindow* win);
// full bounds of that monitor, including the dock or taskbar
Rect AppShellMonitorRect(MainWindow* win);
// a remembered frame kept on the display it overlaps most
Rect AppShellKeepOnScreen(Rect rect);
// shift rect into the work area. fully: the whole rect must be inside
Rect AppShellShiftToWorkArea(Rect rect, MainWindow* win, bool fully);
// the frame's rectangle while it is not maximized. False when it is not known
bool AppShellNormalWindowRect(MainWindow* win, Rect* out);
// dpi of the monitor the frame is on; 96 where it is not known
int AppShellWindowDpi(MainWindow* win);
int AppShellFrameDpi(MainWindow* win);
int AppShellDpiPx(MainWindow* win, int px);
float AppShellRenderScale(gpui::Window* win);
void AppShellDisableAutoTermination();
void AppShellHandleOpenDocs();
void AppShellOpenDocsReady();
void AppShellWatchMacEvents();
void AppShellMacClientTitleBar(MainWindow* win, bool on);
void AppShellMacFinishMenus();
void AppShellNoteRecentDoc(Str path);
bool AppShellSwipeTurnsPages();
bool AppShellSystemIsDark();
// what the print panel does with the PDF once it closes
enum class PrintedPdf {
    Delete,
    Keep
};
bool AppShellPrintPdf(MainWindow* win, Str path, Str jobTitle, PrintedPdf after);
// presentation mode's auto-hidden cursor (orig hides it with SetCursor(null))
void AppShellShowCursor(MainWindow* win, bool show);
// Fullscreen.PreventSleep: keep the display awake
void AppShellPreventSleep(bool on);
