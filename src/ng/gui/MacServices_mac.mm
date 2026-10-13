/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// AppKit and IOKit services gpui has no counterpart for: images on the
// pasteboard (gpui writes text only), Finder's file icons, keeping the display
// awake.

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

#include "base/Pixmap.h"
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

// the icon Finder shows for the file, or for its extension when the file is
// gone: `px` pixels square, premultiplied BGRA
Pixmap* MacFileIconPixmap(Str path, int px) {
    if (px <= 0 || len(path) == 0) {
        return nullptr;
    }
    NSString* p = [[NSString alloc] initWithBytes:path.s length:(NSUInteger)len(path) encoding:NSUTF8StringEncoding];
    NSWorkspace* ws = [NSWorkspace sharedWorkspace];
    NSImage* icon = nil;
    if (p && [[NSFileManager defaultManager] fileExistsAtPath:p]) {
        icon = [ws iconForFile:p];
    } else if (p.pathExtension.length > 0) {
        icon = [ws iconForFileType:p.pathExtension];
    }
    if (!icon) {
        return nullptr;
    }
    Pixmap* pm = AllocPixmap(px, px, PixmapFormat::BGRA8, true);
    if (!pm) {
        return nullptr;
    }
    memset(pm->data, 0, (size_t)pm->stride * (size_t)px);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    // BGRA, premultiplied: the Pixmap layout
    CGBitmapInfo info = (CGBitmapInfo)kCGImageAlphaPremultipliedFirst | (CGBitmapInfo)kCGBitmapByteOrder32Little;
    CGContextRef ctx = CGBitmapContextCreate(pm->data, (size_t)px, (size_t)px, 8, (size_t)pm->stride, cs, info);
    CGColorSpaceRelease(cs);
    if (!ctx) {
        FreePixmap(pm);
        return nullptr;
    }
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:ctx flipped:NO]];
    [icon drawInRect:NSMakeRect(0, 0, px, px)
            fromRect:NSZeroRect
           operation:NSCompositingOperationSourceOver
            fraction:1.0];
    [NSGraphicsContext restoreGraphicsState];
    CGContextRelease(ctx);
    pm->hasAlpha = true;
    return pm;
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
