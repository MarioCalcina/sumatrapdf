/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// AppKit and IOKit services gpui has no counterpart for: images on the
// pasteboard (gpui writes text only), Finder's file icons, keeping the display
// awake, mail, the default PDF reader, the running instance's port and
// cursors made from pixels.

#include "base/Base.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#import <CoreServices/CoreServices.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#pragma pop_macro("defer")
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

static NSString* ToNSStr(Str s) {
    return [[NSString alloc] initWithBytes:s.s length:(NSUInteger)len(s) encoding:NSUTF8StringEncoding];
}

// a new message in the mail app with the file attached. Through the sharing
// service: no permission to script Mail needed
bool MacComposeEmail(Str path) {
    NSString* p = ToNSStr(path);
    if (!p) {
        return false;
    }
    NSURL* url = [NSURL fileURLWithPath:p];
    NSSharingService* mail = [NSSharingService sharingServiceNamed:NSSharingServiceNameComposeEmail];
    if (!mail || ![mail canPerformWithItems:@[ url ]]) {
        return false;
    }
    mail.subject = url.lastPathComponent;
    [mail performWithItems:@[ url ]];
    return true;
}

static NSString* const kPdfType = @"com.adobe.pdf";

bool MacIsDefaultPdfReader() {
    NSString* me = NSBundle.mainBundle.bundleIdentifier;
    if (@available(macOS 12.0, *)) {
        NSURL* app = [[NSWorkspace sharedWorkspace] URLForApplicationToOpenContentType:UTTypePDF];
        return app && [[NSBundle bundleWithURL:app].bundleIdentifier isEqualToString:me];
    }
    CFStringRef handler = LSCopyDefaultRoleHandlerForContentType((__bridge CFStringRef)kPdfType, kLSRolesAll);
    bool isMe = handler && [(__bridge NSString*)handler caseInsensitiveCompare:me] == NSOrderedSame;
    if (handler) {
        CFRelease(handler);
    }
    return isMe;
}

// macOS may ask the user to confirm
void MacMakeDefaultPdfReader() {
    if (@available(macOS 12.0, *)) {
        [[NSWorkspace sharedWorkspace] setDefaultApplicationAtURL:NSBundle.mainBundle.bundleURL
                                                toOpenContentType:UTTypePDF
                                                completionHandler:^(NSError* err) {
                                                  if (err) {
                                                      logf("MacMakeDefaultPdfReader: %s\n",
                                                           Str((char*)err.localizedDescription.UTF8String));
                                                  }
                                                }];
        return;
    }
    LSSetDefaultRoleHandlerForContentType((__bridge CFStringRef)kPdfType, kLSRolesAll,
                                          (__bridge CFStringRef)NSBundle.mainBundle.bundleIdentifier);
}

// --- the running instance ---------------------------------------------------

// A command line started while SumatraPDF runs hands its files to it (orig's
// ReuseInstance) as DDE commands sent to this port. The reply is the running
// instance's pid, so the sender can bring it to the front.
static NSString* const kInstancePortName = @"org.sumatrapdfreader.SumatraPDF.instance";
static void (*gOnInstanceCmds)(Str) = nullptr;
static CFMessagePortRef gInstancePort = nullptr;

static CFDataRef OnInstanceMessage(CFMessagePortRef, SInt32, CFDataRef data, void*) {
    if (data && gOnInstanceCmds) {
        gOnInstanceCmds(Str((char*)CFDataGetBytePtr(data), (int)CFDataGetLength(data)));
    }
    pid_t pid = getpid();
    return CFDataCreate(nullptr, (const UInt8*)&pid, sizeof(pid));
}

