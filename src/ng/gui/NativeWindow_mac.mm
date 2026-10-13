/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#import <PDFKit/PDFKit.h>
#pragma pop_macro("defer")
#undef BOOL
#include "base/MacTypesShow.h"

#include "base/UITask.h"
#include "base/File.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Translations.h"
#include "Version.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/NativeFileDlg.h"
#include "SumatraDialogs.h"

// AppKit turns automatic termination back on at the end of launch when it
// still counts zero windows. Under lldb the app never becomes frontmost, so
// that quits the process with status 0.
void AppShellDisableAutoTermination() {
    NSProcessInfo* info = [NSProcessInfo processInfo];
    info.automaticTerminationSupportEnabled = NO;
    [info disableAutomaticTermination:@"SumatraPDF"];
}

// Backing pixels per point. The canvas is drawn in points, so a tile rendered
// at GetZoomReal is upscaled on a Retina display.
float AppShellRenderScale(gpui::Window* win) {
    NSView* view = (__bridge NSView*)gp::PlatWindowHandle(win);
    if (!view || !view.window) {
        return 1;
    }
    float scale = (float)view.window.backingScaleFactor;
    if (!(scale > 0)) {
        return 1;
    }
    return scale;
}

static StrVec gOpenDocs;
static bool gOpenDocsReady = false;

// the window the user last worked in
static MainWindow* KeyMainWindow() {
    NSWindow* key = [NSApp keyWindow] ?: [NSApp mainWindow];
    for (MainWindow* win : gWindows) {
        NSView* view = (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin);
        if (key && view.window == key) {
            return win;
        }
    }
    return len(gWindows) > 0 ? gWindows[0] : CreateAndShowMainWindow(nullptr);
}

static void OpenQueuedDocs() {
    int n = len(gOpenDocs);
    MainWindow* win = n > 0 ? KeyMainWindow() : nullptr;
    if (!win) {
        return;
    }
    Vec<Str> paths;
    for (Str path : gOpenDocs) {
        VecAppend(paths, path);
    }
    OpenDroppedFiles(win, paths.els, n);
    gOpenDocs.Reset();
    AppShellActivateWindow(win);
}

@interface SumatraOpenDocs : NSObject
@end

@implementation SumatraOpenDocs
- (void)openDocs:(NSAppleEventDescriptor*)event withReply:(NSAppleEventDescriptor*)reply {
    (void)reply;
    NSAppleEventDescriptor* docs = [[event paramDescriptorForKeyword:keyDirectObject] coerceToDescriptorType:typeAEList];
    for (NSInteger i = 1; i <= docs.numberOfItems; i++) {
        NSString* path = [[docs descriptorAtIndex:i] fileURLValue].path;
        if (path) {
            gOpenDocs.Append(Str((char*)path.UTF8String));
        }
    }
    if (gOpenDocsReady) {
        uitask::Post(MkFunc0Void(OpenQueuedDocs), "OpenQueuedDocs");
    }
}
@end

// Finder's "Open With" and `open -a` deliver documents as an Apple event, not
// in argv. Those arriving before the first window exists wait in gOpenDocs.
void AppShellHandleOpenDocs() {
    static SumatraOpenDocs* handler = [[SumatraOpenDocs alloc] init];
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:handler
                                                       andSelector:@selector(openDocs:withReply:)
                                                     forEventClass:kCoreEventClass
                                                        andEventID:kAEOpenDocuments];
}

void AppShellOpenDocsReady() {
    gOpenDocsReady = true;
    OpenQueuedDocs();
}

static MainWindow* MainWindowFor(NSWindow* window) {
    for (MainWindow* win : gWindows) {
        NSView* view = (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin);
        if (window && view.window == window) {
            return win;
        }
    }
    return nullptr;
}

static NSString* ToNSString(Str s) {
    return [[NSString alloc] initWithBytes:s.s length:(NSUInteger)len(s) encoding:NSUTF8StringEncoding];
}

// switches an open window between a normal title bar and the full-size content
// view gpui's WindowOpen gives a client-titlebar window
void AppShellMacClientTitleBar(MainWindow* win, bool on) {
    NSView* view = win ? (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin) : nil;
    NSWindow* window = view.window;
    if (!window || !(window.styleMask & NSWindowStyleMaskTitled)) {
        return;
    }
    bool isOn = (window.styleMask & NSWindowStyleMaskFullSizeContentView) != 0;
    if (isOn == on) {
        return;
    }
    if (on) {
        window.styleMask |= NSWindowStyleMaskFullSizeContentView;
    } else {
        window.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
    }
    window.titleVisibility = on ? NSWindowTitleHidden : NSWindowTitleVisible;
    window.titlebarAppearsTransparent = on;
    window.titlebarSeparatorStyle = on ? NSTitlebarSeparatorStyleNone : NSTitlebarSeparatorStyleAutomatic;
}

