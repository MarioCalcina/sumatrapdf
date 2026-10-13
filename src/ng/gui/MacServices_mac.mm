/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// AppKit and IOKit services gpui has no counterpart for: images on the
// pasteboard (gpui writes text only), keeping the display awake.

#include "base/Base.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#pragma pop_macro("defer")
#undef BOOL
#include "base/MacTypesShow.h"

#include "gui/MacServices.h"

// PNG for the apps that read it, TIFF for the ones that only take AppKit's
// own image type (Preview's New from Clipboard, older Cocoa apps)
bool MacPasteboardSetPng(Str png) {
    if (len(png) == 0) {
        return false;
    }
    NSData* data = [NSData dataWithBytes:png.s length:(NSUInteger)len(png)];
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    bool ok = [pb setData:data forType:NSPasteboardTypePNG];
    NSImage* img = [[NSImage alloc] initWithData:data];
    NSData* tiff = img.TIFFRepresentation;
    if (tiff) {
        ok = [pb setData:tiff forType:NSPasteboardTypeTIFF] || ok;
    }
    return ok;
}

static IOPMAssertionID gNoSleep = kIOPMNullAssertionID;

// Fullscreen.PreventSleep: no display sleep while a document is shown fullscreen
void MacPreventDisplaySleep(bool on) {
    if (on == (gNoSleep != kIOPMNullAssertionID)) {
        return;
    }
    if (!on) {
        IOPMAssertionRelease(gNoSleep);
        gNoSleep = kIOPMNullAssertionID;
        return;
    }
    IOReturn res = IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
                                               CFSTR("SumatraPDF fullscreen"), &gNoSleep);
    if (res != kIOReturnSuccess) {
        gNoSleep = kIOPMNullAssertionID;
    }
}
