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
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Theme.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"

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

// the hooks into AppKit events gpui does not forward
void AppShellWatchMacEvents() {
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMagnify
                                          handler:^NSEvent*(NSEvent* ev) {
                                            OnMagnify(ev);
                                            return ev;
                                          }];

    static SumatraAppearanceObserver* observer = [[SumatraAppearanceObserver alloc] init];
    [NSApp addObserver:observer forKeyPath:@"effectiveAppearance" options:0 context:nullptr];
}

// --- printing ----------------------------------------------------------------

// keeps the document alive while the print sheet is up, then deletes the
// temporary PDF it was made from
@interface SumatraPrintJob : NSObject {
  @public
    PDFDocument* doc;
    NSString* path;
}
@end

@implementation SumatraPrintJob
- (void)printOperationDidRun:(NSPrintOperation*)op success:(MacObjcBool)success contextInfo:(void*)ctx {
    (void)op;
    (void)success;
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
    // balances the retain in AppShellPrintPdf
    (void)CFBridgingRelease(ctx);
}
@end

// the macOS print panel for a PDF file, as a sheet on the window. On success
// the file is deleted once the panel closes.
bool AppShellPrintPdf(MainWindow* win, Str path, Str jobTitle) {
    NSView* view = win ? (__bridge NSView*)gp::PlatWindowHandle(win->gpuiWin) : nil;
    NSWindow* window = view.window;
    NSString* nsPath = ToNSString(path);
    if (!window || !nsPath) {
        return false;
    }
    PDFDocument* doc = [[PDFDocument alloc] initWithURL:[NSURL fileURLWithPath:nsPath]];
    if (!doc) {
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
    job->path = nsPath;
    [op runOperationModalForWindow:window
                          delegate:job
                    didRunSelector:@selector(printOperationDidRun:success:contextInfo:)
                       contextInfo:(__bridge_retained void*)job];
    return true;
}