// --- menu bar ----------------------------------------------------------------

static NSMenuItem* StdMenuItem(Str title, SEL action, NSString* key, NSEventModifierFlags mods) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:ToNSString(title) action:action keyEquivalent:key];
    item.keyEquivalentModifierMask = mods;
    return item;
}

// What every Mac app has and gpui's menu bar, built from the app's menu model,
// leaves out: Services and Hide in the app menu, Minimize and Bring All to Front
// in the Window menu, which gpui finds by its English title, so it's translated
// here. The actions go to AppKit through the responder chain.
void AppShellMacFinishMenus() {
    NSMenu* bar = [NSApp mainMenu];
    NSMenu* appMenu = bar.numberOfItems > 0 ? [bar itemAtIndex:0].submenu : nil;

    // About, Settings..., separator, Quit (InstallNativeMenu)
    constexpr NSInteger kAppMenuItems = 4;
    if (appMenu.numberOfItems == kAppMenuItems) {
        NSMenuItem* services = StdMenuItem(Tr("Services"), nil, @"", 0);
        services.submenu = [[NSMenu alloc] initWithTitle:services.title];
        NSApp.servicesMenu = services.submenu;
        NSEventModifierFlags cmd = NSEventModifierFlagCommand;
        NSArray<NSMenuItem*>* items = @[
            [NSMenuItem separatorItem],
            services,
            [NSMenuItem separatorItem],
            StdMenuItem(fmt(Tr("Hide %s").s, StrL(kAppName)), @selector(hide:), @"h", cmd),
            StdMenuItem(Tr("Hide Others"), @selector(hideOtherApplications:), @"h", cmd | NSEventModifierFlagOption),
            StdMenuItem(Tr("Show All"), @selector(unhideAllApplications:), @"", 0),
        ];
        for (NSUInteger i = 0; i < items.count; i++) {
            [appMenu insertItem:items[i] atIndex:(NSInteger)(2 + i)];
        }
        [appMenu insertItem:[NSMenuItem separatorItem] atIndex:1];
    }

    // AppKit adds the window list below
    NSMenu* windowMenu = NSApp.windowsMenu;
    if (!windowMenu || [windowMenu indexOfItemWithTarget:nil andAction:@selector(performMiniaturize:)] >= 0) {
        return;
    }
    NSString* title = ToNSString(Tr("Window"));
    windowMenu.title = title;
    for (NSMenuItem* item in bar.itemArray) {
        if (item.submenu == windowMenu) {
            item.title = title;
        }
    }
    NSArray<NSMenuItem*>* items = @[
        StdMenuItem(Tr("Minimize"), @selector(performMiniaturize:), @"m", NSEventModifierFlagCommand),
        StdMenuItem(Tr("Zoom"), @selector(performZoom:), @"", 0),
        [NSMenuItem separatorItem],
        StdMenuItem(Tr("Bring All to Front"), @selector(arrangeInFront:), @"", 0),
        [NSMenuItem separatorItem],
    ];
    for (NSUInteger i = 0; i < items.count; i++) {
        [windowMenu insertItem:items[i] atIndex:(NSInteger)i];
    }
}

// --- trackpad pinch ----------------------------------------------------------

// gpui has no gesture events. Magnify events are taken before it sees them and
// the ones that arrive before the UI queue runs become a single zoom.
struct PinchZoom {
    MainWindow* win = nullptr;
    float factor = 1;
    float x = 0;
    float y = 0;
    bool posted = false;
};

static PinchZoom gPinch;

static void ApplyPinchZoom() {
    PinchZoom p = gPinch;
    gPinch = PinchZoom{};
    DocCanvasPinchZoom(p.win, p.factor, p.x, p.y);
}

static void OnMagnify(NSEvent* ev) {
    MainWindow* win = MainWindowFor(ev.window);
    if (!win) {
        return;
    }
    // the same point gpui's view gives its mouse events: dips, y down
    NSView* view = (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin);
    NSPoint pt = [view convertPoint:ev.locationInWindow fromView:nil];
    if (!view.isFlipped) {
        pt.y = view.bounds.size.height - pt.y;
    }
    if (gPinch.win != win) {
        gPinch.factor = 1;
    }
    gPinch.win = win;
    gPinch.factor *= std::max(0.1f, 1.f + (float)ev.magnification);
    gPinch.x = (float)pt.x;
    gPinch.y = (float)pt.y;
    if (!gPinch.posted) {
        gPinch.posted = true;
        uitask::Post(MkFunc0Void(ApplyPinchZoom), "ApplyPinchZoom");
    }
}

