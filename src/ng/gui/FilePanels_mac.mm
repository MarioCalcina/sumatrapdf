/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// macOS's open panel for the open prompts: several files at once and orig's
// file-type filters, which gpui's PathPrompt has neither of.

#include "gui/GpuiBridge.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#pragma pop_macro("defer")
#include "base/MacTypesShow.h"

#include "base/File.h"
#include "Translations.h"
#include "gui/FilePanels.h"

#include "SumatraLog.h"

static NSString* ToNSString(Str s) {
    return [[NSString alloc] initWithBytes:s.s length:(NSUInteger)len(s) encoding:NSUTF8StringEncoding];
}

// the extensions of one filter group ("*.pdf;*.xps" -> pdf, xps); nil when it
// matches every file ("*.*"). A pattern that is not "*.<ext>" (file_id.diz,
// *.fb2.zip) can't be an allowed type and is left out.
static NSArray<NSString*>* ExtensionsOf(Str patterns) {
    NSMutableArray<NSString*>* exts = [NSMutableArray array];
    StrVec parts;
    Split(&parts, patterns, StrL(";"), true);
    for (Str p : parts) {
        if (str::Eq(p, StrL("*.*")) || str::Eq(p, StrL("*"))) {
            return nil;
        }
        if (!str::StartsWith(p, StrL("*.")) || len(p) < 3) {
            continue;
        }
        Str ext(p.s + 2, len(p) - 2);
        if (str::IndexOfChar(ext, '.') >= 0) {
            continue;
        }
        [exts addObject:ToNSString(ext)];
    }
    return exts;
}

// the popup under the panel's file list that switches between the groups
@interface SumatraFilterChoice : NSObject
@property(weak) NSOpenPanel* panel;
// per popup item: NSArray<NSString*> of extensions, or NSNull for all files
@property(strong) NSArray* types;
- (void)filterChanged:(NSPopUpButton*)sender;
@end

@implementation SumatraFilterChoice
- (void)filterChanged:(NSPopUpButton*)sender {
    NSInteger i = sender.indexOfSelectedItem;
    if (i < 0 || i >= (NSInteger)self.types.count) {
        return;
    }
    id t = self.types[(NSUInteger)i];
    self.panel.allowedFileTypes = [t isKindOfClass:[NSArray class]] ? t : nil;
}
@end

// Adds orig's filter ("Name\1*.a;*.b\1Name\1*.*\1") to the panel: the first
// group applies, a popup picks another. Returns what keeps the popup's target
// alive while the panel is up.
static SumatraFilterChoice* AddFilters(NSOpenPanel* panel, Str filter) {
    StrVec parts;
    Split(&parts, filter, StrL("\1"), true);
    if (len(parts) < 2) {
        return nil;
    }
    NSMutableArray* types = [NSMutableArray array];
    NSPopUpButton* popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 320, 26) pullsDown:NO];
    for (int i = 0; i + 1 < len(parts); i += 2) {
        NSArray<NSString*>* exts = ExtensionsOf(parts[i + 1]);
        if (exts && exts.count == 0) {
            continue;
        }
        [popup addItemWithTitle:ToNSString(parts[i])];
        [types addObject:exts ? (id)exts : (id)[NSNull null]];
    }
    if (types.count == 0) {
        return nil;
    }
    id first = types[0];
    panel.allowedFileTypes = [first isKindOfClass:[NSArray class]] ? first : nil;
    if (types.count == 1) {
        return nil;
    }

    auto* choice = [[SumatraFilterChoice alloc] init];
    choice.panel = panel;
    choice.types = types;
    popup.target = choice;
    popup.action = @selector(filterChanged:);
    [popup sizeToFit];

    NSTextField* label = [NSTextField labelWithString:ToNSString(Tr("File type:"))];
    [label sizeToFit];
    constexpr CGFloat kGap = 8;
    constexpr CGFloat kPad = 12;
    CGFloat dy = std::max(popup.frame.size.height, label.frame.size.height) + 2 * kPad;
    CGFloat dx = label.frame.size.width + kGap + popup.frame.size.width + 2 * kPad;
    NSView* box = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, dx, dy)];
    [label setFrameOrigin:NSMakePoint(kPad, (dy - label.frame.size.height) / 2)];
    [popup setFrameOrigin:NSMakePoint(kPad + label.frame.size.width + kGap, (dy - popup.frame.size.height) / 2)];
    [box addSubview:label];
    [box addSubview:popup];
    panel.accessoryView = box;
    panel.accessoryViewDisclosed = YES;
    return choice;
}

bool MacOpenPanel(Str title, Str filter, Str initialPath, OpenPanelSelect select, StrVec* pathsOut) {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = select == OpenPanelSelect::Many;
    if (len(title) > 0) {
        panel.message = ToNSString(title);
    }
    TempStr dir = len(initialPath) == 0 ? TempStr{}
                  : dir::Exists(initialPath) ? str::DupTemp(initialPath)
                                              : path::GetDirTemp(initialPath);
    if (len(dir) > 0 && dir::Exists(dir)) {
        panel.directoryURL = [NSURL fileURLWithPath:ToNSString(dir) isDirectory:YES];
    }
    SumatraFilterChoice* choice = AddFilters(panel, filter);
    NSModalResponse res = [panel runModal];
    choice = nil;
    if (res != NSModalResponseOK) {
        return false;
    }
    for (NSURL* url in panel.URLs) {
        const char* s = url.path.UTF8String;
        if (s && *s) {
            pathsOut->Append(Str((char*)s));
        }
    }
    logf("MacOpenPanel: %d file(s)\n", len(*pathsOut));
    return len(*pathsOut) > 0;
}