// false when another instance has the port
bool MacStartInstanceServer(void (*onCmds)(Str)) {
    if (gInstancePort) {
        return true;
    }
    CFMessagePortContext ctx{};
    gInstancePort =
        CFMessagePortCreateLocal(nullptr, (__bridge CFStringRef)kInstancePortName, OnInstanceMessage, &ctx, nullptr);
    if (!gInstancePort) {
        return false;
    }
    gOnInstanceCmds = onCmds;
    CFRunLoopSourceRef src = CFMessagePortCreateRunLoopSource(nullptr, gInstancePort, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopCommonModes);
    CFRelease(src);
    return true;
}

static void ActivateApp(pid_t pid) {
    NSRunningApplication* app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if (!app.bundleURL) {
        return;
    }
    // what `open -a` does: a launched process may not activate another app itself
    NSWorkspaceOpenConfiguration* cfg = [NSWorkspaceOpenConfiguration configuration];
    cfg.activates = YES;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    [[NSWorkspace sharedWorkspace] openApplicationAtURL:app.bundleURL
                                          configuration:cfg
                                      completionHandler:^(NSRunningApplication*, NSError*) {
                                        dispatch_semaphore_signal(done);
                                      }];
    constexpr int64_t kWaitSecs = 3;
    dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, kWaitSecs * NSEC_PER_SEC));
}

// false when no instance runs or it doesn't answer
bool MacSendToRunningInstance(Str cmds) {
    CFMessagePortRef port = CFMessagePortCreateRemote(nullptr, (__bridge CFStringRef)kInstancePortName);
    if (!port) {
        return false;
    }
    constexpr CFTimeInterval kTimeoutSecs = 5;
    CFDataRef data = CFDataCreate(nullptr, (const UInt8*)cmds.s, len(cmds));
    CFDataRef reply = nullptr;
    SInt32 res =
        CFMessagePortSendRequest(port, 0, data, kTimeoutSecs, kTimeoutSecs, kCFRunLoopDefaultMode, &reply);
    CFRelease(data);
    CFRelease(port);
    pid_t pid = 0;
    bool ok = res == kCFMessagePortSuccess && reply && CFDataGetLength(reply) == sizeof(pid);
    if (ok) {
        memcpy(&pid, CFDataGetBytePtr(reply), sizeof(pid));
    }
    if (reply) {
        CFRelease(reply);
    }
    if (ok) {
        ActivateApp(pid);
    }
    return ok;
}

// "/Applications/SumatraPDF.app"
TempStr MacAppBundlePathTemp() {
    return str::DupTemp(Str((char*)NSBundle.mainBundle.bundlePath.fileSystemRepresentation));
}

// A cursor from premultiplied BGRA pixels at a display's `scale`; the hot spot
// is in points from the top left. Retained: MacCursorRelease frees it.
void* MacCursorNew(const Pixmap* pm, float scale, int hotX, int hotY) {
    if (!pm || pm->format != PixmapFormat::BGRA8 || !(scale > 0)) {
        return nullptr;
    }
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo info = (CGBitmapInfo)kCGImageAlphaPremultipliedFirst | (CGBitmapInfo)kCGBitmapByteOrder32Little;
    CGContextRef ctx = CGBitmapContextCreate(pm->data, (size_t)pm->width, (size_t)pm->height, 8, (size_t)pm->stride,
                                             cs, info);
    CGColorSpaceRelease(cs);
    if (!ctx) {
        return nullptr;
    }
    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (!img) {
        return nullptr;
    }
    NSSize size = NSMakeSize(pm->width / scale, pm->height / scale);
    NSImage* image = [[NSImage alloc] initWithCGImage:img size:size];
    CGImageRelease(img);
    NSCursor* cursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(hotX, hotY)];
    return (__bridge_retained void*)cursor;
}

void MacCursorRelease(void* cursor) {
    if (cursor) {
        CFRelease(cursor);
    }
}

void MacCursorSet(void* cursor) {
    [(__bridge NSCursor*)cursor set];
}

bool MacCursorIsCurrent(void* cursor) {
    return cursor && [NSCursor currentCursor] == (__bridge NSCursor*)cursor;
}