// --- system appearance -------------------------------------------------------

// true while macOS shows dark mode, including when Auto has switched to it
bool AppShellSystemIsDark() {
    NSAppearance* appearance = [NSApplication sharedApplication].effectiveAppearance;
    NSAppearanceName name =
        [appearance bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [name isEqualToString:NSAppearanceNameDarkAqua];
}

@interface SumatraAppearanceObserver : NSObject
@end

@implementation SumatraAppearanceObserver
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)ctx {
    (void)keyPath;
    (void)object;
    (void)change;
    (void)ctx;
    // re-resolve the System theme once AppKit is done switching
    uitask::Post(MkFunc0Void(UpdateThemeAfterSystemColorChange), "UpdateThemeAfterSystemColorChange");
}
@end

static MainWindow* gSmartZoomWin = nullptr;

static void ApplySmartZoom() {
    MainWindow* win = gSmartZoomWin;
    gSmartZoomWin = nullptr;
    if (win && IsMainWindowValid(win) && win->IsDocLoaded()) {
        win->ToggleZoom();
    }
}

// two-finger double tap: orig's press-and-tap, which toggles the zoom
static void OnSmartMagnify(NSEvent* ev) {
    MainWindow* win = MainWindowFor(ev.window);
    if (!win || gSmartZoomWin) {
        return;
    }
    gSmartZoomWin = win;
    uitask::Post(MkFunc0Void(ApplySmartZoom), "ApplySmartZoom");
}

// --- trackpad swipe ----------------------------------------------------------

// Trackpad settings' "Swipe between pages" takes two-finger swipes; its
// three-finger choice arrives as swipe events instead (OnSwipe)
bool AppShellSwipeTurnsPages() {
    return [NSEvent isSwipeTrackingFromScrollEventsEnabled];
}

static MainWindow* gSwipeWin = nullptr;
static int gSwipeDir = 0;

static void ApplySwipe() {
    MainWindow* win = gSwipeWin;
    gSwipeWin = nullptr;
    DocCanvasSwipePage(win, gSwipeDir);
}

// deltaX > 0 goes back, as in Safari: to the previous page
static void OnSwipe(NSEvent* ev) {
    MainWindow* win = MainWindowFor(ev.window);
    int dir = ev.deltaX > 0 ? 1 : (ev.deltaX < 0 ? -1 : 0);
    if (!win || dir == 0 || gSwipeWin) {
        return;
    }
    gSwipeWin = win;
    gSwipeDir = dir;
    uitask::Post(MkFunc0Void(ApplySwipe), "ApplySwipe");
}

// the hooks into AppKit events gpui does not forward
void AppShellWatchMacEvents() {
    NSEventMask mask = NSEventMaskMagnify | NSEventMaskSmartMagnify | NSEventMaskSwipe;
    [NSEvent addLocalMonitorForEventsMatchingMask:mask
                                          handler:^NSEvent*(NSEvent* ev) {
                                            if (ev.type == NSEventTypeSmartMagnify) {
                                                OnSmartMagnify(ev);
                                            } else if (ev.type == NSEventTypeSwipe) {
                                                OnSwipe(ev);
                                            } else {
                                                OnMagnify(ev);
                                            }
                                            return ev;
                                          }];

    static SumatraAppearanceObserver* observer = [[SumatraAppearanceObserver alloc] init];
    [NSApp addObserver:observer forKeyPath:@"effectiveAppearance" options:0 context:nullptr];
}

// --- printing ----------------------------------------------------------------

// keeps the document alive while the print sheet is up, then deletes the
// PDF it was made from when that was a temporary one
@interface SumatraPrintJob : NSObject {
  @public
    PDFDocument* doc;
    NSString* deletePath;
}
@end

@implementation SumatraPrintJob
- (void)printOperationDidRun:(NSPrintOperation*)op success:(MacObjcBool)success contextInfo:(void*)ctx {
    (void)op;
    (void)success;
    if (deletePath) {
        [[NSFileManager defaultManager] removeItemAtPath:deletePath error:nil];
    }
    // balances the retain in AppShellPrintPdf
    (void)CFBridgingRelease(ctx);
}
@end

