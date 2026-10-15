/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's OLE drag-out of the canvas (Canvas.cpp): the selected text or an image
// dragged to another app. gpui has no drag source, so AppKit's dragging session
// starts from the frame's view. The session takes the mouse-up: onEnd tells the
// canvas the button is up again.

#include "gui/GpuiBridge.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#pragma pop_macro("defer")
#include "base/MacTypesShow.h"

#include "gui/DragOut.h"

// longest edge of the dragged image's picture, in points (orig's
// kDragImageThumbnailSize)
constexpr CGFloat kDragImageThumbnailSize = 220;
// the dragged text's picture: its first characters on one line
constexpr int kDragTextPreviewChars = 60;

@interface SumatraDragSource : NSObject <NSDraggingSource>
@property(nonatomic) DragOutEndFn onEnd;
@property(nonatomic) void* user;
@end

@implementation SumatraDragSource
- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    return NSDragOperationCopy;
}

- (void)draggingSession:(NSDraggingSession*)session
           endedAtPoint:(NSPoint)screenPoint
              operation:(NSDragOperation)operation {
    if (self.onEnd) {
        self.onEnd(self.user, operation != NSDragOperationNone);
    }
}
@end

static SumatraDragSource* gDragSource = nil;

static NSString* ToNSStr(Str s) {
    return [[NSString alloc] initWithBytes:s.s length:(NSUInteger)len(s) encoding:NSUTF8StringEncoding];
}

// The mouse event the drag starts from. A test's input is gpui's, not AppKit's,
// so then there is none: make one where the mouse is.
static NSEvent* DragEvent(NSView* view, gp::Window* gw) {
    NSEvent* ev = [NSApp currentEvent];
    if (ev && (ev.type == NSEventTypeLeftMouseDragged || ev.type == NSEventTypeLeftMouseDown)) {
        return ev;
    }
    NSPoint inView = NSMakePoint(gw->mouseX, view.isFlipped ? gw->mouseY : view.bounds.size.height - gw->mouseY);
    NSPoint inWindow = [view convertPoint:inView toView:nil];
    return [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
                              location:inWindow
                         modifierFlags:0
                             timestamp:NSProcessInfo.processInfo.systemUptime
                          windowNumber:view.window.windowNumber
                               context:nil
                           eventNumber:0
                            clickCount:1
                              pressure:1];
}

static bool BeginDrag(gp::Window* gw, NSPasteboardItem* item, NSImage* picture, DragOutEndFn onEnd, void* user) {
    NSView* view = gw ? (__bridge NSView*)gp::PlatWindowHandle(gw) : nil;
    if (!view || !view.window) {
        return false;
    }
    NSEvent* ev = DragEvent(view, gw);
    if (!ev) {
        return false;
    }
    if (!gDragSource) {
        gDragSource = [SumatraDragSource new];
    }
    gDragSource.onEnd = onEnd;
    gDragSource.user = user;

    // the picture is centered on the mouse
    NSDraggingItem* di = [[NSDraggingItem alloc] initWithPasteboardWriter:item];
    NSPoint at = [view convertPoint:ev.locationInWindow fromView:nil];
    NSSize sz = picture.size;
    [di setDraggingFrame:NSMakeRect(at.x - (sz.width / 2), at.y - (sz.height / 2), sz.width, sz.height)
                contents:picture];
    @try {
        NSDraggingSession* session = [view beginDraggingSessionWithItems:@[ di ] event:ev source:gDragSource];
        session.animatesToStartingPositionsOnCancelOrFail = YES;
        return session != nil;
    } @catch (NSException* e) {
        return false;
    }
}

// the text's first line, drawn as the system draws a label
static NSImage* TextPicture(NSString* text) {
    NSString* line = [text componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet].firstObject;
    if (line.length > kDragTextPreviewChars) {
        line = [[line substringToIndex:kDragTextPreviewChars] stringByAppendingString:@"…"];
    }
    NSDictionary* attrs = @{
        NSFontAttributeName : [NSFont systemFontOfSize:NSFont.systemFontSize],
        NSForegroundColorAttributeName : NSColor.labelColor
    };
    NSSize sz = [line sizeWithAttributes:attrs];
    sz.width = MAX(ceil(sz.width), 1);
    sz.height = MAX(ceil(sz.height), 1);
    return [NSImage imageWithSize:sz
                          flipped:NO
                   drawingHandler:^BOOL(NSRect r) {
                     [line drawAtPoint:NSZeroPoint withAttributes:attrs];
                     return YES;
                   }];
}

bool MacDragOutText(gp::Window* gw, Str text, DragOutEndFn onEnd, void* user) {
    NSString* s = ToNSStr(text);
    if (s.length == 0) {
        return false;
    }
    NSPasteboardItem* item = [NSPasteboardItem new];
    [item setString:s forType:NSPasteboardTypeString];
    return BeginDrag(gw, item, TextPicture(s), onEnd, user);
}

// PNG and TIFF for the apps that take an image, the same PNG as a file for the
// ones that take files (Finder copies it where it is dropped)
bool MacDragOutImage(gp::Window* gw, Str png, Str fileName, DragOutEndFn onEnd, void* user) {
    NSData* data = [NSData dataWithBytes:png.s length:(NSUInteger)len(png)];
    NSImage* image = [[NSImage alloc] initWithData:data];
    if (!image || image.size.width <= 0 || image.size.height <= 0) {
        return false;
    }
    NSPasteboardItem* item = [NSPasteboardItem new];
    [item setData:data forType:NSPasteboardTypePNG];
    NSData* tiff = image.TIFFRepresentation;
    if (tiff) {
        [item setData:tiff forType:NSPasteboardTypeTIFF];
    }
    NSString* dir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"SumatraPDF-drag"];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    NSString* path = [dir stringByAppendingPathComponent:ToNSStr(fileName)];
    if ([data writeToFile:path atomically:YES]) {
        [item setString:[NSURL fileURLWithPath:path].absoluteString forType:NSPasteboardTypeFileURL];
    }

    CGFloat k = kDragImageThumbnailSize / MAX(image.size.width, image.size.height);
    if (k < 1) {
        image.size = NSMakeSize(floor(image.size.width * k), floor(image.size.height * k));
    }
    return BeginDrag(gw, item, image, onEnd, user);
}

// for tests: the types on the drag pasteboard (or the general one, the
// clipboard), and its text when it has some
TempStr MacPasteboardTemp(bool drag) {
    NSPasteboard* pb = drag ? [NSPasteboard pasteboardWithName:NSPasteboardNameDrag] : NSPasteboard.generalPasteboard;
    str::Builder out;
    // grows with every write, so a test can tell new contents from old
    out.Append(fmt("changes=%d types=", (int)pb.changeCount));
    bool first = true;
    for (NSPasteboardType t in pb.types) {
        out.Append(fmt(first ? "%s" : ",%s", Str(t.UTF8String)));
        first = false;
    }
    NSString* s = [pb stringForType:NSPasteboardTypeString];
    if (s) {
        out.Append(fmt(" text='%s'", Str(s.UTF8String)));
    }
    return ToStrTemp(out);
}