// the macOS print panel for a PDF file, as a sheet on the window. False when
// PDFKit can't print it (locked, printing not allowed). With PrintedPdf::Delete
// the file is deleted once the panel closes.
bool AppShellPrintPdf(MainWindow* win, Str path, Str jobTitle, PrintedPdf after) {
    NSView* view = win ? (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin) : nil;
    NSWindow* window = view.window;
    NSString* nsPath = ToNSString(path);
    if (!window || !nsPath) {
        return false;
    }
    PDFDocument* doc = [[PDFDocument alloc] initWithURL:[NSURL fileURLWithPath:nsPath]];
    if (!doc || doc.isLocked || !doc.allowsPrinting) {
        return false;
    }
    NSPrintInfo* info = [[NSPrintInfo sharedPrintInfo] copy];
    NSPrintOperation* op = [doc printOperationForPrintInfo:info scalingMode:kPDFPrintPageScaleToFit autoRotate:YES];
    if (!op) {
        return false;
    }
    op.jobTitle = ToNSString(jobTitle) ?: @"SumatraPDF";
    op.showsPrintPanel = YES;
    op.showsProgressPanel = YES;
    op.printPanel.options |= NSPrintPanelShowsCopies | NSPrintPanelShowsPageRange | NSPrintPanelShowsPaperSize |
                             NSPrintPanelShowsOrientation | NSPrintPanelShowsScaling | NSPrintPanelShowsPreview;

    SumatraPrintJob* job = [[SumatraPrintJob alloc] init];
    job->doc = doc;
    job->deletePath = after == PrintedPdf::Delete ? nsPath : nil;
    [op runOperationModalForWindow:window
                          delegate:job
                    didRunSelector:@selector(printOperationDidRun:success:contextInfo:)
                       contextInfo:(__bridge_retained void*)job];
    return true;
}

// macOS's AddPathToRecentDocs(): the recent documents in the Dock menu
void AppShellNoteRecentDoc(Str path) {
    NSString* s = ToNSString(path);
    if (!s) {
        return;
    }
    [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:s]];
}

// --- the save panel ----------------------------------------------------------

// off while a -dbg-control script drives the app: it can't answer the panel
static bool gNativeFileDlgOn = true;

bool NativeFileDlgEnabled() {
    return gNativeFileDlgOn;
}

void NativeFileDlgSetEnabled(bool enabled) {
    gNativeFileDlgOn = enabled;
}

static void FreeSaveArgs(SavePathArgs* args) {
    str::Free(args->title);
    str::Free(args->initialPath);
    str::Free(args->defExt);
    str::Free(args->filter);
    str::Free(args->nativeFile);
    str::Free(args->nativeDir);
    delete args;
}

// the name the panel returned, with the default extension when it has none
static TempStr SavedPathTemp(SavePathArgs* args, NSURL* url) {
    const char* s = url ? url.path.UTF8String : nullptr;
    if (!s) {
        return {};
    }
    TempStr path = str::DupTemp(Str((char*)s));
    Str ext = args->defExt;
    if (!args->noDefExt && len(ext) > 0 && len(path::GetExtTemp(path)) == 0) {
        path = str::JoinTemp(path, ext);
    }
    return path;
}

static void SaveFileDlgNow(SavePathArgs* args) {
    MainWindow* win = args->win;
    NSView* view = IsMainWindowValidAndNotClosing(win) ? (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin) : nil;
    NSWindow* window = view.window;
    if (!window) {
        args->path = {};
        args->onDone.Call(args);
        FreeSaveArgs(args);
        return;
    }

    Str name = len(args->nativeFile) > 0 ? args->nativeFile : args->initialPath;
    TempStr dir = len(args->nativeDir) > 0 ? str::DupTemp(args->nativeDir) : path::GetDirTemp(args->initialPath);
    NSSavePanel* panel = [NSSavePanel savePanel];
    panel.canCreateDirectories = YES;
    panel.nameFieldStringValue = ToNSString(path::GetBaseNameTemp(name)) ?: @"";
    if (len(dir) > 0 && dir::Exists(dir)) {
        panel.directoryURL = [NSURL fileURLWithPath:ToNSString(dir) isDirectory:YES];
    }
    if (args->nativeTitle && len(args->title) > 0) {
        panel.message = ToNSString(args->title);
    }
    // the port's dialog reports no filter either: the callers go by the extension
    args->filterIndex = 0;
    [panel beginSheetModalForWindow:window
                  completionHandler:^(NSModalResponse res) {
                    TempStr path = res == NSModalResponseOK ? SavedPathTemp(args, panel.URL) : TempStr{};
                    logf("NativeSaveFileDlg: '%s'\n", path);
                    args->path = path;
                    args->onDone.Call(args);
                    FreeSaveArgs(args);
                    if (IsMainWindowValid(win)) {
                        AppShellInvalidate(win);
                    }
                  }];
}

// macOS's save panel as a sheet on the window, in place of the port's dialog.
// Takes ownership of args; shown from the ui task queue once gpui has unwound
void NativeSaveFileDlg(SavePathArgs* args) {
    uitask::Post(MkFunc0(SaveFileDlgNow, args), "NativeSaveFileDlg");
}
